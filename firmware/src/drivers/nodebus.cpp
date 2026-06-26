#include "drivers/nodebus.hpp"

namespace osmium {

uint16_t update_crc16(uint16_t crc, uint8_t data) {
  crc ^= ((uint16_t)data << 8);
  for (uint8_t i = 0; i < 8; i++) {
    if (crc & 0x8000) {
      crc = (crc << 1) ^ NODEBUS_CRC_POLY;
    } else {
      crc <<= 1;
    }
  }
  return crc;
}

}
