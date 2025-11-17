#include "HardwareAPI.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>

#define THERMISTOR_PIN PA_0
#define FAN_RELAY_PIN PD_6
#define FAN_CURRENT_PIN PA_1
#define PELTIER_RELAY_PIN PD_5
#define PELTIER_CURRENT_PIN PA_2
#define RX PA10
#define TX PA9

HardwareAPI hardwareAPI(THERMISTOR_PIN, FAN_RELAY_PIN, FAN_CURRENT_PIN,
                        PELTIER_RELAY_PIN, PELTIER_CURRENT_PIN);
HardwareSerial Serial1(RX, TX);
HardwareTimer Timer2(TIM2);

// global variables
int sampleCount = 0;
const int num_samples = 5000;
int fanStatus = 0;
int pelStatus = 0;
int logData = 0;

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
volatile float avgPeltierCurrent = 0;
volatile float avgFanVoltage = 0;
volatile float avgPeltierVoltage = 0;
volatile float avgFanPower = 0;
volatile float avgPeltierPower = 0;
volatile float avgTempF = 0;

// states
enum SAMP_DATA_ST {SAMPLE_INIT, SAMP_READ, SAMP_AVG};
enum SEND_DATA_ST {SEND};
enum RELAY_CTRL_ST {RELAY};

int SampleData(int state);
int SendData(int state);
int RelayControl(int state);
volatile bool SendFlag = false;

// task functions
int SampleData(int state)
{
    switch (state){
        case SAMPLE_INIT:
            sampleCount = 0;
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
            SendFlag = true;
            sampleCount = 0;
            state = SAMP_READ;
            break;
    }

    return state;
}
  
int SendData(int state)
{   
    if (!SendFlag) return state;
    Serial1.print(avgFanVoltage); Serial1.print(",");
    Serial1.print(avgFanCurrent); Serial1.print(",");
    Serial1.print(avgFanPower); Serial1.print(",");
    Serial1.print(avgPeltierVoltage); Serial1.print(",");
    Serial1.print(avgPeltierCurrent); Serial1.print(",");
    Serial1.print(avgPeltierPower); Serial1.print(",");
    Serial1.print(avgTempF); Serial1.print(",");
    Serial1.print(fanStatus); Serial1.print(",");
    Serial1.print(pelStatus); Serial1.print(",");
    Serial1.println(logData);


    avgFanCurrent = 0;
    avgPeltierCurrent = 0;
    avgFanVoltage = 0;
    avgPeltierVoltage = 0;
    avgFanPower = 0;
    avgPeltierPower = 0;
    avgTempF = 0;

    SendFlag = false;
    return state;
}

int RelayControl(int state)
{
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
volatile bool TimerFlag = false;
void TimerISR()
{
    TimerFlag = true;
}

void setup() {
    Serial1.begin(115200);
    Serial1.println("Hey, ESP ! STM32 RTOS Initialized");
    Serial.begin(115200);
    Serial.println("RTOS STARTED");

    // turn fan and peltier on initially
    hardwareAPI.turnFanOn();
    fanStatus = 1;
    hardwareAPI.turnPeltierOn();
    pelStatus = 1;

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
    if (TimerFlag) {
        TimerFlag = false;
        for (int i = 0; i < numTasks; i++)
        {
            if (tasks[i].elapsedTime >= tasks[i].period){
                tasks[i].state = tasks[i].Function(tasks[i].state);
                tasks[i].elapsedTime = 0;
            }
            tasks[i].elapsedTime += TICK;
        }
    }
    if (Serial1.available()) {
        String message = Serial1.readStringUntil('\n');
        Handle_My_ESP(message);
        Serial.print(message);
    }
}
