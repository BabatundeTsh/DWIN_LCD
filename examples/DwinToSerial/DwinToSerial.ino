/*
  #############################
  Authur - Babatunde .Tsh
  #############################
  DwinToSerial.ino - Print everything the DWIN panel sends to Serial Monitor

  What it does:
    - Listens to the DWIN display over Serial1 (or SoftwareSerial)
    - Prints any touch/VP event (VP address + value) to the Serial Monitor

  DWIN project setup (DGUS Tool):
    - Add any touch button / touch control to a page
    - Set its "Touch VP" to any address, e.g. 0x1000
    - Configure it as "Return key value" so it sends 1 (and 0 on release)
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
DWIN_LCD dwin(Serial1);  // MEGA / ESP32 / STM32

// ---------------------------------------------------------------------------
// Touch callback - called every time the panel sends an event
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
  Serial.print(F("VP = 0x"));
  Serial.print(vp, HEX);
  Serial.print(F("   value = "));
  Serial.println(value);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);  // USB serial -> Serial Monitor
  dwin.begin(115200);
  dwin.setCRC(false);
  dwin.attachTouchCallback(onTouch);

  Serial.println(F("Listening for DWIN events..."));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
  // Non-blocking: reads bytes from the display and fires onTouch()
  dwin.process();
}