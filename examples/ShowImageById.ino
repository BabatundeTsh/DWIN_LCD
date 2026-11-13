/*
  ShowImageById.ino - Display an image stored on the DWIN panel by its ID

  What it does:
    - Shows an image at boot
    - Advances to the next image every 2 seconds
    - A touch button (VP 0x1000) also advances the image manually

  How it works:
    The panel keeps its images in an ICL file inside its flash.
    Each image has an index ID assigned in DGUS Tool.
    Command 0x70 tells the panel "show the image with this ID".
    Frame format (different from the VP protocol!):
        AA 70 <ID_H> <ID_L> CC 33 C3 3C

  DWIN project setup (DGUS Tool):
    - Load the images you want into the ICL file / background image list
    - Note their IDs (shown next to each image in DGUS Tool)
    - Optionally add a touch button with VP = 0x1000
      and action "Return key value" (sends 1 on press)

      
      Authur - Babatunde
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

// Image IDs to cycle through (must match IDs in your DWIN project)
const uint16_t IMAGE_IDS[] = { 0, 1, 2, 3 };
const uint8_t  IMAGE_COUNT = sizeof(IMAGE_IDS) / sizeof(IMAGE_IDS[0]);

// Optional touch button to advance
const uint16_t VP_BTN_NEXT = 0x1000;

// Optional text label showing the current image ID
const uint16_t VP_TEXT     = 0x2000;
const uint16_t VP_TEXT_CLR = 0x3000;

const uint32_t AUTO_ADVANCE_MS = 2000;

// ---------------------------------------------------------------------------
// Serial selection
// ---------------------------------------------------------------------------
#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11);     // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);
#endif

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
uint8_t currentIndex = 0;

// ---------------------------------------------------------------------------
// Core: show an image by its ID
// ---------------------------------------------------------------------------
void showImageById(uint16_t picId) {
    // Send the raw 0x70 frame directly (not the 0x5A5A VP protocol)
    Serial1.write(0xAA);
    Serial1.write(0x70);
    Serial1.write((uint8_t)(picId >> 8));
    Serial1.write((uint8_t)(picId & 0xFF));
    Serial1.write(0xCC);
    Serial1.write(0x33);
    Serial1.write(0xC3);
    Serial1.write(0x3C);

    Serial.print(F("Show image ID "));
    Serial.println(picId);

    // Optional: update a text label with the ID
    char buf[16];
    snprintf(buf, sizeof(buf), "ID: %u", picId);
    dwin.writeStringColored(VP_TEXT, VP_TEXT_CLR, buf, DWIN_WHITE);
}

// ---------------------------------------------------------------------------
// Advance to the next image
// ---------------------------------------------------------------------------
void nextImage() {
    currentIndex = (uint8_t)((currentIndex + 1) % IMAGE_COUNT);
    showImageById(IMAGE_IDS[currentIndex]);
}

// ---------------------------------------------------------------------------
// Touch callback
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
    if (vp == VP_BTN_NEXT && value == 1) {
        nextImage();
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
    dwin.setPage(0);                // make sure page 0 is active

    delay(500);                     // let the panel finish its boot

    // Show the first image
    showImageById(IMAGE_IDS[0]);
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();                 // handle touch events

    // Auto-advance every AUTO_ADVANCE_MS
    static uint32_t last = 0;
    if (millis() - last >= AUTO_ADVANCE_MS) {
        last = millis();
        nextImage();
    }
}
