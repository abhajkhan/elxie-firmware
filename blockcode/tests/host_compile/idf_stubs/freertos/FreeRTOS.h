#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint32_t TickType_t; typedef int BaseType_t; typedef unsigned UBaseType_t;
typedef void* SemaphoreHandle_t; typedef void* TaskHandle_t; typedef void* StreamBufferHandle_t;
typedef void (*TaskFunction_t)(void*);
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configMAX_PRIORITIES 25
typedef struct { uint32_t owner; uint32_t count; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0, 0}
extern "C" { void vPortEnterCritical(portMUX_TYPE*); void vPortExitCritical(portMUX_TYPE*); }
#define portENTER_CRITICAL(m) vPortEnterCritical(m)
#define portEXIT_CRITICAL(m) vPortExitCritical(m)
