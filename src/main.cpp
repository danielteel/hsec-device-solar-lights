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

bool isLightOn=false;
bool lastLightOn=false;

DHTesp dht;

const char *WiFiSSID = SECRET_WIFI_SSID;
const char *WiFiPass = SECRET_WIFI_PASS;

const uint32_t picturePeriod = 2000;
const uint32_t weatherPeriod = 2000;
const uint32_t logPeriod = 120000;

const uint32_t howLongBeforeRestartIfNotConnecting = 300000;//restart esp32 if havent been able to connect to server for 5 minutes


const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";
const char* ntpServer3 = "time.windows.com";
const char* timeZone = "MST7MDT,M3.2.0,M11.1.0";//https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

StorageData storageData;


Net NetClient(SECRET_DEVICE_NAME, SECRET_ENCROKEY, SECRET_HOST_ADDRESS, SECRET_HOST_PORT);


void updateLightMode(bool forceSend){
    if (storageData.lightMode==0){
        digitalWrite(lightPin, LOW);
        isLightOn=false;
    }else if (storageData.lightMode==1){
        digitalWrite(lightPin, HIGH);
        isLightOn=true;
    }else{
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo, 0)){
            digitalWrite(lightPin, LOW);
            isLightOn=false;
        }else{
            DanTime currentTime={timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec};
            bool autoModeStatus=isInTimeWindow( &currentTime, &storageData.autoStartTime, &storageData.autoEndTime );
        
            digitalWrite(lightPin, autoModeStatus);
            isLightOn=autoModeStatus;
        }
    }
    if (isLightOn!=lastLightOn || forceSend){
        NetClient.sendString(String("lightOn=")+String((uint8_t)isLightOn));
        lastLightOn=isLightOn;
    }
}


void packetReceived(uint8_t* data, uint32_t dataLength){
    sensor_t * s;
    int32_t* numValue=(int32_t*)(data+1);

    DanTime* time=(DanTime*)(data+1);

    switch (data[0]){
        case 0:
            storageData.lightMode=0;
            commitStorage(storageData);
            NetClient.sendString(String("lightMode=")+String(storageData.lightMode));
            updateLightMode(false);
            break;
        case 1:
            storageData.lightMode=1;
            commitStorage(storageData);
            NetClient.sendString(String("lightMode=")+String(storageData.lightMode));
            updateLightMode(false);
            break;
        case 2:
            storageData.lightMode=2;
            commitStorage(storageData);
            NetClient.sendString(String("lightMode=")+String(storageData.lightMode));
            updateLightMode(false);
            break;
        case 3:
            storageData.autoStartTime=*time;
            if (storageData.autoStartTime.hours>23) storageData.autoStartTime.hours=23;
            if (storageData.autoStartTime.minutes>59) storageData.autoStartTime.minutes=59;
            if (storageData.autoStartTime.seconds>59) storageData.autoStartTime.seconds=59;
            
            commitStorage(storageData);
            NetClient.sendString(String("onTime=")+String(storageData.autoStartTime.hours)+String(":")+String(storageData.autoStartTime.minutes)+String(":")+String(storageData.autoStartTime.seconds));
            updateLightMode(false);
            break;
        case 4:
            storageData.autoEndTime=*time;
            if (storageData.autoEndTime.hours>23) storageData.autoEndTime.hours=23;
            if (storageData.autoEndTime.minutes>59) storageData.autoEndTime.minutes=59;
            if (storageData.autoEndTime.seconds>59) storageData.autoEndTime.seconds=59;

            commitStorage(storageData);
            NetClient.sendString(String("offTime=")+String(storageData.autoEndTime.hours)+String(":")+String(storageData.autoEndTime.minutes)+String(":")+String(storageData.autoEndTime.seconds));
            updateLightMode(false);
            break;
        case 6:
            s = esp_camera_sensor_get();
            if (s) s->set_quality(s, *numValue);
            storageData.quality=*numValue;
            commitStorage(storageData);
            break;
        case 7:
            s = esp_camera_sensor_get();
            if (s) s->set_framesize(s, (framesize_t)*numValue);
            storageData.frame_size=(framesize_t)*numValue;
            commitStorage(storageData);
            break;
    }
}

void onConnected(){
    Serial.println("NetClient Connected");
    NetClient.sendString(String("lightMode=")+String(storageData.lightMode));
    NetClient.sendString(String("onTime=")+String(storageData.autoStartTime.hours)+String(":")+String(storageData.autoStartTime.minutes)+String(":")+String(storageData.autoStartTime.seconds));
    NetClient.sendString(String("offTime=")+String(storageData.autoEndTime.hours)+String(":")+String(storageData.autoEndTime.minutes)+String(":")+String(storageData.autoEndTime.seconds));
    updateLightMode(true);
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
    defaultStorage.autoStartTime={20, 0, 0};
    defaultStorage.autoEndTime={22, 0, 0};
    defaultStorage.lightMode=2;
    defaultStorage.frame_size=FRAMESIZE_HQVGA;
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

void loop(){
    static uint32_t lastConnectTime=0;
    static uint8_t failReconnects=0;

    static uint32_t lastReadyTime=0;
    static uint32_t lastPictureSendTime=0;
    static uint32_t lastWeatherSendTime=0;
    static uint32_t lastLogTime=0;

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

            if (isTimeToExecute(lastPictureSendTime, picturePeriod)){
                CAMERA_CAPTURE capture;
                if (cameraCapture(capture)){
                    NetClient.sendBinary(capture.jpgBuff, capture.jpgBuffLen);
                    cameraCaptureCleanup(capture);
                }else{
                    Serial.println("failed to capture ");
                }
            }
            if (isTimeToExecute(lastWeatherSendTime, weatherPeriod)){
                float humidity = dht.getHumidity();
                float temperature = dht.getTemperature()*1.8f+32.0f;
                NetClient.sendString(String("humidity=")+String(humidity, 1));
                NetClient.sendString(String("temperature=")+String(temperature, 1));

                struct tm timeinfo;
                if(getLocalTime(&timeinfo, 0)){
                    NetClient.sendString(String("currentTime=")+String(timeinfo.tm_hour)+String(":")+String(timeinfo.tm_min)+":"+String(timeinfo.tm_sec));
                }
                
            }
            if (isTimeToExecute(lastLogTime, logPeriod)){
                NetClient.sendString("log");
            }
        }
    }

    if (isTimeToExecute(lastUpdateLight, 1000)){
        updateLightMode(false);
    }
    
    if (currentTime-lastReadyTime > howLongBeforeRestartIfNotConnecting || lastReadyTime>currentTime){//Crude edge case handling, if overflow, just restart
        ESP.restart();
    }
}