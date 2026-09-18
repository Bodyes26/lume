#pragma once

// Host render harness — task API backed by real threads.
//
// The flush worker is not faked away: xTaskCreateStatic starts a std::thread
// running the same flushWorkLoop the device runs, and notify-give/notify-take
// are a counting semaphore. That keeps SceneManager's deferral rule
// (renderIfDirty skips while _flushInFlight) intact, so the harness observes the
// real refresh tier and the real dirty-window for every frame it dumps — which
// is the whole point: a wrong dirty rect is invisible in a screenshot and
// obvious in the flush log.

#include <condition_variable>
#include <mutex>
#include <thread>

#include "FreeRTOS.h"

namespace host_task {

inline std::mutex& mutex() {
  static std::mutex m;
  return m;
}
inline std::condition_variable& cv() {
  static std::condition_variable c;
  return c;
}
inline unsigned& notifications() {
  static unsigned n = 0;
  return n;
}

}  // namespace host_task

inline TickType_t xTaskGetTickCount() { return 0; }
inline void vTaskDelayUntil(TickType_t*, TickType_t) { std::this_thread::yield(); }
inline void vTaskDelay(TickType_t) { std::this_thread::yield(); }
inline void vTaskSuspend(TaskHandle_t) {}
inline void vTaskResume(TaskHandle_t) {}

inline BaseType_t xTaskCreate(void (*fn)(void*), const char*, uint32_t, void* arg, UBaseType_t, TaskHandle_t* handle) {
  std::thread(fn, arg).detach();
  if (handle) *handle = reinterpret_cast<TaskHandle_t>(1);
  return pdPASS;
}

inline TaskHandle_t xTaskCreateStatic(void (*fn)(void*), const char*, uint32_t, void* arg, UBaseType_t, StackType_t*,
                                      StaticTask_t*) {
  std::thread(fn, arg).detach();
  return reinterpret_cast<TaskHandle_t>(1);
}

inline void xTaskNotifyGive(TaskHandle_t) {
  {
    std::lock_guard<std::mutex> lock(host_task::mutex());
    host_task::notifications()++;
  }
  host_task::cv().notify_one();
}

inline uint32_t ulTaskNotifyTake(BaseType_t, uint32_t) {
  std::unique_lock<std::mutex> lock(host_task::mutex());
  host_task::cv().wait(lock, [] { return host_task::notifications() > 0; });
  host_task::notifications()--;
  return 1;
}

inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 0; }
