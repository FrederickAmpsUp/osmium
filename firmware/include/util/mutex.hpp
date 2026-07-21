#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <utility>

namespace osmium {

template<typename T>
class Mutex {
public:
  template<typename... Args>
  explicit Mutex(Args&&... args) : object(std::forward<Args>(args)...) {
    this->mutex = xSemaphoreCreateMutex();
  }

  class Guard {
  public:
    Guard(SemaphoreHandle_t mutex, T& object) : mutex(mutex), object(object) {
      xSemaphoreTake(this->mutex, portMAX_DELAY);
    }

    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;

    Guard(Guard&& other)
        : mutex(other.mutex),
          object(other.object),
          owns_lock(other.owns_lock)
    {
        other.owns_lock = false;
    }

    ~Guard() {
      if (owns_lock)
        xSemaphoreGive(this->mutex);
    }

    T *operator->() { return &this->object; }
    T &operator*() { return this->object; }
  private:
    bool owns_lock = true;
    SemaphoreHandle_t mutex;
    T& object;
  };

  Guard lock() {
    return Guard(this->mutex, this->object);
  }

private:
  SemaphoreHandle_t mutex;
  T object;
};

}
