#pragma once

#include <stdint.h>


typedef struct {
    int8_t autoStartHours;
    int8_t autoStartMinutes;
    int8_t autoEndHours;
    int8_t autoEndMinutes;
    int8_t lightMode;
} StorageData;

const uint32_t INITIALIZED_VALUE = 0x0BADBEEF + sizeof(StorageData);

bool initStorage(StorageData* defaultStorageData, StorageData& storageData);//returns true if memory was already initialized
void commitStorage(StorageData& storageData);