#pragma once

#include <Stream.h>
#include <cstdlib>
#include <cstdint>
#include "nodebus.hpp"

namespace osmium {

class NodebusParser {
public:
  NodebusParser(uint8_t id);

  enum Result {
    OVERISZE_ERR = -3,
    CRC_ERR = -2,
    WAITING = -1,
    PARSING =  0,
    READY   =  1 
  };

  Result update(Stream &stream);

  const uint8_t *get_payload(uint8_t *size = NULL) const;
  inline uint8_t get_sender_id() {
    return this->sender_id;
  }

  // max number of bytes to parse in one update() call
  static constexpr size_t BYTE_BUDGET = 8;
private:
  enum {
    WAIT_SOF,
    READ_HEADER,
    READ_PAYLOAD,
    READ_CRC
  } state = WAIT_SOF;

  uint8_t packet_id;
  uint8_t packet_version;

  uint8_t sender_id;
  size_t payload_size; 
  uint8_t payload[NODEBUS_PAYLOAD_MAX_SIZE];

  uint16_t packet_crc;
  uint16_t computed_crc;

  size_t bytes_read;

  uint8_t id;
};

} // namespace osmium
