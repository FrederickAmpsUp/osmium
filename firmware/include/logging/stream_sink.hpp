#pragma once

#include "log_sink.hpp"
#include <Stream.h>

namespace osmium {

class StreamSink : public LogSink {
public:
  StreamSink(Stream &stream);

  virtual ~StreamSink() override = default;
private:
  Stream &stream;
  virtual void write(const char *str) override;
};

} // namespace osmium
