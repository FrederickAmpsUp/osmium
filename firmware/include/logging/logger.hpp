#pragma once

#include <cstdlib>

namespace osmium {

class LogSink;

static const size_t LOGGER_MAX_SINKS = 8;
static const size_t LOGGER_MAX_SIZE = 256;

class Logger {
public:
  enum Level {
    TRACE, DEBUG, INFO,
    WARN, ERROR,
  };

  static constexpr const Level DEFAULT_LEVEL = WARN;

  inline void set_level(Level level) { this->level = level; }

  template<typename... Args>
  void log(Level level, const char *fmt, Args... args);

  template<typename... Args>
  inline void trace(const char *fmt, Args... args) { this->log(TRACE, fmt, args...); }
  template<typename... Args>
  inline void debug(const char *fmt, Args... args) { this->log(DEBUG, fmt, args...); }
  template<typename... Args>
  inline void info(const char *fmt, Args... args) { this->log(INFO, fmt, args...); }
  template<typename... Args>
  inline void warn(const char *fmt, Args... args) { this->log(WARN, fmt, args...); }
  template<typename... Args>
  inline void error(const char *fmt, Args... args) { this->log(ERROR, fmt, args...); }

  void add_sink(LogSink &sink);

  static const char *level_string(Level level) {
    switch (level) {
      case TRACE: return "TRACE";
      case DEBUG: return "DEBUG";
      case  INFO: return  "INFO";
      case  WARN: return  "WARN";
      case ERROR: return "ERROR";
      default:    return  "UNKN";
    }
  }

  static Logger &global();
private:
  Level level = DEFAULT_LEVEL;

  LogSink *sinks[LOGGER_MAX_SINKS];
  size_t num_sinks = 0;
};

} // namespace osmium

#include "log_sink.hpp"
#include <Arduino.h>
#include <cstdio>

namespace osmium {

template<typename... Args>
void Logger::log(Logger::Level level, const char *fmt, Args... args) {
  if (this->num_sinks == 0 || level < this->level) return;

  // first step just does the formatting specified by the caller
  char msg_buf[LOGGER_MAX_SIZE];
  snprintf(msg_buf, sizeof(msg_buf), fmt, args...);

  // second step takes the result of that and adds the timestamp and level
  char log_buf[LOGGER_MAX_SIZE];
  snprintf(log_buf, sizeof(log_buf), "[%lu] [%s] %s", millis(), Logger::level_string(level), msg_buf);

  for (size_t i = 0; i < this->num_sinks; ++i) {
    if (this->sinks[i]) this->sinks[i]->log(level, log_buf);
  }
}
} // namespace osmium
