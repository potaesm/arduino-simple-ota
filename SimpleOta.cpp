#include "Arduino.h"
#include "SimpleWifi.h"
#if defined(ESP8266)
#warning "Using ESP8266"
#include "FS.h"
#include <ESP8266HTTPClient.h>
#include "ESP8266WiFi.h"
#elif defined(ESP32)
#warning "Using ESP32"
#include "SPIFFS.h"
#include "Update.h"
#include <HTTPClient.h>
#include "WiFi.h"
#else
#error "No ESP8266 or ESP32 detected"
#endif

#define DEVMODE 1

// Download file via proxy server
// char *proxyServerURL = "http://node-pxy.herokuapp.com";

char *__wifiSSID;
char *__wifiPassword;

bool __downloadFileToSPIFFS(WiFiClient wiFiClient, String fileURL, String fileName)
{
    bool isDownloaded = false;
    File file;
    HTTPClient Http;
    // fileURL = String(proxyServerURL) + "/download-file?url=" + fileURL;
    Http.begin(wiFiClient, fileURL);
#if defined(DEVMODE)
    Serial.printf("Begin HTTP GET request: %s\n", fileURL.c_str());
#endif
    int httpCode = Http.GET();
    if (httpCode > 0)
    {
#if defined(DEVMODE)
        Serial.printf("Status code: %d\n", httpCode);
#endif
        if (httpCode == HTTP_CODE_OK)
        {
            file = SPIFFS.open(fileName, "w");
            int len = Http.getSize();
#if defined(DEVMODE)
            Serial.printf("File size: %d bytes\n", len);
            Serial.print("Downloading...\n");
#endif
            uint8_t buff[512] = {0};
            WiFiClient *stream = Http.getStreamPtr();
            while (Http.connected() && (len > 0 || len == -1))
            {
                if (!isWiFiConnected())
                    connectWifi(__wifiSSID, __wifiPassword);
                size_t size = stream->available();
                if (size)
                {
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
#if defined(DEVMODE)
                    // Serial.printf("Read: %d bytes\n", c);
                    // Serial.printf("Remaining: %d bytes\n", size);
#endif
                    file.write(buff, c);
                    if (len > 0)
                        len -= c;
                }
                // delayMicroseconds(1);
                ESP.wdtFeed();
            }
            isDownloaded = true;
#if defined(DEVMODE)
            Serial.print("HTTP connection closed or file end\n");
#endif
        }
    }
#if defined(DEVMODE)
    else
    {
        Serial.printf("HTTP GET failed: %s\n", Http.errorToString(httpCode).c_str());
    }
#endif
    Http.end();
    file.close();
    return isDownloaded;
}

void __updateFromSPIFFS(String fileName)
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
    size_t fileSize = file.size();
    if (!Update.begin(fileSize))
    {
#if defined(DEVMODE)
        Update.printError(Serial);
#endif
        return;
    }
    Update.writeStream(file);
    if (!Update.end())
    {
#if defined(DEVMODE)
        Update.printError(Serial);
#endif
        return;
    }
    file.close();
#if defined(DEVMODE)
    Serial.println("Updated!");
    Serial.println("Rebooting...");
#endif
    ESP.restart();
}

void initializeOta(WiFiClient wiFiClient, char *wifiSSID, char *wifiPassword, String fileURL, String fileName)
{
    __wifiSSID = wifiSSID;
    __wifiPassword = wifiPassword;
    ESP.wdtDisable();
    ESP.wdtEnable(WDTO_8S);
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
    if (SPIFFS.exists(fileName))
        SPIFFS.remove(fileName);
    bool isDownloaded = __downloadFileToSPIFFS(wiFiClient, fileURL, fileName);
    if (isDownloaded)
    {
        disconnectWifi();
        __updateFromSPIFFS(fileName);
        return;
    }
#if defined(DEVMODE)
    Serial.println("Update file is not downloaded");
#endif
}