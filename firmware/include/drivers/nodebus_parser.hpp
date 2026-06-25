#pragma once

#include <Stream.h>
#include <cstdlib>
#include <cstdint>

namespace osmium {

class NodebusParser {
public:
  NodebusParser(uint8_t id);

  enum Result {
    CRC_ERR = -2,
    WAITING = -1,
    PARSING =  0,
    READY   =  1 
  };

  Result update(Stream &stream);

  const uint8_t *get_payload(uint8_t *size = NULL) const;

  // max number of bytes to parse in one update() call
  static constexpr size_t BYTE_BUDGET = 8;

  // may refactor these into a separate file later
  static constexpr uint8_t SOF[] = {0xDE, 0xAD, 0xBE, 0xEF};
  static constexpr size_t SOF_SIZE = sizeof(SOF);

  static constexpr size_t PAYLOAD_MAX_SIZE = 256;
private:
  static uint16_t update_crc16(uint16_t crc, uint8_t data);

  enum {
    WAIT_SOF,
    READ_HEADER,
    READ_PAYLOAD,
    READ_CRC
  } state = WAIT_SOF;

  uint8_t packet_id;
  uint16_t payload_size;
 
  uint8_t payload[PAYLOAD_MAX_SIZE];

  uint16_t packet_crc;
  uint16_t computed_crc;

  size_t bytes_read;

  uint8_t id;
};

} // namespace osmium
