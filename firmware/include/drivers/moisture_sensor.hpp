#pragma once

#include <cstdint>

namespace osmium {

class MoistureSensor {
public:
  MoistureSensor(int charge_pin, int sense_pin);

  void begin();

  // Q0.16, calibrated moisture
  uint16_t sample();

  uint16_t sample_raw();
private:
  int charge_pin, sense_pin;

  static constexpr uint8_t ADC_BITS = 12;
};

/* TODO: calibration curve/functions */

} // namespace osmium
