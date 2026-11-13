/*
  DwinToSerial.ino - Print everything the DWIN panel sends to Serial Monitor

  What it does:
    - Listens to the DWIN display over Serial1 (or SoftwareSerial)
    - Prints any touch/VP event (VP address + value) to the Serial Monitor

  DWIN project setup (DGUS Tool):
    - Add any touch button / touch control to a page
    - Set its "Touch VP" to any address, e.g. 0x1000
    - Configure it as "Return key value" so it sends 1 (and 0 on release)

    
  Authur - Babatunde
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11); // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);            // MEGA / ESP32 / STM32
#endif

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
    Serial.begin(115200);            // USB serial -> Serial Monitor

    #if USE_SOFTWARE_SERIAL
        dwinSerial.begin(9600);
    #else
        Serial1.begin(9600);         // must match the panel's baud rate
    #endif

    dwin.begin(9600);
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
