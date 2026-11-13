/*
  SendText.ino - Send text strings to a DWIN display

  What it does:
    - Writes a static "Hello DWIN!" string at boot
    - Then updates the text once per second with a live counter

  DWIN project setup (DGUS Tool):
    - Add a Text Display control on page 0
    - Set its "Text VP" to 0x2000
    - Word length: at least 16 words (32 bytes) to fit our strings

    
  Authur - Babatunde
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint16_t VP_TEXT = 0x2000;     // must match the DWIN project

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
    dwin.setCRC(false);              // enable if your panel uses CRC
    dwin.attachTouchCallback(nullptr);// no touch handling in this sketch

    dwin.setBacklight(48);
    dwin.setPage(0);

    // Send a static string once
    dwin.writeString(VP_TEXT, "Hello DWIN!");
    Serial.println(F("Sent: Hello DWIN!"));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    static uint32_t last = 0;
    static uint32_t counter = 0;

    if (millis() - last >= 1000) {
        last = millis();

        char buf[32];
        snprintf(buf, sizeof(buf), "Count: %lu", (unsigned long)counter++);

        dwin.writeString(VP_TEXT, buf);
        Serial.print(F("Sent: "));
        Serial.println(buf);
    }
}
