#pragma once

#include "log_sink.hpp"

namespace osmium {

class SerialSink : public LogSink {
public:
  SerialSink() = default;

  virtual ~SerialSink() override = default;
private:
  virtual void write(const char *str) override;
};

} // namespace osmium
