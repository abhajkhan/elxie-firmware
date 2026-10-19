#pragma once
#include "FreeRTOS.h"
extern "C" {
void vTaskDelay(TickType_t); void vTaskDelayUntil(TickType_t* prev, TickType_t inc); TickType_t xTaskGetTickCount(void);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t, const char*, uint32_t, void*, UBaseType_t, TaskHandle_t*, BaseType_t);
}
