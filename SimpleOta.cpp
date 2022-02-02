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

String proxyServerURL = "http://node-pxy.herokuapp.com/download-file?url=";

char *_wifiSSID;
char *_wifiPassword;

bool downloadFileToSPIFFS(WiFiClient wiFiClient, String fileURL, String fileName)
{
    bool isDownloaded = false;
    File file;
    HTTPClient Http;
    Http.begin(wiFiClient, proxyServerURL + fileURL);
    Serial.printf("Begin HTTP GET request: %s\n", fileURL.c_str());
    int httpCode = Http.GET();
    if (httpCode > 0)
    {
        Serial.printf("Status code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK)
        {
            if (SPIFFS.exists(fileName))
                SPIFFS.remove(fileName);
            file = SPIFFS.open(fileName, "w");
            int len = Http.getSize();
            Serial.printf("Payload size: %d bytes\n", len);
            uint8_t buff[1024] = {0};
            WiFiClient *stream = Http.getStreamPtr();
            while (Http.connected() && (len > 0 || len == -1))
            {
                if (!isWiFiConnected())
                {
                    connectWifi(_wifiSSID, _wifiPassword);
                }
                size_t size = stream->available();
                if (size)
                {
                    int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                    // Serial.printf("Read: %d bytes\n", c);
                    // Serial.printf("Remaining: %d bytes\n", size);
                    file.write(buff, c);
                    if (len > 0)
                        len -= c;
                }
                // delayMicroseconds(1);
                ESP.wdtFeed();
            }
            isDownloaded = true;
            Serial.print("HTTP connection closed or file end\n");
        }
    }
    else
    {
        Serial.printf("HTTP GET failed: %s\n", Http.errorToString(httpCode).c_str());
    }
    Http.end();
    file.close();
    return isDownloaded;
}

void updateFromSPIFFS(String fileName)
{
    Serial.println("Opening update file...");
    if (!SPIFFS.exists(fileName))
    {
        Serial.println("Update file not found");
        return;
    }
    File file = SPIFFS.open(fileName, "r");
    Serial.println("Updating...");
    size_t fileSize = file.size();
    if (!Update.begin(fileSize))
    {
        Update.printError(Serial);
        return;
    }
    Update.writeStream(file);
    if (!Update.end())
    {
        Update.printError(Serial);
        return;
    }
    file.close();
    Serial.println("Update success!");
    Serial.println("Rebooting...");
    ESP.restart();
}

void initializeOta(WiFiClient wiFiClient, char *wifiSSID, char *wifiPassword, String fileURL, String fileName)
{
    _wifiSSID = wifiSSID;
    _wifiPassword = wifiPassword;
    ESP.wdtDisable();
    ESP.wdtEnable(WDTO_8S);
    Serial.println("Connecting WiFi...");
    connectWifi(wifiSSID, wifiPassword);
    Serial.println("Checking Internet...");
    checkInternet(wiFiClient, wifiSSID, wifiPassword);
    Serial.println("Initializing SPIFFS...");
    if (!SPIFFS.begin())
    {
        Serial.println("Unable to activate SPIFFS");
        return;
    }
    bool isDownloaded = downloadFileToSPIFFS(wiFiClient, fileURL, fileName);
    if (isDownloaded)
    {
        disconnectWifi();
        updateFromSPIFFS(fileName);
        return;
    }
    Serial.println("Update file is not downloaded");
}