#include "core/protocol/message.hpp"
#include <cstring>
#include <cstdlib>

namespace osmium {

size_t DataRequestMessage::serialize(uint8_t *buf, size_t max_size) {
  if (max_size < sizeof(this->data_provider_id)) return 0;

  memcpy(buf, &this->data_provider_id, sizeof(this->data_provider_id));

  return sizeof(this->data_provider_id);
}

bool DataRequestMessage::deserialize(DataRequestMessage *msg, const uint8_t *buf, size_t buf_size) {
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

bool DataResponseMessage::deserialize(DataResponseMessage *msg, const uint8_t *buf, size_t buf_size) {
  if (!msg) return false;
  msg->dynalloc = true;

  msg->size = buf_size;

  msg->data = (uint8_t *)malloc(msg->size);
  if (!msg->data) return false;

  memcpy(msg->data, buf, msg->size);

  return true;
}

void DataResponseMessage::free() && {
  if (!this->dynalloc) return;

  if (this->data) ::free((void *)this->data);
}

} // namespace osmium
