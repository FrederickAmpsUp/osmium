#pragma once

#include "log_level.hpp"

namespace osmium {

class LogSink {
public:
  inline void log(LogLevel level, const char *str) {
    if (level >= this->level) this->write(str);
  }
  inline void set_level(LogLevel level) { this->level = level; }
  
  virtual ~LogSink() = default;
private:
  virtual void write(const char *str) = 0;

  LogLevel level = LOG_LEVEL_DEFAULT;
};

} // namespace osmium
