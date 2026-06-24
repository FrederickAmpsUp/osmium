#include "logging/serial_sink.hpp"
#include <Arduino.h>

namespace osmium {

void SerialSink::write(const char *str) {
  Serial.println(str);
}

} // namespace osmium
