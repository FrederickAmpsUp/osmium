#include "core/protocol/message.hpp"
#include <cstring>
#include <cstdlib>

namespace osmium {

size_t DataRequestMessage::serialize(uint8_t *buf, size_t max_size) {
  if (max_size < sizeof(this->data_provider_id)) return 0;

  memcpy(buf, &this->data_provider_id, sizeof(this->data_provider_id));

  return sizeof(this->data_provider_id);
}

bool DataRequestMessage::deserialize(DataRequestMessage *msg, uint8_t *buf, size_t buf_size) {
  if (!msg) return false;

  if (buf_size < sizeof(msg->data_provider_id)) return false;

  memcpy(&msg->data_provider_id, buf, sizeof(msg->data_provider_id));

  return true;
}

size_t DataResponseMessage::serialize(uint8_t *buf, size_t max_size) {
  if (max_size < this->size) return 0;

  memcpy(buf, this->data, this->size);
  return this->size;
}

bool DataResponseMessage::deserialize(DataResponseMessage *msg, uint8_t *buf, size_t buf_size) {
  if (!msg) return false;
  this->dynalloc = true;

  this->size = buf_size;

  this->data = (uint8_t *)malloc(this->size);
  if (!this->data) return false;

  memcpy(this->data, buf, this->size);

  return true;
}

void DataResponseMessage::free() && {
  if (!this->dynalloc) return;

  if (this->data) ::free((void *)this->data);
}

} // namespace osmium
