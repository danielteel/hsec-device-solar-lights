#include <Arduino.h>
#include <Esp.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <time.h>

#include "DHTesp.h"
#include "camera.h"
#include "config.h"
#include "json_utils.h"
#include "secrets.h"
#include "storage.h"
#include "common.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");
extern const uint8_t rootca_crt_bundle_end[] asm("_binary_data_cert_x509_crt_bundle_bin_end");

namespace {

enum LightMode : uint8_t {
    kLightOff = 0,
    kLightOn = 1,
    kLightAuto = 2,
};

DHTesp dht;
StorageData storageData;
WebSocketsClient webSocket;

uint32_t lastReadyMs = 0;
uint32_t lastWiFiLogMs = 0;
uint32_t lastImageMs = 0;
uint32_t lastWeatherMs = 0;
uint32_t lastLightUpdateMs = 0;
bool webSocketStarted = false;
bool lightIsOn = false;
bool lastLightIsOn = false;

bool sendJson(const String &json) {
    return webSocket.isConnected() && webSocket.sendTXT(json.c_str());
}

void restartDevice(const char *reason) {
    Serial.println(reason);
    delay(1000);
    ESP.restart();
    while (true) {
        delay(1000);
    }
}

bool currentLocalTime(DanTime &currentTime) {
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo, 0)) {
        return false;
    }

    currentTime = {
        static_cast<uint8_t>(timeInfo.tm_hour),
        static_cast<uint8_t>(timeInfo.tm_min),
        static_cast<uint8_t>(timeInfo.tm_sec),
    };
    return true;
}

bool parseActionTime(const String &value, DanTime &parsedTime) {
    int hours;
    int minutes;
    int seconds;
    char trailing;
    if (sscanf(value.c_str(), "%d:%d:%d%c", &hours, &minutes, &seconds, &trailing) != 3) {
        return false;
    }

    if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59 || seconds < 0 || seconds > 59) {
        return false;
    }

    parsedTime = {
        static_cast<uint8_t>(hours),
        static_cast<uint8_t>(minutes),
        static_cast<uint8_t>(seconds),
    };
    return true;
}

void sendStoredState() {
    sendJson(jsonStoredState(
        storageData.lightMode,
        formatTime(storageData.autoStartTime),
        formatTime(storageData.autoEndTime),
        formatFrameSize(storageData.frameSize),
        storageData.quality
    ));
}

void updateLightMode(bool forceSend = false) {
    if (storageData.lightMode == kLightOff) {
        lightIsOn = false;
    } else if (storageData.lightMode == kLightOn) {
        lightIsOn = true;
    } else {
        DanTime currentTime;
        lightIsOn = currentLocalTime(currentTime) &&
            isInTimeWindow(&currentTime, &storageData.autoStartTime, &storageData.autoEndTime);
    }

    digitalWrite(LIGHT_PIN, lightIsOn ? HIGH : LOW);
    if (forceSend || lightIsOn != lastLightIsOn) {
        sendJson(jsonTelemetry("lightOn", static_cast<int32_t>(lightIsOn)));
        lastLightIsOn = lightIsOn;
    }
}

bool setLightMode(uint8_t lightMode) {
    if (lightMode > kLightAuto) {
        return false;
    }

    storageData.lightMode = lightMode;
    commitStorage(storageData);
    sendJson(jsonTelemetry("lightMode", static_cast<int32_t>(storageData.lightMode)));
    updateLightMode();
    return true;
}

bool setAutoTime(const String &value, DanTime &target, const char *stateKey) {
    DanTime newTime;
    if (!parseActionTime(value, newTime)) {
        Serial.printf("Invalid %s value: %s\n", stateKey, value.c_str());
        return false;
    }

    target = newTime;
    commitStorage(storageData);
    sendJson(jsonTelemetry(stateKey, formatTime(target)));
    updateLightMode();
    Serial.printf("Updated %s to %s\n", stateKey, formatTime(target).c_str());
    return true;
}

bool setFrameSize(const String &value) {
    framesize_t frameSize;
    if (!parseFrameSize(value, frameSize)) {
        Serial.printf("Invalid frame_size value: %s\n", value.c_str());
        return false;
    }

    if (!setCameraFrameSize(frameSize)) {
        Serial.println("Failed to update camera frame size");
        return false;
    }

    storageData.frameSize = frameSize;
    commitStorage(storageData);
    sendJson(jsonTelemetry("frame_size", formatFrameSize(storageData.frameSize)));
    Serial.printf("Updated frame_size to %s\n", formatFrameSize(storageData.frameSize).c_str());
    return true;
}

bool setJpegQuality(const String &value) {
    if (!isDigits(value)) {
        Serial.printf("Invalid quality value: %s\n", value.c_str());
        return false;
    }

    int quality = value.toInt();
    if (!isValidJpegQuality(quality)) {
        Serial.printf("Quality out of range: %d\n", quality);
        return false;
    }

    if (!setCameraJpegQuality(quality)) {
        Serial.println("Failed to update camera quality");
        return false;
    }

    storageData.quality = quality;
    commitStorage(storageData);
    sendJson(jsonTelemetry("quality", static_cast<int32_t>(storageData.quality)));
    Serial.printf("Updated quality to %d\n", storageData.quality);
    return true;
}

bool handleDeviceAction(const DeviceAction &request, String &error) {
    const String &action = request.action;
    if (action == "lightOff") {
        return setLightMode(kLightOff);
    }
    if (action == "lightOn") {
        return setLightMode(kLightOn);
    }
    if (action == "lightAuto") {
        return setLightMode(kLightAuto);
    }
    if (action == "light") {
        return setLightMode(request.boolValue ? kLightOn : kLightOff);
    }
    if (action == "setOnTime") {
        error = "Invalid time. Expected HH:MM:SS.";
        return setAutoTime(request.value, storageData.autoStartTime, "onTime");
    }
    if (action == "setOffTime") {
        error = "Invalid time. Expected HH:MM:SS.";
        return setAutoTime(request.value, storageData.autoEndTime, "offTime");
    }
    if (action == "frame_size") {
        error = "Invalid frame size.";
        return setFrameSize(request.value);
    }
    if (action == "quality") {
        error = "Invalid quality.";
        return setJpegQuality(request.value);
    }

    error = "Unknown action.";
    return false;
}

void sendActionResult(const DeviceAction &request, bool success, const String &error = "") {
    sendJson(jsonActionResult(request, success, error));
}

void handleWebSocketText(uint8_t *payload, size_t length) {
    String message;
    message.reserve(length + 1);
    for (size_t i = 0; i < length; i++) {
        message += static_cast<char>(payload[i]);
    }

    Serial.printf("WSS RX: %s\n", message.c_str());
    DeviceAction request = parseDeviceAction(message);
    if (!request.valid) {
        return;
    }

    String error;
    bool success = handleDeviceAction(request, error);
    sendActionResult(request, success, error);
    Serial.printf("WSS action %s: %s\n", request.action.c_str(), success ? "success" : error.c_str());
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.printf("WSS connected: %s\n", payload);
            lastReadyMs = millis();
            sendJson(jsonDeviceReady());
            sendStoredState();
            updateLightMode(true);
            break;

        case WStype_DISCONNECTED:
            Serial.println("WSS disconnected");
            break;

        case WStype_TEXT:
            handleWebSocketText(payload, length);
            break;

        case WStype_ERROR:
            Serial.println("WSS error");
            break;

        default:
            break;
    }
}

void connectWiFi() {
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.setHostname(SECRET_DEVICE_NAME);
    WiFi.mode(WIFI_STA);
    WiFi.setMinSecurity(WIFI_AUTH_OPEN);
    WiFi.setSleep(WIFI_PS_NONE);
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);

    Serial.printf("Connecting Wi-Fi as %s\n", SECRET_DEVICE_NAME);
}

bool waitForWiFi() {
    Serial.print("Waiting for Wi-Fi");
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && !hasElapsed(startMs, WIFI_CONNECT_TIMEOUT_MS)) {
        delay(WIFI_CONNECT_POLL_MS);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi connection timed out");
        return false;
    }

    Serial.print("Wi-Fi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
}

bool syncClock() {
    Serial.print("Syncing clock for TLS");
    time_t now = time(nullptr);
    uint32_t startMs = millis();
    while (now < MIN_VALID_UNIX_TIME && !hasElapsed(startMs, CLOCK_SYNC_TIMEOUT_MS)) {
        delay(CLOCK_SYNC_POLL_MS);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println();

    if (now < MIN_VALID_UNIX_TIME) {
        Serial.println("Clock sync failed; WSS TLS validation may fail");
        return false;
    }

    Serial.print("Clock synced: ");
    Serial.println(ctime(&now));
    return true;
}

void setupWebSocket() {
    if (webSocketStarted || WiFi.status() != WL_CONNECTED) {
        return;
    }

    String path = String(WSS_PATH) + "?deviceId=" + SECRET_DEVICE_NAME;
    if (String(SECRET_DEVICE_TOKEN).length()) {
        path += "&token=" + String(SECRET_DEVICE_TOKEN);
    }

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 4)
    webSocket.beginSslWithBundle(
        SECRET_HOST_ADDRESS,
        SECRET_HOST_PORT,
        path.c_str(),
        rootca_crt_bundle_start,
        rootca_crt_bundle_end - rootca_crt_bundle_start
    );
#else
    webSocket.beginSslWithBundle(SECRET_HOST_ADDRESS, SECRET_HOST_PORT, path.c_str(), rootca_crt_bundle_start);
#endif
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(WSS_RECONNECT_INTERVAL_MS);
    webSocket.enableHeartbeat(WSS_HEARTBEAT_INTERVAL_MS, WSS_HEARTBEAT_TIMEOUT_MS, WSS_HEARTBEAT_MISSED_COUNT);
    webSocketStarted = true;

    Serial.printf("Connecting WSS to wss://%s:%u%s\n", SECRET_HOST_ADDRESS, SECRET_HOST_PORT, path.c_str());
}

void setupTime() {
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    setenv("TZ", TIME_ZONE, 1);
    tzset();
}

StorageData defaultStorageData() {
    return {
        DEFAULT_AUTO_START_TIME,
        DEFAULT_AUTO_END_TIME,
        DEFAULT_LIGHT_MODE,
        CAMERA_FRAME_SIZE,
        CAMERA_JPEG_QUALITY
    };
}

bool repairStorageData(StorageData &data) {
    bool changed = false;
    if (!isValidTime(data.autoStartTime)) {
        data.autoStartTime = DEFAULT_AUTO_START_TIME;
        changed = true;
    }
    if (!isValidTime(data.autoEndTime)) {
        data.autoEndTime = DEFAULT_AUTO_END_TIME;
        changed = true;
    }
    if (data.lightMode > kLightAuto) {
        data.lightMode = DEFAULT_LIGHT_MODE;
        changed = true;
    }
    if (!isValidFrameSize(data.frameSize)) {
        data.frameSize = CAMERA_FRAME_SIZE;
        changed = true;
    }
    if (!isValidJpegQuality(data.quality)) {
        data.quality = CAMERA_JPEG_QUALITY;
        changed = true;
    }
    return changed;
}

void setupStorage() {
    StorageData defaults = defaultStorageData();
    initStorage(&defaults, storageData);

    if (repairStorageData(storageData)) {
        commitStorage(storageData);
        Serial.println("Storage settings repaired");
    }
}

void sendCameraImage() {
    CameraCapture capture;
    if (!captureCameraImage(capture)) {
        return;
    }

    String metadata = jsonImageMetadata(capture.jpgLength);

    Serial.printf("WSS TX image: %u bytes\n", static_cast<unsigned int>(capture.jpgLength));
    if (!sendJson(metadata) || !webSocket.sendBIN(capture.jpgBuffer, capture.jpgLength)) {
        Serial.println("WSS image send failed");
    }

    cleanupCameraCapture(capture);
}

void sendWeatherTelemetry() {
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature() * 1.8f + 32.0f;
    String currentTimeText;

    DanTime currentTime;
    if (currentLocalTime(currentTime)) {
        currentTimeText = formatTime(currentTime);
    }

    sendJson(jsonWeatherTelemetry(humidity, temperature, currentTimeText));
}

void serviceWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    if (shouldRunNow(lastWiFiLogMs, WIFI_RECONNECT_LOG_INTERVAL_MS)) {
        Serial.println("Wi-Fi disconnected; reconnecting...");
        WiFi.reconnect();
    }
}

void serviceConnectedDevice() {
    setupWebSocket();
    webSocket.loop();
    if (!webSocket.isConnected()) {
        return;
    }

    lastReadyMs = millis();
    if (shouldRunNow(lastImageMs, IMAGE_INTERVAL_MS)) {
        sendCameraImage();
    }
    if (shouldRunNow(lastWeatherMs, WEATHER_INTERVAL_MS)) {
        sendWeatherTelemetry();
    }
}

} // namespace

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing...");
    Serial.printf("PSRAM size: %u bytes\n", ESP.getPsramSize());

    dht.setup(DHT_PIN, DHTesp::DHT22);
    setupTime();
    setupStorage();
    if (!setupCamera(storageData.frameSize, storageData.quality)) {
        restartDevice("Camera is required; restarting...");
    }

    pinMode(LIGHT_PIN, OUTPUT);
    digitalWrite(LIGHT_PIN, LOW);
    connectWiFi();
    if (!waitForWiFi()) {
        restartDevice("Wi-Fi is required; restarting...");
    }
    if (!syncClock()) {
        restartDevice("Clock is required for TLS; restarting...");
    }
}

void loop() {
    serviceWiFi();
    if (WiFi.status() == WL_CONNECTED) {
        serviceConnectedDevice();
    }

    if (shouldRunNow(lastLightUpdateMs, LIGHT_UPDATE_INTERVAL_MS)) {
        updateLightMode();
    }

    if (hasElapsed(lastReadyMs, RESTART_IF_NOT_READY_MS)) {
        ESP.restart();
    }
}
