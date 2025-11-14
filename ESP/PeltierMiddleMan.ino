#include <WiFi.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <HardwareSerial.h>

#define RX_PIN 21
#define TX_PIN 19
HardwareSerial mySerial(2);


const char* ssid = "CALEB 1621";
const char* password = "Q52|3z54";


const char* serverHost = "Calebs-Laptop.local";
const int serverPort = 3000;
const char* websocketPath = "/?id=esp";


WebSocketsClient webSocket;


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WS Disconnected");
      break;
    case WStype_CONNECTED:
        Serial.printf("WS Connected");
        break;

    case WStype_TEXT: { // This is a message from the server
        Serial.printf("[WS] Got payload: %s\n", payload);

        // Parse the incoming JSON command
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
        }

        if (doc["type"] == "controlData") {
            JsonObject data = doc["data"];
            
            if (data.containsKey("fanStatus")) {
                bool fanOn = data["fanStatus"];
                Serial.printf("Setting Fan Status to: %s\n", fanOn ? "ON" : "OFF");

                // Communicate to STM32
                mySerial.printf("fan,%d\n", fanOn);
            }

            if (data.containsKey("pelStatus")) {
                bool pelOn = data["pelStatus"];
                Serial.printf("Setting Peltier Status to: %s\n", pelOn ? "ON" : "OFF");

                // Communicate to STM32
                mySerial.printf("peltier,%d\n", pelOn);
            }
        }
        break;
    }
    
    case WStype_PING:
        break;
    case WStype_PONG:
        break;
  }
}

void parseUART(String msg) {
  Serial.print("Received from STM32: ");
  Serial.println(msg);

  float fanVoltage, fanCurrent, fanPower, pelVoltage, pelCurrent, pelPower, temperature;

  int fanStatus, pelStatus, logData;

  int items = sscanf(msg.c_str(), "%f,%f,%f,%f,%f,%f,%f,%d,%d,%d", &fanVoltage, &fanCurrent, &fanPower, &pelVoltage, &pelCurrent, &pelPower, &temperature, &fanStatus, &pelStatus, &logData);

  if (items != 10) {
    Serial.printf("Wrong amt of data entered from STM32");
    return;
  }

  StaticJsonDocument<512> doc;
  doc["fanVoltage"] = fanVoltage;  // 5.0,0.1,0.5,5.0,0.1,0.5,76.3,1,1,0
  doc["fanCurrent"] = fanCurrent;
  doc["fanPower"] = fanPower;
  doc["pelVoltage"] = pelVoltage;
  doc["pelCurrent"] = pelCurrent;
  doc["pelPower"] = pelPower;
  doc["temperature"] = temperature;
  doc["fanStatus"] = (fanStatus == 1);
  doc["pelStatus"] = (pelStatus == 1);
  doc["logData"] = (logData == 1);


  StaticJsonDocument<768> wrapperObj;
  wrapperObj["type"] = "sensorData";
  wrapperObj["data"] = doc;

  String requestBody;
  serializeJson(wrapperObj, requestBody);

  Serial.print("Sending: ");
  Serial.println(requestBody);
  webSocket.sendTXT(requestBody);

}

void checkStm32Data() {
  if (mySerial.available()) {
    String msg = mySerial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      parseUART(msg);
    }
  }
}

void checkKeyboardInput() {
  if (Serial.available()) { // Check the Serial Monitor
    String msg = Serial.readStringUntil('\n'); // Read from the Serial Monitor
    msg.trim();
    if (msg.length() > 0) {
      parseUART(msg); // Send the keyboard data to your existing parser
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Connect to wifi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi Connected");
  
  // Connect to web socket
  webSocket.begin(serverHost, serverPort, websocketPath);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

}

void loop() {

  webSocket.loop();
  checkStm32Data(); // Checks for STM32 data

  // checkKeyboardInput();

}
