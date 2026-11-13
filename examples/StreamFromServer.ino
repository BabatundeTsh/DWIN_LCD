/*
  StreamFromServer.ino - Fetch an image from a PHP server and stream it
                         to a DWIN display over serial

  Flow:
    ESP32 --HTTP--> PHP server (raw RGB565)
         --UART--> DWIN display (VP block)
        (Wi-Fi)         (Serial2)

  Hardware:
    - ESP32 dev board
    - DWIN display UART2 connected to ESP32 pins 16 (RX) / 17 (TX)
    - Common GND

  DWIN project setup (DGUS Tool):
    Add a "Picture" / "Image Display" control on page 0.
      Data source VP = 0x4000
      Width          = 64 pixels (match your PHP server's w)
      Height         = 64 pixels (match your PHP server's h)
      Format         = RGB565 (16-bit)
*/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
const char *WIFI_SSID     = "YourWiFiName";
const char *WIFI_PASSWORD = "YourWiFiPassword";

// URL of the PHP script (with optional size parameters)
const char *IMAGE_URL =
    "http://yourserver/image_server.php?w=64&h=64&f=logo.png";

// Image geometry (must match the DGUS Picture control)
const uint16_t IMG_W   = 64;
const uint16_t IMG_H   = 64;
const uint16_t VP_IMG  = 0x4000;

// DWIN UART on ESP32 (Serial2 uses pins 16/17 by default)
#define DWIN_RX_PIN 16
#define DWIN_TX_PIN 17

const uint32_t DWIN_BAUD    = 115200;
const uint32_t REFRESH_MS   = 10000;   // refetch every 10 s

// ---------------------------------------------------------------------------
// DWIN object on Serial2
// ---------------------------------------------------------------------------
DWIN_LCD dwin(Serial2);

// ---------------------------------------------------------------------------
// Wi-Fi connection
// ---------------------------------------------------------------------------
bool connectWifi(uint32_t timeoutMs = 20000) {
    Serial.print(F("Connecting to Wi-Fi"));
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print(F("Wi-Fi OK. IP = "));
        Serial.println(WiFi.localIP());
        return true;
    }
    Serial.println(F("Wi-Fi failed"));
    return false;
}

// ---------------------------------------------------------------------------
// Streaming core: read one row from the HTTP stream, byte-swap, send to DWIN
// ---------------------------------------------------------------------------
bool streamFromServer(const char *url) {
    if (WiFi.status() != WL_CONNECTED) {
        if (!connectWifi()) return false;
    }

    HTTPClient http;
    http.setTimeout(10000);
    http.useHTTP10(true);              // required for streamed reads
    http.begin(url);

    Serial.print(F("GET "));
    Serial.println(url);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.print(F("HTTP error "));
        Serial.println(code);
        http.end();
        return false;
    }

    int contentLen = http.getSize();
    int expected   = IMG_W * IMG_H * 2;
    Serial.print(F("Content-Length: "));
    Serial.print(contentLen);
    Serial.print(F("  (expected "));
    Serial.print(expected);
    Serial.println(')');

    if (contentLen > 0 && contentLen != expected) {
        Serial.println(F("Size mismatch - aborting"));
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    if (!stream) {
        Serial.println(F("No HTTP stream"));
        http.end();
        return false;
    }

    // Allocate the row buffer once
    uint16_t *row = (uint16_t *)malloc(IMG_W * sizeof(uint16_t));
    if (!row) {
        Serial.println(F("Out of memory"));
        http.end();
        return false;
    }

    uint32_t t0 = millis();
    Serial.println(F("Streaming..."));

    for (uint16_t y = 0; y < IMG_H; y++) {
        // Read one full row from the HTTP stream, waiting up to 2 s
        uint32_t rowStart = millis();
        while (stream->available() < (int)(IMG_W * 2)) {
            if (millis() - rowStart > 2000) {
                Serial.print(F("Timeout at row ")); Serial.println(y);
                free(row);
                http.end();
                return false;
            }
            delay(1);
        }

        stream->readBytes((uint8_t *)row, IMG_W * 2);

        // Server sent little-endian RGB565; DWIN expects big-endian words.
        for (uint16_t x = 0; x < IMG_W; x++) {
            uint16_t v = row[x];
            row[x] = (uint16_t)((v >> 8) | (v << 8));
        }

        // Push this row to the panel
        if (!dwin.writeVP((uint16_t)(VP_IMG + y * IMG_W), row, IMG_W)) {
            Serial.print(F("DWIN write failed at row ")); Serial.println(y);
            free(row);
            http.end();
            return false;
        }

        // Progress
        if ((y & 7) == 7) {
            Serial.print('.');
        }
    }

    free(row);
    http.end();

    Serial.println();
    Serial.print(F("Done in "));
    Serial.print(millis() - t0);
    Serial.println(F(" ms"));
    return true;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);

    // ---- DWIN UART ----
    Serial2.begin(DWIN_BAUD, SERIAL_8N1, DWIN_RX_PIN, DWIN_TX_PIN);
    dwin.begin(DWIN_BAUD);
    dwin.setCRC(false);
    dwin.setWaitForAck(false);        // essential for streaming speed

    dwin.setBacklight(48);
    dwin.setPage(0);

    // ---- Wi-Fi ----
    connectWifi();

    // ---- First fetch ----
    delay(500);
    streamFromServer(IMAGE_URL);
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();                    // touch events still work

    // Periodic refresh
    static uint32_t last = 0;
    if (millis() - last >= REFRESH_MS) {
        last = millis();
        streamFromServer(IMAGE_URL);
    }

    // Manual refresh on serial 'r'
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'r' || c == 'R') {
            Serial.println(F("Manual refresh"));
            streamFromServer(IMAGE_URL);
        }
    }
}