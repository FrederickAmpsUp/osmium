#pragma once

#include "logger.hpp"

namespace osmium {

class LogSink {
public:
  inline void log(Logger::Level level, const char *str) {
    if (level >= this->level) this->write(str);
  }
  inline void set_level(Logger::Level level) { this->level = level; }
  
  virtual ~LogSink() = default;
private:
  virtual void write(const char *str) = 0;

  Logger::Level level = Logger::DEFAULT_LEVEL;
};

} // namespace osmium
