#pragma once

#include <HardwareSerial.h>

namespace osmium {

class RS485 {
public:
  RS485(int rx, int tx, int dd, int port=1);

  void begin();
private:
  HardwareSerial hardware_serial;
};

} // namespace osmium
