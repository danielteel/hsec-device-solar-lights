#pragma once

#include <Arduino.h>
#include <esp_camera.h>

#include "config.h"

struct CameraCapture {
    uint8_t *jpgBuffer = nullptr;
    size_t jpgLength = 0;
    camera_fb_t *frameBuffer = nullptr;
};

bool setupCamera(framesize_t frameSize = CAMERA_FRAME_SIZE, int jpegQuality = CAMERA_JPEG_QUALITY);
bool setCameraFrameSize(framesize_t frameSize);
bool setCameraJpegQuality(int jpegQuality);
bool setCameraDayMode();
bool setCameraNightMode();
bool captureCameraImage(CameraCapture &capture);
void cleanupCameraCapture(CameraCapture &capture);
