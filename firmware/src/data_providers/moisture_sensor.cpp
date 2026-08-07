#include <data_providers/moisture_sensor.hpp>

namespace osmium {

MoistureSensorDataProvider::MoistureSensorDataProvider(int charge_pin, int sense_pin)
  : sensor(charge_pin, sense_pin) {
  *this->mutex.lock() = (uint8_t *)&this->moisture;
}

void MoistureSensorDataProvider::begin() {
  this->sensor.begin();
  this->moisture = 0;
}

const DataProvider::Metadata MoistureSensorDataProvider::metadata = {
  .label = "Moisture",
  .unit  = "%",

  .dtype = "Q0.16",
  .dsize = 2
};

int MoistureSensorDataProvider::sample() {
  auto lock = this->mutex.lock();
  this->moisture = this->sensor.sample();
  return SAMPLE_OK;
}

Mutex<uint8_t *>::Guard MoistureSensorDataProvider::lock_data() {
  return this->mutex.lock();
}

}
