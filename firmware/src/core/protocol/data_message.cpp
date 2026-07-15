#include "core/protocol/message.hpp"
#include <cstring>

namespace osmium {

size_t DataMessage::serialize(uint8_t *buf, size_t max_size) {
  if (max_size < this->size) return 0;

  memcpy(buf, this->data, this->size);
  return this->size;
}

} // namespace osmium
