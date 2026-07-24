/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "Pixmap.h"
#include "TgaReader.h"

namespace tga {

#define TGA_FOOTER_SIGNATURE "TRUEVISION-XFILE."

enum ImageType {
    Type_Palette = 1,
    Type_Truecolor = 2,
    Type_Grayscale = 3,
    Type_Palette_RLE = 9,
    Type_Truecolor_RLE = 10,
    Type_Grayscale_RLE = 11,
};

enum ImageFlag {
    Flag_Alpha = 0x0F,
    Flag_InvertX = 0x10,
    Flag_InvertY = 0x20,
    Flag_Reserved = 0xC0,
};

enum ImageAlpha {
    Alpha_Ignore = 0,
    Alpha_Normal = 3,
    Alpha_Premultiplied = 4,
};

#pragma pack(push, 1)

struct TgaHeader {
    u8 idLength;
    u8 cmapType;
    u8 imageType;
    u16 cmapFirstEntry;
    u16 cmapLength;
    u8 cmapBitDepth;
    u16 offsetX, offsetY;
    u16 width, height;
    u8 bitDepth;
    u8 flags;
};

struct TgaFooter {
    u32 extAreaOffset;
    u32 devAreaOffset;
    char signature[18];
};

struct TgaExtArea {
    u16 size;
    char author[41];
    char comments[4][81];
    u16 dateTime[6];
    u8 fields_14_to_15[47];
    char progName[41];
    u16 progVersion;
    char progVersionC;
    u32 fields_18_to_23[6];
    u8 alphaType;
};

#pragma pack(pop)

static_assert(sizeof(TgaHeader) == 18, "wrong size of TgaHeader structure");
static_assert(sizeof(TgaFooter) == 26, "wrong size of TgaFooter structure");
static_assert(sizeof(TgaExtArea) == 495, "wrong size of TgaExtArea structure");

static u16 readLE16(u8* data) {
    u16 v0 = *data++;
    u16 v1 = (u16)*data << 8;
    return v0 | v1;
}

static u16 convLE(u16 x) {
    u8* data = (u8*)&x;
    return readLE16(data);
}

static u32 readLE32(u8* data) {
    u32 v0 = *data++;
    u32 v1 = (u32)*data++ << 8;
    u32 v2 = (u32)*data++ << 16;
    u32 v3 = (u32)*data << 24;
    return v0 | v1 | v2 | v3;
}

static u32 convLE(u32 x) {
    u8* data = (u8*)&x;
    return readLE32(data);
}

static bool HasVersion2Footer(const u8* data, size_t n) {
    if (n < sizeof(TgaHeader) + sizeof(TgaFooter)) {
        return false;
    }
    const TgaFooter* footerLE = (const TgaFooter*)(data + n - sizeof(TgaFooter));
    return str::EqN(footerLE->signature, TGA_FOOTER_SIGNATURE, sizeof(footerLE->signature));
}

static const TgaExtArea* GetExtAreaPtr(const u8* data, size_t n) {
    if (!HasVersion2Footer(data, n)) {
        return nullptr;
    }
    const TgaFooter* footerLE = (const TgaFooter*)(data + n - sizeof(TgaFooter));
    if (convLE(footerLE->extAreaOffset) < sizeof(TgaHeader) ||
        convLE(footerLE->extAreaOffset) + sizeof(TgaExtArea) + sizeof(TgaFooter) > n) {
        return nullptr;
    }
    const TgaExtArea* extAreaLE = (const TgaExtArea*)(data + convLE(footerLE->extAreaOffset));
    if (convLE(extAreaLE->size) < sizeof(TgaExtArea)) {
        return nullptr;
    }
    return extAreaLE;
}

// note: we only support the more common bit depths:
// http://www.ryanjuckett.com/programming/graphics/26-parsing-colors-in-a-tga-file
static int GetPixelBits(const TgaHeader* headerLE, ImageAlpha aType = Alpha_Normal) {
    int bits;
    if (Type_Palette == headerLE->imageType || Type_Palette_RLE == headerLE->imageType) {
        if (1 != headerLE->cmapType || 8 != headerLE->bitDepth && 16 != headerLE->bitDepth) {
            return 0;
        }
        bits = headerLE->cmapBitDepth;
    } else if (Type_Truecolor == headerLE->imageType || Type_Truecolor_RLE == headerLE->imageType) {
        bits = headerLE->bitDepth;
    } else if (Type_Grayscale == headerLE->imageType || Type_Grayscale_RLE == headerLE->imageType) {
        if (8 != headerLE->bitDepth || (headerLE->flags & Flag_Alpha)) {
            return 0;
        }
        return 8;
    } else {
        return 0;
    }

    int alphaBits = (headerLE->flags & Flag_Alpha);
    if (15 == bits && 0 == alphaBits) {
        return bits;
    }
    if (16 == bits && (0 == alphaBits || Alpha_Ignore == aType)) {
        return bits;
    }
    if (16 == bits && 1 == alphaBits) {
        return bits;
    }
    if (24 == bits && 0 == alphaBits) {
        return bits;
    }
    if (32 == bits && (0 == alphaBits || Alpha_Ignore == aType)) {
        return bits;
    }
    if (32 == bits && 8 == alphaBits && Alpha_Normal == aType) {
        return bits;
    }
    if (32 == bits && 8 == alphaBits && Alpha_Premultiplied == aType) {
        return bits;
    }
    return 0;
}

static ImageAlpha GetAlphaType(const u8* data, size_t n) {
    const TgaExtArea* extAreaLE = GetExtAreaPtr(data, n);
    if (!extAreaLE) {
        return Alpha_Normal;
    }

    switch (extAreaLE->alphaType) {
        case Alpha_Normal:
            return Alpha_Normal;
        case Alpha_Premultiplied:
            return Alpha_Premultiplied;
        default:
            return Alpha_Ignore;
    }
}

// checks whether this could be data for a TGA image
bool HasSignature(Str d) {
    size_t n = (size_t)d.len;
    const u8* data = (const u8*)d.s;
    if (HasVersion2Footer(data, n)) {
        return true;
    }
    // fall back to checking for values that would be valid for a TGA image
    if (n < sizeof(TgaHeader)) {
        return false;
    }
    const TgaHeader* headerLE = (const TgaHeader*)data;
    if (headerLE->cmapType != 0 && headerLE->cmapType != 1) {
        return false;
    }
    if ((headerLE->flags & Flag_Reserved)) {
        return false;
    }
    if (!GetPixelBits(headerLE)) {
        return false;
    }
    return true;
}

struct ReadState {
    const u8* data;
    const u8* end;
    ImageType type;
    int n;
    bool isRLE;
    int repeat;
    bool repeatSame;
    struct {
        int firstEntry;
        int length;
        const u8* data;
        int n;
    } cmap;
    bool failed;
};

static u8 Scale5To8(u32 v) {
    return (u8)((v << 3) | (v >> 2));
}

static void CopyPixelToBGRA(u8* dst, const u8* src, int bits, int alphaBits, ImageAlpha alphaType) {
    switch (bits) {
        case 15:
        case 16: {
            u16 v = readLE16((u8*)src);
            dst[0] = Scale5To8(v & 0x1f);
            dst[1] = Scale5To8((v >> 5) & 0x1f);
            dst[2] = Scale5To8((v >> 10) & 0x1f);
            dst[3] = (bits == 16 && alphaBits == 1 && alphaType != Alpha_Ignore) ? ((v & 0x8000) ? 255 : 0) : 255;
            break;
        }
        case 24:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = 255;
            break;
        case 32:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = alphaType == Alpha_Ignore ? 255 : src[3];
            break;
        default:
            ReportIf(true);
            break;
    }
}

static void ReadPixel(ReadState& s, u8* dst, int bits, int alphaBits, ImageAlpha alphaType) {
    if (s.isRLE && 0 == s.repeat && s.data < s.end) {
        s.repeat = (*s.data & 0x7F) + 1;
        s.repeatSame = (*s.data & 0x80);
        s.data++;
    }
    if (s.data + s.n > s.end) {
        s.failed = true;
        return;
    }

    int idx;
    const u8* src = nullptr;
    switch (s.type) {
        case Type_Palette:
        case Type_Palette_RLE:
            idx = ((u8)s.data[0] | (2 == s.n ? ((u8)s.data[1] << 8) : 0)) - s.cmap.firstEntry;
            if (0 <= idx && idx < s.cmap.length) {
                src = s.cmap.data + idx * s.cmap.n;
            } else {
                s.failed = true;
            }
            break;
        case Type_Truecolor:
        case Type_Truecolor_RLE:
            src = s.data;
            break;
        case Type_Grayscale:
        case Type_Grayscale_RLE:
            dst[0] = dst[1] = dst[2] = s.data[0];
            dst[3] = 255;
            break;
    }
    if (src) {
        CopyPixelToBGRA(dst, src, bits, alphaBits, alphaType);
    }

    if (!s.isRLE || 0 == --s.repeat || !s.repeatSame) {
        s.data += s.n;
    }
}

Pixmap* PixmapFromData(Str d) {
    size_t dataLen = (size_t)d.len;
    const u8* data = (const u8*)d.s;

    if (dataLen > INT_MAX || dataLen < sizeof(TgaHeader)) {
        return nullptr;
    }

    ReadState s = {nullptr};
    const TgaHeader* headerLE = (const TgaHeader*)d.s;
    s.data = data + sizeof(TgaHeader) + headerLE->idLength;
    s.end = data + dataLen;
    if (s.data > s.end) {
        return nullptr;
    }
    if (1 == headerLE->cmapType) {
        s.cmap.data = s.data;
        s.cmap.n = (headerLE->cmapBitDepth + 7) / 8;
        s.cmap.length = convLE(headerLE->cmapLength);
        s.cmap.firstEntry = convLE(headerLE->cmapFirstEntry);
        s.data += s.cmap.length * s.cmap.n;
        if (s.data > s.end) {
            return nullptr;
        }
    }
    s.type = (ImageType)headerLE->imageType;
    s.n = (headerLE->bitDepth + 7) / 8;
    s.isRLE = headerLE->imageType >= 8;

    ImageAlpha alphaType = GetAlphaType(data, dataLen);
    int bits = GetPixelBits(headerLE, alphaType);
    if (!bits) {
        return nullptr;
    }

    int w = convLE(headerLE->width);
    int h = convLE(headerLE->height);
    if (w <= 0 || h <= 0) {
        return nullptr;
    }
    int alphaBits = (headerLE->flags & Flag_Alpha);
    bool invertX = (headerLE->flags & Flag_InvertX);
    bool invertY = (headerLE->flags & Flag_InvertY);

    Pixmap* pixmap = AllocPixmap(w, h, PixmapFormat::BGRA8, alphaType == Alpha_Premultiplied);
    if (!pixmap) {
        return nullptr;
    }
    for (int y = 0; y < h; y++) {
        u8* rowOut = pixmap->data + pixmap->stride * (invertY ? y : h - 1 - y);
        for (int x = 0; x < w; x++) {
            ReadPixel(s, rowOut + 4 * (invertX ? w - 1 - x : x), bits, alphaBits, alphaType);
        }
    }
    if (s.failed) {
        FreePixmap(pixmap);
        return nullptr;
    }
    return pixmap;
}

inline bool memeq3(const char* pix1, const char* pix2) {
    return pix1[0] == pix2[0] && pix1[1] == pix2[1] && pix1[2] == pix2[2];
}

static void GetPixmapPixelBGR(Pixmap* pixmap, int x, int y, char bgr[3]) {
    const u8* src = pixmap->data + (size_t)y * pixmap->stride + (size_t)x * PixmapBytesPerPixel(pixmap->format);
    if (pixmap->format == PixmapFormat::RGBA8) {
        bgr[0] = (char)src[2];
        bgr[1] = (char)src[1];
        bgr[2] = (char)src[0];
        return;
    }
    bgr[0] = (char)src[0];
    bgr[1] = (char)src[1];
    bgr[2] = (char)src[2];
}

static bool PixmapPixelEq3(Pixmap* pixmap, int x1, int x2, int y) {
    char p1[3], p2[3];
    GetPixmapPixelBGR(pixmap, x1, y, p1);
    GetPixmapPixelBGR(pixmap, x2, y, p2);
    return memeq3(p1, p2);
}

Str PixmapToTgaFormat(Pixmap* pixmap) {
    if (!pixmap || !pixmap->data || (u32)pixmap->width > USHRT_MAX || (u32)pixmap->height > USHRT_MAX) {
        return {};
    }
    if (pixmap->format != PixmapFormat::BGRA8 && pixmap->format != PixmapFormat::BGR8 &&
        pixmap->format != PixmapFormat::RGBA8) {
        return {};
    }

    u16 w = (u16)pixmap->width;
    u16 h = (u16)pixmap->height;
    TgaHeader headerLE{};
    headerLE.imageType = Type_Truecolor_RLE;
    headerLE.width = convLE(w);
    headerLE.height = convLE(h);
    headerLE.bitDepth = 24;
    TgaFooter footerLE = {0, 0, TGA_FOOTER_SIGNATURE};

    str::Builder tgaData;
    tgaData.Append(Str((char*)&headerLE, (int)sizeof(headerLE)));
    for (int k = 0; k < h; k++) {
        int y = h - 1 - k;
        for (int i = 0, j = 1; i < w; i += j, j = 1) {
            // determine the length of a run of identical pixels
            while (i + j < w && j < 128 && PixmapPixelEq3(pixmap, i, i + j, y)) {
                j++;
            }
            if (j > 1) {
                tgaData.AppendChar((char)(j - 1 + 128));
                char pixel[3];
                GetPixmapPixelBGR(pixmap, i, y, pixel);
                tgaData.Append(Str(pixel, 3));
            } else {
                // determine the length of a run of different pixels
                while (i + j < w && j <= 128 && !PixmapPixelEq3(pixmap, i + j - 1, i + j, y)) {
                    j++;
                }
                if (i + j < w || j > 128) {
                    j--;
                }
                tgaData.AppendChar((char)(j - 1));
                for (int x = i; x < i + j; x++) {
                    char pixel[3];
                    GetPixmapPixelBGR(pixmap, x, y, pixel);
                    tgaData.Append(Str(pixel, 3));
                }
            }
        }
    }
    tgaData.Append(Str((char*)&footerLE, (int)sizeof(footerLE)));

    // don't compress the image data if that increases the file size
    if ((size_t)len(tgaData) > sizeof(headerLE) + (size_t)(w * h * 3) + sizeof(footerLE)) {
        tgaData.RemoveAt(0, len(tgaData));
        headerLE.imageType = Type_Truecolor;
        tgaData.Append(Str((char*)&headerLE, (int)sizeof(headerLE)));
        for (int k = 0; k < h; k++) {
            int y = h - 1 - k;
            for (int x = 0; x < w; x++) {
                char pixel[3];
                GetPixmapPixelBGR(pixmap, x, y, pixel);
                tgaData.Append(Str(pixel, 3));
            }
        }
        tgaData.Append(Str((char*)&footerLE, (int)sizeof(footerLE)));
    }

    return tgaData.TakeStr();
}
} // namespace tga
