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

  // max number of bytes to parse in one update() call
  static constexpr size_t BYTE_BUDGET = 8;

  // may refactor these into a separate file later
  static constexpr uint8_t SOF_BYTE = 0xAA;
  static constexpr size_t PAYLOAD_MAX_SIZE = 256;
private:
  enum {
    WAIT_SOF,
    READ_HEADER,
    READ_PAYLOAD,
    READ_CRC
  } state = WAIT_SOF;

  uint8_t header_id;
  uint16_t payload_size;
 
  uint8_t payload[PAYLOAD_MAX_SIZE];

  uint8_t bytes_read;

  uint8_t id;
};

} // namespace osmium
