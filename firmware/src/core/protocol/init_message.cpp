#include "core/protocol/message.hpp"
#include <cstring>

namespace osmium {

size_t InitRequestMessage::serialize(uint8_t *buf, size_t max_size) {
  size_t required_size =
    sizeof(this->id) +
    sizeof(this->num_data_providers);
  for (uint32_t i = 0; i < this->num_data_providers; ++i) {
    const DataProvider::Metadata *m = this->data_providers[i]->get_metadata();

    required_size +=
      2 + strlen(m->label) +
      2 + strlen(m->unit)  +
      2 + strlen(m->dtype) +
      sizeof(uint32_t); // dsize
  }

  if (max_size < required_size) return 0;

  uint8_t *ptr = buf;

#define cpy(d, s) do { memcpy(ptr, d, s); ptr += s; } while(0)
#define cpy_zts(s) do { uint16_t len = strlen(s); cpy(&len, sizeof(len)); cpy(s, len); } while(0)

  cpy(&this->id, sizeof(this->id));
  cpy(&this->num_data_providers, sizeof(this->num_data_providers));

  for (uint32_t i = 0; i < this->num_data_providers; ++i) {
    const DataProvider::Metadata *m = this->data_providers[i]->get_metadata();

    cpy_zts(m->label);
    cpy_zts(m->unit);
    cpy_zts(m->dtype);

    uint32_t dsize = m->dsize;
    cpy(&dsize, sizeof(dsize));
  }

#undef cpy
#undef cpy_zts

  return required_size;
}

} // namespace osmium
