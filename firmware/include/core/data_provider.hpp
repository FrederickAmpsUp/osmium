#pragma once

#include <cstdlib>
#include <cstdint>
#include "util/mutex.hpp"

namespace osmium {

class DataProvider {
public:
  struct Metadata {
    const char *label;
    const char *unit;
    
    // e.g. "f32", "u8"
    const char *dtype;
    size_t      dsize;
  };

  virtual const Metadata *get_metadata() const = 0;

  virtual int sample() = 0;

  // returned guard must be release as soon as the data is consumed.
  // references to *Guard are invalidated once Guard goes out of scope
  virtual Mutex<const uint8_t *>::Guard lock_data() = 0;

  static constexpr int SAMPLE_OK = 0;

  virtual ~DataProvider() = default;
private:
};

} // namespace osmium
