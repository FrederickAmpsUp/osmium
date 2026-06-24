#include "logging/logger.hpp"

namespace osmium {

void Logger::add_sink(LogSink &sink) {
  if (this->num_sinks < LOGGER_MAX_SINKS) {
    this->sinks[this->num_sinks] = &sink;
    this->num_sinks++;
  }
}

Logger &Logger::global() {
  static Logger logger;
  return logger;
}

void Logger::write(LogLevel level, const char *str) {
  char log_buf[LOGGER_MAX_SIZE];
  snprintf(log_buf, sizeof(log_buf), "[%lu] [%s] %s", millis(), Logger::level_string(level), str);

  for (size_t i = 0; i < this->num_sinks; ++i) {
    if (this->sinks[i]) this->sinks[i]->log(level, log_buf);
  }
}

} // namespace osmium
