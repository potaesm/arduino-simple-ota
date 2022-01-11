#ifndef SimpleOta_h
#define SimpleOta_h
#include "Arduino.h"
#if defined(ESP8266)
#include "ESP8266WiFi.h"
#elif defined(ESP32)
#include "WiFi.h"
#endif

extern void initializeOta(WiFiClient wiFiClient, char *wifiSSID, char *wifiPassword, String fileURL, String fileName = "update.bin");

#endif