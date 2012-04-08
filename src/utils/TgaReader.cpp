/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "TgaReader.h"
#include "WinUtil.h"

using namespace Gdiplus;

namespace tga {

#define TGA_FOOTER_SIGNATURE "TRUEVISION-XFILE.\0"

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

#pragma pack(pop)

// note: we only support the more common bit depths:
// http://www.ryanjuckett.com/programming/graphics/26-parsing-colors-in-a-tga-file
static PixelFormat GetPixelFormat(TgaHeader *header, ImageAlpha aType=Alpha_Normal)
{
    int bits = header->bitDepth;
    int alphaBits = (header->flags & Flag_Alpha);

    if (Type_Palette == header->imageType || Type_Palette_RLE == header->imageType) {
        if (header->bitDepth != 8 && header->bitDepth != 16 || 1 != header->cmapType)
            return 0;
        bits = header->cmapBitDepth;
    }
    else if (Type_Truecolor == header->imageType || Type_Truecolor_RLE == header->imageType) {
        if (header->cmapType != 0)
            return 0;
    }
    else if (Type_Grayscale == header->imageType || Type_Grayscale_RLE == header->imageType) {
        if (0 == header->cmapType && 8 == header->bitDepth && 0 == alphaBits)
            return PixelFormat24bppRGB;
        return 0;
    }
    else
        return 0;

    if (15 == bits && 0 == alphaBits)
        return PixelFormat16bppRGB555;
    if (16 == bits && (1 == alphaBits || 0 == alphaBits))
        return PixelFormat16bppRGB555;
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

static bool HasTgaFooter(const char *data, size_t len)
{
    return len >= sizeof(TgaHeader) + 26 &&
           str::EqN(data + len - 18, TGA_FOOTER_SIGNATURE, 18);
}

bool HasSignature(const char *data, size_t len)
{
    // check for TGA version 2 footer
    if (HasTgaFooter(data, len))
        return true;
    // fall back to checking for values that would be valid for a TGA image
    if (len < sizeof(TgaHeader))
        return false;
    TgaHeader *header = (TgaHeader *)data;
    if (!GetPixelFormat(header))
        return false;
    if ((header->flags & Flag_Reserved))
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

    ImageAlpha aType = Alpha_Normal;
    if (HasTgaFooter(data, len)) {
        uint32_t extensionArea = LEtoHl(*(uint32_t *)(data + len - 26));
        if (data + extensionArea >= s.data && extensionArea + 495 + 26 <= len) {
            uint8_t f24 = data[extensionArea + 494];
            aType = 3 == f24 ? Alpha_Normal : 4 == f24 ? Alpha_Premultiplied : Alpha_Ignore;
        }
    }

    PixelFormat format = GetPixelFormat(s.header, aType);
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
    SizeI size = GetBitmapSize(hbmp);
    int stride = ((size.dx * 3 + 3) / 4) * 4;
    size_t bmpBytes;
    ScopedMem<char> bmpData((char *)::SerializeBitmap(hbmp, &bmpBytes));
    if (!bmpData)
        return NULL;

    str::Str<char> tgaData;

    TgaHeader header = { 0 };
    header.imageType = Type_Truecolor_RLE;
    header.width = LEtoHs(size.dx);
    header.height = LEtoHs(size.dy);
    header.bitDepth = 24;
    tgaData.Append((char *)&header, sizeof(header));

    for (int k = 0; k < size.dy; k++) {
        char *line = bmpData + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFO) + k * stride;
        for (int i = 0, j = 1; i < size.dx; i += j, j = 1) {
            // determine the length of a run of identical pixels
            while (i + j < size.dx && j < 128 && memeq3(line + i * 3, line + (i + j) * 3)) {
                j++;
            }
            if (j > 1) {
                tgaData.Append(j - 1 + 128);
                tgaData.Append(line + i * 3, 3);
            } else {
                // determine the length of a run of different pixels
                while (i + j < size.dx && j < 128 && !memeq3(line + (i + j - 1) * 3, line + (i + j) * 3)) {
                    j++;
                }
                tgaData.Append(j - 1);
                tgaData.Append(line + i * 3, j * 3);
            }
        }
    }
    tgaData.Append("\0\0\0\0\0\0\0\0" TGA_FOOTER_SIGNATURE, 26);

    if (bmpBytesOut)
        *bmpBytesOut = tgaData.Size();
    return (unsigned char *)tgaData.StealData();
}

}
