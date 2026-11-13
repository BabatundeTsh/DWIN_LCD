/*
  IconAnimation.ino - Animate icons on a DWIN display

  Demonstrates:
    1) Sprite animation      - cycled frames (e.g. spinning fan)
    2) Blinking icon         - on/off at a fixed rate
    3) Marquee / scanner     - a lit segment walks across N icons
    4) Progress fill         - gauge fills 0..N then resets
    5) Position move (opt.)  - slides an icon if its control exposes X/Y VPs

  DWIN project setup (DGUS Tool):
    Add these "Icon Display" controls on page 0:

      Sprite fan     VP = 0x2000   icons: 8 frames (fan_0 .. fan_7)
      Blinker        VP = 0x2001   icons: 2 (blink_off, blink_on)
      Marquee A..E   VP = 0x2010 .. 0x2014   icons: 2 each (off, on)
      Progress       VP = 0x2020   icons: 11 (prog_0 .. prog_10)
      Moving icon    VP = 0x2030   icons: 1
                     X VP = 0x2031   Y VP = 0x2032  (optional)

  Every animation runs on its own timer so they all appear independent.

  
  Authur - Babatunde
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

// ---------------------------------------------------------------------------
// VP map
// ---------------------------------------------------------------------------
const uint16_t VP_SPRITE      = 0x2000;   // 8 frames -> spinning fan
const uint16_t VP_BLINK       = 0x2001;   // 2 frames -> blinking
const uint16_t VP_MARQUEE     = 0x2010;   // 5 segments (0x2010..0x2014)
const uint8_t  MARQUEE_LEN    = 5;
const uint16_t VP_PROGRESS    = 0x2020;   // 11 frames -> progress bar
const uint8_t  PROGRESS_MAX   = 10;

const uint16_t VP_MOVE_ICON   = 0x2030;   // 1 frame
const uint16_t VP_MOVE_X      = 0x2031;   // optional X position VP
const uint16_t VP_MOVE_Y      = 0x2032;   // optional Y position VP

// Optional touch button to pause/resume all animation
const uint16_t VP_BTN_PAUSE   = 0x1000;

// ---------------------------------------------------------------------------
// Serial selection
// ---------------------------------------------------------------------------
#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11);  // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);
#endif

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
bool paused = false;

// ---------------------------------------------------------------------------
// Touch callback - pause/resume
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
    if (vp == VP_BTN_PAUSE && value == 1) {
        paused = !paused;
        Serial.println(paused ? F("Animation PAUSED") : F("Animation RUNNING"));
    }
}

// ===========================================================================
// Animation 1: sprite / frame sequence
// ===========================================================================
void animateSprite() {
    static uint8_t frame = 0;
    static const uint8_t FRAMES = 8;

    dwin.writeVP(VP_SPRITE, (uint16_t)frame);
    frame = (uint8_t)((frame + 1) % FRAMES);
}

// ===========================================================================
// Animation 2: blinking icon
// ===========================================================================
void animateBlink() {
    static bool on = false;
    on = !on;
    dwin.writeVP(VP_BLINK, (uint16_t)(on ? 1 : 0));
}

// ===========================================================================
// Animation 3: marquee / scanner bar
// ===========================================================================
void animateMarquee() {
    static uint8_t pos = 0;

    // Turn all segments off
    for (uint8_t i = 0; i < MARQUEE_LEN; i++) {
        dwin.writeVP((uint16_t)(VP_MARQUEE + i), 0);
    }
    // Turn the current one on
    dwin.writeVP((uint16_t)(VP_MARQUEE + pos), 1);

    pos = (uint8_t)((pos + 1) % MARQUEE_LEN);
}

// ===========================================================================
// Animation 4: progress fill 0..N
// ===========================================================================
void animateProgress() {
    static uint8_t level = 0;

    dwin.writeVP(VP_PROGRESS, (uint16_t)level);

    if (level >= PROGRESS_MAX) level = 0;
    else                       level++;
}

// ===========================================================================
// Animation 5: moving icon via X/Y VPs (optional)
// ===========================================================================
void animateMove() {
    static uint16_t x = 50;
    static int8_t   dir = 1;
    static const uint16_t X_MIN = 50;
    static const uint16_t X_MAX = 750;

    dwin.writeVP(VP_MOVE_X, x);

    // Also toggle the icon itself between 0/1 if the moving control is
    // a single-frame icon - keeps it visible if the panel ignores X/Y.
    static bool flip = false;
    flip = !flip;
    dwin.writeVP(VP_MOVE_ICON, (uint16_t)(flip ? 1 : 0));

    if (dir > 0) {
        x += 25;
        if (x >= X_MAX) { x = X_MAX; dir = -1; }
    } else {
        x -= 25;
        if (x <= X_MIN) { x = X_MIN; dir = 1; }
    }
}

// ===========================================================================
// setup()
// ===========================================================================
void setup() {
    Serial.begin(115200);
    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.setWaitForAck(false);   // animations are write-only; skip ACK for speed
    dwin.attachTouchCallback(onTouch);

    dwin.setBacklight(48);
    dwin.setPage(0);

    // Reset all animated icons to a known state
    dwin.writeVP(VP_SPRITE, 0);
    dwin.writeVP(VP_BLINK, 0);
    for (uint8_t i = 0; i < MARQUEE_LEN; i++)
        dwin.writeVP((uint16_t)(VP_MARQUEE + i), 0);
    dwin.writeVP(VP_PROGRESS, 0);
    dwin.writeVP(VP_MOVE_X, 50);

    Serial.println(F("Icon animation demo started"));
}

// ===========================================================================
// loop()
// ===========================================================================
void loop() {
    dwin.process();   // touch handling always runs

    if (paused) return;

    uint32_t now = millis();

    // ---- Sprite: fast (every 80 ms) ----
    static uint32_t tSprite = 0;
    if (now - tSprite >= 80) {
        tSprite = now;
        animateSprite();
    }

    // ---- Blink: slow (every 500 ms) ----
    static uint32_t tBlink = 0;
    if (now - tBlink >= 500) {
        tBlink = now;
        animateBlink();
    }

    // ---- Marquee: medium (every 150 ms) ----
    static uint32_t tMarquee = 0;
    if (now - tMarquee >= 150) {
        tMarquee = now;
        animateMarquee();
    }

    // ---- Progress: slow (every 250 ms) ----
    static uint32_t tProgress = 0;
    if (now - tProgress >= 250) {
        tProgress = now;
        animateProgress();
    }

    // ---- Move: smooth (every 40 ms) ----
    static uint32_t tMove = 0;
    if (now - tMove >= 40) {
        tMove = now;
        animateMove();
    }
}
