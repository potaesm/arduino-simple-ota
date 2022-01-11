#include "Arduino.h"
#include "SimpleWifi.h"
#include "FS.h"
#if defined(ESP8266)
#warning "Using ESP8266"
#include <ESP8266HTTPClient.h>
#include "ESP8266WiFi.h"
#elif defined(ESP32)
#warning "Using ESP32"
#include <HTTPClient.h>
#include "WiFi.h"
#else
#error "No ESP8266 or ESP32 detected"
#endif

#define DEVMODE 1

bool downloadFileToSPIFFS(WiFiClient wiFiClient, String fileURL, String fileName)
{
    bool isDownloaded = false;
    File file;
    HTTPClient Http;
    Http.begin(wiFiClient, fileURL);
#if defined(DEVMODE)
    Serial.printf("Begin HTTP GET request: %s\n", fileURL);
#endif
    int httpCode = Http.GET();
    if (httpCode > 0)
    {
#if defined(DEVMODE)
        Serial.printf("Status code: %d\n", httpCode);
#endif
        if (httpCode == HTTP_CODE_OK)
        {
            if (SPIFFS.exists(fileName))
            {
                SPIFFS.remove(fileName);
            }
            file = SPIFFS.open(fileName, "w");
            int len = Http.getSize();
#if defined(DEVMODE)
            Serial.printf("Payload size: %d bytes\n", len);
#endif
            uint8_t buff[2048] = {0};
            WiFiClient *stream = Http.getStreamPtr();
            while (Http.connected() && (len > 0 || len == -1))
            {
                size_t size = stream->available();
                if (size)
                {
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
#if defined(DEVMODE)
                    Serial.printf("Read: %d bytes\n", c);
                    Serial.printf("Remaining: %d bytes\n", size);
#endif
                    file.write(buff, c);
                    if (len > 0)
                    {
                        len -= c;
                    }
                }
                delayMicroseconds(1);
            }
            isDownloaded = true;
#if defined(DEVMODE)
            Serial.print("HTTP connection closed or file end\n");
#endif
        }
    }
    else
    {
#if defined(DEVMODE)
        Serial.printf("HTTP GET failed: %s\n", Http.errorToString(httpCode).c_str());
#endif
    }
    Http.end();
    file.close();
    return isDownloaded;
}

void updateFromSPIFFS(String fileName)
{
#if defined(DEVMODE)
    Serial.println("Opening update file...");
#endif
    if (!SPIFFS.exists(fileName))
    {
#if defined(DEVMODE)
        Serial.println("Update file not found");
#endif
        return;
    }
    File file = SPIFFS.open(fileName, "r");
#if defined(DEVMODE)
    Serial.println("Updating...");
#endif
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace, U_FLASH))
    {
#if defined(DEVMODE)
        Update.printError(Serial);
#endif
    }
    while (file.available())
    {
        uint8_t ibuffer[128];
        file.read((uint8_t *)ibuffer, 128);
#if defined(DEVMODE)
        Serial.println((char *)ibuffer);
#endif
        Update.write(ibuffer, sizeof(ibuffer));
    }
    if (Update.end(true))
    {
#if defined(DEVMODE)
        Serial.println("Update success!");
        Serial.println("Rebooting...");
#endif
    }
    else
    {
#if defined(DEVMODE)
        Update.printError(Serial);
#endif
    }
    file.close();
}

void initializeOta(WiFiClient wiFiClient, char *wifiSSID, char *wifiPassword, String fileURL, String fileName)
{
#if defined(DEVMODE)
    Serial.println("Connecting WiFi...");
#endif
    connectWifi(wifiSSID, wifiPassword);
#if defined(DEVMODE)
    Serial.println("Checking Internet...");
#endif
    checkInternet(wiFiClient, wifiSSID, wifiPassword);
#if defined(DEVMODE)
    Serial.println("Initializing SPIFFS...");
#endif
    if (!SPIFFS.begin())
    {
#if defined(DEVMODE)
        Serial.println("Unable to activate SPIFFS");
#endif
        return;
    }
    else
    {
#if defined(DEVMODE)
        Serial.println("SPIFFS is activated");
#endif
    }
    bool isDownloaded = downloadFileToSPIFFS(wiFiClient, fileURL, fileName);
    if (isDownloaded)
    {
        disconnectWifi();
        updateFromSPIFFS(fileName);
    }
    else
    {
#if defined(DEVMODE)
        Serial.println("Update file is not downloaded");
#endif
    }
}