/*
  SinglePlot.ino - DWIN LCD single-curve live plot demo

  Hardware:
    - Arduino MEGA / ESP32 (uses Serial1)  --or--  UNO with SoftwareSerial
    - DWIN DGUS/T5L display, TX -> Arduino RX, RX -> Arduino TX, GND common

  DWIN project setup (DGUS Tool):
    - Page 0
    - Curve control  #1:  Data source VP = 0x4000
                          Point count  = 100
                          Mode         = Real-time (auto scroll)
                          Y range      = configured in DGUS Tool
    - Text control   :    Text VP  = 0x2000
                          Color VP = 0x3000
    - Touch button   :    VP       = 0x1000  (returns 1 on press)

    
     Authur - Babatunde
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Serial selection
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0   // set to 1 for UNO/Nano

#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11);   // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);              // MEGA / ESP32 / STM32
#endif

// ---------------------------------------------------------------------------
// VP map  (must match your DWIN project)
// ---------------------------------------------------------------------------
const uint16_t VP_TEXT        = 0x2000;   // text content
const uint16_t VP_TEXT_COLOR  = 0x3000;   // text color (RGB565)
const uint16_t VP_TOUCH_BTN   = 0x1000;   // clear button
const uint16_t VP_CURVE       = 0x4000;   // curve data source
const uint16_t VP_CURVE_MIN   = 0x4100;   // optional Y-min VP (may not exist)
const uint16_t VP_CURVE_MAX   = 0x4101;   // optional Y-max VP (may not exist)

// ---------------------------------------------------------------------------
// Curve configuration
// ---------------------------------------------------------------------------
const uint16_t CURVE_POINTS   = 100;      // window length shown on display
const bool     REAL_TIME_MODE = true;     // true  -> only new points sent
                                          // false -> whole window redrawn
                                          //          each sample (static/buffer
                                          //          DGUS curve mode)

DWIN_Curve plot(dwin, VP_CURVE, CURVE_POINTS);

// ---------------------------------------------------------------------------
// Touch callback
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
    Serial.print(F("Touch VP=0x"));
    Serial.print(vp, HEX);
    Serial.print(F("  value="));
    Serial.println(value);

    if (vp == VP_TOUCH_BTN && value == 1) {
        plot.clear();
        dwin.writeStringColored(VP_TEXT, VP_TEXT_COLOR, "Plot cleared", DWIN_YELLOW);
    }
}

// ---------------------------------------------------------------------------
// Simulated sensor (replace with real reading)
// ---------------------------------------------------------------------------
float readSensor() {
    // Smooth sine wave with a little noise, range ~ 10..90
    static float phase = 0.0f;
    phase += 0.10f;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
    float noise = (random(-20, 20)) / 100.0f;
    return 50.0f + 40.0f * sinf(phase) + noise;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    dwin.begin(9600);          // store baud (sketch already called .begin)
    dwin.setCRC(false);        // enable if your panel requires CRC
    dwin.setWaitForAck(true);  // set false if panel doesn't reply -> faster writes
    dwin.attachTouchCallback(onTouch);

    // Backlight + initial page
    dwin.setBacklight(48);
    dwin.setPage(0);

    // Optional: set Y-axis range if the DGUS curve has min/max VPs.
    // Comment this out if your project doesn't define them.
    dwin.curveSetRange(VP_CURVE_MIN, VP_CURVE_MAX, 0, 100);

    // Initialise the curve helper
    plot.begin();
    plot.set8Bit(false);            // 16-bit curve (use true for 0..255)
    plot.setAutoRedraw(!REAL_TIME_MODE);

    // Optional: scaling from engineering units -> curve counts.
    // Here the sensor is already 0..100, so gain=1, offset=0.
    plot.setScale(1.0f, 0.0f);

    // Friendly banner
    dwin.writeStringColored(VP_TEXT, VP_TEXT_COLOR, "Single Plot Demo", DWIN_GREEN);

    randomSeed(analogRead(0));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();   // non-blocking: handle touch events

    static uint32_t last = 0;
    const uint32_t interval = 100;    // 10 samples/second

    if (millis() - last >= interval) {
        last = millis();

        // 1) Read the sensor
        float value = readSensor();

        // 2) Push it to the curve (scaling is applied inside pushScaled)
        plot.pushScaled(value);

        // 3) Update the live text label
        char buf[24];
        snprintf(buf, sizeof(buf), "%.2f", value);
        dwin.writeString(VP_TEXT, buf);

        // 4) Color-code the label based on threshold
        if (value > 80.0f) {
            dwin.setTextColor(VP_TEXT_COLOR, DWIN_RED);
        } else if (value < 20.0f) {
            dwin.setTextColor(VP_TEXT_COLOR, DWIN_CYAN);
        } else {
            dwin.setTextColor(VP_TEXT_COLOR, DWIN_GREEN);
        }
    }
}
