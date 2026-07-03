#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "config.h"

struct DanTime {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

bool hasElapsed(uint32_t lastRunMs, uint32_t intervalMs);
bool shouldRunNow(uint32_t &lastRunMs, uint32_t intervalMs);
bool isDigits(const String &value);
bool isValidTime(DanTime time);
bool isValidFrameSize(framesize_t frameSize);
bool isValidJpegQuality(int jpegQuality);
bool isInTimeWindow(const DanTime *currentTime, const DanTime *startTime, const DanTime *endTime);

String formatTime(DanTime time);
String formatFrameSize(framesize_t frameSize);
bool parseFrameSize(const String &value, framesize_t &frameSize);
