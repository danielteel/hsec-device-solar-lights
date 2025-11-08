#include "utils.h"

bool isTimeToExecute(uint32_t &lastTime, uint32_t interval){
    uint32_t currentTime = millis();
    if ((currentTime - lastTime) >= interval || currentTime < lastTime){
        lastTime = currentTime;
        return true;
    }
    return false;
}


bool isInTimeWindow(DanTime* currentTime, DanTime* startTime, DanTime* endTime){
    float start=(float)startTime->hours+((float)startTime->minutes/60.0f)+((float)startTime->seconds/3600.0f);
    float end=(float)endTime->hours+((float)endTime->minutes/60.0f)+((float)endTime->seconds/3600.0f);
    float current=(float)currentTime->hours+((float)currentTime->minutes/60.0f)+((float)currentTime->seconds/3600.0f);

    if (start<end){
        if (current>=start && current<end) return true;
    }
    if (end<start){
        if (current>=start) return true;
        if (current<end) return true;
    }
    return false;
}