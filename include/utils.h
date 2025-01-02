#pragma once

#include <Arduino.h>

bool isTimeToExecute(uint32_t &lastTime, uint32_t interval);
void buildKey(const char* keyString, uint8_t* key);