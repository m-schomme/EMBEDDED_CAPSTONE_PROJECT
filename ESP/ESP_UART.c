#define RX 21
#define TX 19
HardwareSerial mySerial(2);
String incoming = "";
void setup() {
  delay(5000);
  Serial.begin(115200);
  mySerial.begin(115200, SERIAL_8N1, RX, TX);
  Serial.println("ESP32 ready");
}

void loop() {
  // read data
  if (mySerial.available()) {
    char c = mySerial.read();
    // Serial.print(c);
    if (c == '|') {
      Serial.print("Rec eived from STM32: ");
      Serial.println(incoming);
      incoming = "";
    } else {
      incoming += c;
    }
  }
  if(Serial.available()){
    mySerial.println(Serial.readStringUntil('\n'));
  }

  // send data
  // mySerial.println("Hello from ESP");
  // delay(5000);
  delay(10);
}
