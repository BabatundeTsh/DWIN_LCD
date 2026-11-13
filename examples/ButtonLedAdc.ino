/*
  ButtonLedAdc.ino - DWIN LCD button + ADC demo (no curves)

  What it does:
    1) Receives touch-button events from the DWIN panel.
       Button value 1 -> LED ON,  value 0 -> LED OFF.
    2) Reads an analog pin every 250 ms and displays
       the raw ADC value + voltage on a DWIN text control.

  Hardware:
    - Arduino MEGA / ESP32 (Serial1)   --or--   UNO with SoftwareSerial
    - DWIN DGUS/T5L display
    - LED on pin LED_PIN (through a resistor)
    - Analog sensor on ADC_PIN (e.g. potentiometer wiper)

  DWIN project setup (DGUS Tool):
    - Page 0
    - Touch button  : VP = 0x1000   -> action "Return key value" (0 or 1)
    - Text control  : Text VP  = 0x2000
                      Color VP = 0x3000   (optional, for colored text)
                      
  Authur - Babatunde
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// User configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint8_t  LED_PIN       = 13;   // built-in LED on most boards
const uint8_t  ADC_PIN       = A0;   // analog input
const uint32_t ADC_INTERVAL  = 250;  // ms between ADC reads (4 Hz)
const float    VREF          = 5.0f; // 5.0 on UNO/MEGA, 3.3 on ESP32
const uint16_t ADC_MAX       = 1023; // 1023 on UNO/MEGA, 4095 on ESP32

// ---------------------------------------------------------------------------
// VP map (must match your DWIN project)
// ---------------------------------------------------------------------------
const uint16_t VP_TOUCH_BTN  = 0x1000;   // button returns 0 or 1
const uint16_t VP_TEXT       = 0x2000;   // text content
const uint16_t VP_TEXT_COLOR = 0x3000;   // text color (RGB565)

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
bool     ledState    = false;
uint32_t lastAdcRead = 0;

// ---------------------------------------------------------------------------
// Touch callback - fires whenever the panel reports a touch
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
    Serial.print(F("Touch  VP=0x"));
    Serial.print(vp, HEX);
    Serial.print(F("  value="));
    Serial.println(value);

    if (vp == VP_TOUCH_BTN) {
        // Button sends 1 for ON, 0 for OFF
        ledState = (value != 0);
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);

        // Acknowledge on the display
        dwin.writeStringColored(VP_TEXT, VP_TEXT_COLOR,
                                ledState ? "LED ON" : "LED OFF",
                                ledState ? DWIN_GREEN : DWIN_RED);
    }
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    dwin.begin(9600);
    dwin.setCRC(false);           // enable if your panel uses CRC
    dwin.setWaitForAck(true);     // set false if panel doesn't ACK
    dwin.attachTouchCallback(onTouch);

    // Panel setup
    dwin.setBacklight(48);
    dwin.setPage(0);

    // Banner
    dwin.writeStringColored(VP_TEXT, VP_TEXT_COLOR, "Ready", DWIN_WHITE);
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    // 1) Keep the RX parser running so touch events reach onTouch()
    dwin.process();

    // 2) Read ADC every ADC_INTERVAL ms and update the display
    uint32_t now = millis();
    if (now - lastAdcRead >= ADC_INTERVAL) {
        lastAdcRead = now;

        uint16_t adc     = analogRead(ADC_PIN);
        float    voltage = (adc * VREF) / (float)ADC_MAX;

        // Format:  "ADC: 512   V: 2.50"
        char buf[32];
        snprintf(buf, sizeof(buf), "ADC: %u   V: %.2f", (unsigned)adc, voltage);

        dwin.writeString(VP_TEXT, buf);

        // Color-code the value: low = cyan, mid = green, high = red
        if (adc < (ADC_MAX / 3)) {
            dwin.setTextColor(VP_TEXT_COLOR, DWIN_CYAN);
        } else if (adc < (2 * ADC_MAX / 3)) {
            dwin.setTextColor(VP_TEXT_COLOR, DWIN_GREEN);
        } else {
            dwin.setTextColor(VP_TEXT_COLOR, DWIN_RED);
        }

        // Mirror on the Arduino serial monitor
        Serial.println(buf);
    }
}
