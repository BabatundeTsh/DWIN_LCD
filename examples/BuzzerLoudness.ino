/*
  BuzzerLoudness.ino - Adjust the DWIN display buzzer loudness

  What it does:
    1) Unlocks the buzzer configuration registers
    2) Sets the buzzer PWM duty cycle (loudness) to a chosen value
    3) Beeps at three different loudness levels so you can hear the difference
    4) Then ramps the loudness up and down continuously

  DWIN register map used (page 0x00):
    0x27  BUZZ_Set_En      -> write 0x5A to enable buzzer configuration
    0x2B  BUZZ_Freq_Duty   -> 16-bit PWM duty ratio (higher = louder)
    0x2D  BUZZ_Time        -> beep duration after touch, unit 10 ms

  Factory default duty is 0x00F0 (~8% high level)[reference:2].
  Typical usable range: 0x0010 (very quiet) .. 0x0200 (max loudness).
  Values above ~0x0400 may clip or distort.

  
  Authur - Babatunde
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint8_t  REG_BUZZ_EN   = 0x27;   // BUZZ_Set_En
const uint8_t  REG_BUZZ_DUTY = 0x2B;   // BUZZ_Freq_Duty (2 bytes)
const uint8_t  REG_BUZZ_TIME = 0x2D;   // BUZZ_Time (1 byte)

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
// Helper: set the buzzer PWM duty cycle (loudness)
//   duty: 0x0000 = silent ... 0xFFFF = full duty
//   A good starting range is 0x0010 .. 0x0200.
// ---------------------------------------------------------------------------
bool setBuzzerLoudness(uint16_t duty) {
    // Unlock buzzer configuration (write 0x5A to register 0x27)
    if (!dwin.writeRegister(0x00, REG_BUZZ_EN, 0x5A)) {
        Serial.println(F("Buzzer unlock failed"));
        return false;
    }

    // Write the 16-bit duty ratio to register 0x2B (big-endian: high byte first)
    uint8_t data[2];
    data[0] = (uint8_t)(duty >> 8);
    data[1] = (uint8_t)(duty & 0xFF);

    if (!dwin.writeRegister(0x00, REG_BUZZ_DUTY, data, 2)) {
        Serial.println(F("Buzzer duty write failed"));
        return false;
    }

    Serial.print(F("Buzzer loudness set to 0x"));
    Serial.println(duty, HEX);
    return true;
}

// ---------------------------------------------------------------------------
// Helper: beep once (short beep using register 0x2D)
//   time10ms: beep duration in units of 10 ms
//             e.g. 5 = 50 ms, 10 = 100 ms, 50 = 500 ms
// ---------------------------------------------------------------------------
bool beep(uint8_t time10ms) {
    return dwin.writeRegister(0x00, REG_BUZZ_TIME, time10ms);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.setWaitForAck(true);     // register writes need ACK confirmation

    Serial.println(F("DWIN buzzer loudness demo"));
    delay(500);

    // ----- Demo 1: three fixed loudness levels -----
    Serial.println(F("Testing quiet / medium / loud beeps"));

    setBuzzerLoudness(0x0040);    // quiet
    beep(20);                     // 200 ms beep
    delay(600);

    setBuzzerLoudness(0x00F0);    // medium (factory default)
    beep(20);
    delay(600);

    setBuzzerLoudness(0x0200);    // loud
    beep(20);
    delay(600);

    Serial.println(F("Demo 1 done - starting loudness ramp"));
}

// ---------------------------------------------------------------------------
// loop() : ramp loudness up and down
// ---------------------------------------------------------------------------
void loop() {
    static uint16_t duty      = 0x0010;
    static int8_t   direction = 1;    // +1 = louder, -1 = quieter

    setBuzzerLoudness(duty);
    beep(3);                          // 30 ms beep at the new loudness

    // Step the duty cycle
    if (direction > 0) {
        duty = (uint16_t)(duty + 0x0010);
        if (duty >= 0x0200) { duty = 0x0200; direction = -1; }
    } else {
        duty = (uint16_t)(duty - 0x0010);
        if (duty <= 0x0010) { duty = 0x0010; direction = 1; }
    }

    delay(300);   // pause between beeps
}
