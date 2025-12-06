
#include "HardwareAPI.h"


// Constructor
HardwareAPI::HardwareAPI(int thermistorPin, int fanRelayPin, int fanCurrentPin, int peltierRelayPin, int peltierCurrentPin) {
    _thermistorPin = thermistorPin;
    _fanRelayPin = fanRelayPin;
    _fanCurrentPin = fanCurrentPin;
    _peltierRelayPin = peltierRelayPin;
    _peltierCurrentPin = peltierCurrentPin;

    _fanRelayStatus = 0;
    _peltierRelayStatus = 0;
    pinMode(fanRelayPin, OUTPUT);
    pinMode(peltierRelayPin, OUTPUT);

    analogReadResolution(12);

    // turnFanOff();
    // turnPeltierOff();
    // _baseFanADCValue = _getCurrentADC(1000, _fanCurrentPin);
    // _basePeltierADCValue = _getCurrentADC(1000, _peltierCurrentPin);

    _testing = 0;

}

// Constructor for Testing
HardwareAPI::HardwareAPI(bool testing) {
    _testing = 1;
}


void HardwareAPI::setBaseADC() {
    turnFanOff();
    turnPeltierOff();
    delay(5000);
    _baseFanADCValue = _getCurrentADC(5000, _fanCurrentPin);
    _basePeltierADCValue = _getCurrentADC(5000, _peltierCurrentPin);
}


// Thermistor

float HardwareAPI::getTemperature() {
    if (_testing) return random(73, 75);
    int adcValue = analogRead(_thermistorPin);
    float voltage = adcValue * (_thermistorVCC / _adcRange);
    float thermistorResistance = _thermistorResistorValue * (_thermistorVCC / voltage - 1);

    float kelvin = 1.0 / ((1.0 / 298.15) + (1.0 / 3950) * log(thermistorResistance / 10000));
    float celsius = kelvin - 273.15;
    float fahrenheit = (celsius * 9.0 / 5.0) + 32.0;

    return fahrenheit;
}



// Fan
void HardwareAPI::turnFanOn() {
    _fanRelayStatus = 1;
    if (_testing) return;
    digitalWrite(_fanRelayPin, HIGH);
}


void HardwareAPI::turnFanOff() {
    _fanRelayStatus = 0;
    if (_testing) return;
    digitalWrite(_fanRelayPin, LOW);
}


bool HardwareAPI::toggleFan() {
    _fanRelayStatus = !_fanRelayStatus;
    if (_testing) return _fanRelayStatus;
    digitalWrite(_fanRelayPin, _fanRelayStatus ? HIGH : LOW);
    return _fanRelayStatus;
}

bool HardwareAPI::getFanStatus() {
    return _fanRelayStatus;
}





// Peltier
void HardwareAPI::turnPeltierOn() {
    _peltierRelayStatus = 1;
    if (_testing) return;
    digitalWrite(_peltierRelayPin, HIGH);
}


void HardwareAPI::turnPeltierOff() {
    _peltierRelayStatus = 0;
    if (_testing) return;
    digitalWrite(_peltierRelayPin, LOW);
}


bool HardwareAPI::togglePeltier() {
    _peltierRelayStatus = !_peltierRelayStatus;
    if (_testing) return _peltierRelayStatus;
    digitalWrite(_peltierRelayPin, _peltierRelayStatus ? HIGH : LOW);
    return _peltierRelayStatus;
}

bool HardwareAPI::getPeltierStatus() {
    return _peltierRelayStatus;
}




// Fan get methods

float HardwareAPI::getFanCurrent() {
    return _getCurrent(_fanCurrentPin);
}

float HardwareAPI::getFanCurrent(int samples) {
    return _getCurrent(samples, _fanCurrentPin);
}

float HardwareAPI::getFanVoltage() {
    return _getVoltage(_fanCurrentPin);
}

float HardwareAPI::getFanPower() {
    return _getPower(_fanCurrentPin);
}

float HardwareAPI::getFanPower(int samples) {
    return _getPower(samples, _fanCurrentPin);
}



// Peltier get methods

float HardwareAPI::getPeltierCurrent() {
    return _getCurrent(_peltierCurrentPin);
}

float HardwareAPI::getPeltierCurrent(int samples) {
    return _getCurrent(samples, _peltierCurrentPin);
}

float HardwareAPI::getPeltierVoltage() {
    return _getVoltage(_peltierCurrentPin);
}

float HardwareAPI::getPeltierPower() {
    return _getPower(_peltierCurrentPin);
}

float HardwareAPI::getPeltierPower(int samples) {
    return _getPower(samples, _peltierCurrentPin);
}






// Current Sensor
float HardwareAPI::_getCurrent(int sensorPin) {
    return _getCurrent(1, sensorPin);
}

int HardwareAPI::_testRead(int sensorPin) {
    if (sensorPin == _fanCurrentPin) {
        return getFanStatus() ? random(_baseFanADCValue + 40, _baseFanADCValue + 50) : random(_baseFanADCValue -10, _baseFanADCValue + 10);
    } else {
        return getFanStatus() ? random(_basePeltierADCValue + 115, _basePeltierADCValue + 140) : random(_basePeltierADCValue -10, _basePeltierADCValue + 10);
    }
}

float HardwareAPI::_getCurrent(int samples, int sensorPin) {
    float adcValue = 0;
    for (int i = 0; i < samples; i++) {
        if (_testing) {
            adcValue += _testRead(sensorPin);
        } else {
            adcValue += analogRead(sensorPin);
        }
    }
    
    adcValue /= samples;

    float voltage = adcValue * (3.3 / 4095.0);
    float difference = abs(voltage - (sensorPin == _fanCurrentPin ? (_baseFanADCValue / 4095.0 * 3.3) : (_basePeltierADCValue / 4095.0 * 3.3)));
    float current = difference / ((.185 * (sensorPin == _fanCurrentPin ? _fanMultiplier : _peltierMultiplier))/2);
    // current = 0;

    if (current <= .15) current = 0;

    return current;
}

float HardwareAPI::_getCurrentADC(int samples, int sensorPin) {
    float adcValue = 0;
    for (int i = 0; i < samples; i++) {
        adcValue += analogRead(sensorPin);
    }
    return (adcValue / samples);
}

float HardwareAPI::_getVoltage(int sensorPin) {
    return _powerCalculationVoltage;
}

float HardwareAPI::_getPower(int sensorPin) {
    return _getCurrent(1) * _getVoltage(sensorPin);
}

float HardwareAPI::_getPower(int samples, int sensorPin) {
    return _getCurrent(sensorPin) * _getVoltage(sensorPin);
}
