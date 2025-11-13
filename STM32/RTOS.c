#include "HardwareAPI.h"
#include <Arduino.h>
#include <math.h>

#define THERMISTOR_PIN PA0
#define FAN_RELAY_PIN PA1
#define FAN_CURRENT_PIN PA2
#define PELTIER_RELAY_PIN PA3
#define PELTIER_CURRENT_PIN PA4
#define RX PA10
#define TX PA9

HardwareAPI hardwareAPI(THERMISTOR_PIN, FAN_RELAY_PIN, FAN_CURRENT_PIN,
                        PELTIER_RELAY_PIN, PELTIER_CURRENT_PIN);
HardwareSerial Serial1(RX, TX);
HardwareTimer Timer2(TIM2);

// global variables
int sampleCount = 0;
const int num_samples = 10;

// task struct
typedef struct {
    int state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*Function)(int);
} task;

// periods
const unsigned long TICK = 1;           // 1 ms
const unsigned long samp_period = 1;    // 1khz
const unsigned long send_period = samp_period * num_samples;
const unsigned long relay_period = 50;

// tasks
const int numTasks = 3;
task tasks[numTasks];

// data
volatile float avgFanCurrent = 0;
float avgPeltierCurrent = 0;
float avgFanVoltage = 0;
float avgPeltierVoltage = 0;
float avgFanPower = 0;
float avgPeltierPower = 0;
float avgTempF = 0;

// states
enum SAMP_DATA_ST {SAMPLE_INIT, SAMP_READ, SAMP_AVG};
enum SEND_DATA_ST {SEND};
enum RELAY_CTRL_ST {RELAY};

int SampleData(int state);
int SendData(int state);
int RelayControl(int state);

// task functions
int SampleData(int state)
{
    switch (state){
        case SAMPLE_INIT:
            printTask("SampleData");
            sampleCount = 0;
            avgFanCurrent = 0;
            avgPeltierCurrent = 0;
            avgFanVoltage = 0;
            avgPeltierVoltage = 0;
            avgFanPower = 0;
            avgPeltierPower = 0;
            avgTempF = 0;
            state = SAMP_READ;
            break;
        case SAMP_READ:
            avgFanCurrent += hardwareAPI.getFanCurrent();
            avgPeltierCurrent += hardwareAPI.getPeltierCurrent();
            avgFanVoltage += hardwareAPI.getFanVoltage();
            avgPeltierVoltage += hardwareAPI.getPeltierVoltage();
            avgFanPower += hardwareAPI.getFanPower();
            avgPeltierPower += hardwareAPI.getPeltierPower();
            avgTempF += hardwareAPI.getTemperature();
            sampleCount++;

            if (sampleCount >= num_samples) state = SAMP_AVG;
            break;
        case SAMP_AVG:
            avgFanCurrent /= num_samples;
            avgPeltierCurrent /= num_samples;
            avgFanVoltage /= num_samples;
            avgPeltierVoltage /= num_samples;
            avgFanPower /= num_samples;
            avgPeltierPower /= num_samples;
            avgTempF /= num_samples;
            state = SAMPLE_INIT;
            break;
    }

    return state;
}
  
int SendData(int state)
{   
    printTask("SendData");
    Serial1.println("Fan Current: ");
    Serial1.print(avgFanCurrent);
    Serial1.println("Peltier Current: ");
    Serial1.print(avgPeltierCurrent);

    Serial1.println("Fan Voltage: ");
    Serial1.print(avgFanVoltage);
    Serial1.println("Peltier Voltage: ");
    Serial1.print(avgPeltierVoltage);

    Serial1.println("Fan Power: ");
    Serial1.print(avgFanPower);
    Serial1.println("Peltier Power: ");
    Serial1.print(avgPeltierPower);

    Serial1.println("Temp: ");
    Serial1.print(avgTempF);

    if (Serial1.available()) {
        String message = Serial1.readStringUntil('\n');
        Handle_My_ESP(message);
        Serial.print(message);
    }
    return state;
}

int RelayControl(int state)
{
    printTask("RelayControl");
    float temp = hardwareAPI.getTemperature();
    if (temp > 80.0f) hardwareAPI.turnFanOff();
    else hardwareAPI.turnFanOn();
    return state;
}

// front end button interrupt handler
void Handle_My_ESP(String message) 
{
    message.trim();

    if (message == "FAN_ON") {
        hardwareAPI.turnFanOn();
        Serial.println("Fan turned on");
    }
    else if (message == "FAN_OFF") {
        hardwareAPI.turnFanOff();
        Serial.println("Fan turned off");
    }
    else if (message == "PELTIER_ON") {
        hardwareAPI.turnPeltierOn();
        Serial.println("Peltier turned on");
    }
    else if (message == "PELTIER_OFF") {
        hardwareAPI.turnPeltierOff();
        Serial.println("Peltier turned off");
    }
    else {
        Serial.println("Unknown command");
    }

}
// timer
void TimerISR()
{
    for (int i = 0; i < numTasks; i++)
    {
        if (tasks[i].elapsedTime >= tasks[i].period){
            tasks[i].state = tasks[i].Function(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += TICK;
    }
}

void setup() {
    Serial1.begin(115200);
    Serial1.println("Hey, ESP ! STM32 RTOS Initialized");
    Serial.begin(115200);

    tasks[0].state = SAMPLE_INIT;
    tasks[0].period = samp_period;
    tasks[0].elapsedTime = samp_period;
    tasks[0].Function = &SampleData;

    tasks[1].state = SEND;
    tasks[1].period = send_period;
    tasks[1].elapsedTime = send_period;
    tasks[1].Function = &SendData;

    tasks[2].state = RELAY;
    tasks[2].period = relay_period;
    tasks[2].elapsedTime = relay_period;
    tasks[2].Function = &RelayControl;

    Timer2.setPrescaleFactor(80);
    Timer2.setOverflow(1000, HERTZ_FORMAT);
    Timer2.attachInterrupt(TimerISR);
    Timer2.resume();
}

void loop() {

}

// helper
void printTask(const char* taskName){
    Serial.println("Task ");
    Serial.print(taskName);
}