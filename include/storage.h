#pragma once
#include <stdint.h>
#include "esp_camera.h"
#include "utils.h"

typedef struct {
    DanTime autoStartTime;
    DanTime autoEndTime;
    uint8_t lightMode;
    
    framesize_t frame_size;
    int quality;
} StorageData;

const uint32_t INITIALIZED_VALUE = 0x0FEDBEEF + sizeof(StorageData);

bool initStorage(StorageData* defaultStorageData, StorageData& storageData);//returns true if memory was already initialized
void commitStorage(StorageData& storageData);