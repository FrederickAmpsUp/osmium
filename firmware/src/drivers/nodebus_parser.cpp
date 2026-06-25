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
        if (byte == SOF[this->bytes_read])
          this->bytes_read++; // continue reading if the byte matches the nth byte of SOF
        else if (byte == SOF[0])
          this->bytes_read = 1; // restart from 1 if the byte matches the first byte of SOF
        else
          this->bytes_read = 0; // restart from 0 if the byte does not match

        if (this->bytes_read == SOF_SIZE) {
          log.trace("SOF matched, reading packet.");
          this->state = READ_HEADER;

          this->bytes_read = 0;
          this->packet_id = 0;
          this->payload_size = 0;
          this->computed_crc = 0xFFFF;
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
        if (this->bytes_read < PAYLOAD_MAX_SIZE)
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

// stupid language
constexpr uint8_t NodebusParser::SOF[];

uint16_t NodebusParser::update_crc16(uint16_t crc, uint8_t data) {
  crc ^= ((uint16_t)data << 8);
  for (uint8_t i = 0; i < 8; i++) {
    if (crc & 0x8000) {
      crc = (crc << 1) ^ 0x1021; // 0x1021 is the standard CCITT polynomial
    } else {
      crc <<= 1;
    }
  }
  return crc;
}

} // namespace osmium
