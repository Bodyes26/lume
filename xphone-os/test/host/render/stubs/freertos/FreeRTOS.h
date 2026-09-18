#pragma once

// Host render harness — FreeRTOS types the UI layer names but never needs.
//
// Input.h owns a portMUX and a sampling task; Scene.cpp owns the flush worker.
// The harness drives both synchronously (Input::update() samples inline when no
// task was started, and the flush worker is never woken because there is no
// panel), so these are type-level stubs only.

#include <cstdint>

typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef uint8_t StackType_t;
typedef unsigned int UBaseType_t;
typedef int BaseType_t;

struct StaticTask_t {
  int reserved;
};

struct portMUX_TYPE {
  int reserved;
};

#define portMUX_INITIALIZER_UNLOCKED \
  { 0 }
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#define portMAX_DELAY 0xFFFFFFFFu
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))
