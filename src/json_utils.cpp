#include "json_utils.h"

#include <ArduinoJson.h>
#include <math.h>

#include "config.h"

namespace {

String jsonString(JsonDocument &doc) {
    String json;
    serializeJson(doc, json);
    return json;
}

bool jsonBoolValue(JsonVariantConst variant, bool defaultValue) {
    if (variant.is<bool>()) {
        return variant.as<bool>();
    }
    if (variant.is<int>()) {
        return variant.as<int>() != 0;
    }
    if (!variant.is<const char*>()) {
        return defaultValue;
    }

    String value = variant.as<const char*>();
    if (value.equalsIgnoreCase("true") || value == "1" || value.equalsIgnoreCase("on")) {
        return true;
    }
    if (value.equalsIgnoreCase("false") || value == "0" || value.equalsIgnoreCase("off")) {
        return false;
    }
    return defaultValue;
}

String jsonStringValue(JsonVariantConst variant) {
    if (variant.is<const char*>()) {
        return String(variant.as<const char*>());
    }

    String text;
    serializeJson(variant, text);
    return text;
}

JsonObject addAction(JsonArray actions, const char *name, const char *label, const char *type, const char *stateKey = nullptr) {
    JsonObject action = actions.add<JsonObject>();
    action["name"] = name;
    action["label"] = label;
    action["type"] = type;
    if (stateKey) {
        action["stateKey"] = stateKey;
    }
    return action;
}

} // namespace

String jsonTelemetry(const String &key, const String &value) {
    JsonDocument doc;
    doc["type"] = "telemetry";
    doc[key.c_str()] = value;
    return jsonString(doc);
}

String jsonTelemetry(const String &key, int32_t value) {
    JsonDocument doc;
    doc["type"] = "telemetry";
    doc[key.c_str()] = value;
    return jsonString(doc);
}

String jsonWeatherTelemetry(float humidity, float temperature, const String &currentTime) {
    JsonDocument doc;
    doc["type"] = "telemetry";
    if (isfinite(humidity)) {
        doc["humidity"] = serialized(String(humidity, 1));
    }
    if (isfinite(temperature)) {
        doc["temperature"] = serialized(String(temperature, 1));
    }
    if (currentTime.length()) {
        doc["currentTime"] = currentTime;
    }

    return jsonString(doc);
}

String jsonStoredState(uint8_t lightMode, const String &onTime, const String &offTime, const String &frameSize, int32_t quality) {
    JsonDocument doc;
    doc["type"] = "telemetry";
    doc["lightMode"] = lightMode;
    doc["onTime"] = onTime;
    doc["offTime"] = offTime;
    doc["frame_size"] = frameSize;
    doc["quality"] = quality;

    return jsonString(doc);
}

String jsonDeviceReady() {
    JsonDocument doc;
    doc["type"] = "deviceReady";

    JsonArray actions = doc["actions"].to<JsonArray>();
    addAction(actions, "lightOff", "Light Off", "button");
    addAction(actions, "lightOn", "Light On", "button");
    addAction(actions, "lightAuto", "Auto", "button");
    addAction(actions, "setOnTime", "Set On Time", "time", "onTime");
    addAction(actions, "setOffTime", "Set Off Time", "time", "offTime");

    JsonObject frameSize = addAction(actions, "frame_size", "Frame Size", "enum", "frame_size");
    JsonArray frameSizeOptions = frameSize["options"].to<JsonArray>();
    for (const CameraFrameSizeOption &option : CAMERA_FRAME_SIZE_OPTIONS) {
        JsonObject item = frameSizeOptions.add<JsonObject>();
        item["value"] = option.name;
        item["label"] = option.name;
    }

    JsonObject quality = addAction(actions, "quality", "Quality", "number", "quality");
    quality["min"] = CAMERA_MIN_JPEG_QUALITY;
    quality["max"] = CAMERA_MAX_JPEG_QUALITY;

    return jsonString(doc);
}

DeviceAction parseDeviceAction(const String &message) {
    JsonDocument doc;
    DeviceAction parsed;
    if (deserializeJson(doc, message) || doc["type"] != "action") {
        return parsed;
    }

    parsed.valid = true;
    parsed.action = doc["action"] | "";

    JsonVariantConst value = doc["value"];
    if (!value.isNull()) {
        parsed.value = jsonStringValue(value);
        parsed.boolValue = jsonBoolValue(value, false);
    }
    return parsed;
}

String jsonImageMetadata(size_t jpgLength) {
    JsonDocument doc;
    doc["type"] = "image";
    doc["format"] = "jpeg";
    doc["length"] = jpgLength;

    return jsonString(doc);
}

String jsonActionResult(const DeviceAction &request, bool success, const String &error) {
    JsonDocument doc;
    doc["type"] = "actionResult";
    doc["action"] = request.action;
    doc["success"] = success;
    if (!success && error.length()) {
        doc["error"] = error;
    }

    return jsonString(doc);
}
