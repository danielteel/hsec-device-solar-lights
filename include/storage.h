#pragma once
#include <stdint.h>
#include "esp_camera.h"

typedef struct {
    uint8_t autoStartHours;
    uint8_t autoStartMinutes;
    uint8_t autoStartSeconds;
    uint8_t autoEndHours;
    uint8_t autoEndMinutes;
    uint8_t autoEndSeconds;
    uint8_t lightMode;
    
    framesize_t frame_size;
    int quality;
} StorageData;

const uint32_t INITIALIZED_VALUE = 0x0FEDBEEF + sizeof(StorageData);

bool initStorage(StorageData* defaultStorageData, StorageData& storageData);//returns true if memory was already initialized
void commitStorage(StorageData& storageData);