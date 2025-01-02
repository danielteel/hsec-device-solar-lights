#include "utils.h"

bool isTimeToExecute(uint32_t &lastTime, uint32_t interval){
    uint32_t currentTime = millis();
    if ((currentTime - lastTime) >= interval || currentTime < lastTime){
        lastTime = currentTime;
        return true;
    }
    return false;
}

void buildKey(const char* keyString, uint8_t* key){
    char tempHex[] = { 0,0,0 };
    for (uint8_t i = 0; i < 64; i += 2) {
        tempHex[0] = keyString[i];
        tempHex[1] = keyString[i + 1];
        key[i >> 1] = strtoul(tempHex, nullptr, 16);
    }
}