#include "core/protocol/message.hpp"
#include <cstring>
#include <cstdlib>

namespace osmium {

constexpr uint32_t wire_metadata_size =
  sizeof(uint16_t) + // label (blob offset)
  sizeof(uint16_t) + // unit  (blob offset)
  sizeof(uint16_t) + // dtype (blob offset)
  sizeof(uint32_t);  // dsize

size_t InitRequestMessage::serialize(uint8_t *buf, size_t max_size) {
  uint32_t str_blob_size = 0;

  for (uint32_t i = 0; i < this->num_data_providers; ++i) {
    const auto *metadata = this->data_providers[i];

      // +1 for null terminator
    str_blob_size +=
      1 + strlen(metadata->label) +
      1 + strlen(metadata->unit)  +
      1 + strlen(metadata->dtype);
  }

  size_t required_size =
    sizeof(this->id) +
    sizeof(this->num_data_providers) +
    wire_metadata_size * this->num_data_providers +
    str_blob_size;

  if (max_size < required_size) return 0;

  uint8_t *ptr = buf;

#define cpy(d, s) do { memcpy(ptr, d, s); ptr += s; } while(0)

  // calc offset into str blob, copy it to ptr, then cpy string to str_blob_ptr
#define cpy_str(s) do { \
    uint16_t offset = str_blob_ptr - str_blob; cpy(&offset, sizeof(offset)); \
    size_t len = strlen(s) + 1; memcpy(str_blob_ptr, s, len); str_blob_ptr += len; } while(0)

  cpy(&this->id, sizeof(this->id));
  cpy(&this->num_data_providers, sizeof(this->num_data_providers));

  uint8_t *str_blob = ptr + wire_metadata_size * this->num_data_providers;
  uint8_t *str_blob_ptr = str_blob;

  for (uint32_t i = 0; i < this->num_data_providers; ++i) {
    const auto *metadata = this->data_providers[i];
    cpy_str(metadata->label);
    cpy_str(metadata->unit);
    cpy_str(metadata->dtype);

    // local binding for uint32_t instead of size_t
    uint32_t dsize = metadata->dsize;
    cpy(&dsize, sizeof(dsize));
  }

#undef cpy
#undef cpy_str

  return required_size;
}

bool InitRequestMessage::deserialize(InitRequestMessage *msg, uint8_t *buf, size_t buf_size) {
  if (!msg) return false;

  size_t fixed_size = sizeof(msg->id) + sizeof(msg->num_data_providers);
  if (buf_size < fixed_size) return false;

  uint8_t *ptr = buf;
  uint8_t *const end = buf + buf_size;

#define cpy(d, s) do { if ((size_t)(end-ptr) < s) {std::move(*msg).free(); return false;} \
                       memcpy(d, ptr, s); ptr += s; } while(0)

  cpy(&msg->id, sizeof(msg->id));
  cpy(&msg->num_data_providers, sizeof(msg->num_data_providers));

  size_t wire_metadata_sizes =
    wire_metadata_size * msg->num_data_providers;

  if (buf_size < fixed_size + wire_metadata_sizes) return false;

  size_t str_blob_size =
    buf_size - (fixed_size + wire_metadata_sizes);

  size_t blob_size =
    sizeof(DataProvider::Metadata *) * msg->num_data_providers +
    sizeof(DataProvider::Metadata)   * msg->num_data_providers +
    str_blob_size;

  msg->blob = calloc(1, blob_size);
  if (!msg->blob) return false;

  msg->data_providers = (const DataProvider::Metadata **)msg->blob;
  DataProvider::Metadata *metadatas =
      (DataProvider::Metadata *)(
          (uint8_t *)msg->blob +
          sizeof(DataProvider::Metadata*) * msg->num_data_providers
      );
 
  char *wire_str_blob = (char *)buf + fixed_size + wire_metadata_sizes;
  char *str_blob = (char *)msg->blob + 
    sizeof(DataProvider::Metadata *) * msg->num_data_providers +
    sizeof(DataProvider::Metadata)   * msg->num_data_providers;

  memcpy(str_blob, wire_str_blob, str_blob_size);

  for (uint32_t i = 0; i < msg->num_data_providers; ++i) {
    auto *metadata = metadatas + i;

    // fill the reference slot in the array
    msg->data_providers[i] = metadata;

    uint16_t label_offset, unit_offset, dtype_offset;
    cpy(&label_offset, sizeof(label_offset));
    cpy(&unit_offset,  sizeof(unit_offset));
    cpy(&dtype_offset, sizeof(dtype_offset));
    
    metadata->label = str_blob + label_offset;
    metadata->unit  = str_blob + unit_offset;
    metadata->dtype = str_blob + dtype_offset;

    uint32_t dsize;
    cpy(&dsize, sizeof(dsize));
    metadata->dsize = dsize;
  }

  return true;
}

void InitRequestMessage::free() && {
  if (this->blob) ::free(this->blob);
  this->blob = NULL;
  this->num_data_providers = 0;
  this->data_providers = NULL;
}

size_t InitResponseMessage::serialize(uint8_t *buf, size_t max_size) {
  size_t required_size = sizeof(this->id);

  if (max_size < required_size) return 0;

  memcpy(buf, &this->id, sizeof(this->id));

  return required_size;
}

bool InitResponseMessage::deserialize(InitResponseMessage *msg, uint8_t *buf, size_t buf_size) {
  if (!msg) return false;

  if (buf_size != sizeof(msg->id)) return false;

  memcpy(&msg->id, buf, sizeof(msg->id));

  return true;
}

} // namespace osmium
