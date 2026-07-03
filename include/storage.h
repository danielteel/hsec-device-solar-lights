#pragma once
#include <stdint.h>
#include "common.h"
#include "esp_camera.h"

struct StorageData {
    DanTime autoStartTime;
    DanTime autoEndTime;
    uint8_t lightMode;

    framesize_t frameSize;
    int quality;
};

bool initStorage(const StorageData *defaultStorageData, StorageData &storageData);
void commitStorage(const StorageData &storageData);
