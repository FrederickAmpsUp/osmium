#pragma once

#include <cstdlib>
#include "log_level.hpp"

namespace osmium {

class LogSink;

static const size_t LOGGER_MAX_SINKS = 8;
static const size_t LOGGER_MAX_SIZE = 256;

class Logger {
public:
  Logger(const Logger &) = delete;  // delete copy constructors
  Logger& operator=(const Logger &) = delete;

  inline void set_level(LogLevel level) { this->level = level; }

  template<typename... Args>
  void log(LogLevel level, const char *fmt, Args... args);

  template<typename... Args>
  inline void trace(const char *fmt, Args... args) { this->log(LogLevel::TRACE, fmt, args...); }
  template<typename... Args>
  inline void debug(const char *fmt, Args... args) { this->log(LogLevel::DEBUG, fmt, args...); }
  template<typename... Args>
  inline void info(const char *fmt, Args... args) { this->log(LogLevel::INFO, fmt, args...); }
  template<typename... Args>
  inline void warn(const char *fmt, Args... args) { this->log(LogLevel::WARN, fmt, args...); }
  template<typename... Args>
  inline void error(const char *fmt, Args... args) { this->log(LogLevel::ERROR, fmt, args...); }

  void add_sink(LogSink &sink);

  static const char *level_string(LogLevel level) {
    switch (level) {
      case LogLevel::TRACE: return "TRACE";
      case LogLevel::DEBUG: return "DEBUG";
      case LogLevel:: INFO: return  "INFO";
      case LogLevel:: WARN: return  "WARN";
      case LogLevel::ERROR: return "ERROR";
      default:    return  "UNKN";
    }
  }

  static Logger &global();
private:
  LogLevel level = LOG_LEVEL_DEFAULT;

  LogSink *sinks[LOGGER_MAX_SINKS];
  size_t num_sinks = 0;
};

} // namespace osmium

#include "log_sink.hpp"
#include <Arduino.h>
#include <cstdio>

namespace osmium {

template<typename... Args>
void Logger::log(LogLevel level, const char *fmt, Args... args) {
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
