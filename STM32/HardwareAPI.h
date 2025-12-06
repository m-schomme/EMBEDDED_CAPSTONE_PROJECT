#include "Arduino.h"


class HardwareAPI {

public:
    // Constructor
    HardwareAPI(int thermistorPin, int fanRelayPin, int fanCurrentPin, int peltierRelayPin, int peltierCurrentPin);
    // Constructor for testing
    HardwareAPI(bool testing);

    // Temperature
    float getTemperature();  // Returns Fahrenheit value

    // Fan
    void turnFanOn();
    void turnFanOff();
    bool toggleFan();
    bool getFanStatus();

    // Fan Sensor
    float getFanCurrent();
    float getFanCurrent(int samples);
    float getFanVoltage();
    float getFanPower();
    float getFanPower(int samples);

    
    // Peltier
    void turnPeltierOn();
    void turnPeltierOff();
    bool togglePeltier();
    bool getPeltierStatus();
    
    // Peltier Sensor
    float getPeltierCurrent();
    float getPeltierCurrent(int samples);
    float getPeltierVoltage();
    float getPeltierPower();
    float getPeltierPower(int samples);

    void setBaseADC();

private:

    int _adcRange = 4095;

    // Thermistor
    // Pins
    int _thermistorPin;

    // Values
    float _thermistorResistorValue = 10000;
    float _thermistorVCC = 3.3;

    // Fan Relay
    int _fanRelayPin;
    bool _fanRelayStatus;
    
    // Fan Current Sensor
    int _fanCurrentPin;
    
    // Peltier Relay
    int _peltierRelayPin;
    bool _peltierRelayStatus;
    
    // Peltier Current Sensor
    int _peltierCurrentPin;


    // Power calculation variables
    float _powerCalculationVoltage = 5;
    float _max_voltage = .5921 * 5;
    // float _zero_voltage = 1.47;


    float _getCurrentADC(int samples, int sensorPin);

    // Power Helpers
    float _getCurrent(int sensorPin);
    float _getCurrent(int samples, int sensorPin);
    float _getVoltage(int sensorPin);
    float _getPower(int sensorPin);
    float _getPower(int samples, int sensorPin);

    int _baseFanADCValue = 1798;
    int _basePeltierADCValue = 1798;

    float _peltierMultiplier = .92;
    float _fanMultiplier = .51;



    /*
        Testing environment variables
    */
    bool _testing = 0;

    int _testRead(int sensorPin);


};

