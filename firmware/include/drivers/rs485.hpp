#pragma once

#include <HardwareSerial.h>
#include <Stream.h>

namespace osmium {

class RS485 : public Stream {
public:
  RS485(int rx, int tx, int dd, int port=1);

  void begin();

  virtual int available() override { return this->hardware_serial.available(); }
  virtual int read() override { return this->hardware_serial.read(); }
  virtual int peek() override { return this->hardware_serial.peek(); }

  virtual inline size_t write(uint8_t v) override { return this->hardware_serial.write(v); }
  virtual inline size_t write(const uint8_t *b, size_t s) override { return this->hardware_serial.write(b, s); }
  virtual inline int availableForWrite() override { return this->hardware_serial.availableForWrite(); }
  virtual void flush() override { return this->hardware_serial.flush(); }

  virtual ~RS485() override = default;
private:
  HardwareSerial hardware_serial;
};

} // namespace osmium
