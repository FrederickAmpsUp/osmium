#pragma once

#include <cstdint>
#include <cstdlib>

namespace osmium {

constexpr uint8_t NODEBUS_SOF[] = {0xDE, 0xAD, 0xBE, 0xEF};
constexpr size_t NODEBUS_SOF_SIZE = sizeof(NODEBUS_SOF);

constexpr size_t NODEBUS_PAYLOAD_MAX_SIZE = 256;

constexpr uint16_t NODEBUS_CRC_INIT = 0xFFFF;
constexpr uint16_t NODEBUS_CRC_POLY = 0x1021;

constexpr uint8_t NODEBUS_PACKET_VERSION_V1 = 0x01;

uint16_t update_crc16(uint16_t crc, uint8_t data);

/**
 * =================================================================================
 * NODEBUS v1 PACKET FORMAT SPECIFICATION
 * =================================================================================
 * * PACKET LAYOUT:
 * +-------------------+--------------------+----------------+---------------------+-------------------+-----------------+
 * |    SOF (4B)       |    Version (1B)    | Target ID (1B) |  Varint Size (1-4B) |   Payload (XB)    |    CRC (2B)     |
 * +-------------------+--------------------+----------------+---------------------+-------------------+-----------------+
 * |  0xDE AD BE EF    |        0x01        |   0x00 - 0xFF  |      Base-128       |  Dynamic (0-MAX)  |  CCITT (Big-E)  |
 * +-------------------+--------------------+----------------+---------------------+-------------------+-----------------+
 * * FIELD DETAILS:
 * * 1. SOF (Start of Frame)
 * - Size: 4 Bytes
 * - Value: 0xDEADBEEF
 * * 2. Protocol Version
 * - Size: 1 Byte
 * - Value: 0x01
 * * 3. Target Node ID
 * - Size: 1 Byte
 * - Value Range: 0x00 (Broadcast), 0x01 - 0xFF (Unicast)
 * * 4. Varint Payload Size
 * - Size: Variable (1 to 4 Bytes)
 * - Encoding: Base-128 / Protocol Buffers Varint
 * * Bits 0–6: 7-bit payload size chunk
 * * Bit 7 (MSB): Continuation bit (1 = more varint bytes follow; 0 = final byte)
 * - Mapping: Direct 1:1 map (Payload Length = Parsed Integer value)
 * * 5. Payload Data
 * - Size: 0 to NODEBUS_PAYLOAD_MAX_SIZE Bytes
 * - Encoding: Raw binary data
 * * 6. Cyclic Redundancy Check (CRC)
 * - Size: 2 Bytes
 * - Algorithm: CRC-16-CCITT (Polynomial: 0x1021, Init: 0xFFFF)
 * - Coverage: All bytes after SOF through the final payload byte
 * - Byte Order: Big-Endian (MSB first)
 * * =================================================================================
 */

} // namespace osmium
