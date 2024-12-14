import Chart from 'chart.js/auto';

let chart;
let startTime = Date.now();

async function fetchSteps() {
  try {
    const response = await fetch('http:///3.133.102.136:5000/?total_steps_taken');
    const data = await response.json();
    return data.steps;
  } 
  catch (error) {
    console.error('Error fetching steps:', error);
    return null;
  }
}

async function updateChart() {
  const steps = await fetchSteps();
  if (steps !== null) {
    const elapsedSeconds = Math.floor((Date.now() - startTime) / 1000); // Calculate elapsed seconds
    chart.data.labels.push(elapsedSeconds);
    chart.data.datasets[0].data.push(steps);
    chart.update();
  }
}

async function initializeChart() {
  const canvas = document.getElementById('StepCounter');
  const ctx = canvas.getContext('2d');

  chart = new Chart(ctx, {
    type: 'line', 
    data: {
      labels: [1, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100], 
      datasets: [
        {
          label: 'Steps', 
          data: [], 
          borderColor: "red",
          borderWidth: 2, 
        },
      ],
    },
    options: {
      responsive: true, 
      scales: {
        x: {
          title: {
            display: true, text: 'Time (s)',
          },
        },
        y: {
          title: {
            display: true, text: 'Steps', 
          },
        },
      },
    },
  });
  setInterval(updateChart, 1000);
}
initializeChart();
