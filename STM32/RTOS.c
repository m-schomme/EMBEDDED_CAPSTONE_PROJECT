#include "HardwareAPI.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>

#define THERMISTOR_PIN PA0
#define FAN_RELAY_PIN PB10
#define FAN_CURRENT_PIN PA1
#define PELTIER_RELAY_PIN PB4
#define PELTIER_CURRENT_PIN PA4
#define THREE_VOLT PB5

HardwareAPI hardwareAPI(THERMISTOR_PIN, FAN_RELAY_PIN, FAN_CURRENT_PIN,
                        PELTIER_RELAY_PIN, PELTIER_CURRENT_PIN);
HardwareSerial Serial1(USART1);
HardwareTimer Timer2(TIM2);

// global variables
int sampleCount = 0;
const int num_samples = 1000;
int fanStatus = 0;
int pelStatus = 0;
String send_data;


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
const unsigned long send_period = num_samples * samp_period;
const unsigned long relay_period = 50;
const unsigned long log_period = 60000; // log data every min
// tasks
const int numTasks = 4;
task tasks[numTasks];

// data
volatile float avgFanCurrent = 0;
volatile float avgPeltierCurrent = 0;
volatile float avgFanVoltage = 0;
volatile float avgPeltierVoltage = 0;
volatile float avgFanPower = 0;
volatile float avgPeltierPower = 0;
volatile float avgTempF = 0;

volatile float powerManagementFanPowerSum = 0;
volatile float powerManagementPeltierPowerSum = 0;
volatile int powerManagementSumSamples = 0;

// states
enum SAMP_DATA_ST {SAMPLE_INIT, SAMP_READ, SAMP_AVG, AWAIT_SEND};
enum SEND_DATA_ST {SEND};
enum RELAY_CTRL_ST {RELAY};
enum LOG_DATA_ST {LOG_DATA};

int SampleData(int state);
int SendData(int state);
int RelayControl(int state);
int LogData(int state);
volatile bool SendFlag = false;
volatile bool logData = false;

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
            powerManagementFanPowerSum += hardwareAPI.getFanPower();
            powerManagementPeltierPowerSum += hardwareAPI.getPeltierPower();
            powerManagementSumSamples++;
            avgTempF += hardwareAPI.getTemperature();
            sampleCount++;
            if (sampleCount >= num_samples) state = SAMP_AVG;
            break;
        case SAMP_AVG:
            avgFanCurrent /= num_samples;
            avgPeltierCurrent /= num_samples;
            avgFanVoltage /= num_samples;
            avgPeltierVoltage /= num_samples;
            avgFanPower = avgFanVoltage * avgFanCurrent;
            avgPeltierPower = avgPeltierVoltage * avgPeltierCurrent;
            avgTempF /= num_samples;
            fanStatus = hardwareAPI.getFanStatus();
            pelStatus = hardwareAPI.getPeltierStatus();
            SendFlag = true;
            sampleCount = 0;
            state = AWAIT_SEND;
            break;
        case AWAIT_SEND:
            if (SendFlag) break;
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
    Serial1.print(logData);
    Serial1.print("|");

    avgFanCurrent = 0;
    avgPeltierCurrent = 0;
    avgFanVoltage = 0;
    avgPeltierVoltage = 0;
    avgFanPower = 0;
    avgPeltierPower = 0;
    avgTempF = 0;

    SendFlag = false;
    logData = false;
    return state;
}

int RelayControl(int state)
{
    float temp = hardwareAPI.getTemperature();
    if (temp > 80.0f) {
        hardwareAPI.turnFanOn();
        hardwareAPI.turnPeltierOn();
    }
    
    if (powerManagementSumSamples >= 40000) {
        powerManagementFanPowerSum /= powerManagementSumSamples;
        powerManagementPeltierPowerSum /= powerManagementSumSamples;
        if ((powerManagementFanPowerSum + powerManagementPeltierPowerSum) > 2){
            hardwareAPI.turnFanOff();
            hardwareAPI.turnPeltierOff();
        }
        powerManagementSumSamples = 0;
    }
    return state;
}

int LogData(int state){
    logData = true;
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
    Serial1.setTx(PA9);
    Serial1.setRx(PA10);
    Serial1.begin(115200);
    Serial1.println("Hey, ESP ! STM32 RTOS Initialized");
    Serial.begin(115200);
    Serial.println("RTOS STARTED");

    // intialize 3.3 V
    pinMode(THREE_VOLT, OUTPUT);
    digitalWrite(THREE_VOLT, HIGH);

    // turn fan and peltier on initially
    hardwareAPI.turnFanOn();
    hardwareAPI.turnPeltierOff();

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

    tasks[3].state = LOG_DATA;
    tasks[3].period = log_period;
    tasks[3].elapsedTime = log_period;
    tasks[3].Function = &LogData;

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
