#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWebSocket.h>  // For WebSockets
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <LittleDB.h>  // https://github.com/pouriamoosavi/LittleDB
#include <WiFiManager.h>  // For WiFi configuration page

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* externalUrl = "http://192.168.1.100/api";  // Replace with the actual IP or hostname of the external device

String getDataJson() {
  int res = execQuery("select from data");  // Assuming this selects all rows
  String json = "[";
  if (res == 0) {
    for (uint32_t i = 0; i < selectedRows.rowsLen; i++) {
      SelectData_t* row = selectedRows.rows[i];
      String datetime = getText(row, "datetime");
      String sys_volts = getText(row, "sys_volts");
      String fan_status = getText(row, "fan_status");
      String fan_amps = getText(row, "fan_amps");
      String pel_status = getText(row, "pel_status");
      String pel_amps = getText(row, "pel_amps");
      String pel_temp = getText(row, "pel_temp");
      json += "{\"datetime\":\"" + datetime + "\",\"sys_volts\":" + sys_volts + ",\"fan_status\":" + fan_status + ",\"fan_amps\":" + fan_amps + ",\"pel_status\":" + pel_status + ",\"pel_amps\":" + pel_amps + ",\"pel_temp\":" + pel_temp + "},";
    }
    if (selectedRows.rowsLen > 0) {
      json.remove(json.length() - 1);
    }
  }
  json += "]";
  return json;
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    String json = getDataJson();
    client->text(json);
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  // WiFi configuration using WiFiManager
  WiFiManager wm;
  bool res = wm.autoConnect("ESP32ConfigAP");  // Starts AP for config if no credentials
  if (!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  } else {
    Serial.println("Connected to WiFi");
  }

  // Check and initialize database
  int dbRes = execQuery("use db monitoringdb");
  if (dbRes != 0) {
    dbRes = execQuery("create db monitoringdb");
    if (dbRes != 0) {
      Serial.println("Failed to create DB");
      return;
    }
    dbRes = execQuery("use db monitoringdb");
    if (dbRes != 0) {
      Serial.println("Failed to use DB after creation");
      return;
    }
  }

  // Create table if not exists (attempt create, ignore -2 if already exists)
  const char* createTableQuery = "create table data (datetime text, sys_volts text, fan_status tinyint, fan_amps text, pel_status tinyint, pel_amps text, pel_temp text)";
  dbRes = execQuery(createTableQuery);
  if (dbRes != 0 && dbRes != -2) {
    Serial.println("Failed to create table");
  }

  // WebSocket handler
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Serve the main webpage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", getIndexHtml());
  });

  // REST API to insert data (POST JSON: { "datetime": "YYYY-MM-DD HH:MM:SS", "sys_volts": 12.5, "fan_status": 1, "fan_amps": 1.2, "pel_status": 0, "pel_amps": 3.4, "pel_temp": 25.0 })
  server.on("/api/insert", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {  // Process only once for small payloads
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, data, len);
        if (error) {
          request->send(400, "text/plain", "Invalid JSON");
          return;
        }
        String datetime = doc["datetime"].as<String>();
        String sys_volts = doc["sys_volts"].as<String>();
        String fan_status = doc["fan_status"].as<String>();
        String fan_amps = doc["fan_amps"].as<String>();
        String pel_status = doc["pel_status"].as<String>();
        String pel_amps = doc["pel_amps"].as<String>();
        String pel_temp = doc["pel_temp"].as<String>();

        String query = "insert into data values (\"" + datetime + "\",\"" + sys_volts + "\",\"" + fan_status + "\",\"" + fan_amps + "\",\"" + pel_status + "\",\"" + pel_amps + "\",\"" + pel_temp + "\")";
        int res = execQuery(query.c_str());
        if (res == 0) {
          // Broadcast updated data to all WebSocket clients
          String json = getDataJson();
          ws.textAll(json);
          request->send(200, "text/plain", "OK");
        } else {
          request->send(500, "text/plain", "Insert failed");
        }
      }
    });

  server.begin();
}

void loop() {
  ws.cleanupClients();  // Clean up WebSocket clients periodically
}

const char* getIndexHtml() {
return R"raw(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Monitoring</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns/bundle.js"></script>
    <link href='https://fonts.googleapis.com/css?family=Oswald' rel='stylesheet'>
    <link href="https://fonts.googleapis.com/css2?family=Oswald:wght@200;300;400;500;600;700&display=swap" rel="stylesheet">
    <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-datalabels"></script>
</head>
<body>
    <h1 style="font-family: 'Oswald'; text-align: center;">24-HOUR ENERGY USAGE DASHBOARD</h1>

    <div style="display: flex; gap: 20px; height: calc(100vh - 120px);">
        <div style="flex: 1; display: flex; flex-direction: column; gap: 20px;">
            <div style="flex: 1; display: flex; flex-direction: column; min-height: 0;">
                <h3 style="margin: 0 0 10px 0;">CURRENT</h3>
                <div style="flex: 1; position: relative;">
                    <canvas id="currentChart"></canvas>
                </div>
            </div>
            <div style="flex: 1; display: flex; flex-direction: column; min-height: 0;">
                <h3 style="margin: 0 0 10px 0;">VOLTAGE</h3>
                <div style="flex: 1; position: relative;">
                    <canvas id="voltageChart"></canvas>
                </div>
            </div>
        </div>
        <div style="flex: 2; margin-left: 20px; display: flex; flex-direction: column; min-height: 0;">
            <h3 style="margin: 0 0 10px 0;">POWER & TEMPERATURE</h3>
            <div style="flex: 1; position: relative; min-height: 0;">
                <canvas id="powerTempChart"></canvas>
            </div>
            <div style="display: flex; gap: 10px; justify-content: flex-end; margin-top: 15px;">
                <button id="fanBtn" class="control-btn active" onclick="toggleButton('fanBtn')">FAN ON</button> 
                <button id="peltierBtn" class="control-btn active" onclick="toggleButton('peltierBtn')">PELTIER ON</button>
            </div>
        </div>
    </div>

    <script>
        Chart.register(ChartDataLabels);
        const current = document.getElementById('currentChart').getContext('2d');
        const voltage = document.getElementById('voltageChart').getContext('2d');
        const powerTemp = document.getElementById('powerTempChart').getContext('2d');

        let currentChart, voltageChart, powerTempChart;

        const externalUrl = 'http://192.168.1.100/api';
        const wsUrl = 'ws://' + location.host + '/ws';
        const socket = new WebSocket(wsUrl);

        socket.onmessage = function(event) {
            const data = JSON.parse(event.data);
            updateCharts(data);
        };

        function updateCharts(data) {
            if (data.length === 0) return;

            // Extract times and data
            const times = data.map(d => d.datetime ? d.datetime.substring(11, 16) : '');
            const fanAmps = data.map(d => parseFloat(d.fan_amps) || 0);
            const pelAmps = data.map(d => parseFloat(d.pel_amps) || 0);
            const sysVolts = data.map(d => parseFloat(d.sys_volts) || 0);
            const power = data.map(d => parseFloat(d.sys_volts) * (parseFloat(d.fan_amps) + parseFloat(d.pel_amps)) || 0);
            const temp = data.map(d => parseFloat(d.pel_temp) || 0);

            // Initialize or update Current chart
            if (!currentChart) {
                currentChart = new Chart(current, {
                    type: 'line',
                    data: {
                        labels: times,
                        datasets: [
                        {
                            label: 'Fan',
                            data: fanAmps,
                            borderColor: 'rgba(75, 192, 192, 1)',
                            borderWidth: 1,
                            pointBackgroundColor: 'rgba(75, 192, 192, 1)'
                        },
                        {
                            label: 'Peltier',
                            data: pelAmps,
                            borderColor: 'rgba(102, 255, 0, 1)',
                            borderWidth: 1,
                            pointBackgroundColor: 'rgba(102, 255, 0, 1)'
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: {
                            legend: {
                                labels: {
                                usePointStyle: true,
                                pointStyle: 'rect',
                                pointStyleWidth: 60,
                                }
                            },
                            datalabels: {
                                display: false,
                            },
                        },
                        scales: {
                            y: {
                                title: {
                                    display: true,
                                    text: 'Amps',
                                },
                                beginAtZero: true,
                            },
                            x: {
                                title: {
                                    display: true,
                                    text: 'Time',
                                },
                                beginAtZero: true,
                            },
                        },
                    }
                });
            } else {
                currentChart.data.labels = times;
                currentChart.data.datasets[0].data = fanAmps;
                currentChart.data.datasets[1].data = pelAmps;
                currentChart.update();
            }

            // Initialize or update Voltage chart
            if (!voltageChart) {
                voltageChart = new Chart(voltage, {
                    type: 'line',
                    id: 'voltageChart',
                    data: {
                        labels: times,
                        datasets: [
                        {
                            label: 'System Voltage',
                            data: sysVolts,
                            borderColor: 'rgba(153, 102, 255, 1)',
                            borderWidth: 1,
                            pointBackgroundColor: 'rgba(153, 102, 255, 1)'
                        }
                        ]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: {
                            legend: {
                                labels: {
                                usePointStyle: true,
                                pointStyle: 'rect',
                                pointStyleWidth: 60
                                }
                            },
                            datalabels: {
                                display: false,
                            },
                        },
                        scales: {
                            y: {
                                    title: {
                                        display: true,
                                        text: 'Volts',
                                    },
                                    beginAtZero: true,
                                },
                            x: {
                                title: {
                                    display: true,
                                    text: 'Time',
                                },
                                beginAtZero: true,
                            },
                        },
                    }
                });
            } else {
                voltageChart.data.labels = times;
                voltageChart.data.datasets[0].data = sysVolts;
                voltageChart.update();
            }

            // Initialize or update Power & Temperature chart
            if (!powerTempChart) {
                powerTempChart = new Chart(powerTemp, {
                    type: 'bar',
                    data: {
                        labels: times,
                        datasets: [
                        {
                            label: 'Power',
                            type: 'bar',
                            data: power,
                            borderColor: 'rgba(255, 159, 64, 1)',
                            borderWidth: 1,
                            backgroundColor: 'rgba(255, 159, 64, 0.6)',
                            yAxisID: 'y',
                            datalabels: {
                                anchor: 'end',
                                align: 'start',
                                clamp: true,
                                color: 'white',
                                font: {
                                    weight: '500',
                                    size: 24,
                                    family: 'Oswald',
                                },
                                formatter: (value) => value.toFixed(1),
                            },
                        },
                        {
                            label: 'Temperature',
                            type: 'line',
                            data: temp,
                            borderColor: 'rgba(255, 99, 132, 1)',
                            borderWidth: 1,
                            pointBackgroundColor: 'rgba(255, 99, 132, 1)',
                            yAxisID: 'y2',
                            datalabels: { display: false,},
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: {
                            legend: {
                                labels: {
                                usePointStyle: true,
                                pointStyle: 'rect',
                                pointStyleWidth: 60
                                }
                            },
                            datalabels: {
                                display: true,
                            }  
                        },
                        scales: {
                            y: {
                                type: 'linear',
                                title: {
                                    display: true,
                                    text: 'Watts',
                                },
                                beginAtZero: true,
                                },
                            y2: {
                                position: 'right',
                                title: {
                                    display: true,
                                    text: 'Fahrenheit',
                                },
                                beginAtZero: true,
                                grid: {
                                    drawOnChartArea: false,
                                }
                            },
                            x: {
                                title: {
                                    display: true,
                                    text: 'Time',
                                },
                                beginAtZero: true,
                            },
                        }
                    }
                });
            } else {
                powerTempChart.data.labels = times;
                powerTempChart.data.datasets[0].data = power;
                powerTempChart.data.datasets[1].data = temp;
                powerTempChart.update();
            }
        }

        // Toast notification function
        function showToast(message) {
            const toast = document.createElement('div');
            toast.textContent = message;
            toast.style.cssText = `
                position: fixed;
                top: 20px;
                right: 20px;
                background-color: #333;
                color: white;
                padding: 16px 24px;
                border-radius: 4px;
                z-index: 1000;
                font-family: Oswald;
                box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
                animation: slideIn 0.3s ease;
            `;
            document.body.appendChild(toast);
            setTimeout(() => toast.remove(), 3000);
        }

        // Toggle button ON/OFF state
        function toggleButton(buttonId) {
            const btn = document.getElementById(buttonId);
            const deviceName = buttonId === 'fanBtn' ? 'Fan' : 'Peltier';
            const deviceId = buttonId === 'fanBtn' ? 'fan' : 'peltier';
            
            if (btn.classList.contains('active')) {
                btn.classList.remove('active');
                btn.classList.add('inactive');
                btn.innerText = btn.innerText.replace('ON', 'OFF');
                showToast(deviceName + ' has been turned off');
                setDevice(deviceId, false);
            } else {
                btn.classList.remove('inactive');
                btn.classList.add('active');
                btn.innerText = btn.innerText.replace('OFF', 'ON');
                showToast(deviceName + ' has been turned on');
                setDevice(deviceId, true);
            }
        }

        
        async function setDevice(device, state) {
            try {
                await fetch(externalUrl + '/control', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ device, state })
                });
            } catch (e) {
                console.error('Failed to set device');
            }
        }
        
        setInterval(() => {
            if (socket.readyState === WebSocket.OPEN) {
                socket.send('refresh');
            }
        }, 1000);
    </script>
    <style>
        body {
            margin: 0;
            padding: 10px;
            font-family: Oswald, Arial, sans-serif;
        }
        h1 {
            font-family: Oswald;
            text-align: center;
        }
        h3 {
            font-family: Oswald;
            font-weight: 400;
            text-align: start;
            padding-left: 0;
        }
        .control-btn {
            padding: 12px 24px;
            font-size: 14px;
            font-family: Oswald;
            font-weight: 500;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .control-btn.active {
            background-color: #4CAF50;
            color: white;
            box-shadow: 0 2px 8px rgba(76, 175, 80, 0.4);
        }
        .control-btn.active:hover {
            background-color: #45a049;
            box-shadow: 0 4px 12px rgba(76, 175, 80, 0.6);
        }
        .control-btn.inactive {
            background-color: #cccccc;
            color: #666666;
            box-shadow: none;
        }
        .control-btn.inactive:hover {
            background-color: #b3b3b3;
        }
        @keyframes slideIn {
            from {
                transform: translateX(400px);
                opacity: 0;
            }
            to {
                transform: translateX(0);
                opacity: 1;
            }
        }
    </style>
</body>
</html>
)raw";
}


// External API Design (to be implemented on the other device):
// - POST /api/control : Body JSON { "device": "fan" or "peltier", "state": true/false } to set the state
// The other device should handle the actual GPIO for fan and peltier on/off.
// After changing state or periodically (e.g., every second), the external device should POST to this ESP32's /api/insert with current data, including statuses, to update DB and trigger WS broadcast.
