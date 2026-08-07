#pragma once

#include <cstddef>
#include <cstdint>
#include "core/data_provider.hpp"

namespace osmium {

enum class MessageType : uint8_t {
  NONE = 0,
  INIT_REQUEST_MESSAGE,
  INIT_RESPONSE_MESSAGE,
  DATA_REQUEST_MESSAGE,
  DATA_RESPONSE_MESSAGE,
};


/**
 * Serialized format:
 *
 * | id                 | u8  |
 * | num_data_providers | u32 |
 * | data_providers     | num_data_providers * 80 bytes |
 * | str_blob           |
 *
 * Blob layout:
 *
 * +-------------------------------+
 * | Metadata*[num_data_providers] |
 * +-------------------------------+
 * | Metadata[num_data_providers]  |
 * +-------------------------------+
 * | String data                   |
 * +-------------------------------+
 *
 * After deserialization:
 *   data_providers points to the first section of the blob.
 *   Each Metadata* points to an element in the Metadata array.
 *   label, unit, and dtype point into the string data section.
 *
 * The entire blob is owned by InitRequestMessage.
 */
struct InitRequestMessage {
  static constexpr MessageType TYPE = MessageType::INIT_REQUEST_MESSAGE;

  uint8_t id; // id == 0 -> requesting new id

  uint32_t num_data_providers = 0;
  const DataProvider::Metadata **data_providers = NULL;

  size_t serialize(uint8_t *buf, size_t max_size);
  static bool deserialize(InitRequestMessage *msg, const uint8_t *buf, size_t buf_size);

  void free() &&;
private:
  void *blob = NULL;
};

struct InitResponseMessage {
  static constexpr MessageType TYPE = MessageType::INIT_RESPONSE_MESSAGE;

  uint8_t id; // id to assign
  
  size_t serialize(uint8_t *buf, size_t max_size);
  static bool deserialize(InitResponseMessage *msg, const uint8_t *buf, size_t buf_size);

  void free() && {}
};

struct DataRequestMessage {
  static constexpr MessageType TYPE = MessageType::DATA_REQUEST_MESSAGE;

  uint32_t data_provider_id;
  
  size_t serialize(uint8_t *buf, size_t max_size);
  static bool deserialize(DataRequestMessage *msg, const uint8_t *buf, size_t buf_size);

  void free() && {}
};

struct DataResponseMessage {
  static constexpr MessageType TYPE = MessageType::DATA_RESPONSE_MESSAGE;

  size_t size = 0;
  uint8_t *data = NULL;

  size_t serialize(uint8_t *buf, size_t max_size);
  static bool deserialize(DataResponseMessage *msg, const uint8_t *buf, size_t buf_size);

  void free() &&;
private:
  bool dynalloc = false;
};

template<typename T>
size_t serialize(const T &message, uint8_t *buf, size_t max_size) {
  if (max_size < 1) return 0;

  const uint8_t type = T::TYPE;

  auto msg_size = message.serialize(buf + 1, max_size - 1);
  buf[0] = type;

  return msg_size + 1;
}

inline MessageType get_type(const uint8_t *buf, size_t buf_size) {
  if (buf_size < 1) return MessageType::NONE;
  return (MessageType)buf[0];
}

template<typename T>
bool deserialize(T *message, const uint8_t *buf, size_t buf_size) {
  if (buf_size < 1) return false;

  const MessageType expected_type = T::TYPE;
  const MessageType real_type = get_type(buf, buf_size);

  if (expected_type != real_type) return false;

  return T::deserialize(message, buf + 1, buf_size - 1);
}

} // namespace osmium
