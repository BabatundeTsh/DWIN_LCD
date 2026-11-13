/*
  Backlight.ino - Change DWIN display backlight from the Arduino

  What it does:
    - Fades the backlight smoothly up and down (10% -> 100% -> 10%)
    - Optional: touch buttons at VP 0x1000 / 0x1001 set LOW / HIGH levels

  DWIN project setup (DGUS Tool):
    - Nothing required for the fade demo.
    - For the touch version, add two touch buttons:
         Button A -> Touch VP = 0x1000  (Return key value = 1)
         Button B -> Touch VP = 0x1001  (Return key value = 1)

         Authur - Babatunde
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint8_t  BACKLIGHT_MIN = 10;   // 1..64 (0 = off)
const uint8_t  BACKLIGHT_MAX = 64;   // full brightness
const uint32_t FADE_PERIOD   = 40;   // ms per brightness step

const uint16_t VP_BTN_DIM    = 0x1000;   // dim button
const uint16_t VP_BTN_FULL   = 0x1001;   // full-brightness button

// ---------------------------------------------------------------------------
// Serial selection
// ---------------------------------------------------------------------------
#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11);   // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);
#endif

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
uint8_t  level     = BACKLIGHT_MIN;
int8_t   direction = 1;              // +1 = rising, -1 = falling
bool     autoFade  = true;           // disabled by touch buttons

// ---------------------------------------------------------------------------
// Touch callback
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
    if (value == 0) return;          // ignore release events

    if (vp == VP_BTN_DIM) {
        autoFade = false;
        level = 16;
        dwin.setBacklight(level);
        Serial.println(F("Backlight: DIM"));
    }
    else if (vp == VP_BTN_FULL) {
        autoFade = false;
        level = BACKLIGHT_MAX;
        dwin.setBacklight(level);
        Serial.println(F("Backlight: FULL"));
    }
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.attachTouchCallback(onTouch);

    // Set an initial level
    dwin.setBacklight(BACKLIGHT_MAX);
    Serial.println(F("Backlight demo started"));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();   // handle touch events

    if (!autoFade) return;

    static uint32_t last = 0;
    if (millis() - last < FADE_PERIOD) return;
    last = millis();

    // Step brightness up or down
    level = (uint8_t)(level + direction);

    if (level >= BACKLIGHT_MAX) { level = BACKLIGHT_MAX; direction = -1; }
    if (level <= BACKLIGHT_MIN) { level = BACKLIGHT_MIN; direction =  1; }

    dwin.setBacklight(level);
}
