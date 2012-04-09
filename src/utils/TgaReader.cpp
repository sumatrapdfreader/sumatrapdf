/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "TgaReader.h"

using namespace Gdiplus;

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

#pragma pack(push)
#pragma pack(1)

struct TgaHeader {
    uint8_t     idLength;
    uint8_t     cmapType;
    uint8_t     imageType;
    uint16_t    cmapFirstEntry;
    uint16_t    cmapLength;
    uint8_t     cmapBitDepth;
    uint16_t    offsetX, offsetY;
    uint16_t    width, height;
    uint8_t     bitDepth;
    uint8_t     flags;
};

struct TgaFooter {
    uint32_t    extAreaOffset;
    uint32_t    devAreaOffset;
    const char  signature[18];
};

struct TgaExtArea {
    uint16_t    size;
    uint8_t     fields_11_to_23[492];
    uint8_t     alphaType;
};

#pragma pack(pop)

static bool HasVersion2Footer(const char *data, size_t len)
{
    if (len < sizeof(TgaHeader) + sizeof(TgaFooter))
        return false;
    TgaFooter *footer = (TgaFooter *)(data + len - sizeof(TgaFooter));
    return str::EqN(footer->signature, TGA_FOOTER_SIGNATURE, sizeof(footer->signature));
}

// note: we only support the more common bit depths:
// http://www.ryanjuckett.com/programming/graphics/26-parsing-colors-in-a-tga-file
static PixelFormat GetPixelFormat(TgaHeader *header, ImageAlpha aType=Alpha_Normal)
{
    int bits = header->bitDepth;
    int alphaBits = (header->flags & Flag_Alpha);

    if (Type_Palette == header->imageType || Type_Palette_RLE == header->imageType) {
        if (1 != header->cmapType || 8 != header->bitDepth && 16 != header->bitDepth)
            return 0;
        bits = header->cmapBitDepth;
    }
    else if (Type_Truecolor == header->imageType || Type_Truecolor_RLE == header->imageType) {
        if (0 != header->cmapType)
            return 0;
    }
    else if (Type_Grayscale == header->imageType || Type_Grayscale_RLE == header->imageType) {
        if (0 != header->cmapType || 8 != header->bitDepth || 0 != alphaBits)
            return 0;
        // using a non-indexed format so that we don't have to bother with a palette
        return PixelFormat24bppRGB;
    }
    else
        return 0;

    if (15 == bits && 0 == alphaBits)
        return PixelFormat16bppRGB555;
    if (16 == bits && (0 == alphaBits || Alpha_Ignore == aType))
        return PixelFormat16bppRGB555;
    if (16 == bits && 1 == alphaBits)
        return PixelFormat16bppARGB1555;
    if (24 == bits && 0 == alphaBits)
        return PixelFormat24bppRGB;
    if (32 == bits && (0 == alphaBits || Alpha_Ignore == aType))
        return PixelFormat32bppRGB;
    if (32 == bits && 8 == alphaBits && Alpha_Normal == aType)
        return PixelFormat32bppARGB;
    if (32 == bits && 8 == alphaBits && Alpha_Premultiplied == aType)
        return PixelFormat32bppPARGB;
    return 0;
}

static ImageAlpha GetAlphaType(const char *data, size_t len)
{
    if (!HasVersion2Footer(data, len))
        return Alpha_Normal;

    TgaFooter *footer = (TgaFooter *)(data + len - sizeof(TgaFooter));
    if (LEtoHl(footer->extAreaOffset) >= sizeof(TgaHeader) &&
        LEtoHl(footer->extAreaOffset) + sizeof(TgaExtArea) + sizeof(TgaFooter) <= len) {
        TgaExtArea *extArea = (TgaExtArea *)(data + LEtoHl(footer->extAreaOffset));
        if (LEtoHs(extArea->size) >= sizeof(TgaExtArea)) {
            switch (extArea->alphaType) {
            case Alpha_Normal:          return Alpha_Normal;
            case Alpha_Premultiplied:   return Alpha_Premultiplied;
            default:                    return Alpha_Ignore;
            }
        }
    }

    return Alpha_Normal;
}

// checks whether this could be data for a TGA image
bool HasSignature(const char *data, size_t len)
{
    if (HasVersion2Footer(data, len))
        return true;
    // fall back to checking for values that would be valid for a TGA image
    if (len < sizeof(TgaHeader))
        return false;
    TgaHeader *header = (TgaHeader *)data;
    if ((header->flags & Flag_Reserved))
        return false;
    if (!GetPixelFormat(header))
        return false;
    return true;

}

struct ReadState {
    TgaHeader *header;
    const char *data;
    const char *end;
    const char *colormap;
    int repeat;
    int n;
};

static void ReadPixel(ReadState& s, char *dst)
{
    bool isRLE = s.header->imageType >= 8;
    if (isRLE && 0 == s.repeat && s.data < s.end) {
        s.repeat = ((*s.data & 0x7F) + 1) * ((*s.data & 0x80) ? -1 : 1);
        s.data++;
    }
    if (s.data + s.n > s.end)
        return;

    int idx;
    switch (s.header->imageType) {
    case Type_Palette: case Type_Palette_RLE:
        idx = (uint8_t)s.data[0] + (2 == s.n ? ((uint8_t)s.data[1] << 8) : 0) - LEtoHs(s.header->cmapFirstEntry);
        if (0 <= idx && idx < LEtoHs(s.header->cmapLength)) {
            int n2 = (s.header->cmapBitDepth + 7) / 8;
            for (int k = 0; k < n2; k++) {
                dst[k] = s.colormap[idx * n2 + k];
            }
        }
        break;
    case Type_Truecolor: case Type_Truecolor_RLE:
        for (int k = 0; k < s.n; k++) {
            dst[k] = s.data[k];
        }
        break;
    case Type_Grayscale: case Type_Grayscale_RLE:
        dst[0] = dst[1] = dst[2] = s.data[0];
        break;
    }

    s.data += s.n;
    if (isRLE) {
        if (s.repeat > 0)
            s.repeat--;
        else if (++s.repeat < 0)
            s.data -= s.n;
    }
}

Gdiplus::Bitmap *ImageFromData(const char *data, size_t len)
{
    if (len < sizeof(TgaHeader))
        return NULL;

    ReadState s = { 0 };
    s.header = (TgaHeader *)data;
    s.data = data + sizeof(TgaHeader) + s.header->idLength;
    s.end = data + len;
    if (1 == s.header->cmapType) {
        s.colormap = s.data;
        s.data += LEtoHs(s.header->cmapLength) * ((s.header->cmapBitDepth + 7) / 8);
    }
    s.n = (s.header->bitDepth + 7) / 8;

    PixelFormat format = GetPixelFormat(s.header, GetAlphaType(data, len));
    if (!format)
        return NULL;

    int w = LEtoHs(s.header->width);
    int h = LEtoHs(s.header->height);
    int n = ((format >> 8) & 0x3F) / 8;
    bool invertX = (s.header->flags & Flag_InvertX);
    bool invertY = (s.header->flags & Flag_InvertY);

    Bitmap bmp(w, h, format);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&Rect(0, 0, w, h), ImageLockModeWrite, format, &bmpData);
    if (ok != Ok)
        return NULL;
    for (int y = 0; y < h; y++) {
        char *rowOut = (char *)bmpData.Scan0 + bmpData.Stride * (invertY ? y : h - y - 1);
        for (int x = 0; x < w; x++) {
            ReadPixel(s, rowOut + n * (invertX ? w - x - 1 : x));
        }
    }
    bmp.UnlockBits(&bmpData);
    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, format);
}

inline bool memeq3(char *pix1, char *pix2)
{
    return *(WORD *)pix1 == *(WORD *)pix2 && pix1[2] == pix2[2];
}

unsigned char *SerializeBitmap(HBITMAP hbmp, size_t *bmpBytesOut)
{
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    if ((ULONG)bmpInfo.bmWidth > USHRT_MAX || (ULONG)bmpInfo.bmHeight > USHRT_MAX)
        return 0;

    WORD w = (WORD)bmpInfo.bmWidth;
    WORD h = (WORD)bmpInfo.bmHeight;
    int stride = ((w * 3 + 3) / 4) * 4;
    ScopedMem<char> bmpData((char *)malloc(stride * h));
    if (!bmpData)
        return NULL;

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hDC = GetDC(NULL);
    if (!GetDIBits(hDC, hbmp, 0, h, bmpData, &bmi, DIB_RGB_COLORS)) {
        ReleaseDC(NULL, hDC);
        return NULL;
    }
    ReleaseDC(NULL, hDC);

    TgaHeader header = { 0 };
    header.imageType = Type_Truecolor_RLE;
    header.width = LEtoHs(w);
    header.height = LEtoHs(h);
    header.bitDepth = 24;
    TgaFooter footer = { 0, 0, TGA_FOOTER_SIGNATURE };

    str::Str<char> tgaData;
    tgaData.Append((char *)&header, sizeof(header));
    for (int k = 0; k < h; k++) {
        char *line = bmpData + k * stride;
        for (int i = 0, j = 1; i < w; i += j, j = 1) {
            // determine the length of a run of identical pixels
            while (i + j < w && j < 128 && memeq3(line + i * 3, line + (i + j) * 3)) {
                j++;
            }
            if (j > 1) {
                tgaData.Append(j - 1 + 128);
                tgaData.Append(line + i * 3, 3);
            } else {
                // determine the length of a run of different pixels
                while (i + j < w && j < 128 && !memeq3(line + (i + j - 1) * 3, line + (i + j) * 3)) {
                    j++;
                }
                tgaData.Append(j - 1);
                tgaData.Append(line + i * 3, j * 3);
            }
        }
    }
    tgaData.Append((char *)&footer, sizeof(footer));

    if (bmpBytesOut)
        *bmpBytesOut = tgaData.Size();
    return (unsigned char *)tgaData.StealData();
}

}
