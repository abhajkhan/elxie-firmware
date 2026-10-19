#pragma once
#include "FreeRTOS.h"
extern "C" { SemaphoreHandle_t xSemaphoreCreateMutex(void); BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t); BaseType_t xSemaphoreGive(SemaphoreHandle_t); }
