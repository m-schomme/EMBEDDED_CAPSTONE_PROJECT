#include "HardwareAPI.h"
#include <Arduino.h>
#include <math.h>

#define THERMISTOR_PIN PA0
#define FAN_RELAY_PIN PA1
#define FAN_CURRENT_PIN PA2
#define PELTIER_RELAY_PIN PA3
#define PELTIER_CURRENT_PIN PA4

HardwareAPI hardwareAPI(THERMISTOR_PIN, FAN_RELAY_PIN, FAN_CURRENT_PIN,
                        PELTIER_RELAY_PIN, PELTIER_CURRENT_PIN);
HardwareSerial Serial1(PA10, PA9);

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
    return state;
}

int RelayControl(int state)
{
    float temp = hardwareAPI.getTemperature();
    if (temp > 80.0f) hardwareAPI.turnFanOff();
    else hardwareAPI.turnFanOn();
    return state;
}

// timer
TIM_HandleTypeDef htim2;
void TIM2_Callback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        for (int i = 0; i < numTasks; i++)
        {
            if (tasks[i].elapsedTime >= tasks[i].period){
                tasks[i].state = tasks[i].Function(tasks[i].state);
                tasks[i].elapsedTime = 0;
            }
            tasks[i].elapsedTime += TICK;
     }
    }
}

static void MX_TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 80 - 1;              // 1 MHz
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000 - 1;               // 1 kHz
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(&htim2);
    HAL_TIM_Base_Start_IT(&htim2);

    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

// interrupt handler
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

// clock (stolen from STM32CubeIDE :) 
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage*/
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Handle_My_Error();
  }

  /** Initializes the RCC Oscillators according to the specified parameters in the RCC_OscInitTypeDef structure.*/
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Handle_My_Error();
  }

  /** Initializes the CPU, AHB and APB buses clocks*/
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Handle_My_Error();
  }
}

// error handler
void Handle_My_Error() 
{
  while (1)
  {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }
}

void setup() {
    Serial.begin(115200); // to see on debugger
    Serial1.begin(9600);
    Serial.println("STM32 RTOS Initialized");

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

    HAL_Init();
    SystemClock_Config();
    MX_TIM2_Init();
}

void loop() {
  delay(1000);
}