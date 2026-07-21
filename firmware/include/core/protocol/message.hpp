#pragma once

#include <cstddef>
#include <cstdint>
#include "core/data_provider.hpp"

namespace osmium {

enum class MessageType : uint8_t {
  INIT_REQUEST_MESSAGE,
  DATA_MESSAGE,
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
  uint8_t id; // id == 0 -> requesting new id

  uint32_t num_data_providers = 0;
  const DataProvider::Metadata **data_providers = NULL;

  size_t serialize(uint8_t *buf, size_t max_size);
  bool deserialize(InitRequestMessage *msg, uint8_t *buf, size_t buf_size);

  void free() &&;
private:
  void *blob = NULL;
};

struct InitResponseMessage {
  uint8_t id; // id to assign
  
  size_t serialize(uint8_t *buf, size_t max_size);
  bool deserialize(InitResponseMessage *msg, uint8_t *buf, size_t buf_size);

  void free() && {}
};

struct DataRequestMessage {
  uint32_t data_provider_id;
  
  size_t serialize(uint8_t *buf, size_t max_size);
  bool deserialize(DataRequestMessage *msg, uint8_t *buf, size_t buf_size);

  void free() && {}
};

struct DataResponseMessage {
  size_t size = 0;
  uint8_t *data = NULL;

  size_t serialize(uint8_t *buf, size_t max_size);
  bool deserialize(DataResponseMessage *msg, uint8_t *buf, size_t buf_size);

  void free() &&;
private:
  bool dynalloc = false;
};

} // namespace osmium
