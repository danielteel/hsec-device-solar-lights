#pragma once

#include <Arduino.h>
#include <stdint.h>

struct DeviceAction {
    bool valid = false;
    String action;
    String value;
    bool boolValue = false;
};

String jsonTelemetry(const String &key, const String &value);
String jsonTelemetry(const String &key, int32_t value);
String jsonStoredState(uint8_t lightMode, const String &onTime, const String &offTime, const String &frameSize, int32_t quality);
String jsonWeatherTelemetry(float humidity, float temperature, const String &currentTime);
String jsonDeviceReady();
String jsonImageMetadata(size_t jpgLength);
String jsonActionResult(const DeviceAction &request, bool success, const String &error = "");
DeviceAction parseDeviceAction(const String &message);
