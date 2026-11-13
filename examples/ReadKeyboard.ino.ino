/*
  ReadKeyboard.ino - Read text typed on the DWIN on-screen keyboard
                     and print it to the Serial Monitor

  What it does:
    - Reads a Text Input VP every 300 ms
    - If the string changed, prints it to the Serial Monitor
    - Also uses the touch callback for immediate updates when the
      panel pushes the "Enter" key event.

  DWIN project setup (DGUS Tool):
    - Add a "Text Input" control on page 0
        Input VP    = 0x2000
        Word length = 32 (64 bytes = up to 32 ASCII characters)
        Keyboard    = any (English / numeric / full)
    - If you want immediate push on Enter:
        mark the control's VP as "Upload" and add a "Return key code"
        to a nearby touch-VP (e.g. 0x1000) so the callback fires.

  Note: The user must tap the input field on the panel, type, and
  press Enter on the on-screen keyboard for the text to be stored.
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint16_t VP_KEYBOARD_IN = 0x2000;   // Text Input VP in the DWIN project
const uint16_t VP_ENTER_EVT   = 0x1000;   // optional touch event on Enter
const uint8_t  MAX_CHARS      = 32;       // must match the control's word length

const uint32_t POLL_INTERVAL  = 300;      // ms between polls

// ---------------------------------------------------------------------------
// Serial selection
// ---------------------------------------------------------------------------
#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11);      // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);
#endif

// ---------------------------------------------------------------------------
// Last-read state (to detect changes)
// ---------------------------------------------------------------------------
char lastString[MAX_CHARS + 1] = {0};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Read `MAX_CHARS` words from the input VP, convert to a null-terminated
// ASCII string. Returns true if the string actually changed.
bool readKeyboard(char *out, uint8_t maxLen) {
    uint16_t words[MAX_CHARS];
    memset(words, 0, sizeof(words));

    uint8_t n = dwin.readVP(VP_KEYBOARD_IN, maxLen, words);
    if (n == 0) return false;               // no response

    // Pack low byte of each word into a char; stop at 0x0000 terminator
    uint8_t i = 0;
    for (; i < n && i < maxLen; i++) {
        uint8_t lo = (uint8_t)(words[i] & 0xFF);
        if (lo == 0x00) break;
        out[i] = (char)lo;
    }
    out[i] = '\0';

    // Compare with the previous string
    if (strcmp(out, lastString) == 0) return false;

    strncpy(lastString, out, maxLen);
    lastString[maxLen] = '\0';
    return true;
}

// Optional: read the string via a single "0x83" auto-upload event.
// We only use this when the panel pushes the whole keyboard buffer at once,
// which most panels do NOT do — the callback is generally triggered by
// the Enter key event, and we then fall back to readKeyboard().

// ---------------------------------------------------------------------------
// Touch callback - fires when the panel reports a touch/auto-upload event
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
    // If your project pushes the Enter button on VP_ENTER_EVT,
    // immediately fetch the full string.
    if (vp == VP_ENTER_EVT && value == 1) {
        char buf[MAX_CHARS + 1];
        if (readKeyboard(buf, MAX_CHARS)) {
            Serial.print(F("Enter pressed -> "));
            Serial.println(buf);
        } else {
            Serial.println(F("Enter pressed (no change)"));
        }
    }
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.setWaitForAck(true);            // needed for readVP()
    dwin.attachTouchCallback(onTouch);

    dwin.setBacklight(48);
    dwin.setPage(0);

    Serial.println(F("Tap the text input on the panel and type..."));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();                       // touch events

    static uint32_t last = 0;
    if (millis() - last >= POLL_INTERVAL) {
        last = millis();

        char buf[MAX_CHARS + 1];
        if (readKeyboard(buf, MAX_CHARS)) {
            Serial.print(F("Keyboard: \""));
            Serial.print(buf);
            Serial.println('"');
        }
    }
}