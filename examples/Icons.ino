/*
  Icons.ino - Display and change status icons on a DWIN display

  Icons shown:
    - Battery      (0 = empty ... 5 = full)   VP 0x2000
    - Network bars (0 = none  ... 4 = full)   VP 0x2001
    - Bluetooth    (0 = off, 1 = on)          VP 0x2002
    - Wi-Fi bars   (0 = none  ... 4 = full)   VP 0x2003

  DWIN project setup (DGUS Tool):
    Create four "Icon Display" controls on page 0.
    For each one:
      - Set the Icon VP to the address shown above.
      - Add the icon images in the order you want (index 0, 1, 2, ...).
      - Set "Icon count" to the number of images (e.g. 6 for battery).
    Optionally add touch buttons to change each icon's state.

  This sketch:
    - Cycles every icon through all of its states automatically.
    - Also reacts to touch buttons (if present) to change individual icons.
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

// ---------------------------------------------------------------------------
// VP map (must match your DWIN project)
// ---------------------------------------------------------------------------
const uint16_t VP_BATTERY     = 0x2000;   // 0..5
const uint16_t VP_NETWORK     = 0x2001;   // 0..4
const uint16_t VP_BLUETOOTH   = 0x2002;   // 0..1
const uint16_t VP_WIFI        = 0x2003;   // 0..4

// Optional touch buttons to change icons manually
const uint16_t VP_BTN_BATT_UP = 0x1000;   // press -> battery +1
const uint16_t VP_BTN_NET_UP  = 0x1001;   // press -> network +1
const uint16_t VP_BTN_BT_TOGGLE = 0x1002; // press -> bluetooth toggle
const uint16_t VP_BTN_WIFI_UP = 0x1003;   // press -> wifi +1

// Icon ranges
const uint8_t BATTERY_MAX = 5;
const uint8_t NETWORK_MAX = 4;
const uint8_t WIFI_MAX    = 4;

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
// Current icon states (Arduino-side mirror)
// ---------------------------------------------------------------------------
uint8_t batteryLevel = 3;   // 0..5
uint8_t networkBars  = 2;   // 0..4
bool    bluetoothOn  = false;
uint8_t wifiBars     = 3;   // 0..4

// ---------------------------------------------------------------------------
// Icon setters (write the state to the display)
// ---------------------------------------------------------------------------
void setBattery(uint8_t level) {
    if (level > BATTERY_MAX) level = BATTERY_MAX;
    batteryLevel = level;
    dwin.writeVP(VP_BATTERY, (uint16_t)level);
    Serial.print(F("Battery  -> ")); Serial.println(level);
}

void setNetwork(uint8_t bars) {
    if (bars > NETWORK_MAX) bars = NETWORK_MAX;
    networkBars = bars;
    dwin.writeVP(VP_NETWORK, (uint16_t)bars);
    Serial.print(F("Network  -> ")); Serial.println(bars);
}

void setBluetooth(bool on) {
    bluetoothOn = on;
    dwin.writeVP(VP_BLUETOOTH, (uint16_t)(on ? 1 : 0));
    Serial.print(F("Bluetooth-> ")); Serial.println(on ? F("ON") : F("OFF"));
}

void setWifi(uint8_t bars) {
    if (bars > WIFI_MAX) bars = WIFI_MAX;
    wifiBars = bars;
    dwin.writeVP(VP_WIFI, (uint16_t)bars);
    Serial.print(F("Wi-Fi    -> ")); Serial.println(bars);
}

// ---------------------------------------------------------------------------
// Touch callback - optional manual control
// ---------------------------------------------------------------------------
void onTouch(uint16_t vp, uint16_t value) {
    if (value == 0) return;    // ignore release events

    switch (vp) {
        case VP_BTN_BATT_UP:
            setBattery((uint8_t)((batteryLevel + 1) % (BATTERY_MAX + 1)));
            break;
        case VP_BTN_NET_UP:
            setNetwork((uint8_t)((networkBars + 1) % (NETWORK_MAX + 1)));
            break;
        case VP_BTN_BT_TOGGLE:
            setBluetooth(!bluetoothOn);
            break;
        case VP_BTN_WIFI_UP:
            setWifi((uint8_t)((wifiBars + 1) % (WIFI_MAX + 1)));
            break;
        default:
            break;
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
    dwin.setPage(0);

    // Push initial states to the display
    setBattery(batteryLevel);
    setNetwork(networkBars);
    setBluetooth(bluetoothOn);
    setWifi(wifiBars);

    Serial.println(F("Icon demo started"));
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();   // handle touch events

    // Auto-cycle all icons every 2 seconds so you can see them change
    static uint32_t last = 0;
    if (millis() - last >= 2000) {
        last = millis();

        setBattery((uint8_t)((batteryLevel + 1) % (BATTERY_MAX + 1)));
        setNetwork((uint8_t)((networkBars  + 1) % (NETWORK_MAX + 1)));
        setBluetooth(!bluetoothOn);
        setWifi((uint8_t)((wifiBars + 1) % (WIFI_MAX + 1)));
    }
}