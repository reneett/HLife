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
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <TFT_eSPI.h>

//accelerometer stuff
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);
int steps = 0;
float threshold = 1.5;
float baseX = 0.0;
float baseY = 0.0;
float baseZ = 0.0;

//wifi information
char ssid[50]; 
char pass[50]; 

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30 * 1000;

// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

TFT_eSPI tft = TFT_eSPI();

//calibrates acccelerometer for 10 seconds
void calibrateSensor() {
  const int samples = 100;
  float tempX = 0.0, tempY = 0.0, tempZ = 0.0;

  for (int i = 0; i < samples; i++) {
    sensors_event_t event;
    adxl.getEvent(&event);
    tempX += event.acceleration.x;
    tempY += event.acceleration.y;
    tempZ += event.acceleration.z;
    delay(100);
  }
  baseX = tempX / samples;
  baseY = tempY / samples;
  baseZ = tempZ / samples;
  Serial.println("Done Calibrating.");
}

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

// updates display to show steps taken and steps remaining
void updateDisplay() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  float height = tft.height()/2;
  float width = tft.width()/2;
  tft.drawString("Step Count: ", 0, height-50, 2);
  char str[100];
  sprintf(str, "%d", steps);
  tft.drawString(str, width, height-20, 2);
  char str2[100];
  sprintf(str2, "%d steps left!", 10000-steps);
  tft.drawString(str2, 0, height+20, 1);
  if(10000-steps <= 2000){
    tft.drawString("Almost there!", 0, height+40, 1);
  } else if (steps%3 == 0) {
    tft.drawString("You're doing great!", 0, height+40, 1);
  } else {
    tft.drawString("You can do it!", 0, height+40, 1);
  }
  
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
  
  calibrateSensor();

  tft.init();
  tft.setRotation(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
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

  float rootmeansquare = sqrt(accelX*accelX+accelY*accelY+accelZ*accelZ);
  float basemeansquare = sqrt(baseX*baseX+baseY*baseY+baseZ*baseZ);
  Serial.println(basemeansquare+threshold);
  if (rootmeansquare > basemeansquare+threshold) {
    steps++;
    updateDisplay();
  }

  char url[100];
  sprintf(url, "/?total_steps_taken=%d", steps);
  err = http.get("3.133.102.136", 5000, url, NULL);
  
  if (err == 0) {
    Serial.println("startedRequest ok");
    err = http.responseStatusCode();
    if (err >= 0) {
      Serial.print("Got status code: ");
      Serial.println(err);
      err = http.skipResponseHeaders();
      if (err >= 0) {
        
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

  delay(300); //to account for time during the step
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
    char ssid[] = "";
    char pass[] = "";
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