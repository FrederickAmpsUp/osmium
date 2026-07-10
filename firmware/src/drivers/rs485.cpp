#include "drivers/rs485.hpp"

namespace osmium {

RS485::RS485(int rx, int tx, int dd, int port) : hardware_serial(port) {
  this->hardware_serial.setPins(rx, tx, -1, dd);
}

void RS485::begin(unsigned long baud) {
  this->hardware_serial.begin(baud);
  this->hardware_serial.setMode(UART_MODE_RS485_HALF_DUPLEX);
}

} // namespace osmium
