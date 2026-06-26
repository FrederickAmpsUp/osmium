#pragma once

#include <Stream.h>

namespace osmium {

class NodebusSender {
public:
  NodebusSender() = default;

  void send(Stream &stream, uint8_t target_id, const uint8_t *payload, size_t payload_size);
  inline void broadcast(Stream &stream, const uint8_t *payload, size_t payload_size) {
    this->send(stream, 0, payload, payload_size);
  }
private:

};

}
