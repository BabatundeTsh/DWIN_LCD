#include "DWIN_LCD.h"
#include <string.h>
#include <stdlib.h>

// ===========================================================================
// DWIN_LCD
// ===========================================================================
DWIN_LCD::DWIN_LCD(Stream &serial, long baud)
    : _serial(&serial),
      _baud(baud),
      _crcEnabled(false),
      _waitAck(true),
      _rxState(RX_IDLE),
      _rxByteCount(0),
      _rxIdx(0),
      _rxCmd(0),
      _touchCb(nullptr)
{
    memset(_rxBuf, 0, sizeof(_rxBuf));
}

void DWIN_LCD::begin(long baud) {
    _baud = baud;
    _serial.begin( _baud );
    // The user must call SerialX.begin(baud) themselves.
}

void DWIN_LCD::setCRC(bool enable)        { _crcEnabled = enable; }
void DWIN_LCD::setWaitForAck(bool enable) { _waitAck    = enable; }
void DWIN_LCD::attachTouchCallback(DWINTouchCallback cb) { _touchCb = cb; }

// ---------------- CRC16-CCITT ----------------
uint16_t DWIN_LCD::crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
            else              crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

// ---------------- TX ----------------
void DWIN_LCD::sendFrame(uint8_t cmd, const uint8_t *payload, uint16_t payloadLen) {
    uint16_t byteCount = (uint16_t)(1 + payloadLen);
    if (_crcEnabled) byteCount += 2;

    _serial->write(DWIN_HEADER_H);
    _serial->write(DWIN_HEADER_L);
    _serial->write((uint8_t)(byteCount & 0xFF));
    _serial->write(cmd);
    if (payloadLen > 0 && payload) _serial->write(payload, payloadLen);

    if (_crcEnabled) {
        uint16_t totalLen = (uint16_t)(1 + payloadLen);
        uint8_t *tmp = (uint8_t *)malloc(totalLen);
        if (tmp) {
            tmp[0] = cmd;
            if (payloadLen > 0 && payload) memcpy(tmp + 1, payload, payloadLen);
            uint16_t crc = crc16(tmp, totalLen);
            free(tmp);
            _serial->write((uint8_t)(crc >> 8));
            _serial->write((uint8_t)(crc & 0xFF));
        }
    }
}

// ---------------- Blocking RX (sync reads) ----------------
uint16_t DWIN_LCD::readFrame(uint8_t *cmd, uint8_t *payload, uint16_t maxPayload,
                             uint32_t timeout) {
    uint32_t start = millis();
    uint8_t  state = 0;
    uint16_t byteCount = 0;
    uint16_t idx = 0;

    while ((uint32_t)(millis() - start) < timeout) {
        if (!_serial->available()) continue;
        uint8_t b = (uint8_t)_serial->read();

        switch (state) {
            case 0:
                if (b == DWIN_HEADER_H) state = 1;
                break;
            case 1:
                if (b == DWIN_HEADER_L)      state = 2;
                else if (b == DWIN_HEADER_H) state = 1;
                else                         state = 0;
                break;
            case 2:
                byteCount = b;
                idx = 0;
                state = 3;
                break;
            case 3:
                *cmd = b;
                if (byteCount == 0) return 0;
                byteCount--;
                if (byteCount == 0) return 0;
                state = 4;
                break;
            case 4:
                if (idx < maxPayload) payload[idx++] = b;
                if (byteCount > 0) byteCount--;
                if (byteCount == 0) return idx;
                break;
        }
    }
    return 0;
}

bool DWIN_LCD::waitForAck(uint16_t expectedAck, uint32_t timeout) {
    if (!_waitAck) return true;

    uint8_t cmd = 0;
    uint8_t buf[16];
    uint16_t n = readFrame(&cmd, buf, sizeof(buf), timeout);
    if (n < 2) return false;

    for (uint16_t i = 0; i + 1 < n; i++) {
        uint16_t v = ((uint16_t)buf[i] << 8) | buf[i + 1];
        if (v == expectedAck) return true;
    }
    return false;
}

// ---------------- Non-blocking RX ----------------
void DWIN_LCD::process() {
    while (_serial->available()) {
        uint8_t b = (uint8_t)_serial->read();

        switch (_rxState) {
            case RX_IDLE:
                if (b == DWIN_HEADER_H) _rxState = RX_HDR_L;
                break;

            case RX_HDR_L:
                if (b == DWIN_HEADER_L)      _rxState = RX_LEN;
                else if (b == DWIN_HEADER_H) _rxState = RX_HDR_L;
                else                         _rxState = RX_IDLE;
                break;

            case RX_LEN:
                _rxByteCount = b;
                _rxIdx = 0;
                _rxState = (_rxByteCount == 0) ? RX_IDLE : RX_CMD;
                break;

            case RX_CMD:
                _rxCmd = b;
                _rxByteCount--;
                if (_rxByteCount == 0) {
                    dispatchFrame(_rxCmd, _rxBuf, 0);
                    _rxState = RX_IDLE;
                } else {
                    _rxState = RX_DATA;
                }
                break;

            case RX_DATA:
                if (_rxIdx < sizeof(_rxBuf)) _rxBuf[_rxIdx++] = b;
                _rxByteCount--;
                if (_rxByteCount == 0) {
                    uint16_t dataLen = _rxIdx;
                    if (_crcEnabled && dataLen >= 2) dataLen -= 2;
                    dispatchFrame(_rxCmd, _rxBuf, dataLen);
                    _rxState = RX_IDLE;
                }
                break;

            default:
                _rxState = RX_IDLE;
                break;
        }
    }
}

// Handles both DWIN auto-upload (5-byte payload) and standard read
// response (>= 6 bytes) formats for command 0x83.
void DWIN_LCD::dispatchFrame(uint8_t cmd, const uint8_t *payload, uint16_t len) {
    if (cmd != DWIN_CMD_READ_VP) return;
    if (len < 5) return;

    uint16_t vp = ((uint16_t)payload[0] << 8) | payload[1];

    if (len == 5) {
        // Auto-upload format: VP(2) + BYTELEN(1) + VALUE(2)
        uint16_t value = ((uint16_t)payload[3] << 8) | payload[4];
        if (_touchCb) _touchCb(vp, value);
    } else {
        // Standard read response: VP(2) + WORDCOUNT(2) + DATA(2*N)
        uint16_t words = ((uint16_t)payload[2] << 8) | payload[3];
        if (words >= 1) {
            uint16_t value = ((uint16_t)payload[4] << 8) | payload[5];
            if (_touchCb) _touchCb(vp, value);
        }
    }
}

// ---------------- Register R/W ----------------
bool DWIN_LCD::writeRegister(uint8_t page, uint8_t reg, const uint8_t *data, uint8_t len) {
    if (len > 250) return false;
    uint8_t payload[2 + 250];
    payload[0] = page;
    payload[1] = reg;
    if (len > 0 && data) memcpy(payload + 2, data, len);
    sendFrame(DWIN_CMD_WRITE_REG, payload, (uint16_t)(2 + len));
    return waitForAck();
}

bool DWIN_LCD::writeRegister(uint8_t page, uint8_t reg, uint8_t value) {
    return writeRegister(page, reg, &value, 1);
}

uint8_t DWIN_LCD::readRegister(uint8_t page, uint8_t reg, uint8_t len, uint8_t *out) {
    uint8_t payload[3] = { page, reg, len };
    sendFrame(DWIN_CMD_READ_REG, payload, 3);

    uint8_t  cmd = 0;
    uint8_t  buf[128];
    uint16_t n = readFrame(&cmd, buf, sizeof(buf));
    if (n < 3) return 0;

    uint8_t nReg = buf[2];
    uint8_t copy = (nReg > len) ? len : nReg;
    if (copy > 0 && out) memcpy(out, buf + 3, copy);
    return copy;
}

// ---------------- VP R/W ----------------
bool DWIN_LCD::writeVPChunk(uint16_t vp, const uint16_t *data, uint16_t wordCount) {
    if (wordCount == 0 || wordCount > 120 || !data) return false;

    uint8_t payload[4 + 240];
    payload[0] = (uint8_t)(vp >> 8);
    payload[1] = (uint8_t)(vp & 0xFF);
    payload[2] = (uint8_t)(wordCount >> 8);
    payload[3] = (uint8_t)(wordCount & 0xFF);
    for (uint16_t i = 0; i < wordCount; i++) {
        payload[4 + i * 2]     = (uint8_t)(data[i] >> 8);
        payload[4 + i * 2 + 1] = (uint8_t)(data[i] & 0xFF);
    }
    sendFrame(DWIN_CMD_WRITE_VP, payload, (uint16_t)(4 + wordCount * 2));
    return waitForAck();
}

bool DWIN_LCD::writeVP(uint16_t vp, const uint16_t *data, uint16_t len) {
    if (len == 0) return true;
    if (!data) return false;

    const uint16_t MAX_WORDS = 120;
    uint16_t offset = 0;
    while (offset < len) {
        uint16_t remain = len - offset;
        uint16_t chunk  = (remain > MAX_WORDS) ? MAX_WORDS : remain;
        if (!writeVPChunk((uint16_t)(vp + offset), data + offset, chunk)) return false;
        offset += chunk;
    }
    return true;
}

bool DWIN_LCD::writeVP(uint16_t vp, uint16_t value) {
    return writeVPChunk(vp, &value, 1);
}

uint8_t DWIN_LCD::readVP(uint16_t vp, uint16_t len, uint16_t *out) {
    uint8_t payload[4];
    payload[0] = (uint8_t)(vp >> 8);
    payload[1] = (uint8_t)(vp & 0xFF);
    payload[2] = (uint8_t)(len >> 8);
    payload[3] = (uint8_t)(len & 0xFF);
    sendFrame(DWIN_CMD_READ_VP, payload, 4);

    uint8_t  cmd = 0;
    uint8_t  buf[128];
    uint16_t n = readFrame(&cmd, buf, sizeof(buf));
    if (n < 4) return 0;

    uint16_t words = ((uint16_t)buf[2] << 8) | buf[3];
    uint16_t copy  = (words > len) ? len : words;
    if (copy > 62) copy = 62;   // buf holds at most 62 words after header

    for (uint16_t i = 0; i < copy; i++) {
        uint16_t hi = buf[4 + i * 2];
        uint16_t lo = buf[4 + i * 2 + 1];
        out[i] = (uint16_t)((hi << 8) | lo);
    }
    return (uint8_t)copy;
}

// ---------------- High-level helpers ----------------
bool DWIN_LCD::setBacklight(uint8_t level) {
    return writeRegister(0x00, 0x01, level);
}

bool DWIN_LCD::setPage(uint16_t page) {
    return writeVP(0x0084, page);
}

bool DWIN_LCD::writeString(uint16_t vp, const char *str) {
    if (!str) return false;
    uint16_t len = (uint16_t)strlen(str);
    if (len > 128) len = 128;

    uint16_t words[64];
    uint8_t  wordCount = (uint8_t)((len + 1) / 2);
    for (uint8_t i = 0; i < wordCount; i++) {
        uint8_t lo = (i * 2     < len) ? (uint8_t)str[i * 2]     : 0;
        uint8_t hi = (i * 2 + 1 < len) ? (uint8_t)str[i * 2 + 1] : 0;
        words[i] = (uint16_t)(((uint16_t)hi << 8) | lo);
    }
    return writeVP(vp, words, wordCount);
}

bool DWIN_LCD::writeNumber(uint16_t vp, int32_t value) {
    uint32_t u = (uint32_t)value;
    uint16_t w[2] = { (uint16_t)(u >> 16), (uint16_t)(u & 0xFFFF) };
    return writeVP(vp, w, 2);
}

bool DWIN_LCD::writeNumber(uint16_t vp, uint32_t value) {
    uint16_t w[2] = { (uint16_t)(value >> 16), (uint16_t)(value & 0xFFFF) };
    return writeVP(vp, w, 2);
}

bool DWIN_LCD::writeFloat(uint16_t vp, float value) {
    union { float f; uint32_t u; } conv;
    conv.f = value;
    uint16_t w[2] = { (uint16_t)(conv.u >> 16), (uint16_t)(conv.u & 0xFFFF) };
    return writeVP(vp, w, 2);
}

// ---------------- Text color ----------------
bool DWIN_LCD::setTextColor(uint16_t colorVp, uint16_t color565) {
    return writeVP(colorVp, color565);
}

bool DWIN_LCD::setTextColorRGB(uint16_t colorVp, uint8_t r, uint8_t g, uint8_t b) {
    return writeVP(colorVp, DWIN_RGB565(r, g, b));
}

bool DWIN_LCD::setTextColor565(uint16_t colorVp, uint8_t r5, uint8_t g6, uint8_t b5) {
    return writeVP(colorVp, DWIN_RGB565_RAW(r5, g6, b5));
}

bool DWIN_LCD::writeStringColored(uint16_t strVp, uint16_t colorVp,
                                  const char *str, uint16_t color565) {
    bool ok1 = writeString(strVp, str);
    bool ok2 = writeVP(colorVp, color565);
    return ok1 && ok2;
}

void DWIN_LCD::fadeTextColor(uint16_t colorVp, uint16_t fromColor, uint16_t toColor,
                             uint16_t durationMs, uint16_t steps) {
    if (steps < 2) steps = 2;

    uint8_t r0 = (fromColor >> 11) & 0x1F;
    uint8_t g0 = (fromColor >> 5)  & 0x3F;
    uint8_t b0 =  fromColor        & 0x1F;
    uint8_t r1 = (toColor >> 11)   & 0x1F;
    uint8_t g1 = (toColor >> 5)    & 0x3F;
    uint8_t b1 =  toColor          & 0x1F;

    uint16_t stepDelay = durationMs / steps;

    for (uint16_t i = 0; i <= steps; i++) {
        int16_t dr = (int16_t)r1 - (int16_t)r0;
        int16_t dg = (int16_t)g1 - (int16_t)g0;
        int16_t db = (int16_t)b1 - (int16_t)b0;

        uint8_t r = (uint8_t)((int16_t)r0 + (dr * (int16_t)i) / (int16_t)steps);
        uint8_t g = (uint8_t)((int16_t)g0 + (dg * (int16_t)i) / (int16_t)steps);
        uint8_t b = (uint8_t)((int16_t)b0 + (db * (int16_t)i) / (int16_t)steps);

        setTextColor565(colorVp, r, g, b);
        if (i < steps) delay(stepDelay);
    }
}

// ---------------- Curve primitives ----------------
bool DWIN_LCD::curveAddPoint(uint16_t curveVp, uint16_t value) {
    return writeVP(curveVp, value);
}

bool DWIN_LCD::curveAddPoint8(uint16_t curveVp, uint8_t value) {
    return writeVP(curveVp, (uint16_t)value);
}

bool DWIN_LCD::curveAddPoints(uint16_t curveVp, const uint16_t *values, uint16_t count) {
    return writeVP(curveVp, values, count);
}

bool DWIN_LCD::curveAddPoints8(uint16_t curveVp, const uint8_t *values, uint16_t count) {
    if (count == 0) return true;
    if (!values)    return false;

    const uint16_t MAX_CHUNK = 120;
    uint16_t offset = 0;
    while (offset < count) {
        uint16_t remain = count - offset;
        uint16_t chunk  = (remain > MAX_CHUNK) ? MAX_CHUNK : remain;
        uint16_t tmp[MAX_CHUNK];
        for (uint16_t i = 0; i < chunk; i++) tmp[i] = (uint16_t)values[offset + i];
        if (!writeVPChunk((uint16_t)(curveVp + offset), tmp, chunk)) return false;
        offset += chunk;
    }
    return true;
}

bool DWIN_LCD::curveClear(uint16_t curveVp, uint16_t pointCount) {
    const uint16_t MAX_CHUNK = 120;
    uint16_t zeros[MAX_CHUNK];
    memset(zeros, 0, sizeof(zeros));

    uint16_t remaining = pointCount;
    uint16_t offset    = 0;
    while (remaining > 0) {
        uint16_t chunk = (remaining > MAX_CHUNK) ? MAX_CHUNK : remaining;
        if (!writeVPChunk((uint16_t)(curveVp + offset), zeros, chunk)) return false;
        offset    += chunk;
        remaining -= chunk;
    }
    return true;
}

bool DWIN_LCD::curveSetRange(uint16_t minVp, uint16_t maxVp,
                             uint16_t minValue, uint16_t maxValue) {
    bool ok1 = writeVP(minVp, minValue);
    bool ok2 = writeVP(maxVp, maxValue);
    return ok1 && ok2;
}

void DWIN_LCD::flushInput() {
    while (_serial->available()) (void)_serial->read();
    _rxState     = RX_IDLE;
    _rxIdx       = 0;
    _rxByteCount = 0;
}

// ===========================================================================
// DWIN_Curve
// ===========================================================================
DWIN_Curve::DWIN_Curve(DWIN_LCD &dwin, uint16_t curveVp, uint16_t maxPoints)
    : _dwin(dwin),
      _vp(curveVp),
      _maxPoints(maxPoints),
      _count(0),
      _head(0),
      _buf(nullptr),
      _min(0),
      _maxVal(0),
      _autoRedraw(true),
      _eightBit(false),
      _gain(1.0f),
      _offset(0.0f)
{
}

DWIN_Curve::~DWIN_Curve() {
    if (_buf) {
        free(_buf);
        _buf = nullptr;
    }
}

void DWIN_Curve::begin() {
    if (_buf || _maxPoints == 0) return;
    _buf = (uint16_t *)malloc((size_t)_maxPoints * sizeof(uint16_t));
    if (!_buf) {
        _maxPoints = 0;
        return;
    }
    memset(_buf, 0, (size_t)_maxPoints * sizeof(uint16_t));
    _count  = 0;
    _head   = 0;
    _min    = 0;
    _maxVal = 0;
}

uint16_t DWIN_Curve::point(uint16_t i) const {
    if (!_buf || i >= _count || _maxPoints == 0) return 0;
    uint16_t oldest = (uint16_t)((_head + _maxPoints - _count) % _maxPoints);
    return _buf[(oldest + i) % _maxPoints];
}

uint16_t DWIN_Curve::latest() const {
    if (!_buf || _count == 0 || _maxPoints == 0) return 0;
    return _buf[(_head + _maxPoints - 1) % _maxPoints];
}

void DWIN_Curve::recomputeStats() {
    if (!_buf || _count == 0) {
        _min    = 0;
        _maxVal = 0;
        return;
    }
    _min    = point(0);
    _maxVal = point(0);
    for (uint16_t i = 1; i < _count; i++) {
        uint16_t v = point(i);
        if (v < _min)    _min    = v;
        if (v > _maxVal) _maxVal = v;
    }
}

uint16_t DWIN_Curve::scaleValue(float raw) const {
    float v = raw * _gain + _offset;
    if (v < 0.0f) v = 0.0f;
    float maxV = _eightBit ? 255.0f : 65535.0f;
    if (v > maxV) v = maxV;
    return (uint16_t)(v + 0.5f);
}

void DWIN_Curve::pushSilent(uint16_t value) {
    if (!_buf || _maxPoints == 0) return;
    if (_eightBit && value > 255) value = 255;

    _buf[_head] = value;
    _head = (uint16_t)((_head + 1) % _maxPoints);
    if (_count < _maxPoints) _count++;

    recomputeStats();
}

void DWIN_Curve::pushScaledSilent(float rawValue) {
    pushSilent(scaleValue(rawValue));
}

void DWIN_Curve::sendLatest() {
    if (!_buf || _count == 0 || _maxPoints == 0) return;
    uint16_t v = latest();
    if (_eightBit) _dwin.curveAddPoint8(_vp, (uint8_t)v);
    else           _dwin.curveAddPoint(_vp, v);
}

void DWIN_Curve::push(uint16_t value) {
    pushSilent(value);
    if (_autoRedraw) redrawLinear();
    else             sendLatest();
}

void DWIN_Curve::pushScaled(float rawValue) {
    pushScaledSilent(rawValue);
    if (_autoRedraw) redrawLinear();
    else             sendLatest();
}

void DWIN_Curve::redraw() {
    if (!_buf) return;
    redrawLinear();
}

void DWIN_Curve::redrawLinear() {
    if (!_buf || _count == 0 || _maxPoints == 0) return;

    const uint16_t CHUNK = 120;
    uint16_t sent   = 0;
    uint16_t oldest = (uint16_t)((_head + _maxPoints - _count) % _maxPoints);

    while (sent < _count) {
        uint16_t remain = _count - sent;
        uint16_t n      = (remain > CHUNK) ? CHUNK : remain;

        uint16_t tmp[CHUNK];
        for (uint16_t i = 0; i < n; i++) {
            tmp[i] = _buf[(oldest + sent + i) % _maxPoints];
        }
        if (!_dwin.curveAddPoints(_vp, tmp, n)) return;
        sent += n;
    }
}

void DWIN_Curve::clear() {
    if (!_buf) return;
    memset(_buf, 0, (size_t)_maxPoints * sizeof(uint16_t));
    _count  = 0;
    _head   = 0;
    _min    = 0;
    _maxVal = 0;
    _dwin.curveClear(_vp, _maxPoints);
}

// ===========================================================================
// DWIN_MultiCurve
// ===========================================================================
DWIN_MultiCurve::DWIN_MultiCurve(DWIN_LCD &dwin, uint8_t maxCurves, uint16_t maxPoints)
    : _dwin(dwin),
      _maxCurves(maxCurves),
      _count(0),
      _maxPoints(maxPoints),
      _curves(nullptr),
      _autoRedraw(true)
{
}

DWIN_MultiCurve::~DWIN_MultiCurve() {
    if (_curves) {
        for (uint8_t i = 0; i < _count; i++) {
            if (_curves[i]) {
                delete _curves[i];
                _curves[i] = nullptr;
            }
        }
        free(_curves);
        _curves = nullptr;
    }
    _count = 0;
}

void DWIN_MultiCurve::begin() {
    if (_curves || _maxCurves == 0) return;
    _curves = (DWIN_Curve **)malloc((size_t)_maxCurves * sizeof(DWIN_Curve *));
    if (!_curves) {
        _maxCurves = 0;
        return;
    }
    for (uint8_t i = 0; i < _maxCurves; i++) _curves[i] = nullptr;
}

int8_t DWIN_MultiCurve::addCurve(uint16_t curveVp, bool eightBit) {
    if (!_curves || _count >= _maxCurves) return -1;

    DWIN_Curve *c = new DWIN_Curve(_dwin, curveVp, _maxPoints);
    if (!c) return -1;

    c->begin();
    c->set8Bit(eightBit);
    // MultiCurve always coordinates redraws itself, so each channel must
    // not auto-redraw on its own.
    c->setAutoRedraw(false);

    _curves[_count] = c;
    return (int8_t)_count++;
}

DWIN_Curve* DWIN_MultiCurve::curve(uint8_t idx) {
    if (!_curves || idx >= _count) return nullptr;
    return _curves[idx];
}

void DWIN_MultiCurve::setAutoRedraw(bool enable) {
    _autoRedraw = enable;
}

void DWIN_MultiCurve::pushSample(const uint16_t *values) {
    if (!_curves || !values) return;

    for (uint8_t i = 0; i < _count; i++) {
        if (_curves[i]) _curves[i]->pushSilent(values[i]);
    }

    if (_autoRedraw) {
        redrawAll();
    } else {
        for (uint8_t i = 0; i < _count; i++) {
            if (_curves[i]) _curves[i]->sendLatest();
        }
    }
}

void DWIN_MultiCurve::pushSampleScaled(const float *rawValues) {
    if (!_curves || !rawValues) return;

    for (uint8_t i = 0; i < _count; i++) {
        if (_curves[i]) _curves[i]->pushScaledSilent(rawValues[i]);
    }

    if (_autoRedraw) {
        redrawAll();
    } else {
        for (uint8_t i = 0; i < _count; i++) {
            if (_curves[i]) _curves[i]->sendLatest();
        }
    }
}

void DWIN_MultiCurve::pushOne(uint8_t idx, uint16_t value) {
    if (!_curves || idx >= _count) return;
    DWIN_Curve *c = _curves[idx];
    if (!c) return;

    c->pushSilent(value);
    if (_autoRedraw) c->redraw();
    else             c->sendLatest();
}

void DWIN_MultiCurve::redrawAll() {
    if (!_curves) return;
    for (uint8_t i = 0; i < _count; i++) {
        if (_curves[i]) _curves[i]->redraw();
    }
}

void DWIN_MultiCurve::clearAll() {
    if (!_curves) return;
    for (uint8_t i = 0; i < _count; i++) {
        if (_curves[i]) _curves[i]->clear();
    }
}