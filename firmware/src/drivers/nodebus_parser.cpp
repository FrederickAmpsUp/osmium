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

    if (this->state == READ_HEADER || this->state == READ_PAYLOAD)
      this->computed_crc = update_crc16(this->computed_crc, byte);

    switch (this->state) {
      case WAIT_SOF:
        if (byte == NODEBUS_SOF[this->bytes_read])
          this->bytes_read++; // continue reading if the byte matches the nth byte of SOF
        else if (byte == NODEBUS_SOF[0])
          this->bytes_read = 1; // restart from 1 if the byte matches the first byte of SOF
        else
          this->bytes_read = 0; // restart from 0 if the byte does not match

        if (this->bytes_read == NODEBUS_SOF_SIZE) {
          log.trace("SOF matched, reading packet.");
          this->state = READ_HEADER;

          this->bytes_read = 0;
          this->packet_id = 0;
          this->payload_size = 0;
          this->computed_crc = NODEBUS_CRC_INIT;
        }
      break;
      case READ_HEADER:
        if (this->bytes_read == 0) {
          this->packet_id = byte;
          if (this->packet_id && this->id != this->packet_id) {
            log.debug("Ignoring packet addressed %d.", this->packet_id);
            this->state = WAIT_SOF;
            this->bytes_read = 0;
            break;
          }
          this->bytes_read++;
        } else if (this->bytes_read == 1) {
          this->packet_version = byte;
          if (this->packet_version > NODEBUS_PACKET_VERSION_V1) {
            log.debug("Ignoring packet with version %d.", this->packet_version);
            this->state = WAIT_SOF;
            this->bytes_read = 0;
            break;
          }
          this->bytes_read++;
        } else {
          this->payload_size |= (byte & 0x7F) << (this->bytes_read * 7 - 14);
          if (!(byte & 0x80)) {
            if (this->payload_size > NODEBUS_PAYLOAD_MAX_SIZE) {
              log.warn("Payload oversized at %d bytes.", this->payload_size);
            }
            // don't read empty payload
            this->state = (this->payload_size > 0) ? READ_PAYLOAD : READ_CRC;
            this->bytes_read = 0;
          } else {
            this->bytes_read++;
          }
        }
      break;
      case READ_PAYLOAD:
        if (this->bytes_read < NODEBUS_PAYLOAD_MAX_SIZE)
          this->payload[this->bytes_read] = byte;
        if (this->bytes_read >= this->payload_size - 1) {
          this->state = READ_CRC;
          this->bytes_read = 0;
        } else {
          this->bytes_read++;
        }
      break;
      case READ_CRC:
        if (this->bytes_read == 0) {
          this->packet_crc = byte << 8;
          this->bytes_read++;
        } else {
          this->packet_crc |= byte;

          this->bytes_read = 0;
          this->state = WAIT_SOF;

          if (this->packet_crc == this->computed_crc) {
            return READY;
          } else {
            log.warn("CRC mismatch! Computed: 0x%04X, Received: 0x%04X", this->computed_crc, this->packet_crc);
            return CRC_ERR;
          }
        }
      break;
    }
  }

  return PARSING;
}

const uint8_t *NodebusParser::get_payload(uint8_t *size) const {
  if (size) *size = this->payload_size;
  return this->payload;
}

} // namespace osmium
