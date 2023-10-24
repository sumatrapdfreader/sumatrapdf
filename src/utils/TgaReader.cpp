/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
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

#include <pshpack1.h>

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

#include <poppack.h>

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

static bool HasVersion2Footer(const u8* data, size_t len) {
    if (len < sizeof(TgaHeader) + sizeof(TgaFooter)) {
        return false;
    }
    const TgaFooter* footerLE = (const TgaFooter*)(data + len - sizeof(TgaFooter));
    return str::EqN(footerLE->signature, TGA_FOOTER_SIGNATURE, sizeof(footerLE->signature));
}

static const TgaExtArea* GetExtAreaPtr(const u8* data, size_t len) {
    if (!HasVersion2Footer(data, len)) {
        return nullptr;
    }
    const TgaFooter* footerLE = (const TgaFooter*)(data + len - sizeof(TgaFooter));
    if (convLE(footerLE->extAreaOffset) < sizeof(TgaHeader) ||
        convLE(footerLE->extAreaOffset) + sizeof(TgaExtArea) + sizeof(TgaFooter) > len) {
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
static Gdiplus::PixelFormat GetPixelFormat(const TgaHeader* headerLE, ImageAlpha aType = Alpha_Normal) {
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
        // using a non-indexed format so that we don't have to bother with a palette
        return PixelFormat24bppRGB;
    } else {
        return 0;
    }

    int alphaBits = (headerLE->flags & Flag_Alpha);
    if (15 == bits && 0 == alphaBits) {
        return PixelFormat16bppRGB555;
    }
    if (16 == bits && (0 == alphaBits || Alpha_Ignore == aType)) {
        return PixelFormat16bppRGB555;
    }
    if (16 == bits && 1 == alphaBits) {
        return PixelFormat16bppARGB1555;
    }
    if (24 == bits && 0 == alphaBits) {
        return PixelFormat24bppRGB;
    }
    if (32 == bits && (0 == alphaBits || Alpha_Ignore == aType)) {
        return PixelFormat32bppRGB;
    }
    if (32 == bits && 8 == alphaBits && Alpha_Normal == aType) {
        return PixelFormat32bppARGB;
    }
    if (32 == bits && 8 == alphaBits && Alpha_Premultiplied == aType) {
        return PixelFormat32bppPARGB;
    }
    return 0;
}

static ImageAlpha GetAlphaType(const u8* data, size_t len) {
    const TgaExtArea* extAreaLE = GetExtAreaPtr(data, len);
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
bool HasSignature(const ByteSlice& d) {
    size_t len = d.size();
    const u8* data = (const u8*)d.data();
    if (HasVersion2Footer(data, len)) {
        return true;
    }
    // fall back to checking for values that would be valid for a TGA image
    if (len < sizeof(TgaHeader)) {
        return false;
    }
    const TgaHeader* headerLE = (const TgaHeader*)data;
    if (headerLE->cmapType != 0 && headerLE->cmapType != 1) {
        return false;
    }
    if ((headerLE->flags & Flag_Reserved)) {
        return false;
    }
    if (!GetPixelFormat(headerLE)) {
        return false;
    }
    return true;
}

static void SetImageProperty(Gdiplus::Bitmap* bmp, PROPID id, const char* asciiValue) {
    Gdiplus::PropertyItem item;
    item.id = id;
    item.type = PropertyTagTypeASCII;
    item.value = (void*)asciiValue;
    item.length = (ULONG)(str::Len(asciiValue) + 1);
    Gdiplus::Status ok = bmp->SetPropertyItem(&item);
    CrashIf(ok != Gdiplus::Ok);
}

static bool IsFieldSet(const char* field, size_t len, bool isBinary = false) {
    for (size_t i = 0; i < len; i++) {
        if (field[i] && (isBinary || field[i] != ' ')) {
            return isBinary || '\0' == field[len - 1];
        }
    }
    return false;
}

static void CopyMetadata(const u8* data, size_t len, Gdiplus::Bitmap* bmp) {
    const TgaExtArea* extAreaLE = GetExtAreaPtr(data, len);
    if (!extAreaLE) {
        return;
    }

    if (IsFieldSet(extAreaLE->author, sizeof(extAreaLE->author))) {
        SetImageProperty(bmp, PropertyTagArtist, extAreaLE->author);
    }

    if (IsFieldSet((const char*)extAreaLE->dateTime, sizeof(extAreaLE->dateTime), true)) {
        char dateTime[20];
        auto v1 = convLE(extAreaLE->dateTime[2]);
        auto v2 = convLE(extAreaLE->dateTime[1]);
        auto v3 = convLE(extAreaLE->dateTime[0]);
        auto v4 = convLE(extAreaLE->dateTime[3]);
        auto v5 = convLE(extAreaLE->dateTime[4]);
        auto v6 = convLE(extAreaLE->dateTime[5]);
        int count = snprintf(dateTime, dimof(dateTime), "%04u-%02u-%02u %02u:%02u:%02u", v1, v2, v3, v4, v5, v6);
        if (19 == count) {
            SetImageProperty(bmp, PropertyTagDateTime, dateTime);
        }
    }

    if (IsFieldSet(extAreaLE->progName, sizeof(extAreaLE->progName))) {
        char software[49];
        str::BufSet(software, 41, extAreaLE->progName);
        if (convLE(extAreaLE->progVersion) != 0) {
            auto v1 = convLE(extAreaLE->progVersion) / 100;
            auto v2 = convLE(extAreaLE->progVersion) % 100;
            auto v3 = extAreaLE->progVersionC != ' ' ? extAreaLE->progVersionC : '\0';
            snprintf(software + str::Len(software), 9, " %d.%d%c", v1, v2, v3);
            software[48] = '\0';
        }
        SetImageProperty(bmp, PropertyTagSoftwareUsed, software);
    }
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

static inline void CopyPixel(u8* dst, const u8* src, int n) {
    switch (n) {
        case 3:
            dst[2] = src[2]; // fall through
        case 2:
            *(u16*)dst = *(u16*)src;
            break;
        case 4:
            *(u32*)dst = *(u32*)src;
            break;
        default:
            CrashIf(true);
    }
}

static void ReadPixel(ReadState& s, u8* dst) {
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
    switch (s.type) {
        case Type_Palette:
        case Type_Palette_RLE:
            idx = ((u8)s.data[0] | (2 == s.n ? ((u8)s.data[1] << 8) : 0)) - s.cmap.firstEntry;
            if (0 <= idx && idx < s.cmap.length) {
                CopyPixel(dst, s.cmap.data + idx * s.cmap.n, s.cmap.n);
            }
            break;
        case Type_Truecolor:
        case Type_Truecolor_RLE:
            CopyPixel(dst, s.data, s.n);
            break;
        case Type_Grayscale:
        case Type_Grayscale_RLE:
            dst[0] = dst[1] = dst[2] = s.data[0];
            break;
    }

    if (!s.isRLE || 0 == --s.repeat || !s.repeatSame) {
        s.data += s.n;
    }
}

Gdiplus::Bitmap* ImageFromData(const ByteSlice& d) {
    size_t len = d.size();
    const u8* data = (const u8*)d.data();

    if (len < sizeof(TgaHeader)) {
        return nullptr;
    }

    ReadState s = {nullptr};
    const TgaHeader* headerLE = (const TgaHeader*)d.data();
    s.data = data + sizeof(TgaHeader) + headerLE->idLength;
    s.end = data + len;
    if (1 == headerLE->cmapType) {
        s.cmap.data = s.data;
        s.cmap.n = (headerLE->cmapBitDepth + 7) / 8;
        s.cmap.length = convLE(headerLE->cmapLength);
        s.cmap.firstEntry = convLE(headerLE->cmapFirstEntry);
        s.data += s.cmap.length * s.cmap.n;
    }
    s.type = (ImageType)headerLE->imageType;
    s.n = (headerLE->bitDepth + 7) / 8;
    s.isRLE = headerLE->imageType >= 8;

    Gdiplus::PixelFormat format = GetPixelFormat(headerLE, GetAlphaType(data, len));
    if (!format) {
        return nullptr;
    }

    int w = convLE(headerLE->width);
    int h = convLE(headerLE->height);
    int n = ((format >> 8) & 0x3F) / 8;
    bool invertX = (headerLE->flags & Flag_InvertX);
    bool invertY = (headerLE->flags & Flag_InvertY);

    Gdiplus::Bitmap bmp(w, h, format);
    Gdiplus::Rect bmpRect(0, 0, w, h);
    Gdiplus::BitmapData bmpData;
    Gdiplus::Status ok = bmp.LockBits(&bmpRect, Gdiplus::ImageLockModeWrite, format, &bmpData);
    if (ok != Gdiplus::Ok) {
        return nullptr;
    }
    for (int y = 0; y < h; y++) {
        u8* rowOut = (u8*)bmpData.Scan0 + bmpData.Stride * (invertY ? y : h - 1 - y);
        for (int x = 0; x < w; x++) {
            ReadPixel(s, rowOut + n * (invertX ? w - 1 - x : x));
        }
    }
    bmp.UnlockBits(&bmpData);
    if (s.failed) {
        return nullptr;
    }
    CopyMetadata(data, len, &bmp);
    return bmp.Clone(0, 0, w, h, format);
}

inline bool memeq3(const char* pix1, const char* pix2) {
    return *(WORD*)pix1 == *(WORD*)pix2 && pix1[2] == pix2[2];
}

ByteSlice SerializeBitmap(HBITMAP hbmp) {
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    if ((ULONG)bmpInfo.bmWidth > USHRT_MAX || (ULONG)bmpInfo.bmHeight > USHRT_MAX) {
        return {};
    }

    WORD w = (WORD)bmpInfo.bmWidth;
    WORD h = (WORD)bmpInfo.bmHeight;
    int stride = ((w * 3 + 3) / 4) * 4;
    char* bmpData = AllocArrayTemp<char>(stride * h);
    if (!bmpData) {
        return {};
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hDC = GetDC(nullptr);
    if (!GetDIBits(hDC, hbmp, 0, h, bmpData, &bmi, DIB_RGB_COLORS)) {
        ReleaseDC(nullptr, hDC);
        return {};
    }
    ReleaseDC(nullptr, hDC);

    TgaHeader headerLE{};
    headerLE.imageType = Type_Truecolor_RLE;
    headerLE.width = convLE(w);
    headerLE.height = convLE(h);
    headerLE.bitDepth = 24;
    TgaFooter footerLE = {0, 0, TGA_FOOTER_SIGNATURE};

    str::Str tgaData;
    tgaData.Append((char*)&headerLE, sizeof(headerLE));
    for (int k = 0; k < h; k++) {
        const char* line = bmpData + k * stride;
        for (int i = 0, j = 1; i < w; i += j, j = 1) {
            // determine the length of a run of identical pixels
            while (i + j < w && j < 128 && memeq3(line + i * 3, line + (i + j) * 3)) {
                j++;
            }
            if (j > 1) {
                tgaData.AppendChar((char)(j - 1 + 128));
                tgaData.Append(line + i * 3, 3);
            } else {
                // determine the length of a run of different pixels
                while (i + j < w && j <= 128 && !memeq3(line + (i + j - 1) * 3, line + (i + j) * 3)) {
                    j++;
                }
                if (i + j < w || j > 128) {
                    j--;
                }
                tgaData.AppendChar((char)(j - 1));
                tgaData.Append(line + i * 3, j * 3);
            }
        }
    }
    tgaData.Append((char*)&footerLE, sizeof(footerLE));

    // don't compress the image data if that increases the file size
    if (tgaData.size() > sizeof(headerLE) + w * h * 3 + sizeof(footerLE)) {
        tgaData.RemoveAt(0, tgaData.size());
        headerLE.imageType = Type_Truecolor;
        tgaData.Append((char*)&headerLE, sizeof(headerLE));
        for (int k = 0; k < h; k++) {
            tgaData.Append(bmpData + k * stride, w * 3);
        }
        tgaData.Append((char*)&footerLE, sizeof(footerLE));
    }

    u8* data = (u8*)tgaData.StealData();
    return {data, tgaData.size()};
}
} // namespace tga
