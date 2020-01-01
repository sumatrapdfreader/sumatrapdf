/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class ByteWriter {
    uint8_t* dst;
    uint8_t* end;
    bool isLE;

  public:
    ByteWriter(uint8_t* dst, size_t bytesLeft, bool isLE) : dst(dst), isLE(isLE) {
        end = dst + bytesLeft;
    }

    ByteWriter(const ByteWriter& o) {
        this->dst = o.dst;
        this->end = o.end;
        this->isLE = o.isLE;
    }

    size_t Left() const {
        CrashIf(dst > end);
        return end - dst;
    }

    bool Write8(uint8_t b) {
        if (Left() < 1) {
            return false;
        }
        *dst++ = b;
        return true;
    }

    bool Write8x2(uint8_t b1, uint8_t b2) {
        if (Left() < 2) {
            return false;
        }
        *dst++ = b1;
        *dst++ = b2;
        return true;
    }

    bool Write16(uint16_t val) {
        uint8_t b1 = val & 0xFF;
        uint8_t b2 = (val >> 8) & 0xFF;
        if (isLE) {
            return Write8x2(b1, b2);
        }
        return Write8x2(b2, b1);
    }

    bool Write32(uint32_t val) {
        uint8_t b1 = val & 0xFF;
        uint8_t b2 = (val >> 8) & 0xFF;
        uint8_t b3 = (val >> 16) & 0xFF;
        uint8_t b4 = (val >> 24) & 0xFF;
        if (isLE) {
            return Write8x2(b1, b2) && Write8x2(b3, b4);
        }
        return Write8x2(b4, b3) && Write8x2(b2, b1);
    }

    bool Write64(uint64_t val) {
        uint32_t v1 = val & 0xFFFFFFFF;
        uint32_t v2 = (val >> 32) & 0xFFFFFFFF;
        if (isLE) {
            return Write32(v1) && Write32(v2);
        }
        return Write32(v2) && Write32(v1);
    }
};

ByteWriter MakeByteWriterLE(unsigned char* dst, size_t len) {
    return ByteWriter((uint8_t*)dst, len, true);
}

ByteWriter MakeByteWriterLE(char* dst, size_t len) {
    return ByteWriter((uint8_t*)dst, len, true);
}

ByteWriter MakeByteWriterBE(unsigned char* dst, size_t len) {
    return ByteWriter((uint8_t*)dst, len, false);
}

ByteWriter MakeByteWriterBE(char* dst, size_t len) {
    return ByteWriter((uint8_t*)dst, len, false);
}
