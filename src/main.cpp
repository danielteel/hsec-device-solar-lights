#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include <esp_wifi.h>
#include "time.h"
#include "camera.h"
#include "utils.h"
#include "secrets.h"
#include "net.h"
#include <time.h> 
#include "storage.h"
#include "DHTesp.h"


const uint8_t dht11Pin = 13;
const uint8_t lightPin = 14;

DHTesp dht;

const char *WiFiSSID = SECRET_WIFI_SSID;
const char *WiFiPass = SECRET_WIFI_PASS;

const uint32_t sendPeriod = 1500;//How often a thing should be sent, and then the state machine advanced
const uint32_t howLongBeforeRestartIfNotConnecting = 300000;//restart esp32 if havent been able to connect to server for 5 minutes


const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";
const char* ntpServer3 = "time.windows.com";
const char* timeZone = "MST7MDT,M3.2.0,M11.1.0";//https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

StorageData storageData;


Net NetClient(SECRET_DEVICE_NAME, SECRET_ENCROKEY, SECRET_HOST_ADDRESS, SECRET_HOST_PORT);


void packetReceived(uint8_t* data, uint32_t dataLength){
    sensor_t * s;
    switch (data[0]){
        case 0:
            storageData.lightMode=0;
            commitStorage(storageData);
            break;
        case 1:
            storageData.lightMode=1;
            commitStorage(storageData);
            break;
        case 2:
            storageData.lightMode=2;
            commitStorage(storageData);
            break;
        case 3:
            storageData.autoStartHours=data[1];
            if (storageData.autoStartHours>23) storageData.autoStartHours=23;
            if (storageData.autoStartHours<0) storageData.autoStartHours=0;
            commitStorage(storageData);
            break;
        case 4:
            storageData.autoStartMinutes=data[1];
            if (storageData.autoStartMinutes>59) storageData.autoStartMinutes=59;
            if (storageData.autoStartMinutes<0) storageData.autoStartMinutes=0;
            commitStorage(storageData);
            break;
        case 5:
            storageData.autoEndHours=data[1];
            if (storageData.autoEndHours>23) storageData.autoEndHours=23;
            if (storageData.autoEndHours<0) storageData.autoEndHours=0;
            commitStorage(storageData);
            break;
        case 6:
            storageData.autoEndMinutes=data[1];
            if (storageData.autoEndMinutes>59) storageData.autoEndMinutes=59;
            if (storageData.autoEndMinutes<0) storageData.autoEndMinutes=0;
            commitStorage(storageData);
            break;
            
        case 7:
            s = esp_camera_sensor_get();
            if (s) s->set_quality(s, data[1]);
            storageData.quality=data[1];
            commitStorage(storageData);
            break;
        case 8:
            s = esp_camera_sensor_get();
            if (s) s->set_framesize(s, (framesize_t)data[1]);
            storageData.frame_size=(framesize_t)data[1];
            commitStorage(storageData);
            break;
    }
}

void onConnected(){
    Serial.println("NetClient Connected");
    NetClient.sendString("i=Lights Off:void:0,Lights On:void:1,Auto:void:2,Start Hour:byte:3,Start Minute:byte:4,End Hour:byte:5,End Minute:byte:6,Qual(8best-63worst):byte:7,FrameSize(3|5|7):byte:8");
}

void onDisconnected(){
    Serial.println("NetClient disconnected");
}

void WiFiSetup(bool doRandomMAC){
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);

    uint8_t newMac[6]={0,0,0,0,0,0};
    String newHostName=SECRET_DEVICE_NAME;
    if (doRandomMAC){
        esp_fill_random(newMac+1, 5);
        for (int i=0;i<6;i++){
            newHostName+=String(newMac[i], 16);
        }
    }
    WiFi.setHostname(newHostName.c_str());

    WiFi.mode(WIFI_STA);
    if (doRandomMAC) esp_wifi_set_mac(WIFI_IF_STA, newMac);
    WiFi.setMinSecurity(WIFI_AUTH_OPEN);
    WiFi.setSleep(WIFI_PS_NONE);

    WiFi.begin(WiFiSSID, WiFiPass);
    Serial.println("WiFiSetup:");
    Serial.print("    Mac Address:");
    Serial.println(WiFi.macAddress());
    Serial.print("    Hostname:");
    Serial.println(newHostName);
}

void setup(){
    //Setup serial comm
    Serial.begin(115200);
    Serial.println("Initializing...");

    //Setup DHT11
    dht.setup(dht11Pin, DHTesp::DHT22);


    //Setup time
    configTime(0, 0, ntpServer1, ntpServer2, ntpServer3);  // 0, 0 because we will use TZ in the next line
    setenv("TZ", timeZone, 1);            // Set environment variable with your time zone
    tzset();

    //Setup non volatile storage
    StorageData defaultStorage;
    defaultStorage.autoStartHours=20;
    defaultStorage.autoStartMinutes=0;
    defaultStorage.autoEndHours=22;
    defaultStorage.autoEndMinutes=0;
    defaultStorage.lightMode=2;
    defaultStorage.frame_size=FRAMESIZE_HVGA;
    defaultStorage.quality=16;
    initStorage(&defaultStorage, storageData);
    
    //Setup camera
    cameraSetup(storageData.frame_size, storageData.quality);

    //Setup IO
    pinMode(lightPin, OUTPUT);
    digitalWrite(lightPin, LOW);

    //Setup WiFi
    WiFiSetup(false);

    //Setup NetClient
    NetClient.setPacketReceivedCallback(&packetReceived);
    NetClient.setOnConnected(&onConnected);
    NetClient.setOnDisconnected(&onDisconnected);
}


bool isInTimeWindow(int currentHour, int currentMinute, int startHour, int startMinute, int endHour, int endMinute){
    float start=(float)startHour+((float)startMinute/60.0f);
    float end=(float)endHour+((float)endMinute/60.0f);
    float current=(float)currentHour+((float)currentMinute/60.0f);

    if (start<end){
        if (current>=start && current<end) return true;
        return false;
    }
    if (end<start){
        if (current>=start) return true;
        if (current<end) return true;
        return false;
    }
    return false;
}

void updateLightMode(){
    if (storageData.lightMode==0){
        digitalWrite(lightPin, LOW);
    }else if (storageData.lightMode==1){
        digitalWrite(lightPin, HIGH);
    }else{
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo, 0)){
            digitalWrite(lightPin, LOW);
        }else{
            bool autoModeStatus=isInTimeWindow(timeinfo.tm_hour, timeinfo.tm_min, storageData.autoStartHours, storageData.autoStartMinutes, storageData.autoEndHours, storageData.autoEndMinutes);
            digitalWrite(lightPin, autoModeStatus);
        }
    }
}


class StateMachine {
    public:
        StateMachine(uint8_t max){
            maxStates=max;
        }

        uint8_t maxStates=0;
        uint8_t state=0;

        void next(){
            state++;
            if (state>=maxStates){
                state=0;
            }
        }
};

void loop(){
    static StateMachine sendState(2);

    static uint32_t lastConnectTime=0;
    static uint8_t failReconnects=0;

    static uint32_t lastSendTime=0;
    static uint32_t lastReadyTime=0;

    static uint32_t lastUpdateLight=0;

    uint32_t currentTime = millis();

    if (WiFi.status() != WL_CONNECTED){//Reconnect to WiFi
        if (isTimeToExecute(lastConnectTime, 2000)){
            Serial.println("Waiting for autoreconnect...");
            failReconnects++;
            if (failReconnects>30){
                Serial.println("Autoreconnect failed, generating new MAC and retrying...");
                failReconnects=0;
                WiFiSetup(true);
            }
        }
    }else{
        failReconnects=0;
        if (NetClient.loop()){
            lastReadyTime=currentTime;

            if (isTimeToExecute(lastSendTime, sendPeriod)){
                switch (sendState.state){
                    case 0:
                        CAMERA_CAPTURE capture;
                        if (cameraCapture(capture)){
                            NetClient.sendBinary(capture.jpgBuff, capture.jpgBuffLen);
                            cameraCaptureCleanup(capture);
                        }else{
                            Serial.println("failed to capture ");
                        }
                        break;

                    case 1:
                        float humidity = dht.getHumidity();
                        float temperature = dht.getTemperature();
                        String weather=String("w=humidity:")+String(humidity)+String(",temperature:")+String(temperature);
                        NetClient.sendString(weather);
                        break;
                }
                sendState.next();
            }
        }
    }

    if (isTimeToExecute(lastUpdateLight, 100)){
        updateLightMode();
    }
    
    if (currentTime-lastReadyTime > howLongBeforeRestartIfNotConnecting || lastReadyTime>currentTime){//Crude edge case handling, if overflow, just restart
        ESP.restart();
    }
}