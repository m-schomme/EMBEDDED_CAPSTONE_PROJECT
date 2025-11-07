#include "HardwareAPI.h"
#include <Arduino.h>
#include <math.h>

#define THERMISTOR_PIN xx
#define RELAY_PIN xx
#define CURRENT_SENSOR_PIN xx

HardwareAPI hardwareAPI(THERMISTOR_PIN, RELAY_PIN, CURRENT_SENSOR_PIN);

// task struct
typedef struct {
    int state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*Function)(int);
} task;

// periods
const int num_samples = 10;
const unsigned long TICK = 1;           // 1 ms
const unsigned long samp_period = 1;    // 1khz
const unsigned long send_period = samp_period * num_samples;
const unsigned long relay_period = 50;

// tasks
const int numTasks = 3;
task tasks[numTasks];

// data
float avgCurrent = 0;
float avgTempF = 0;
float avgPower = 0;

// states
enum SAMP_DATA_ST {SAMPLE_INIT, SAMP_READ, SAMP_AVG};
enum SEND_DATA_ST {SEND};
enum RELAY_CTRL_ST {RELAY};

int SampleData(int state);
int SendData(int state);
int RelayControl(int state);

// task functions
int SampleData(int state, int num_samples)
{
    switch (state){
        case SAMPLE_INIT:
            sampleCount = 0;
            avgCurrent = 0;
            avgTempF = 0;
            avgPower = 0;
            state = SAMP_READ;
            break;
        case SAMP_READ:
            avgCurrent += hardwareAPI.getCurrent();
            avgTempF += hardwareAPI.getTemperature();
            avgPower += hardwareAPI.getPower();
            samp_count++;

            if (sampleCount >= num_samples) state = SAMP_AVG;
            break;
        case SAMP_AVG:
            avgCurrent /= num_samples;
            avgTempF /= num_samples;
            avgPower /= num_samples;

            state = SAMPLE_INIT;
            break;
    }

    return state;
}

int SendData(int state)
{   
    Serial.print("Current:");
    Serial.print(avgCurrent);
    Serial.print(" Voltage:");
    Serial.print(HardwareAPI.getVoltage());
    Serial.print(" Power:");
    Serial.print(avgPower);
    Serial.print(" Temperature:");
    Serial.println(avgTempF);
    return state;
}

int RelayControl(int state)
{
    float temp = hardwareAPI.getTemperature();
    if (temp > 80.0f) hardwareAPI.turnRelayOff();
    else hardwareAPI.turnRelayOn();
    return state;
}

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
    Serial.begin(9600);

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

}

void loop() {
  TimerISR();
  delay(TICK);
}
