/*
  SDCardStream.ino - Stream an image from an SD card to a DWIN display

  What it does:
    - Reads a raw RGB565 .bin file (or a 24-bit .bmp) from SD
    - Streams it row by row into VP 0x4000 on the DWIN panel
    - Repeats every N seconds (configurable) or on Serial command

  Hardware:
    - Arduino MEGA / ESP32 / UNO
    - SD card module on SPI (CS pin configurable)
    - DWIN display on Serial1 (or SoftwareSerial)

  SD card file formats:
    A) Raw RGB565 binary (.bin)
         - Just pixel data, row-major, 2 bytes per pixel
         - Little-endian on SD (low byte first), byte-swapped in code
    B) 24-bit uncompressed BMP (.bmp)
         - Standard BMP, no palette, bottom-up rows (handled)

  DWIN project setup (DGUS Tool):
    Add a "Picture" / "Image Display" control on page 0:
      Data source VP = 0x4000
      Width, Height  = same as the image you're streaming
      Format         = RGB565 (16-bit)
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "DWIN_LCD.h"

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define USE_SOFTWARE_SERIAL 0        // 1 for UNO/Nano, 0 for MEGA/ESP32

const uint8_t  SD_CS       = 4;      // SD card chip-select pin
const uint16_t IMG_W       = 64;     // image width  in pixels
const uint16_t IMG_H       = 64;     // image height in pixels
const uint16_t VP_IMG      = 0x4000; // destination VP block

const char    *IMG_FILE    = "/image.bin";   // path on SD card
const bool     FILE_IS_BMP = false;          // true for .bmp, false for raw .bin

const uint32_t REFRESH_MS  = 10000;          // auto re-stream every 10 s

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
// Row buffer (allocated once, reused for every row)
// ---------------------------------------------------------------------------
uint16_t *rowBuf = nullptr;

// ---------------------------------------------------------------------------
// Stream a single row of RGB565 pixels to the panel
//   y   = row index (0 = top)
//   row = array of IMG_W pixels, already in RGB565 big-endian-correct form
// ---------------------------------------------------------------------------
bool streamRow(uint16_t y, const uint16_t *row) {
    return dwin.writeVP((uint16_t)(VP_IMG + y * IMG_W), row, IMG_W);
}

// ---------------------------------------------------------------------------
// Stream a raw RGB565 .bin file
//   - File layout: IMG_W * IMG_H * 2 bytes, row-major, little-endian
//   - We byte-swap each pixel to match DWIN's big-endian wire order
// ---------------------------------------------------------------------------
bool streamRawBin(File &f) {
    Serial.println(F("Streaming raw RGB565..."));

    uint32_t t0 = millis();

    for (uint16_t y = 0; y < IMG_H; y++) {
        // Read one row (IMG_W * 2 bytes) from SD
        int n = f.read((uint8_t *)rowBuf, IMG_W * 2);
        if (n != (int)(IMG_W * 2)) {
            Serial.print(F("Short read at row ")); Serial.println(y);
            return false;
        }

        // Byte-swap little-endian -> big-endian for DWIN
        for (uint16_t x = 0; x < IMG_W; x++) {
            uint16_t v = rowBuf[x];
            rowBuf[x] = (uint16_t)((v >> 8) | (v << 8));
        }

        // Push to the panel
        if (!streamRow(y, rowBuf)) {
            Serial.print(F("Write failed at row ")); Serial.println(y);
            return false;
        }
    }

    Serial.print(F("Done in ")); Serial.print(millis() - t0);
    Serial.println(F(" ms"));
    return true;
}

// ---------------------------------------------------------------------------
// Stream a 24-bit uncompressed BMP file
//   - BMP is bottom-up (last row first) -> we buffer and reverse per row
//   - BMP rows are padded to 4-byte boundaries
//   - Converts BGR (BMP) to RGB565 (DWIN)
// ---------------------------------------------------------------------------
bool streamBmp(File &f) {
    // ---- Read + validate the 54-byte header ----
    uint8_t hdr[54];
    if (f.read(hdr, 54) != 54) {
        Serial.println(F("BMP: header read failed"));
        return false;
    }
    if (hdr[0] != 'B' || hdr[1] != 'M') {
        Serial.println(F("BMP: not a BMP file"));
        return false;
    }

    uint32_t dataOffset = hdr[10] | (hdr[11] << 8) |
                          ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
    uint32_t fileW      = hdr[18] | (hdr[19] << 8) |
                          ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24);
    uint32_t fileH      = hdr[22] | (hdr[23] << 8) |
                          ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24);
    uint16_t bpp        = hdr[28] | (hdr[29] << 8);

    if (fileW != IMG_W || fileH != IMG_H) {
        Serial.print(F("BMP: size mismatch (file is "));
        Serial.print(fileW); Serial.print('x'); Serial.print(fileH);
        Serial.print(F(", expected "));
        Serial.print(IMG_W); Serial.print('x'); Serial.print(IMG_H);
        Serial.println(')');
        return false;
    }
    if (bpp != 24) {
        Serial.print(F("BMP: unsupported bpp = ")); Serial.println(bpp);
        return false;
    }

    // BMP rows are padded to a multiple of 4 bytes
    uint32_t rowBytes   = (fileW * 3 + 3) & ~3UL;   // padded
    uint32_t unpadded   = fileW * 3;

    Serial.println(F("Streaming 24-bit BMP..."));
    uint32_t t0 = millis();

    // Reuse rowBuf's memory as a byte scratch area (it's big enough)
    uint8_t *line = (uint8_t *)rowBuf;

    for (uint16_t displayY = 0; displayY < IMG_H; displayY++) {
        // BMP is stored bottom-up: file row 0 = image bottom row
        uint32_t fileRow = (uint32_t)(IMG_H - 1 - displayY);
        uint32_t offset  = dataOffset + fileRow * rowBytes;

        if (!f.seek(offset)) {
            Serial.print(F("BMP: seek failed at row ")); Serial.println(displayY);
            return false;
        }
        if (f.read(line, rowBytes) != (int)rowBytes) {
            Serial.print(F("BMP: short read at row ")); Serial.println(displayY);
            return false;
        }

        // Convert BGR888 -> RGB565
        for (uint16_t x = 0; x < IMG_W; x++) {
            uint8_t b = line[x * 3 + 0];
            uint8_t g = line[x * 3 + 1];
            uint8_t r = line[x * 3 + 2];
            rowBuf[x] = DWIN_RGB565(r, g, b);
        }

        if (!streamRow(displayY, rowBuf)) {
            Serial.print(F("Write failed at row ")); Serial.println(displayY);
            return false;
        }
    }

    Serial.print(F("Done in ")); Serial.print(millis() - t0);
    Serial.println(F(" ms"));
    return true;
}

// ---------------------------------------------------------------------------
// Open the file and dispatch to the correct streamer
// ---------------------------------------------------------------------------
bool streamImageFromSD(const char *path) {
    File f = SD.open(path);
    if (!f) {
        Serial.print(F("SD: cannot open ")); Serial.println(path);
        return false;
    }
    Serial.print(F("Opened ")); Serial.print(path);
    Serial.print(F(" (")); Serial.print(f.size()); Serial.println(F(" bytes)"));

    bool ok;
    if (FILE_IS_BMP) ok = streamBmp(f);
    else             ok = streamRawBin(f);

    f.close();
    return ok;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // ---- SD card ----
    if (!SD.begin(SD_CS)) {
        Serial.println(F("SD init failed - check wiring / card"));
        // Continue anyway; we can still talk to the display
    } else {
        Serial.println(F("SD card OK"));
    }

    dwin.begin(115200);
    dwin.setCRC(false);
    dwin.setWaitForAck(false);        // essential for streaming speed

    dwin.setBacklight(48);
    dwin.setPage(0);

    // Allocate the row buffer (worst case: IMG_W pixels * 3 bytes for BMP)
    uint16_t bufWords = (IMG_W * 3 + 1) / 2;   // bytes -> words, rounded up
    rowBuf = (uint16_t *)malloc(bufWords * sizeof(uint16_t));
    if (!rowBuf) {
        Serial.println(F("Out of memory for row buffer"));
        while (1) delay(1000);
    }

    delay(500);                       // let the panel finish booting

    // Stream once at startup
    streamImageFromSD(IMG_FILE);
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();

    // Re-stream every REFRESH_MS
    static uint32_t last = 0;
    if (millis() - last >= REFRESH_MS) {
        last = millis();
        streamImageFromSD(IMG_FILE);
    }

    // Manual re-stream on Serial command ('r')
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 'r' || c == 'R') {
            streamImageFromSD(IMG_FILE);
        }
    }
}