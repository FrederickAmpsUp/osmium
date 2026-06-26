#include "drivers/nodebus_sender.hpp"
#include "drivers/nodebus.hpp"

namespace osmium {

static inline void send_byte(uint16_t &crc, Stream &stream, uint8_t byte) {
  crc = update_crc16(crc, byte);
  stream.write(byte);
}

void NodebusSender::send(Stream &stream, uint8_t target_id, const uint8_t *payload, size_t payload_size) {
  uint16_t crc = NODEBUS_CRC_INIT;

  // send SOF
  stream.write(NODEBUS_SOF, NODEBUS_SOF_SIZE);

  // send version
  send_byte(crc, stream, NODEBUS_PACKET_VERSION_V1);

  // send target
  send_byte(crc, stream, target_id);

  uint16_t packet_payload_size = payload_size;
  do {
    uint8_t byte = packet_payload_size & 0x7F;
    packet_payload_size >>= 7;

    if (packet_payload_size > 0) {
      byte |= 0x80;
    }

    send_byte(crc, stream, byte);
  } while (packet_payload_size > 0);

  for (int i = 0; i < payload_size; ++i) {
    send_byte(crc, stream, payload[i]);
  }

  stream.write((uint8_t)(crc >> 8));
  stream.write((uint8_t)(crc & 0xFF));
}

}
