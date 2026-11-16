/*
  RTC_Time.ino - Set and read the DWIN display RTC

  What it does:
    1) Sets the panel's real-time clock to a fixed time (e.g., 2025-01-15 14:30:00)
    2) Reads the current time back from the panel
    3) Prints it to the Serial Monitor
    4) Repeats every 5 seconds

  DWIN RTC access (register method):
    - Register 0x1F = RTC_COM_ADJ  -> write 0x5A to enable serial time setting
    - Register 0x20 = RTC_NOW      -> 7 bytes: YY MM DD WW HH MM SS (BCD format)
    - Register 0x20 can also be read to get the current time

  Note: Weekday (WW): 0=Sunday, 1=Monday, ... 6=Saturday
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

// Set this to true only if you want to WRITE the time on boot.
// Leave false to just READ the time without changing it.
const bool SET_TIME_ON_BOOT = true;

// Time to set (used only when SET_TIME_ON_BOOT = true)
const uint8_t SET_YEAR   = 25;   // 2025 -> last two digits
const uint8_t SET_MONTH  = 1;    // January
const uint8_t SET_DAY    = 15;
const uint8_t SET_WEEK   = 3;    // 3 = Wednesday
const uint8_t SET_HOUR   = 14;   // 14:30:00
const uint8_t SET_MINUTE = 30;
const uint8_t SET_SECOND = 0;

// Register addresses
const uint8_t REG_RTC_ADJ = 0x1F;   // write 0x5A here to enable RTC set
const uint8_t REG_RTC_NOW = 0x20;   // 7-byte RTC data

// ---------------------------------------------------------------------------
// Serial selection
// ---------------------------------------------------------------------------
#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11); // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
// Convert a decimal value (0..99) to BCD byte
static uint8_t decToBcd(uint8_t v) {
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

// Convert a BCD byte back to decimal
static uint8_t bcdToDec(uint8_t v) {
    return (uint8_t)(((v >> 4) & 0x0F) * 10 + (v & 0x0F));
}

// ---------------------------------------------------------------------------
// Set the RTC on the display
// ---------------------------------------------------------------------------
bool setRtcTime(uint8_t year, uint8_t month, uint8_t day,
                uint8_t week, uint8_t hour, uint8_t minute, uint8_t second) {

    // Step 1: write 0x5A to register 0x1F to enable serial RTC setting
    if (!dwin.writeRegister(0x00, REG_RTC_ADJ, 0x5A)) {
        Serial.println(F("RTC: failed to unlock (0x1F)"));
        return false;
    }

    // Step 2: write 7 BCD bytes to register 0x20
    uint8_t rtc[7];
    rtc[0] = decToBcd(year);
    rtc[1] = decToBcd(month);
    rtc[2] = decToBcd(day);
    rtc[3] = decToBcd(week);
    rtc[4] = decToBcd(hour);
    rtc[5] = decToBcd(minute);
    rtc[6] = decToBcd(second);

    if (!dwin.writeRegister(0x00, REG_RTC_NOW, rtc, 7)) {
        Serial.println(F("RTC: failed to write time (0x20)"));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Read the RTC from the display
// ---------------------------------------------------------------------------
bool readRtcTime(uint8_t &year, uint8_t &month, uint8_t &day,
                 uint8_t &week, uint8_t &hour, uint8_t &minute, uint8_t &second) {

    uint8_t rtc[7] = {0};
    uint8_t n = dwin.readRegister(0x00, REG_RTC_NOW, 7, rtc);
    if (n != 7) {
        Serial.print(F("RTC: read failed, got "));
        Serial.print(n);
        Serial.println(F(" bytes"));
        return false;
    }

    year   = bcdToDec(rtc[0]);
    month  = bcdToDec(rtc[1]);
    day    = bcdToDec(rtc[2]);
    week   = bcdToDec(rtc[3]);
    hour   = bcdToDec(rtc[4]);
    minute = bcdToDec(rtc[5]);
    second = bcdToDec(rtc[6]);
    return true;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.setWaitForAck(true);     // needed for register reads

    Serial.println(F("DWIN RTC demo"));

    // Optionally set the time once at boot
    if (SET_TIME_ON_BOOT) {
        if (setRtcTime(SET_YEAR, SET_MONTH, SET_DAY,
                       SET_WEEK, SET_HOUR, SET_MINUTE, SET_SECOND)) {
            Serial.println(F("RTC set OK"));
        } else {
            Serial.println(F("RTC set FAILED"));
        }
        delay(200);
    }
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    static uint32_t last = 0;

    if (millis() - last >= 5000) {
        last = millis();

        uint8_t y, mo, d, w, h, mi, s;
        if (readRtcTime(y, mo, d, w, h, mi, s)) {
            // Weekday names
            const char *weekNames[] = {
                "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
            };
            const char *wName = (w <= 6) ? weekNames[w] : "???";

            char buf[48];
            snprintf(buf, sizeof(buf),
                     "20%02u-%02u-%02u %s %02u:%02u:%02u",
                     y, mo, d, wName, h, mi, s);
            Serial.print(F("RTC = "));
            Serial.println(buf);
        }
    }
}