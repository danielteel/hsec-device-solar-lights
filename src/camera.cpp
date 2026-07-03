#include "camera.h"

#include "config.h"
#include "common.h"

namespace {

struct CameraSettings {
    framesize_t frameSize;
    int jpegQuality;
};

CameraSettings activeSettings = {CAMERA_FRAME_SIZE, CAMERA_JPEG_QUALITY};
bool cameraReady = false;

bool isValidCameraSettings(CameraSettings settings) {
    return isValidFrameSize(settings.frameSize) && isValidJpegQuality(settings.jpegQuality);
}

camera_config_t buildCameraConfig(CameraSettings settings) {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAMERA_PIN_D0;
    config.pin_d1 = CAMERA_PIN_D1;
    config.pin_d2 = CAMERA_PIN_D2;
    config.pin_d3 = CAMERA_PIN_D3;
    config.pin_d4 = CAMERA_PIN_D4;
    config.pin_d5 = CAMERA_PIN_D5;
    config.pin_d6 = CAMERA_PIN_D6;
    config.pin_d7 = CAMERA_PIN_D7;
    config.pin_xclk = CAMERA_PIN_XCLK;
    config.pin_pclk = CAMERA_PIN_PCLK;
    config.pin_vsync = CAMERA_PIN_VSYNC;
    config.pin_href = CAMERA_PIN_HREF;
    config.pin_sccb_sda = CAMERA_PIN_SIOD;
    config.pin_sccb_scl = CAMERA_PIN_SIOC;
    config.pin_pwdn = CAMERA_PIN_PWDN;
    config.pin_reset = CAMERA_PIN_RESET;
    config.xclk_freq_hz = CAMERA_XCLK_FREQ_HZ;
    config.frame_size = settings.frameSize;
    config.pixel_format = PIXFORMAT_JPEG;
    config.grab_mode = CAMERA_GRAB_MODE;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = settings.jpegQuality;
    config.fb_count = CAMERA_FB_COUNT;
    return config;
}

bool applyCameraProfile(
    const char *name,
    int brightness,
    int contrast,
    int saturation,
    int agcGain,
    gainceiling_t gainCeiling,
    int aec2
) {
    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        Serial.printf("Camera %s mode failed: sensor unavailable\n", name);
        return false;
    }

    bool ok = true;
    ok = sensor->set_exposure_ctrl(sensor, 1) == 0 && ok;
    ok = sensor->set_gain_ctrl(sensor, 1) == 0 && ok;
    ok = sensor->set_aec2(sensor, aec2) == 0 && ok;
    ok = sensor->set_brightness(sensor, brightness) == 0 && ok;
    ok = sensor->set_contrast(sensor, contrast) == 0 && ok;
    ok = sensor->set_saturation(sensor, saturation) == 0 && ok;
    ok = sensor->set_agc_gain(sensor, agcGain) == 0 && ok;
    ok = sensor->set_gainceiling(sensor, gainCeiling) == 0 && ok;
    ok = sensor->set_bpc(sensor, 1) == 0 && ok;
    ok = sensor->set_wpc(sensor, 1) == 0 && ok;
    ok = sensor->set_raw_gma(sensor, 1) == 0 && ok;
    ok = sensor->set_lenc(sensor, 1) == 0 && ok;

    Serial.printf("Camera %s mode %s\n", name, ok ? "enabled" : "partially failed");
    return ok;
}

bool initCamera(CameraSettings settings) {
    camera_config_t config = buildCameraConfig(settings);
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor) {
        sensor->set_vflip(sensor, CAMERA_VFLIP);
        sensor->set_hmirror(sensor, CAMERA_HMIRROR);
    }

    cameraReady = true;
    activeSettings = settings;
    setCameraDayMode();
    Serial.printf(
        "Camera initialized: %s quality %d\n",
        formatFrameSize(activeSettings.frameSize).c_str(),
        activeSettings.jpegQuality
    );
    return true;
}

bool deinitCamera() {
    if (!cameraReady) {
        return true;
    }

    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        Serial.printf("Camera deinit failed with error 0x%x\n", err);
        return false;
    }

    cameraReady = false;
    delay(50);
    return true;
}

bool configureCamera(CameraSettings settings) {
    if (!isValidCameraSettings(settings)) {
        Serial.printf("Camera config rejected: frame %d quality %d\n", static_cast<int>(settings.frameSize), settings.jpegQuality);
        return false;
    }

    if (cameraReady &&
        settings.frameSize == activeSettings.frameSize &&
        settings.jpegQuality == activeSettings.jpegQuality) {
        return true;
    }

    CameraSettings previousSettings = activeSettings;
    bool hadCamera = cameraReady;

    if (!deinitCamera()) {
        return false;
    }

    if (initCamera(settings)) {
        return true;
    }

    if (hadCamera) {
        Serial.println("Restoring previous camera config");
        initCamera(previousSettings);
    }

    return false;
}

} // namespace

bool setupCamera(framesize_t frameSize, int jpegQuality) {
    return configureCamera({frameSize, jpegQuality});
}

bool setCameraFrameSize(framesize_t frameSize) {
    return configureCamera({frameSize, activeSettings.jpegQuality});
}

bool setCameraJpegQuality(int jpegQuality) {
    return configureCamera({activeSettings.frameSize, jpegQuality});
}

bool setCameraDayMode() {
    return applyCameraProfile(
        "day",
        CAMERA_DAY_BRIGHTNESS,
        CAMERA_DAY_CONTRAST,
        CAMERA_DAY_SATURATION,
        CAMERA_DAY_AGC_GAIN,
        CAMERA_DAY_GAIN_CEILING,
        CAMERA_DAY_AEC2
    );
}

bool setCameraNightMode() {
    return applyCameraProfile(
        "night",
        CAMERA_NIGHT_BRIGHTNESS,
        CAMERA_NIGHT_CONTRAST,
        CAMERA_NIGHT_SATURATION,
        CAMERA_NIGHT_AGC_GAIN,
        CAMERA_NIGHT_GAIN_CEILING,
        CAMERA_NIGHT_AEC2
    );
}

bool captureCameraImage(CameraCapture &capture) {
    capture.jpgBuffer = nullptr;
    capture.jpgLength = 0;
    capture.frameBuffer = esp_camera_fb_get();

    if (!capture.frameBuffer) {
        Serial.println("Camera capture failed");
        return false;
    }

    if (capture.frameBuffer->format == PIXFORMAT_JPEG) {
        capture.jpgBuffer = capture.frameBuffer->buf;
        capture.jpgLength = capture.frameBuffer->len;
        return true;
    }

    bool converted = frame2jpg(capture.frameBuffer, activeSettings.jpegQuality, &capture.jpgBuffer, &capture.jpgLength);
    if (!converted) {
        Serial.println("JPEG conversion failed");
        cleanupCameraCapture(capture);
        return false;
    }

    return true;
}

void cleanupCameraCapture(CameraCapture &capture) {
    if (capture.frameBuffer) {
        if (capture.frameBuffer->format != PIXFORMAT_JPEG && capture.jpgBuffer) {
            free(capture.jpgBuffer);
        }
        esp_camera_fb_return(capture.frameBuffer);
    }

    capture.jpgBuffer = nullptr;
    capture.jpgLength = 0;
    capture.frameBuffer = nullptr;
}
