/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class ByteWriterLE {
    uint8_t *dst;
    size_t bytesLeft;

public:
    ByteWriterLE(unsigned char *dst, size_t len) : dst((uint8_t *)dst), bytesLeft(len) { }
    ByteWriterLE(char *dst, size_t len) : dst((uint8_t *)dst), bytesLeft(len) { }

    bool Write8(uint8_t val) {
        if (!bytesLeft)
            return false;
        *dst++ = val;
        bytesLeft--;
        return true;
    }

    bool Write16(uint16_t val) {
        return Write8(val & 0xFF) && Write8((val >> 8) & 0xFF);
    }

    bool Write32(uint32_t val) {
        return Write8(val & 0xFF) && Write8((val >> 8) & 0xFF) && Write8((val >> 16) & 0xFF) && Write8((val >> 24) & 0xFF);
    }

    bool Write64(uint64_t val) {
        return Write32(val & 0xFFFFFFFF) && Write32((val >> 32) & 0xFFFFFFFF);
    }
};

class ByteWriterBE {
    uint8_t *dst;
    size_t bytesLeft;

public:
    ByteWriterBE(unsigned char *dst, size_t len) : dst((uint8_t *)dst), bytesLeft(len) { }
    ByteWriterBE(char *dst, size_t len) : dst((uint8_t *)dst), bytesLeft(len) { }

    bool Write8(uint8_t val) {
        if (!bytesLeft)
            return false;
        *dst++ = val;
        bytesLeft--;
        return true;
    }

    bool Write16(uint16_t val) {
        return Write8((val >> 8) & 0xFF) && Write8(val & 0xFF);
    }

    bool Write32(uint32_t val) {
        return Write8((val >> 24) & 0xFF) && Write8((val >> 16) & 0xFF) && Write8((val >> 8) & 0xFF) && Write8(val & 0xFF);
    }

    bool Write64(uint64_t val) {
        return Write32((val >> 32) & 0xFFFFFFFF) && Write32(val & 0xFFFFFFFF);
    }
};
