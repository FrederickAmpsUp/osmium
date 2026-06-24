#include "logging/stream_sink.hpp"

namespace osmium {

StreamSink::StreamSink(Stream &stream) : stream(stream) {
}

void StreamSink::write(const char *str) {
  if (this->stream.availableForWrite())
    this->stream.println(str);
}

} // namespace osmium
