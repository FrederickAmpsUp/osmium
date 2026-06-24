#include "drivers/nodebus_parser.hpp"
#include "logging/logger.hpp"

namespace osmium {

NodebusParser::NodebusParser(uint8_t id) : id(id) {
}

NodebusParser::Result NodebusParser::update(Stream &stream) {
  static Logger &log = Logger::global();

  if (stream.available() <= 0) {
    return (this->state == WAIT_SOF) ? WAITING : PARSING;
  }

  size_t rem_bytes = BYTE_BUDGET;

  while (stream.available() && rem_bytes-- > 0) {
    uint8_t byte = stream.read();

    switch (this->state) {
      case WAIT_SOF:
        if (byte == SOF_BYTE) {
          this->state = READ_HEADER;
          this->bytes_read = 0;

          this->header_id = 0;
          this->payload_size = 0;
        }
      break;
      case READ_HEADER:
        if (this->bytes_read == 0) {
          this->header_id = byte;
          if (this->header_id && this->id != this->header_id) {
            log.debug("Ignoring packet addressed %d.", this->header_id);
            this->state = WAIT_SOF;
            break;
          }
          this->bytes_read++;
        } else {
          this->payload_size = byte + 1;
          if (this->payload_size > PAYLOAD_MAX_SIZE) {
            log.warn("Payload oversized at %d bytes.", this->payload_size);
          }
          this->state = READ_PAYLOAD;
          this->bytes_read = 0;
        }
      break;
      case READ_PAYLOAD:
        // todo
      break;
      case READ_CRC:
        // todo
      break;
    }
  }

  return PARSING;
}

} // namespace osmium
