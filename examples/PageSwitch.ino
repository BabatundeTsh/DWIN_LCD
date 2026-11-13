/*
  PageSwitch.ino - Switch DWIN display page from the Arduino

  What it does:
    1) Automatically cycles through pages 0 -> 1 -> 2 -> 0 every 3 seconds.
    2) A touch button on the panel (VP 0x1000) advances to the next page
       immediately when pressed.

  DWIN project setup (DGUS Tool):
    - Create pages 0, 1 and 2 (Page IDs must be 0, 1, 2)
    - On any page, add a touch button with:
         Touch VP = 0x1000
         Action   = Return key value (sends 1 on press)
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0     // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint8_t  TOTAL_PAGES   = 3;      // how many pages your project has
const uint32_t SWITCH_PERIOD = 3000;   // auto-switch every 3 seconds

const uint16_t VP_TOUCH_BTN  = 0x1000; // touch button that advances the page

// ---------------------------------------------------------------------------
// Serial selection
// ---------------------------------------------------------------------------
#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11);    // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);
#endif

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
uint8_t  currentPage  = 0;
uint32_t lastSwitch   = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void gotoPage(uint8_t page) {
    currentPage = page % TOTAL_PAGES;
    dwin.setPage(currentPage);

    Serial.print(F("Switched to page "));
    Serial.println(currentPage);
}

void nextPage() {
    gotoPage((uint8_t)((currentPage + 1) % TOTAL_PAGES));
}

// ---------------------------------------------------------------------------
// Touch callback
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
    if (vp == VP_TOUCH_BTN && value == 1) {
        nextPage();
        lastSwitch = millis();   // reset the auto-switch timer
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

    dwin.setBacklight(48);

    // Show page 0 at boot
    gotoPage(0);
    lastSwitch = millis();
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();   // handle touch events

    // Auto-advance the page every SWITCH_PERIOD ms
    if (millis() - lastSwitch >= SWITCH_PERIOD) {
        lastSwitch = millis();
        nextPage();
    }
}