#include "core/protocol/message.hpp"
#include <cstring>
#include <cstdlib>

namespace osmium {

size_t DataMessage::serialize(uint8_t *buf, size_t max_size) {
  if (max_size < this->size) return 0;

  memcpy(buf, this->data, this->size);
  return this->size;
}

bool DataMessage::deserialize(DataMessage *msg, uint8_t *buf, size_t buf_size) {
  if (!msg) return false;
  this->dynalloc = true;

  this->size = buf_size;

  this->data = (uint8_t *)malloc(this->size);
  if (!this->data) return false;

  memcpy(this->data, buf, this->size);

  return true;
}

void DataMessage::free() && {
  if (!this->dynalloc) return;

  if (this->data) ::free((void *)this->data);
}

} // namespace osmium
