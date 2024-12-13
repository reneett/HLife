#include <Arduino.h>
#include <HttpClient.h>
#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <Wire.h>

//adxl345
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

#include <TFT_eSPI.h>

//accelerometer stuff
int steps = 0;
float threshold = 1.2;
float baseX = 0.0;
float baseY = 0.0;

// This example downloads the URL "http://arduino.cc/"
char ssid[50]; // your network SSID (name)
char pass[50]; // your network password (use for WPA, or use
// as key for WEP)

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30 * 1000;

// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

TFT_eSPI tft = TFT_eSPI();
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);

/* void calibrateSensor() {
    const int samples = 100;
    float tempX = 0.0, tempY = 0.0;

    for (int i = 0; i < samples; i++) {
        tempX += myIMU.readFloatAccelX();
        tempY += myIMU.readFloatAccelY();
        delay(10);
    }

    baseX = tempX / samples;
    baseY = tempY / samples;
    Serial.println("Done Calibrating.");
} */

void nvs_access() {
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
    err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  // Open
  Serial.printf("\n");
  Serial.printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else {
    Serial.printf("Done\n");
    Serial.printf("Retrieving SSID/PASSWD\n");
    size_t ssid_len;
    size_t pass_len;
    err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    err |= nvs_get_str(my_handle, "pass", pass, &pass_len);
    switch (err) {
      case ESP_OK:
        Serial.printf("Done\n");
        break;
      case ESP_ERR_NVS_NOT_FOUND:
        Serial.printf("The value is not initialized yet!\n");
        break;
      default:
        Serial.printf("Error (%s) reading!\n", esp_err_to_name(err));
    }
  }
  // Close
  nvs_close(my_handle);
}

void updateDisplay() {
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  tft.drawCentreString("Step Count: ", 160, 80, 2);
  char str[100];
  sprintf(str, "%d", steps);
  tft.drawCentreString(str, 160, 100, 2);
  char str2[1000];
  sprintf(str2, "%d", 10000-steps);
  sprintf(str2, " steps remaining!");
  tft.drawCentreString(str2, 160, 120, 2);
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  // Retrieve SSID/PASSWD from flash before anything else
  nvs_access();

  delay(1000);
  //accelerometer stuff
  Wire.begin();
  if(!adxl.begin()){
    Serial.println("Could not find ADXL345, check wiring!");
    while(1);
  }
  
  //calibrateSensor();

  tft.init();
  updateDisplay();

  // We start by connecting to a WiFi network
  delay(1000);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  Serial.println(WiFi.macAddress());


}

void loop() {
  int err = 0;
  WiFiClient c;
  HttpClient http(c);

  sensors_event_t event;
  adxl.getEvent(&event);
  float accelX = event.acceleration.x;
  float accelY = event.acceleration.y;
  float accelZ = event.acceleration.z;

  float rootmeansquare =sqrt(accelX*accelX+accelY*accelY+accelZ*accelZ);
  if (rootmeansquare > threshold) {
    steps++;
    updateDisplay();
  }

  //err = http.get(kHostname, kPath);
  char url[100];
  sprintf(url, "/?total_steps_taken=%d", steps);
  err = http.get("3.16.83.61", 5000, url, NULL);
  
  if (err == 0) {
    Serial.println("startedRequest ok");
    err = http.responseStatusCode();
    if (err >= 0) {
      Serial.print("Got status code: ");
      Serial.println(err);
      // Usually you'd check that the response code is 200 or a
      // similar "success" code (200-299) before carrying on,
      // but we'll print out whatever response we get
      err = http.skipResponseHeaders();
      if (err >= 0) {
        int bodyLen = http.contentLength();
        Serial.print("Content length is: ");
        Serial.println(bodyLen);
        Serial.println();
        Serial.println("Body returned follows:");
        // Now we've got to the body, so we can print it out
        unsigned long timeoutStart = millis();
        char c;
        // Whilst we haven't timed out & haven't reached the end of the body
        while ((http.connected() || http.available()) &&
        ((millis() - timeoutStart) < kNetworkTimeout)) {
          if (http.available()) {
            c = http.read();
            // Print out this character
            Serial.print(c);
            bodyLen--;
            // We read something, reset the timeout counter
            timeoutStart = millis();
          } else {
            // We haven't got any data, so let's pause to allow some to
            // arrive
            delay(kNetworkDelay);
          }
        }
      } else {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err);
      }
    } else {
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  } else {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }
  http.stop();

  // And just stop, now that we've tried a download
  // while (1)
  //   ;
  delay(2000);
}

// to set the passcode
/* void setup() {
  Serial.begin(9600);
  delay(1000);
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
    err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  Serial.printf("\n");
  Serial.printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else {
    Serial.printf("Done\n");
    // Write
    Serial.printf("Updating ssid/pass in NVS ... ");
    char ssid[] = "Twilly15:)";
    char pass[] = "twilly1027HOTSPOT";
    err = nvs_set_str(my_handle, "ssid", ssid);
    err |= nvs_set_str(my_handle, "pass", pass);
    Serial.printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes
    // are written to flash storage. Implementations may write to storage at
    // other times, but this is not guaranteed.
    Serial.printf("Committing updates in NVS ... ");
    err = nvs_commit(my_handle);
    Serial.printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
    // Close
    nvs_close(my_handle);
  }
}

void loop() {
// put your main code here, to run repeatedly:
  delay(1000);
} */