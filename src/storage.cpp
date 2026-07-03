#include <EEPROM.h>

#include "storage.h"

namespace {

static constexpr uint32_t fnv1a(const char* text, uint32_t hash = 2166136261UL) {
    return *text ? fnv1a(text + 1, (hash ^ static_cast<uint8_t>(*text)) * 16777619UL) : hash;
}

static constexpr uint32_t kInitializedValue = fnv1a(__DATE__ " " __TIME__);
static constexpr uint32_t kStorageOffset = sizeof(kInitializedValue);

} // namespace

bool initStorage(const StorageData *defaultStorageData, StorageData &storageData) {
    bool wasAlreadyInitialized = true;
    EEPROM.begin(sizeof(StorageData) + sizeof(kInitializedValue));
    if (EEPROM.readULong(0) != kInitializedValue) {
        wasAlreadyInitialized = false;
        EEPROM.writeULong(0, kInitializedValue);

        if (defaultStorageData) {
            EEPROM.writeBytes(kStorageOffset, defaultStorageData, sizeof(StorageData));
        } else {
            for (uint32_t i = 0; i < sizeof(StorageData); i++) {
                EEPROM.writeByte(kStorageOffset + i, 0);
            }
        }

        EEPROM.commit();
    }

    EEPROM.readBytes(kStorageOffset, &storageData, sizeof(StorageData));
    return wasAlreadyInitialized;
}

void commitStorage(const StorageData &storageData) {
    EEPROM.writeBytes(kStorageOffset, &storageData, sizeof(StorageData));
    EEPROM.commit();
}
