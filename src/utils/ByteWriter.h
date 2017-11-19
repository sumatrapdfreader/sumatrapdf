/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class ByteWriter {
    uint8_t *dst;
    size_t bytesLeft;

public:
    ByteWriter(unsigned char *dst, size_t len) : dst((uint8_t *)dst), bytesLeft(len) { }
    ByteWriter(char *dst, size_t len) : dst((uint8_t *)dst), bytesLeft(len) { }

    bool Write8(uint8_t b) {
        if (bytesLeft == 0) {
            return false;
        }
        *dst = b;
        dst++;
        bytesLeft--;
        return true;
    }

    bool Write8x2(uint8_t b1, uint8_t b2) {
        if (bytesLeft < 2) {
            return false;
        }
        *dst++ = b1;
        *dst++ = b2;
        bytesLeft -= 2;
        return true;
    }
};

class ByteWriterLE : public ByteWriter {
public:

    bool Write16(uint16_t val) {
        uint8_t b1 = val & 0xFF;
        uint8_t b2 = (val >> 8) & 0xFF;
        return Write8x2(b1, b2);
    }

    bool Write32(uint32_t val) {
        uint8_t b1 = val & 0xFF;
        uint8_t b2 = (val >> 8) & 0xFF;
        uint8_t b3 = (val >> 16) & 0xFF;
        uint8_t b4 = (val >> 24) & 0xFF;
        return Write8x2(b1, b2) && Write8x2(b3, b4);
    }

    bool Write64(uint64_t val) {
        return Write32(val & 0xFFFFFFFF) && Write32((val >> 32) & 0xFFFFFFFF);
    }
};

class ByteWriterBE : public ByteWriter {
public:

    bool Write16(uint16_t val) {
        uint8_t b1 = val & 0xFF;
        uint8_t b2 = (val >> 8) & 0xFF;
        return Write8x2(b2, b1);
    }

    bool Write32(uint32_t val) {
        uint8_t b1 = val & 0xFF;
        uint8_t b2 = (val >> 8) & 0xFF;
        uint8_t b3 = (val >> 16) & 0xFF;
        uint8_t b4 = (val >> 24) & 0xFF;
        return Write8x2(b4, b3) && Write8x2(b2, b1);
    }

    bool Write64(uint64_t val) {
        return Write32((val >> 32) & 0xFFFFFFFF) && Write32(val & 0xFFFFFFFF);
    }
};
