#include "drivers/moisture_sensor.hpp"

#include <Arduino.h>

namespace osmium {

MoistureSensor::MoistureSensor(int charge_pin, int sense_pin) : charge_pin(charge_pin), sense_pin(sense_pin) {
}

void MoistureSensor::begin() {
  pinMode(this->charge_pin, OUTPUT);
  digitalWrite(this->charge_pin, LOW);

  analogReadResolution(ADC_BITS);
}

uint16_t MoistureSensor::sample() {
  return this->sample_raw(); /* TODO: calibration */
}

uint16_t MoistureSensor::sample_raw() {
  digitalWrite(this->charge_pin, LOW);
  delayMicroseconds(1000); // allow the measured capacitor to discharge

  uint32_t start = micros();
  digitalWrite(this->charge_pin, HIGH); // start charging

  // ~half charged
  // this is an arbitrary value but should be reasonable
  while (analogRead(this->sense_pin) < (1 << (ADC_BITS - 1))) {
    if (micros() - start > 1000)
      return UINT16_MAX;

    delayMicroseconds(1);
  }

  unsigned long long duration = micros() - start;

  // likely want to do some mapping here
  return duration;
}

}
