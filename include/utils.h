#pragma once

#include <Arduino.h>


typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} DanTime;


bool isTimeToExecute(uint32_t &lastTime, uint32_t interval);
bool isInTimeWindow(DanTime* currentTime, DanTime* startTime, DanTime* endTime);