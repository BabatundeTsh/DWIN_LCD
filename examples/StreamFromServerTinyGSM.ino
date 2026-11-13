/*
  StreamFromServerTinyGSM.ino
  Fetch an image from a PHP server using a GSM modem (TinyGSM)
  and stream it to a DWIN display.

  Hardware:
    - ESP32 / MEGA / UNO
    - SIM800 / SIM7000 / SIM7600 / A7672X modem on a hardware serial port
    - DWIN display on a second hardware serial port

  Libraries (install via Library Manager):
    - TinyGSM            (vshymanskyy)
    - DWIN_LCD           (custom, this project)

  DWIN project setup (DGUS Tool):
    Add a "Picture" / "Image Display" control on page 0:
      Data source VP = 0x4000
      Width          = 64   (must match the PHP server's w)
      Height         = 64   (must match the PHP server's h)
      Format         = RGB565 (16-bit)
*/

#include <Arduino.h>

// ---- TinyGSM configuration ------------------------------------------------
#define TINY_GSM_MODEM_SIM800          // <-- change to your modem
// Other options: TINY_GSM_MODEM_SIM7000, TINY_GSM_MODEM_SIM7600,
//                TINY_GSM_MODEM_A7672X, TINY_GSM_MODEM_ESP32, etc.

#include <TinyGsmClient.h>

// ---- APN settings ---------------------------------------------------------
const char apn[]      = "internet";      // your carrier APN
const char gprsUser[] = "";
const char gprsPass[] = "";

// ---- Server URL -----------------------------------------------------------
const char SERVER_HOST[] = "yourserver.com";
const char SERVER_PATH[] = "/image_server.php?w=64&h=64&f=logo.png";
const uint16_t SERVER_PORT = 80;

// ---- Image geometry (must match the DGUS Picture control) -----------------
const uint16_t IMG_W  = 64;
const uint16_t IMG_H  = 64;
const uint16_t VP_IMG = 0x4000;

// ---- Serial port assignments ----------------------------------------------
// Adjust these to your board's free hardware serial ports.
#define SerialMon   Serial          // USB serial monitor
#define SerialGSM   Serial1         // modem
#define SerialDWIN  Serial2         // DWIN display

// ---- Baud rates -----------------------------------------------------------
const uint32_t GSM_BAUD  = 115200;
const uint32_t DWIN_BAUD = 115200;

// ---- DWIN ----------------------------------------------------------------
#include "DWIN_LCD.h"
DWIN_LCD dwin(SerialDWIN);

// ---- Modem ----------------------------------------------------------------
TinyGsm modem(SerialGSM);
TinyGsmClient gsmClient(modem);

// ---- Refresh rate ---------------------------------------------------------
const uint32_t REFRESH_MS = 15000;

// ---------------------------------------------------------------------------
// Connect to the cellular network
// ---------------------------------------------------------------------------
bool connectGsm() {
    SerialMon.println(F("Initialising modem..."));
    if (!modem.init()) {
        SerialMon.println(F("Modem init failed"));
        return false;
    }

    SerialMon.print(F("Waiting for network..."));
    if (!modem.waitForNetwork(60000L)) {
        SerialMon.println(F(" Network timeout"));
        return false;
    }
    SerialMon.println(F(" OK"));

    SerialMon.print(F("Connecting to APN: "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(F(" GPRS failed"));
        return false;
    }
    SerialMon.println(F(" GPRS OK"));

    SerialMon.print(F("Local IP: "));
    SerialMon.println(modem.localIP());
    return true;
}

// ---------------------------------------------------------------------------
// Fetch and stream the image
// ---------------------------------------------------------------------------
bool streamFromServer() {
    SerialMon.print(F("Connecting to "));
    SerialMon.print(SERVER_HOST);
    SerialMon.print(':');
    SerialMon.println(SERVER_PORT);

    if (!gsmClient.connect(SERVER_HOST, SERVER_PORT)) {
        SerialMon.println(F("TCP connect failed"));
        return false;
    }

    // ---- Send HTTP/1.0 GET (forces server to close after body) ----
    gsmClient.print(F("GET "));
    gsmClient.print(SERVER_PATH);
    gsmClient.println(F(" HTTP/1.0"));
    gsmClient.print(F("Host: "));
    gsmClient.println(SERVER_HOST);
    gsmClient.println(F("Connection: close"));
    gsmClient.println();

    // ---- Read status line ----
    uint32_t t0 = millis();
    while (gsmClient.available() < 12 && millis() - t0 < 5000) delay(10);
    if (gsmClient.available() < 12) {
        SerialMon.println(F("No HTTP response"));
        gsmClient.stop();
        return false;
    }

    char statusLine[32] = {0};
    gsmClient.readBytesUntil('\n', statusLine, sizeof(statusLine) - 1);
    SerialMon.print(F("HTTP status: "));
    SerialMon.println(statusLine);
    if (strstr(statusLine, "200") == nullptr) {
        SerialMon.println(F("Non-200 response, aborting"));
        gsmClient.stop();
        return false;
    }

    // ---- Skip HTTP headers (until empty line) ----
    while (gsmClient.connected()) {
        String line = gsmClient.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) break;
    }

    // ---- Allocate row buffer ----
    uint16_t *row = (uint16_t *)malloc(IMG_W * sizeof(uint16_t));
    if (!row) {
        SerialMon.println(F("Out of memory"));
        gsmClient.stop();
        return false;
    }

    SerialMon.println(F("Streaming..."));
    uint32_t streamStart = millis();

    for (uint16_t y = 0; y < IMG_H; y++) {
        // Wait for a full row of bytes
        uint32_t rowStart = millis();
        while (gsmClient.available() < (int)(IMG_W * 2)) {
            if (millis() - rowStart > 5000) {
                SerialMon.print(F("Timeout at row "));
                SerialMon.println(y);
                free(row);
                gsmClient.stop();
                return false;
            }
            delay(5);
        }

        gsmClient.readBytes((uint8_t *)row, IMG_W * 2);

        // Server sends little-endian; DWIN expects big-endian words.
        for (uint16_t x = 0; x < IMG_W; x++) {
            uint16_t v = row[x];
            row[x] = (uint16_t)((v >> 8) | (v << 8));
        }

        // Push this row to the panel
        if (!dwin.writeVP((uint16_t)(VP_IMG + y * IMG_W), row, IMG_W)) {
            SerialMon.print(F("DWIN write failed at row "));
            SerialMon.println(y);
            free(row);
            gsmClient.stop();
            return false;
        }

        if ((y & 7) == 7) SerialMon.print('.');
    }

    free(row);
    gsmClient.stop();

    SerialMon.println();
    SerialMon.print(F("Done in "));
    SerialMon.print(millis() - streamStart);
    SerialMon.println(F(" ms"));
    return true;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    SerialMon.begin(115200);
    delay(500);

    // ---- DWIN UART ----
    SerialDWIN.begin(DWIN_BAUD);
    dwin.begin(DWIN_BAUD);
    dwin.setCRC(false);
    dwin.setWaitForAck(false);          // essential for streaming speed
    dwin.setBacklight(48);
    dwin.setPage(0);

    // ---- GSM UART ----
    SerialGSM.begin(GSM_BAUD);
    delay(200);

    if (!connectGsm()) {
        SerialMon.println(F("GSM init failed — halting."));
        while (true) delay(1000);
    }

    // ---- First fetch ----
    delay(500);
    streamFromServer();
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    dwin.process();                     // touch events still work

    static uint32_t last = 0;
    if (millis() - last >= REFRESH_MS) {
        last = millis();
        streamFromServer();
    }

    // Re‑fetch on serial 'r'
    if (SerialMon.available()) {
        char c = SerialMon.read();
        if (c == 'r' || c == 'R') {
            SerialMon.println(F("Manual refresh"));
            streamFromServer();
        }
    }
}