/*
  StreamImage.ino - Stream a raw RGB565 image to a DWIN display

  What it does:
    - Streams a 32x32 RGB565 image (2048 bytes) into VP 0x4000
    - The image is generated procedurally (color gradient + border)
    - Repeats every 5 seconds

  DWIN project setup (DGUS Tool):
    Add a "Picture" / "Image Display" control on page 0.
      Data source VP = 0x4000
      Width          = 32 pixels
      Height         = 32 pixels
      Format         = RGB565 (16-bit)

  Performance:
    - 32x32 RGB565 = 2048 bytes = 1024 VP words
    - At 9600 baud : ~2.2 s to transfer
    - At 115200 baud: ~0.2 s to transfer  <-- recommended

  To display a real image:
    1) Convert your BMP/JPG to a C array with an online tool
       (search: "image to RGB565 C array")
       or use the Python snippet at the bottom of this file.
    2) Replace generateRow() with a lookup into your array.
*/

#include <Arduino.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint16_t IMG_W  = 32;         // image width  in pixels
const uint16_t IMG_H  = 32;         // image height in pixels
const uint16_t VP_IMG = 0x4000;     // destination VP block

const uint32_t REFRESH_MS = 5000;   // re-stream every 5 seconds

// ---------------------------------------------------------------------------
// Serial selection
// ---------------------------------------------------------------------------
#if USE_SOFTWARE_SERIAL
  #include <SoftwareSerial.h>
  SoftwareSerial dwinSerial(10, 11);   // RX=10, TX=11
  DWIN_LCD dwin(dwinSerial);
#else
  DWIN_LCD dwin(Serial1);
#endif

// ---------------------------------------------------------------------------
// Image source
// ---------------------------------------------------------------------------
// Fill a row buffer with one row of the image (y = row index, 0..IMG_H-1).
// Each entry is one RGB565 pixel. Replace this function with a lookup into
// your own PROGMEM array if you have a real image.
void generateRow(uint16_t y, uint16_t *row) {
    for (uint16_t x = 0; x < IMG_W; x++) {
        // White 1-pixel border
        if (x == 0 || y == 0 || x == IMG_W - 1 || y == IMG_H - 1) {
            row[x] = DWIN_WHITE;
            continue;
        }
        // Diagonal gradient across the interior
        uint8_t r = (uint8_t)(x * 8);           // 0..240
        uint8_t g = (uint8_t)(y * 8);           // 0..240
        uint8_t b = (uint8_t)((x + y) * 4);     // 0..248
        row[x] = DWIN_RGB565(r, g, b);
    }
}

// ---------------------------------------------------------------------------
// Streaming core
// ---------------------------------------------------------------------------
// Streams a full image to the panel, one row at a time.
// Uses writeVP() under the hood, which chunks at 120 words per frame.
//
// Parameters:
//   vp      - destination VP (top-left pixel = vp, row-major order)
//   width   - image width  in pixels
//   height  - image height in pixels
//
// Returns true on success.
bool streamImage(uint16_t vp, uint16_t width, uint16_t height) {
    if (width == 0 || height == 0) return false;
    if (width > 120) {
        // writeVP() chunks internally, so wider rows are fine - but a
        // row is at most 240 bytes on the wire per frame, so wider rows
        // simply take more frames. We keep a full row buffer anyway.
    }

    uint16_t *row = (uint16_t *)malloc(width * sizeof(uint16_t));
    if (!row) {
        Serial.println(F("streamImage: out of memory"));
        return false;
    }

    Serial.print(F("Streaming "));
    Serial.print(width);
    Serial.print('x');
    Serial.print(height);
    Serial.print(F(" image to VP 0x"));
    Serial.println(vp, HEX);

    uint32_t t0 = millis();

    for (uint16_t y = 0; y < height; y++) {
        // 1) Fill the row buffer (replace with a PROGMEM lookup if desired)
        generateRow(y, row);

        // 2) Send the row to the panel.
        //    writeVP() automatically splits rows wider than 120 words.
        if (!dwin.writeVP((uint16_t)(vp + y * width), row, width)) {
            Serial.print(F("streamImage: write failed at row "));
            Serial.println(y);
            free(row);
            return false;
        }

        // 3) Optional progress bar every ~10%
        if (height >= 10 && (y % (height / 10) == 0)) {
            uint8_t pct = (uint8_t)((y * 100UL) / height);
            Serial.print(pct);
            Serial.println('%');
        }
    }

    free(row);

    uint32_t dt = millis() - t0;
    Serial.print(F("Done in "));
    Serial.print(dt);
    Serial.println(F(" ms"));
    return true;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    dwin.begin(115200);
    dwin.setCRC(false);
    dwin.setWaitForAck(false);        // ESSENTIAL: skip ACKs for speed

    dwin.setBacklight(48);
    dwin.setPage(0);

    delay(500);                       // let the panel finish booting

    // Stream the first frame
    streamImage(VP_IMG, IMG_W, IMG_H);
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();                   // touch events still work

    static uint32_t last = 0;
    if (millis() - last >= REFRESH_MS) {
        last = millis();
        streamImage(VP_IMG, IMG_W, IMG_H);
    }
}