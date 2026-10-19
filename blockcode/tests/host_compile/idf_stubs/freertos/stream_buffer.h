#pragma once
#include "FreeRTOS.h"
extern "C" { StreamBufferHandle_t xStreamBufferCreate(size_t, size_t); size_t xStreamBufferSend(StreamBufferHandle_t, const void*, size_t, TickType_t); size_t xStreamBufferReceive(StreamBufferHandle_t, void*, size_t, TickType_t); }
