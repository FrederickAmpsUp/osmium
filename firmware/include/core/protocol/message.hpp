#pragma once

#include <cstddef>
#include <cstdint>
#include "core/data_provider.hpp"

namespace osmium {

enum class MessageType : uint8_t {
  INIT_REQUEST_MESSAGE,
  DATA_MESSAGE,
};

struct InitRequestMessage {
  uint8_t id;

  uint32_t num_data_providers;
  DataProvider **data_providers;

  size_t serialize(uint8_t *buf, size_t max_size);
};

struct DataMessage {
  size_t size;
  uint8_t *data;

  size_t serialize(uint8_t *buf, size_t max_size);
};

} // namespace osmium
