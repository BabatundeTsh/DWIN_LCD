/*
  RegisterRW.ino - Write and read DWIN control registers from Arduino

  What it does:
    1) Writes a test value to a DWIN control register
    2) Reads it back
    3) Prints the written/read values to the Serial Monitor
    4) Repeats every 2 seconds

  DWIN registers (page 0x00 = system registers):
    Reg 0x01  = backlight (0..64)
    Reg 0x02  = buzzer   (0 = off, 1..255 = duration)
    Reg 0x03  = touch beep on/off
    Reg 0x04  = RTC / misc (panel-dependent)

  Control registers are 1-byte values accessed via command 0x80/0x81.
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint8_t  REG_PAGE = 0x00;      // register page (0x00 = system)
const uint8_t  REG_ADDR = 0x01;      // register address (0x01 = backlight)

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
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.setWaitForAck(true);        // keep true so writes are confirmed

    Serial.println(F("DWIN register read/write demo"));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    static uint32_t last = 0;
    static uint8_t  writeVal = 16;   // start dim, then alternate

    if (millis() - last >= 2000) {
        last = millis();

        // ---------- WRITE ----------
        bool okW = dwin.writeRegister(REG_PAGE, REG_ADDR, writeVal);
        Serial.print(F("WRITE reg[0x"));
        Serial.print(REG_ADDR, HEX);
        Serial.print(F("] = "));
        Serial.print(writeVal);
        Serial.print(okW ? F("   OK") : F("   FAILED"));
        Serial.println();

        // ---------- READ BACK ----------
        uint8_t readVal = 0;
        uint8_t n = dwin.readRegister(REG_PAGE, REG_ADDR, 1, &readVal);

        if (n == 1) {
            Serial.print(F("READ  reg[0x"));
            Serial.print(REG_ADDR, HEX);
            Serial.print(F("] = "));
            Serial.print(readVal);
            Serial.println(readVal == writeVal ? F("   MATCH") : F("   MISMATCH"));
        } else {
            Serial.println(F("READ  failed (no response)"));
        }

        Serial.println(F("---"));

        // Alternate between 16 and 64 for the next iteration
        writeVal = (writeVal == 16) ? 64 : 16;
    }
}