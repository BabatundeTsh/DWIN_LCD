/*
  DWIN_LCD.h - Arduino library for DWIN DGUS/T5L HMI displays
  Native protocol: 0x80 (write reg), 0x81 (read reg),
                   0x82 (write VP), 0x83 (read VP).
  Optional CRC16-CCITT on TX.
  Touch events delivered via user callback.
  Text color + single/multi-curve helpers included.
  License: MIT
*/

#ifndef DWIN_LCD_H
#define DWIN_LCD_H

#include <Arduino.h>

// ---------- Protocol constants ----------
#define DWIN_HEADER_H 0x5A
#define DWIN_HEADER_L 0xA5

#define DWIN_CMD_WRITE_REG 0x80
#define DWIN_CMD_READ_REG  0x81
#define DWIN_CMD_WRITE_VP  0x82
#define DWIN_CMD_READ_VP   0x83

#define DWIN_ACK_OK        0x4F4B  // "OK"
#define DWIN_ACK_ERR       0x4E47  // "NG"

// ---------- RGB565 color helpers ----------
#define DWIN_RGB565(r8, g8, b8) \
    ( (uint16_t)((((uint16_t)(r8) & 0xF8) << 8) | \
                 (((uint16_t)(g8) & 0xFC) << 3) | \
                  ((uint16_t)(b8) >> 3)) )

#define DWIN_RGB565_RAW(r5, g6, b5) \
    ( (uint16_t)((((uint16_t)(r5) & 0x1F) << 11) | \
                 (((uint16_t)(g6) & 0x3F) << 5)  | \
                  ((uint16_t)(b5) & 0x1F)) )

// Named colors (RGB565)
#define DWIN_BLACK      0x0000
#define DWIN_WHITE      0xFFFF
#define DWIN_RED        0xF800
#define DWIN_GREEN      0x07E0
#define DWIN_BLUE       0x001F
#define DWIN_YELLOW     0xFFE0
#define DWIN_CYAN       0x07FF
#define DWIN_MAGENTA    0xF81F
#define DWIN_ORANGE     0xFD20
#define DWIN_GRAY       0x8410
#define DWIN_LIGHTGRAY  0xC618
#define DWIN_DARKGRAY   0x4208
#define DWIN_BROWN      0xA145
#define DWIN_PINK       0xF81F
#define DWIN_PURPLE     0x781F
#define DWIN_LIME       0x07E0
#define DWIN_NAVY       0x000F
#define DWIN_TEAL       0x0410
#define DWIN_GOLD       0xFEA0
#define DWIN_SILVER     0xC618

// ---------- Callback typedef ----------
typedef void (*DWINTouchCallback)(uint16_t vp, uint16_t value);

// ===========================================================================
// DWIN_LCD
// ===========================================================================
class DWIN_LCD {
public:
    DWIN_LCD(Stream &serial, long baud = 9600);

    // Store baud rate. The sketch must call SerialX.begin(baud) itself.
    void begin(long baud = 9600);

    void setCRC(bool enable);
    void setWaitForAck(bool enable);

    // ---------- Touch callback ----------
    void attachTouchCallback(DWINTouchCallback cb);
    void process();                       // call frequently (loop / serialEvent)
    inline void loop() { process(); }     // alias

    // ---------- Register R/W ----------
    bool    writeRegister(uint8_t page, uint8_t reg, const uint8_t *data, uint8_t len);
    bool    writeRegister(uint8_t page, uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t page, uint8_t reg, uint8_t len, uint8_t *out);

    // ---------- VP R/W ----------
    bool    writeVP(uint16_t vp, const uint16_t *data, uint16_t len);
    bool    writeVP(uint16_t vp, uint16_t value);
    uint8_t readVP(uint16_t vp, uint16_t len, uint16_t *out);

    // ---------- High-level helpers ----------
    bool setBacklight(uint8_t level);
    bool setPage(uint16_t page);
    bool writeString(uint16_t vp, const char *str);
    bool writeNumber(uint16_t vp, int32_t value);
    bool writeNumber(uint16_t vp, uint32_t value);
    bool writeFloat(uint16_t vp, float value);

    // ---------- Text color ----------
    bool setTextColor(uint16_t colorVp, uint16_t color565);
    bool setTextColorRGB(uint16_t colorVp, uint8_t r, uint8_t g, uint8_t b);
    bool setTextColor565(uint16_t colorVp, uint8_t r5, uint8_t g6, uint8_t b5);
    bool writeStringColored(uint16_t strVp, uint16_t colorVp,
                            const char *str, uint16_t color565);
    void fadeTextColor(uint16_t colorVp, uint16_t fromColor, uint16_t toColor,
                       uint16_t durationMs, uint16_t steps = 16);

    // ---------- Curve primitives (used by DWIN_Curve / DWIN_MultiCurve) ----------
    bool curveAddPoint(uint16_t curveVp, uint16_t value);
    bool curveAddPoint8(uint16_t curveVp, uint8_t value);
    bool curveAddPoints(uint16_t curveVp, const uint16_t *values, uint16_t count);
    bool curveAddPoints8(uint16_t curveVp, const uint8_t *values, uint16_t count);
    bool curveClear(uint16_t curveVp, uint16_t pointCount);
    bool curveSetRange(uint16_t minVp, uint16_t maxVp,
                       uint16_t minValue, uint16_t maxValue);

    void flushInput();

    // ---------- Low-level (public so DWIN_Curve / DWIN_MultiCurve can use them) ----------
    void sendFrame(uint8_t cmd, const uint8_t *payload, uint16_t payloadLen);
    bool waitForAck(uint16_t expectedAck = DWIN_ACK_OK, uint32_t timeout = 200);
    bool writeVPChunk(uint16_t vp, const uint16_t *data, uint16_t wordCount);

private:
    Stream *_serial;
    long    _baud;
    bool    _crcEnabled;
    bool    _waitAck;

    // Persistent RX state machine
    enum RxState { RX_IDLE, RX_HDR_L, RX_LEN, RX_CMD, RX_DATA };
    RxState   _rxState;
    uint16_t  _rxByteCount;
    uint16_t  _rxIdx;
    uint8_t   _rxCmd;
    uint8_t   _rxBuf[256];

    DWINTouchCallback _touchCb;

    uint16_t crc16(const uint8_t *data, uint16_t len);

    uint16_t readFrame(uint8_t *cmd, uint8_t *payload, uint16_t maxPayload,
                       uint32_t timeout = 200);

    void dispatchFrame(uint8_t cmd, const uint8_t *payload, uint16_t len);
};

// ===========================================================================
// DWIN_Curve - single rolling-window curve channel
// ===========================================================================
class DWIN_Curve {
public:
    DWIN_Curve(DWIN_LCD &dwin, uint16_t curveVp, uint16_t maxPoints);
    ~DWIN_Curve();

    // Allocate buffer. Call once in setup().
    void begin();

    // ---- Standard append: buffers + redraws (or sends latest point) ----
    void push(uint16_t value);
    void pushScaled(float rawValue);

    // ---- Silent buffering (used by DWIN_MultiCurve for coordinated redraw) ----
    void pushSilent(uint16_t value);
    void pushScaledSilent(float rawValue);
    void sendLatest();                 // transmit only the newest point

    // ---- Configuration ----
    void setAutoRedraw(bool enable) { _autoRedraw = enable; }
    void set8Bit(bool enable)       { _eightBit   = enable; }
    void setScale(float gain, float offset) { _gain = gain; _offset = offset; }

    // ---- Whole-curve operations ----
    void redraw();
    void clear();

    // ---- Accessors ----
    uint16_t count()      const { return _count; }
    uint16_t point(uint16_t i) const;
    uint16_t latest()     const;
    uint16_t minValue()   const { return _min; }
    uint16_t maxValue()   const { return _maxVal; }
    uint16_t vp()         const { return _vp; }

    void recomputeStats();

private:
    DWIN_LCD &_dwin;
    uint16_t  _vp;
    uint16_t  _maxPoints;
    uint16_t  _count;
    uint16_t  _head;
    uint16_t *_buf;
    uint16_t  _min;
    uint16_t  _maxVal;
    bool      _autoRedraw;
    bool      _eightBit;
    float     _gain;
    float     _offset;

    void     redrawLinear();
    uint16_t scaleValue(float raw) const;
};

// ===========================================================================
// DWIN_MultiCurve - manager for N curves sharing one timebase
// ===========================================================================
class DWIN_MultiCurve {
public:
    DWIN_MultiCurve(DWIN_LCD &dwin, uint8_t maxCurves, uint16_t maxPoints);
    ~DWIN_MultiCurve();

    // Allocate the pointer array. Call once in setup().
    void begin();

    // Register a new curve. Returns its index (0..N-1) or -1 on failure.
    int8_t addCurve(uint16_t curveVp, bool eightBit = false);

    uint8_t     curveCount() const { return _count; }
    DWIN_Curve* curve(uint8_t idx);       // nullptr if out of range

    // If true (default): pushSample() redraws every curve once.
    // If false: only the newest point per curve is transmitted (real-time
    // scroll mode; much lighter on the serial link).
    void setAutoRedraw(bool enable);

    // Push one new sample to every curve (values in registration order).
    void pushSample(const uint16_t *values);
    void pushSampleScaled(const float *rawValues);

    // Push to a single curve by index.
    void pushOne(uint8_t idx, uint16_t value);

    // Whole-group operations.
    void redrawAll();
    void clearAll();

private:
    DWIN_LCD   &_dwin;
    uint8_t     _maxCurves;
    uint8_t     _count;
    uint16_t    _maxPoints;
    DWIN_Curve **_curves;
    bool        _autoRedraw;
};

#endif // DWIN_LCD_H