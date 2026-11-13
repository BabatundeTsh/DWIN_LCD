#include <Arduino.h>
#include "DWIN_LCD.h"

DWIN_LCD dwin(Serial1);

// ---- VP map (must match your DWIN project) ----
const uint16_t VP_TEXT        = 0x2000;
const uint16_t VP_TEXT_COLOR  = 0x3000;
const uint16_t VP_TOUCH_BTN   = 0x1000;

const uint16_t VP_CURVE_A     = 0x4000;   // channel 0
const uint16_t VP_CURVE_B     = 0x4200;   // channel 1
const uint16_t VP_CURVE_C     = 0x4400;   // channel 2

// 3 channels, 100 points each
DWIN_MultiCurve plot(dwin, 3, 100);

void onTouch(uint16_t vp, uint16_t value) {
    if (vp == VP_TOUCH_BTN && value == 1) {
        plot.clearAll();
        dwin.writeStringColored(VP_TEXT, VP_TEXT_COLOR, "Cleared", DWIN_YELLOW);
    }
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);

    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.attachTouchCallback(onTouch);

    dwin.setBacklight(48);
    dwin.setPage(0);

    plot.begin();
    plot.setAutoRedraw(true);   // full-window redraw every sample

    // Register three channels
    plot.addCurve(VP_CURVE_A);   // index 0
    plot.addCurve(VP_CURVE_B);   // index 1
    plot.addCurve(VP_CURVE_C);   // index 2

    // Optional: per-channel scaling. Example: signal 0..3.3V mapped to 0..1000
    plot.curve(0)->setScale(1000.0f / 3.3f, 0.0f);
    plot.curve(1)->setScale(1000.0f / 3.3f, 0.0f);
    plot.curve(2)->setScale(1000.0f / 3.3f, 0.0f);

    dwin.writeStringColored(VP_TEXT, VP_TEXT_COLOR, "3-Channel Plot", DWIN_GREEN);
}

void loop() {
    dwin.process();   // touch events

    static uint32_t last = 0;
    if (millis() - last >= 100) {
        last = millis();

        // Physical signals (0..3.3 V)
        float raw[3];
        raw[0] = analogRead(A0) * (3.3f / 1023.0f);
        raw[1] = analogRead(A1) * (3.3f / 1023.0f);
        raw[2] = analogRead(A2) * (3.3f / 1023.0f);

        // Push one sample to all three curves. Scaling is applied per channel.
        plot.pushSampleScaled(raw);

        // Update a live text label
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f %.2f %.2f", raw[0], raw[1], raw[2]);
        dwin.writeString(VP_TEXT, buf);
    }
}