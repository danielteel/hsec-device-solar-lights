#include "common.h"

namespace {

uint32_t secondsSinceMidnight(DanTime time) {
    return (static_cast<uint32_t>(time.hours) * 60U * 60U) +
           (static_cast<uint32_t>(time.minutes) * 60U) +
           time.seconds;
}

} // namespace

bool hasElapsed(uint32_t lastRunMs, uint32_t intervalMs) {
    return millis() - lastRunMs >= intervalMs;
}

bool shouldRunNow(uint32_t &lastRunMs, uint32_t intervalMs) {
    if (!hasElapsed(lastRunMs, intervalMs)) {
        return false;
    }

    lastRunMs = millis();
    return true;
}

bool isDigits(const String &value) {
    if (!value.length()) {
        return false;
    }

    for (uint16_t i = 0; i < value.length(); i++) {
        if (!isdigit(value[i])) {
            return false;
        }
    }

    return true;
}

String formatTime(DanTime time) {
    char text[9];
    snprintf(text, sizeof(text), "%02u:%02u:%02u", time.hours, time.minutes, time.seconds);
    return String(text);
}

bool isValidTime(DanTime time) {
    return time.hours <= 23 && time.minutes <= 59 && time.seconds <= 59;
}

String formatFrameSize(framesize_t frameSize) {
    for (const CameraFrameSizeOption &option : CAMERA_FRAME_SIZE_OPTIONS) {
        if (option.value == frameSize) {
            return String(option.name);
        }
    }

    return String(static_cast<int>(frameSize));
}

bool isValidFrameSize(framesize_t frameSize) {
    for (const CameraFrameSizeOption &option : CAMERA_FRAME_SIZE_OPTIONS) {
        if (option.value == frameSize) {
            return true;
        }
    }

    return false;
}

bool isValidJpegQuality(int jpegQuality) {
    return jpegQuality >= CAMERA_MIN_JPEG_QUALITY && jpegQuality <= CAMERA_MAX_JPEG_QUALITY;
}

bool parseFrameSize(const String &value, framesize_t &frameSize) {
    bool numeric = isDigits(value);
    int numericValue = numeric ? value.toInt() : -1;

    for (const CameraFrameSizeOption &option : CAMERA_FRAME_SIZE_OPTIONS) {
        if (value.equalsIgnoreCase(option.name) || (numeric && numericValue == option.value)) {
            frameSize = option.value;
            return true;
        }
    }

    return false;
}

bool isInTimeWindow(const DanTime *currentTime, const DanTime *startTime, const DanTime *endTime) {
    uint32_t current = secondsSinceMidnight(*currentTime);
    uint32_t start = secondsSinceMidnight(*startTime);
    uint32_t end = secondsSinceMidnight(*endTime);

    if (start < end) {
        return current >= start && current < end;
    }
    if (end < start) {
        return current >= start || current < end;
    }
    return false;
}
