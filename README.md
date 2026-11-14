# DWIN_LCD

Babatunde Arduino library for **DWIN DGUS / T5L HMI displays**. Implements the native DWIN serial protocol (0x80–0x83) with optional CRC16-CCITT, touch callbacks, text/color helpers, single/multi-curve plotting, and image display/streaming.

No external dependencies for core features — works with any Arduino board via `HardwareSerial` or `SoftwareSerial`. Optional image streaming supports SD cards and HTTP servers (Wi‑Fi or GSM).

---

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [File Structure](#file-structure)
- [Quick Start](#quick-start)
- [Hardware Wiring](#hardware-wiring)
- [DWIN Project Setup](#dwin-project-setup)
- [Library Reference](#library-reference)
  - [DWIN_LCD](#class-dwin_lcd)
  - [DWIN_Curve](#class-dwin_curve)
  - [DWIN_MultiCurve](#class-dwin_multicurve)
- [RGB565 Colors](#rgb565-colors)
- [Protocol Reference](#protocol-reference)
- [Examples](#examples)
  - [1. Send Text to LCD](#1-send-text-to-lcd)
  - [2. Change Backlight](#2-change-backlight)
  - [3. Switch Page](#3-switch-page)
  - [4. Receive Value from LCD to Serial](#4-receive-value-from-lcd-to-serial)
  - [5. Button to LED + ADC Display](#5-button-to-led--adc-display)
  - [6. Write and Read Registers](#6-write-and-read-registers)
  - [7. Set and Read Time (RTC)](#7-set-and-read-time-rtc)
  - [8. Display and Change Icons](#8-display-and-change-icons)
  - [9. Icon Animation](#9-icon-animation)
  - [10. Buzzer Loudness](#10-buzzer-loudness)
  - [11. Single-Curve Plot](#11-single-curve-plot)
  - [12. Multi-Curve Plot](#12-multi-curve-plot)
  - [13. Read Keyboard Input from LCD](#13-read-keyboard-input-from-lcd)
  - [14. Display Image by ID](#14-display-image-by-id)
  - [15. Stream Image to Display](#15-stream-image-to-display)
  - [16. Stream Image from SD Card](#16-stream-image-from-sd-card)
  - [17. Stream Image from a PHP Server](#17-stream-image-from-a-php-server)
  - [18. Stream Image from a Server via TinyGSM](#18-stream-image-from-a-server-via-tinygsm)
- [Troubleshooting](#troubleshooting)
- [Performance Tips](#performance-tips)
- [License](#license)

---

## Features

| Feature | Description |
|---------|-------------|
| **Native protocol** | Full support for DWIN commands 0x80–0x83 |
| **CRC16-CCITT** | Optional CRC on TX/RX frames |
| **Touch callbacks** | Non-blocking, user-registered callback for touch/VP events |
| **Register R/W** | Read/write DWIN control registers (page 0x00 system + others) |
| **VP R/W** | Read/write 16-bit VP words, with automatic chunking |
| **Text helpers** | Write strings, numbers (int32/uint32), and floats |
| **Text input** | Read strings typed on the DWIN on-screen keyboard |
| **Color helpers** | RGB565, RGB888, raw 5/6/5 conversions + 20 named colors |
| **Curves** | Single `DWIN_Curve` and multi-channel `DWIN_MultiCurve` with rolling window |
| **Scaling** | Per-channel engineering-unit conversion (gain + offset) |
| **Dual-mode curves** | Real-time scroll or full-window redraw |
| **Backlight control** | 0–64 with fade support |
| **RTC helpers** | Set/read DWIN real-time clock (BCD format) |
| **Buzzer control** | Adjustable PWM duty cycle for loudness |
| **Image display** | Display pre-loaded images by ID (0x70 / 0x71 commands) |
| **Image streaming** | Stream RGB565 images into a VP block (procedural, SD card, or HTTP) |
| **Remote images** | Fetch and forward images over Wi-Fi or GSM/GPRS/LTE |

---

## Installation

### Arduino IDE (Library Manager style)

1. Create a folder called `DWIN_LCD` in your Arduino `libraries/` directory.
2. Place `DWIN_LCD.h`, `DWIN_LCD.cpp`, and `library.properties` inside it.
3. Restart the Arduino IDE.

```
Arduino/
└── libraries/
    └── DWIN_LCD/
        ├── DWIN_LCD.h
        ├── DWIN_LCD.cpp
        ├── library.properties
        ├── README.md
        ├── LICENSE
        └── examples/
            ├── SendText/
            ├── Backlight/
            ├── PageSwitch/
            ├── ReadToSerial/
            ├── ButtonLedAdc/
            ├── RegisterRW/
            ├── RTC_Time/
            ├── Icons/
            ├── IconAnimation/
            ├── BuzzerLoudness/
            ├── SinglePlot/
            ├── MultiPlot/
            ├── ReadKeyboard/
            ├── ShowImageById/
            ├── StreamImage/
            ├── SDCardStream/
            ├── ServerStream/             (ESP32 + Wi-Fi)
            └── ServerStreamTinyGSM/      (SIM800 / SIM7600, etc.)
```

### Manual (per-sketch)

Copy `DWIN_LCD.h` and `DWIN_LCD.cpp` into the same folder as your `.ino` sketch. The Arduino IDE will include them automatically.

---

## File Structure

```
DWIN_LCD/
├── DWIN_LCD.h            # Public API declarations
├── DWIN_LCD.cpp          # Implementation
├── library.properties    # Arduino IDE metadata
├── README.md             # This file
├── LICENSE               # MIT
└── examples/
    ├── SendText/         # Write strings/numbers to the display
    ├── Backlight/        # Adjust backlight 0..64
    ├── PageSwitch/       # Switch pages from Arduino or touch
    ├── ReadToSerial/     # Print incoming VP/touch events to Serial
    ├── ButtonLedAdc/     # Touch button drives LED + live ADC value
    ├── RegisterRW/       # Read/write control registers
    ├── RTC_Time/         # Set/read display real-time clock
    ├── Icons/            # Battery / network / BT / Wi-Fi icon states
    ├── IconAnimation/    # Sprite, blink, marquee, progress animations
    ├── BuzzerLoudness/   # PWM duty-cycle loudness control
    ├── SinglePlot/       # One live curve
    ├── MultiPlot/        # Three synchronized curves
    ├── ReadKeyboard/     # Read strings typed on the DWIN keyboard
    ├── ShowImageById/    # Display an image by its flash ID
    ├── StreamImage/      # Stream a procedural RGB565 image to a VP
    ├── SDCardStream/     # Stream a raw .bin or .bmp from an SD card
    ├── ServerStream/     # ESP32 + Wi-Fi, HTTP GET row-by-row
    └── ServerStreamTinyGSM/ # Cellular modem (SIM800 etc.) via TinyGSM
```

---

## Quick Start

```cpp
#include "DWIN_LCD.h"

DWIN_LCD dwin(Serial1);       // MEGA / ESP32 / STM32

void onTouch(uint16_t vp, uint16_t value) {
    Serial.print("Touch 0x");
    Serial.print(vp, HEX);
    Serial.print(" = ");
    Serial.println(value);
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);              // must match panel baud
    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.attachTouchCallback(onTouch);

    dwin.setBacklight(48);
    dwin.setPage(0);
    dwin.writeStringColored(0x2000, 0x3000, "Hello DWIN!", DWIN_GREEN);
}

void loop() {
    dwin.process();                   // required for touch events
}
```

---

## Hardware Wiring

| DWIN display pin | Arduino MEGA / ESP32 | Arduino UNO (SoftwareSerial) |
|------------------|----------------------|-------------------------------|
| Display TX       | Serial1 RX (pin 19)  | Digital pin 10 (RX)           |
| Display RX       | Serial1 TX (pin 18)  | Digital pin 11 (TX)           |
| GND              | GND                  | GND                           |
| +5V              | 5V                   | 5V                            |

> **Important:** TX ↔ RX must be crossed. Both sides must use the same baud rate (default 9600).

For **image streaming**, use **115200 baud minimum** (or 921600 on T5L panels). Streaming at 9600 is impractically slow.

---

## DWIN Project Setup

Every control you want to drive from the Arduino must have a **VP address** assigned in DGUS Tool. The examples in this library use this VP layout:

| VP Address | Purpose |
|------------|---------|
| `0x0016`   | Standard DWIN touch-return VP |
| `0x0084`   | Page switch (system) |
| `0x1000`–`0x1003` | Touch buttons |
| `0x2000`–`0x2003` | Text / icons / keyboard input |
| `0x3000`   | Text color |
| `0x4000`+  | Curve data / streamed image block |

### Common control types

| Control | What it needs |
|---------|---------------|
| **Text Display** | A Text VP (and optional Color VP) |
| **Text Input** | An Input VP + word length; keyboard opens on tap |
| **Icon Display** | An Icon VP + a list of icon images (index = VP value) |
| **Curve** | A data VP + point count + Y range |
| **Picture / Image** | A data VP + width + height + RGB565 format |
| **Touch Button** | A Touch VP + "Return key value" action |
| **RTC** | Built-in system registers 0x1F / 0x20 |

For touch events to reach the Arduino, mark the button's VP as **"Upload"** in the touch control's properties — the panel will push changes automatically.

### Image controls (for examples 14–18)

There are two families of image controls:

1. **Pre-loaded images** — you build an ICL file in DGUS Tool, download it to the panel via SD card, and display any image with the `0x70` command using its ID.
2. **VP-backed pictures** — you assign a VP block to an "Image Display" control, and the panel reads raw RGB565 pixels from that VP (row-major, `width × height × 2` bytes).

The examples below use both, depending on the scenario.

---

## Library Reference

### Class `DWIN_LCD`

Main class. Construct with a `Stream&` (Serial1, Serial2, SoftwareSerial, etc.).

```cpp
DWIN_LCD dwin(Serial1);
DWIN_LCD dwin(Serial2, 115200);
DWIN_LCD dwin(mySoftSerial);
```

#### Initialisation

| Method | Description |
|--------|-------------|
| `void begin(long baud = 9600)` | Stores baud rate. **Sketch must still call `SerialX.begin()` itself.** |
| `void setCRC(bool enable)` | Enable/disable CRC16-CCITT on TX. Default: `false`. |
| `void setWaitForAck(bool enable)` | Wait for panel "OK" after each write. Default: `true`. Set `false` for speed on write-only workloads. |
| `void attachTouchCallback(DWINTouchCallback cb)` | Register a `void cb(uint16_t vp, uint16_t value)` callback. Pass `nullptr` to detach. |
| `void process()` / `void loop()` | Non-blocking RX parser. **Call every loop iteration.** Fires the touch callback. |
| `void flushInput()` | Drop pending bytes and reset the RX state machine. |

#### Register R/W

| Method | Description |
|--------|-------------|
| `bool writeRegister(uint8_t page, uint8_t reg, uint8_t value)` | Write a single byte to a control register. |
| `bool writeRegister(uint8_t page, uint8_t reg, const uint8_t *data, uint8_t len)` | Write multiple bytes. |
| `uint8_t readRegister(uint8_t page, uint8_t reg, uint8_t len, uint8_t *out)` | Read `len` bytes. Returns number read (0 on failure). |

#### VP R/W

| Method | Description |
|--------|-------------|
| `bool writeVP(uint16_t vp, uint16_t value)` | Write a single 16-bit word. |
| `bool writeVP(uint16_t vp, const uint16_t *data, uint16_t len)` | Write multiple words (auto-chunked at 120 words/frame). |
| `uint8_t readVP(uint16_t vp, uint16_t len, uint16_t *out)` | Read up to 62 words. Returns number read. |

#### Text / Number / Float Helpers

| Method | Description |
|--------|-------------|
| `bool writeString(uint16_t vp, const char *str)` | Write ASCII string (≤128 chars, packed as 16-bit words). |
| `bool writeNumber(uint16_t vp, int32_t value)` | Write a signed 32-bit int (2 VPs). |
| `bool writeNumber(uint16_t vp, uint32_t value)` | Write an unsigned 32-bit int. |
| `bool writeFloat(uint16_t vp, float value)` | Write IEEE-754 float (2 VPs). |

#### System Helpers

| Method | Description |
|--------|-------------|
| `bool setBacklight(uint8_t level)` | Set backlight 0..64 (register 0x01, page 0x00). |
| `bool setPage(uint16_t page)` | Switch display to a page (writes VP 0x0084). |

#### Text Color Helpers

| Method | Description |
|--------|-------------|
| `bool setTextColor(uint16_t colorVp, uint16_t color565)` | Write a raw RGB565 value. |
| `bool setTextColorRGB(uint16_t colorVp, uint8_t r, uint8_t g, uint8_t b)` | Convert 8-bit RGB → RGB565. |
| `bool setTextColor565(uint16_t colorVp, uint8_t r5, uint8_t g6, uint8_t b5)` | Use raw 5/6/5 components. |
| `bool writeStringColored(uint16_t strVp, uint16_t colorVp, const char *str, uint16_t color565)` | One-shot: text + color. |
| `void fadeTextColor(uint16_t colorVp, uint16_t from, uint16_t to, uint16_t durationMs, uint16_t steps = 16)` | Blocking color fade. |

#### Curve Primitives (used by `DWIN_Curve` )

| Method | Description |
|--------|-------------|
| `bool curveAddPoint(uint16_t vp, uint16_t value)` | Append one 16-bit point. |
| `bool curveAddPoint8(uint16_t vp, uint8_t value)` | Append one 8-bit point. |
| `bool curveAddPoints(uint16_t vp, const uint16_t *values, uint16_t count)` | Append multiple 16-bit points. |
| `bool curveAddPoints8(uint16_t vp, const uint8_t *values, uint16_t count)` | Append multiple 8-bit points. |
| `bool curveClear(uint16_t vp, uint16_t pointCount)` | Zero out the curve. |
| `bool curveSetRange(uint16_t minVp, uint16_t maxVp, uint16_t minVal, uint16_t maxVal)` | Set axis range (if VPs exist). |

---

### Class `DWIN_Curve`

Single rolling-window curve channel with local ring buffer.

```cpp
DWIN_Curve plot(dwin, 0x4000, 100);   // VP, max points
plot.begin();
plot.pushScaled(42.5f);
```

| Method | Description |
|--------|-------------|
| `void begin()` | Allocate ring buffer. Call once in `setup()`. |
| `void push(uint16_t value)` | Append + transmit (redraw or send-latest). |
| `void pushScaled(float raw)` | Same, but applies scale (`gain * raw + offset`). |
| `void pushSilent(uint16_t value)` | Buffer only, no transmit. |
| `void pushScaledSilent(float raw)` | Buffer only, scaled. |
| `void sendLatest()` | Transmit only the newest point. |
| `void setAutoRedraw(bool enable)` | `true` = full redraw; `false` = real-time scroll. |
| `void set8Bit(bool enable)` | Clamp values to 0..255. |
| `void setScale(float gain, float offset)` | Configure engineering-unit scaling. |
| `void redraw()` | Force full-window redraw. |
| `void clear()` | Zero the buffer and the display's VP block. |
| `uint16_t point(uint16_t i) const` | Access a buffered point (0 = oldest). |
| `uint16_t latest() const` | Newest point. |
| `uint16_t minValue() const` / `maxValue() const` | Window stats. |
| `uint16_t vp() const` | The curve's VP address. |


---

## RGB565 Colors

Use the macro `DWIN_RGB565(r8, g8, b8)` to convert 8-bit RGB on the fly, or pick a named constant:

```cpp
dwin.setTextColor(0x3000, DWIN_RGB565(128, 200, 255));  // custom
dwin.setTextColor(0x3000, DWIN_RED);                    // named
```

Named constants: `DWIN_BLACK`, `DWIN_WHITE`, `DWIN_RED`, `DWIN_GREEN`, `DWIN_BLUE`, `DWIN_YELLOW`, `DWIN_CYAN`, `DWIN_MAGENTA`, `DWIN_ORANGE`, `DWIN_GRAY`, `DWIN_LIGHTGRAY`, `DWIN_DARKGRAY`, `DWIN_BROWN`, `DWIN_PINK`, `DWIN_PURPLE`, `DWIN_LIME`, `DWIN_NAVY`, `DWIN_TEAL`, `DWIN_GOLD`, `DWIN_SILVER`.

For raw 5/6/5 components: `DWIN_RGB565_RAW(r5, g6, b5)`.

---

## Protocol Reference

All VP/register frames follow the native DWIN structure:

```
<0x5A><0xA5><ByteCount><Command><Data…>[<CRC16>]
```

| Command | Meaning |
|---------|---------|
| `0x80` | Write control registers |
| `0x81` | Read control registers |
| `0x82` | Write VP words |
| `0x83` | Read VP words / auto-upload from panel |

Common ACK responses:
- `0x4F4B` = "OK"
- `0x4E47` = "NG"

Touch events arrive as `0x83` frames. The library handles both the 5-byte auto-upload format and the standard read-response format.

### Image commands (different header)

Image commands use a **different header** (`0xAA`) and **fixed terminator** (`0xCC 0x33 0xC3 0x3C`). They are sent as raw bytes, **not** through the VP protocol.

| Command | Frame |
|---------|-------|
| Display full-screen image | `AA 70 <Pic_ID_H> <Pic_ID_L> CC 33 C3 3C` |
| Crop + paste image area | `AA 71 <Pic_ID> <Xs> <Ys> <Xe> <Ye> <X> <Y> CC 33 C3 3C` |
| Save current screen to buffer | `AA E2 <Pic_ID> CC 33 C3 3C` |

---

## Examples

### 1. Send Text to LCD

```cpp
#include "DWIN_LCD.h"
DWIN_LCD dwin(Serial1);

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);
    dwin.begin(9600);
    dwin.writeString(0x2000, "Hello DWIN!");
}

void loop() {
    static uint32_t last = 0;
    if (millis() - last >= 1000) {
        last = millis();
        char buf[32];
        snprintf(buf, sizeof(buf), "Count: %lu", millis() / 1000);
        dwin.writeString(0x2000, buf);
    }
}
```

**DWIN setup:** Text Display control with Text VP = `0x2000`, word length ≥ 16.

---

### 2. Change Backlight

```cpp
void setup() {
    Serial1.begin(9600);
    dwin.begin(9600);
    dwin.setBacklight(48);      // 0 = off, 64 = max
}

void loop() {
    static uint8_t level = 10;
    static int8_t dir = 1;
    dwin.setBacklight(level);
    level = (uint8_t)(level + dir);
    if (level >= 64) { level = 64; dir = -1; }
    if (level <= 10) { level = 10; dir =  1; }
    delay(40);
}
```

**No DWIN setup needed** — register 0x01 is a built-in system register.

---

### 3. Switch Page

```cpp
void setup() {
    Serial1.begin(9600);
    dwin.begin(9600);
    dwin.setPage(0);
}

void loop() {
    dwin.process();
    static uint32_t last = 0;
    if (millis() - last >= 3000) {
        last = millis();
        static uint8_t page = 0;
        page = (page + 1) % 3;
        dwin.setPage(page);
    }
}
```

**No DWIN setup needed** — page switch uses VP `0x0084`.

---

### 4. Receive Value from LCD to Serial

```cpp
void onTouch(uint16_t vp, uint16_t value) {
    Serial.print("VP = 0x");
    Serial.print(vp, HEX);
    Serial.print("   value = ");
    Serial.println(value);
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);
    dwin.begin(9600);
    dwin.attachTouchCallback(onTouch);
}

void loop() {
    dwin.process();
}
```

**DWIN setup:** mark button VPs as **"Upload"** so the panel pushes them automatically.

---

### 5. Button to LED + ADC Display

```cpp
const uint8_t LED_PIN = 13;
const uint16_t VP_BTN = 0x1000;
const uint16_t VP_TXT = 0x2000;
const uint16_t VP_CLR = 0x3000;

bool ledState = false;

void onTouch(uint16_t vp, uint16_t value) {
    if (vp == VP_BTN) {
        ledState = (value != 0);
        digitalWrite(LED_PIN, ledState);
        dwin.writeStringColored(VP_TXT, VP_CLR,
                                ledState ? "LED ON" : "LED OFF",
                                ledState ? DWIN_GREEN : DWIN_RED);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    Serial1.begin(9600);
    dwin.begin(9600);
    dwin.attachTouchCallback(onTouch);
}

void loop() {
    dwin.process();

    static uint32_t last = 0;
    if (millis() - last >= 250) {
        last = millis();
        uint16_t adc = analogRead(A0);
        float v = adc * 5.0f / 1023.0f;
        char buf[32];
        snprintf(buf, sizeof(buf), "ADC: %u   V: %.2f", adc, v);
        dwin.writeString(VP_TXT, buf);
    }
}
```

**DWIN setup:** touch button (VP `0x1000`), text control (`0x2000`), color VP (`0x3000`).

---

### 7. Set and Read Time (RTC)

```cpp
uint8_t decToBcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }
uint8_t bcdToDec(uint8_t v) { return ((v >> 4) * 10) + (v & 0x0F); }

bool setRtc(uint8_t y, uint8_t mo, uint8_t d, uint8_t w,
            uint8_t h, uint8_t mi, uint8_t s) {
    dwin.writeRegister(0x00, 0x1F, 0x5A);   // unlock
    uint8_t rtc[7] = {
        decToBcd(y), decToBcd(mo), decToBcd(d), decToBcd(w),
        decToBcd(h), decToBcd(mi), decToBcd(s)
    };
    return dwin.writeRegister(0x00, 0x20, rtc, 7);
}

bool readRtc(uint8_t &y, uint8_t &mo, uint8_t &d, uint8_t &w,
             uint8_t &h, uint8_t &mi, uint8_t &s) {
    uint8_t rtc[7];
    if (dwin.readRegister(0x00, 0x20, 7, rtc) != 7) return false;
    y  = bcdToDec(rtc[0]); mo = bcdToDec(rtc[1]);
    d  = bcdToDec(rtc[2]); w  = bcdToDec(rtc[3]);
    h  = bcdToDec(rtc[4]); mi = bcdToDec(rtc[5]);
    s  = bcdToDec(rtc[6]);
    return true;
}

void setup() {
    Serial1.begin(9600);
    dwin.begin(9600);
    setRtc(25, 1, 15, 3, 14, 30, 0);   // 2025-01-15 Wed 14:30:00
}
```

**No DWIN setup needed** — registers 0x1F/0x20 are built-in RTC registers.

---

### 8. Display and Change Icons

```cpp
const uint16_t VP_BATTERY   = 0x2000;   // 0..5
const uint16_t VP_NETWORK   = 0x2001;   // 0..4
const uint16_t VP_BLUETOOTH = 0x2002;   // 0..1
const uint16_t VP_WIFI      = 0x2003;   // 0..4

void setBattery(uint8_t lvl)   { dwin.writeVP(VP_BATTERY,   lvl); }
void setNetwork(uint8_t bars)  { dwin.writeVP(VP_NETWORK,   bars); }
void setBluetooth(bool on)     { dwin.writeVP(VP_BLUETOOTH, on ? 1 : 0); }
void setWifi(uint8_t bars)     { dwin.writeVP(VP_WIFI,      bars); }

void loop() {
    static uint32_t last = 0;
    if (millis() - last >= 2000) {
        last = millis();
        setBattery((millis() / 2000) % 6);
        setNetwork((millis() / 2000) % 5);
        setBluetooth((millis() / 4000) & 1);
        setWifi((millis() / 2000) % 5);
    }
}
```

**DWIN setup:** four **Icon Display** controls, each with an image list matching its VP's value range (e.g. battery → 6 images `batt0..batt5`).

---

### 9. Icon Animation

```cpp
const uint16_t VP_SPRITE   = 0x2000;   // 8 frames
const uint16_t VP_BLINK    = 0x2001;   // 2 frames
const uint16_t VP_MARQUEE  = 0x2010;   // 5 segments (0x2010..0x2014)
const uint16_t VP_PROGRESS = 0x2020;   // 11 frames

void animateSprite() {
    static uint8_t f = 0;
    dwin.writeVP(VP_SPRITE, f);
    f = (f + 1) % 8;
}

void animateBlink() {
    static bool on = false;
    on = !on;
    dwin.writeVP(VP_BLINK, on ? 1 : 0);
}

void animateMarquee() {
    static uint8_t pos = 0;
    for (uint8_t i = 0; i < 5; i++)
        dwin.writeVP(VP_MARQUEE + i, 0);
    dwin.writeVP(VP_MARQUEE + pos, 1);
    pos = (pos + 1) % 5;
}

void animateProgress() {
    static uint8_t lvl = 0;
    dwin.writeVP(VP_PROGRESS, lvl);
    lvl = (lvl + 1) % 11;
}

void loop() {
    dwin.process();
    uint32_t now = millis();

    static uint32_t t1 = 0, t2 = 0, t3 = 0, t4 = 0;
    if (now - t1 >=  80) { t1 = now; animateSprite();   }
    if (now - t2 >= 500) { t2 = now; animateBlink();    }
    if (now - t3 >= 150) { t3 = now; animateMarquee();  }
    if (now - t4 >= 250) { t4 = now; animateProgress(); }
}
```

**DWIN setup:** several Icon Display controls with multi-frame image lists.

---

### 10. Buzzer Loudness

```cpp
bool setBuzzerLoudness(uint16_t duty) {
    dwin.writeRegister(0x00, 0x27, 0x5A);   // unlock
    uint8_t d[2] = { (uint8_t)(duty >> 8), (uint8_t)(duty & 0xFF) };
    return dwin.writeRegister(0x00, 0x2B, d, 2);
}

bool beep(uint8_t time10ms) {
    return dwin.writeRegister(0x00, 0x2D, time10ms);
}

void loop() {
    static uint16_t duty = 0x0010;
    static int8_t dir = 1;

    setBuzzerLoudness(duty);
    beep(3);   // 30 ms beep

    if (dir > 0) { duty += 0x10; if (duty >= 0x200) { duty = 0x200; dir = -1; } }
    else         { duty -= 0x10; if (duty <= 0x010) { duty = 0x010; dir =  1; } }
    delay(300);
}
```

**Duty range:** `0x0010` (quiet) → `0x0200` (loud). `0x00F0` is factory default.  
**No DWIN setup needed** — registers 0x27, 0x2B, 0x2D are built-in.

---

### 11. Single-Curve Plot

```cpp
const uint16_t VP_CURVE = 0x4000;
DWIN_Curve plot(dwin, VP_CURVE, 100);

void setup() {
    Serial1.begin(9600);
    dwin.begin(9600);
    dwin.setPage(0);
    plot.begin();
    plot.setAutoRedraw(false);            // real-time scroll mode
    plot.setScale(100.0f / 3.3f, 0.0f);   // 0..3.3V -> 0..100
}

void loop() {
    dwin.process();

    static uint32_t last = 0;
    if (millis() - last >= 100) {
        last = millis();
        float v = analogRead(A0) * 3.3f / 1023.0f;
        plot.pushScaled(v);
    }
}
```




### 14. Display Image by ID

DWIN panels display images from their internal flash using the **0x70 "Picture Display"** command. You build an ICL file in DGUS Tool, download it to the panel via SD card, and then display any image by its ID.

```cpp
/*
  ShowImageById.ino - Display an image stored on the DWIN panel by its ID
*/

#include "DWIN_LCD.h"

DWIN_LCD dwin(Serial1);

const uint16_t IMAGE_IDS[] = { 0, 1, 2, 3 };
const uint8_t  IMAGE_COUNT = sizeof(IMAGE_IDS) / sizeof(IMAGE_IDS[0]);

const uint16_t VP_BTN_NEXT = 0x1000;
const uint16_t VP_TEXT     = 0x2000;
const uint16_t VP_TEXT_CLR = 0x3000;

const uint32_t AUTO_ADVANCE_MS = 2000;

uint8_t currentIndex = 0;

void showImageById(uint16_t picId) {
    // Raw 0x70 frame (different from the 0x5A5A VP protocol)
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

    char buf[16];
    snprintf(buf, sizeof(buf), "ID: %u", picId);
    dwin.writeStringColored(VP_TEXT, VP_TEXT_CLR, buf, DWIN_WHITE);
}

void nextImage() {
    currentIndex = (uint8_t)((currentIndex + 1) % IMAGE_COUNT);
    showImageById(IMAGE_IDS[currentIndex]);
}

void onTouch(uint16_t vp, uint16_t value) {
    if (vp == VP_BTN_NEXT && value == 1) nextImage();
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);
    dwin.begin(9600);
    dwin.setCRC(false);
    dwin.attachTouchCallback(onTouch);

    dwin.setBacklight(48);
    dwin.setPage(0);

    delay(500);
    showImageById(IMAGE_IDS[0]);
}

void loop() {
    dwin.process();

    static uint32_t last = 0;
    if (millis() - last >= AUTO_ADVANCE_MS) {
        last = millis();
        nextImage();
    }
}
```

**Frame sent:** `AA 70 <ID_H> <ID_L> CC 33 C3 3C`

**DWIN setup:** load images into the ICL file in DGUS Tool. Note each image's ID (index) — that's what you pass to `showImageById()`.

**Optional 0x71 crop-and-paste** for composite screens:

```cpp
void showImageArea(uint16_t picId,
                   uint16_t srcX1, uint16_t srcY1,
                   uint16_t srcX2, uint16_t srcY2,
                   uint16_t dstX,  uint16_t dstY) {
    Serial1.write(0xAA);
    Serial1.write(0x71);
    Serial1.write((uint8_t)(picId >> 8));  Serial1.write((uint8_t)(picId & 0xFF));
    Serial1.write((uint8_t)(srcX1 >> 8));  Serial1.write((uint8_t)(srcX1 & 0xFF));
    Serial1.write((uint8_t)(srcY1 >> 8));  Serial1.write((uint8_t)(srcY1 & 0xFF));
    Serial1.write((uint8_t)(srcX2 >> 8));  Serial1.write((uint8_t)(srcX2 & 0xFF));
    Serial1.write((uint8_t)(srcY2 >> 8));  Serial1.write((uint8_t)(srcY2 & 0xFF));
    Serial1.write((uint8_t)(dstX  >> 8));  Serial1.write((uint8_t)(dstX  & 0xFF));
    Serial1.write((uint8_t)(dstY  >> 8));  Serial1.write((uint8_t)(dstY  & 0xFF));
    Serial1.write(0xCC); Serial1.write(0x33);
    Serial1.write(0xC3); Serial1.write(0x3C);
}
```




## Troubleshooting

| Symptom | Likely cause / fix |
|---------|---------------------|
| Nothing happens at all | TX ↔ RX swapped. GND not common. Baud mismatch. |
| Writes succeed but nothing shows | Wrong VP in DGUS Tool. Word length too small. Control set to read from a different VP. |
| Touch events never fire | Button VP not marked "Upload". Or the callback wasn't attached. |
| Keyboard input reads as empty | Input control not tapped yet, or word length mismatch, or VP wrong. |
| `readRegister` / `readVP` return 0 | `setWaitForAck(false)` is set — reads need ACK waiting. Or baud mismatch. |
| Text shows as garbage | Wrong encoding (CJK panel — need GB2312 pairs). Wrong word length. |
| Curve doesn't plot | Control set to wrong mode (real-time vs. buffer). Y range too small. |
| Icons show wrong image | Icon count mismatch in DGUS Tool, or VP value out of range. |
| Image by ID (0x70) does nothing | Wrong Pic_ID, image not in panel flash, or terminator `CC 33 C3 3C` missing. |
| Streamed image garbled | Wrong VP size, wrong byte order, or Picture control size mismatch. |
| Streamed image shifted horizontally | Row width mismatch between sketch and control. |
| Streamed image upside-down | BMP row order inverted (fix `fileRow = IMG_H - 1 - displayY`). |
| Colors look swapped (R↔B) | RGB565 byte-swap done in the wrong direction; try removing the swap. |
| HTTP stream times out at row 0 | `http.useHTTP10(true)` missing (chunked encoding breaks raw reads). |
| TinyGSM returns empty stream | `HTTPClient` + `TinyGsmClient` incompatibility — use manual HTTP/1.0 GET. |
| Slow response | Every write waits for ACK. Try `setWaitForAck(false)` or raise baud to 115200. |
| RTC doesn't advance | Panel lacks RTC hardware, or `0x5A` unlock wasn't written before the time. |

**Quick diagnostics:**

```cpp
// Print every byte coming from the panel
void loop() {
    while (Serial1.available()) {
        Serial.print(Serial1.read(), HEX);
        Serial.print(' ');
    }
}
```

---

## Performance Tips

- **Baud rate** — 9600 is safe for text/icons. **Use 115200 minimum for image streaming** — ideally 921600 on T5L panels.
- **Skip ACKs** — `dwin.setWaitForAck(false)` roughly halves write time on write-only workloads (animations, curves in real-time mode, backlight fades, image streaming). **Keep it `true` when using `readVP()` / `readRegister()` / keyboard reads.**
- **Real-time curves** — set `plot.setAutoRedraw(false)` to send only the newest point (1 word/sample) instead of the full window (100 words/sample).
- **8-bit curves** — use `plot.set8Bit(true)` when values fit in 0..255 to save bandwidth and RAM.
- **Batch writes** — `writeVP(vp, data, count)` sends multiple words in one frame, far more efficient than N single-word calls.
- **Don't poll fast** — update icons/labels only when the value changes.
- **Memory** — each `DWIN_Curve` allocates `maxPoints × 2` bytes. Each image row buffer allocates `IMG_W × 2` (or `× 3` for BMP). On UNO/Nano, keep buffers ≤ 50 points per curve, images ≤ 32×32.
- **Prefer pre-loaded images** — `showImageById()` costs 8 bytes on the wire. Streaming a 64×64 image costs 8 KB. Use streaming only for genuinely dynamic content.
- **Server caching** — if your PHP server converts/resizes on every request, cache the output `.bin` on disk. Massive speedup.
- **Partial updates** — if only part of an image changed, stream only those rows.

**Streaming time reference:**

| Image size | Bytes | 115200 baud | 921600 baud |
|------------|-------|-------------|-------------|
| 32×32 | 2 KB | ~0.2 s | ~0.03 s |
| 64×64 | 8 KB | ~0.7 s | ~0.1 s |
| 100×100 | 20 KB | ~1.8 s | ~0.25 s |
| 128×128 | 32 KB | ~3 s | ~0.4 s |
| 160×120 | 38 KB | ~3.5 s | ~0.5 s |

---

## License

MIT License — free to use, modify, and distribute. See source headers for details.

---

## Contributing

Pull requests welcome. When reporting an issue, please include:

- Arduino board + core version
- DWIN panel model and firmware version
- Baud rate and wiring
- Minimal sketch that reproduces the problem
- Serial Monitor output (including raw hex if relevant)

---

*Built for the DWIN DGUS / T5L ecosystem. Tested on MEGA 2560, ESP32, and UNO (SoftwareSerial). Image streaming tested with SIM800L, SIM7600, and ESP32 Wi-Fi.*
