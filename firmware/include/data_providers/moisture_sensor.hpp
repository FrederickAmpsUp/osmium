#pragma once

#include "core/data_provider.hpp"
#include "drivers/moisture_sensor.hpp"

namespace osmium {

class MoistureSensorDataProvider : public DataProvider {
public:
  MoistureSensorDataProvider(int charge_pin, int sense_pin);

  // no copy/move
  MoistureSensorDataProvider(MoistureSensorDataProvider &&) = delete;
  MoistureSensorDataProvider &operator=(MoistureSensorDataProvider &&) = delete;
  MoistureSensorDataProvider(const MoistureSensorDataProvider&) = delete;
  MoistureSensorDataProvider& operator=(const MoistureSensorDataProvider&) = delete;

  void begin();

  static const Metadata metadata;

  virtual const Metadata *get_metadata() const override {
    return &metadata;
  }

  virtual int sample() override;

  virtual Mutex<uint8_t *>::Guard lock_data() override;

  virtual ~MoistureSensorDataProvider() override = default;
private:
  MoistureSensor sensor;

  uint16_t moisture;
  Mutex<uint8_t *> mutex;
};

}
