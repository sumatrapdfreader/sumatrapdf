/*
 * WinDrawLib
 * Copyright (c) 2015-2018 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef WDL_H
#define WDL_H

#include <windows.h>
#include <objidl.h> /* IStream */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define WD_INLINE inline
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define WD_INLINE static inline
#elif defined(__GNUC__)
#define WD_INLINE static __inline__
#elif defined(_MSC_VER)
#define WD_INLINE static __inline
#else
#define WD_INLINE static
#endif

/***************
 ***  Color  ***
 ***************/

/* 32-bit integer type representing a color.
 *
 * The color is made of four 8-bit components: red, green, blue and also
 * (optionally) alpha.
 *
 * The red, green and blue components range from most intensive (255) to
 * least intensive (0), and the alpha component from fully opaque (255) to
 * fully transparent (0).
 */
typedef DWORD WD_COLOR;

#define WD_ARGB(a, r, g, b)                                                                      \
    ((((WD_COLOR)(a)&0xff) << 24) | (((WD_COLOR)(r)&0xff) << 16) | (((WD_COLOR)(g)&0xff) << 8) | \
     (((WD_COLOR)(b)&0xff) << 0))
#define WD_RGB(r, g, b) WD_ARGB(255, (r), (g), (b))

#define WD_AVALUE(color) (((WD_COLOR)(color)&0xff000000U) >> 24)
#define WD_RVALUE(color) (((WD_COLOR)(color)&0x00ff0000U) >> 16)
#define WD_GVALUE(color) (((WD_COLOR)(color)&0x0000ff00U) >> 8)
#define WD_BVALUE(color) (((WD_COLOR)(color)&0x000000ffU) >> 0)

/* Create WD_COLOR from GDI's COLORREF. */
#define WD_COLOR_FROM_GDI_EX(a, cref) WD_ARGB((a), GetRValue(cref), GetGValue(cref), GetBValue(cref))
#define WD_COLOR_FROM_GDI(cref) WD_COLOR_FROM_GDI_EX(255, (cref))

/* Get GDI's COLORREF from WD_COLOR. */
#define WD_COLOR_TO_GDI(color) RGB(WD_RVALUE(color), WD_GVALUE(color), WD_BVALUE(color))

/*****************************
 ***  2D Geometry Objects  ***
 *****************************/

typedef struct WD_POINT_tag WD_POINT;
struct WD_POINT_tag {
    float x;
    float y;
};

typedef struct WD_RECT_tag WD_RECT;
struct WD_RECT_tag {
    float x0;
    float y0;
    float x1;
    float y1;
};

typedef struct WD_MATRIX_tag WD_MATRIX;
struct WD_MATRIX_tag {
    float m11;
    float m12;
    float m21;
    float m22;
    float dx;
    float dy;
};

/************************
 ***  Initialization  ***
 ************************/

/* If the library is to be used in a context of multiple threads concurrently,
 * application has to provide pointers to synchronization functions.
 *
 * Note that even then, object instances (like e.g. canvas, brushes, images)
 * cannot be used concurrently, each thread must work with its own objects.
 *
 * This function may be called only once, prior to any other use of the library,
 * even prior any call to wdInitialize().
 *
 * Additionally wdPreInitialize() allows application to disable certain
 * features:
 *
 * WD_DISABLE_D2D: Disable D2D back-end.
 *
 * WD_DISABLE_GDIPLUS: Disable GDI+ back-end.
 *
 * Note: If all back-ends are disabled, wdInitialize() will subsequently fail.
 *
 * Note 2: wdPreinitialize() can (unlike wdInitialize()) be called from
 * DllMain() context.
 */

#define WD_DISABLE_D2D 0x0001
#define WD_DISABLE_GDIPLUS 0x0002

void wdPreInitialize(void (*fnLock)(void), void (*fnUnlock)(void), DWORD dwFlags);

/* Initialization functions may be called multiple times, even concurrently
 * (assuming a synchronization function have been provided via wdPreInitialize()).
 *
 * The library maintains a counter for each module and it gets really
 * uninitialized when the respective counter drops back to zero.
 *
 * The corresponding pairs of wdInitialize() and wdTerminate() should be
 * always called with the same flags, otherwise you may cause a resource leak.
 *
 * Note: These functions cannot be called from DllMain().
 */
#define WD_INIT_COREAPI 0x0000
#define WD_INIT_IMAGEAPI 0x0001
#define WD_INIT_STRINGAPI 0x0002

BOOL wdInitialize(DWORD dwFlags);
void wdTerminate(DWORD dwFlags);

/* Returns the current backend.
 * Returns -1 if there is none.
 */
#define WD_BACKEND_D2D 1
#define WD_BACKEND_GDIPLUS 2

int wdBackend(void);

/*******************************
 ***  Opaque Object Handles  ***
 *******************************/

typedef struct WD_BRUSH_tag* WD_HBRUSH;
typedef struct WD_HSTROKESTYLE_tag* WD_HSTROKESTYLE;
typedef struct WD_CANVAS_tag* WD_HCANVAS;
typedef struct WD_FONT_tag* WD_HFONT;
typedef struct WD_IMAGE_tag* WD_HIMAGE;
typedef struct WD_CACHEDIMAGE_tag* WD_HCACHEDIMAGE;
typedef struct WD_PATH_tag* WD_HPATH;

/***************************
 ***  Canvas Management  ***
 ***************************/

/* Canvas is an abstract object which can be painted with this library. */

/* The following flags modify default behavior of the canvas:
 *
 * WD_CANVAS_DOUBLEBUFFER: Enforces double-buffering. Note that Direct2D is
 * implicitly double-buffering so this option actually changes only behavior
 * of the GDI+ back-end.
 *
 * WD_CANVAS_NOGDICOMPAT: Disables GDI compatibility of the canvas. The canvas
 * can save some work at the cost the application cannot safely call
 * wdStartGdi().
 *
 * WD_CANVAS_LAYOUTRTL: By default, the canvas coordinate system has the
 * origin in the left top corner of the device context or window it is created
 * for. However with this flag the canvas shall have origin located in right
 * top corner and the x-coordinate shall grow to the left from it.
 */
#define WD_CANVAS_DOUBLEBUFFER 0x0001
#define WD_CANVAS_NOGDICOMPAT 0x0002
#define WD_CANVAS_LAYOUTRTL 0x0004

WD_HCANVAS wdCreateCanvasWithPaintStruct(HWND hWnd, PAINTSTRUCT* pPS, DWORD dwFlags);
WD_HCANVAS wdCreateCanvasWithHDC(HDC hDC, const RECT* pRect, DWORD dwFlags);
void wdDestroyCanvas(WD_HCANVAS hCanvas);

/* All drawing, filling and bit-blitting operations to it should be only
 * performed between wdBeginPaint() and wdEndPaint() calls.
 *
 * Note the canvas (and all resource created from it) may be cached for the
 * reuse only in the following circumstances (all conditions have to be met):
 *
 * - The canvas has been created with wdCreateCanvasWithPaintStruct() and
 *   is used strictly for handling WM_PAINT.
 * - wdEndPaint() returns TRUE.
 *
 * The cached canvas retains all the contents; so on the next WM_PAINT,
 * the application can repaint only those arts of the canvas which need
 * to present something new/different.
 */
void wdBeginPaint(WD_HCANVAS hCanvas);
BOOL wdEndPaint(WD_HCANVAS hCanvas);

/* This is supposed to be called to resize cached canvas (see above), if it
 * needs to be resized, typically as a response to WM_SIZE message.
 *
 * (Note however, that the painted contents of the canvas is lost.)
 */
BOOL wdResizeCanvas(WD_HCANVAS hCanvas, UINT uWidth, UINT uHeight);

/* Unless you create the canvas with the WD_CANVAS_NOGDICOMPAT flag, you may
 * also use GDI to paint on it. To do so, call wdStartGdi() to acquire HDC.
 * When done, release the HDC with wdEndGdi(). (Note that between those two
 * calls, only GDI can be used for painting on the canvas.)
 */
HDC wdStartGdi(WD_HCANVAS hCanvas, BOOL bKeepContents);
void wdEndGdi(WD_HCANVAS hCanvas, HDC hDC);

/* Clear the whole canvas with the given color. */
void wdClear(WD_HCANVAS hCanvas, WD_COLOR color);

/* If both, pRect and hPath are set, the clipping is set to the intersection
 * of both. If none of them is set, the clipping is reset and painting is not
 * clipped at all. */
void wdSetClip(WD_HCANVAS hCanvas, const WD_RECT* pRect, const WD_HPATH hPath);

/* The painting is by default measured in pixel units: 1.0f corresponds to
 * the pixel width or height, depending on the current axis.
 *
 * Origin (the point [0.0f, 0.0f]) corresponds always the top left pixel of
 * the canvas.
 *
 * Though this can be changed if a transformation is applied on the canvas.
 * Transformation is determined by a matrix which can specify translation,
 * rotation and scaling (in both axes), or any combination of these operations.
 */
void wdRotateWorld(WD_HCANVAS hCanvas, float cx, float cy, float fAngle);
void wdTranslateWorld(WD_HCANVAS hCanvas, float dx, float dy);
void wdTransformWorld(WD_HCANVAS hCanvas, const WD_MATRIX* pMatrix);
void wdResetWorld(WD_HCANVAS hCanvas);

/**************************
 ***  Image Management  ***
 **************************/

/* All these functions are usable only if the library has been initialized with
 * the flag WD_INIT_IMAGEAPI.
 *
 * Note that unlike most other resources (including WD_HCACHEDIMAGE), WD_HIMAGE
 * is not canvas-specific and can be used for painting on any canvas.
 */

/* For wdCreateImageFromBuffer */
#define WD_PIXELFORMAT_PALETTE 1                /* 1 byte per pixel. cPalette is used */
#define WD_PIXELFORMAT_R8G8B8 2                 /* 3 bytes per pixel. RGB24 */
#define WD_PIXELFORMAT_R8G8B8A8 3               /* 4 bytes per pixel. RGBA32 */
#define WD_PIXELFORMAT_B8G8R8A8 4               /* 4 bytes per pixel. BGRA32 (and bottom-up; as GDI usually expects) */
#define WD_PIXELFORMAT_B8G8R8A8_PREMULTIPLIED 5 /* Same but with pre-multiplied alpha */

#define WD_ALPHA_IGNORE 0
#define WD_ALPHA_USE 1               /* Note: Only bitmaps with RGBA32 pixel format are supported. */
#define WD_ALPHA_USE_PREMULTIPLIED 2 /* Note: Only bitmaps with RGBA32 pixel format are supported. */

WD_HIMAGE wdCreateImageFromHBITMAP(HBITMAP hBmp);
WD_HIMAGE wdCreateImageFromHBITMAPWithAlpha(HBITMAP hBmp, int alphaMode);
WD_HIMAGE wdLoadImageFromFile(const WCHAR* pszPath);
WD_HIMAGE wdLoadImageFromIStream(IStream* pStream);
WD_HIMAGE wdLoadImageFromResource(HINSTANCE hInstance, const WCHAR* pszResType, const WCHAR* pszResName);
WD_HIMAGE wdCreateImageFromBuffer(UINT uWidth, UINT uHeight, UINT uStride, const BYTE* pBuffer, int pixelFormat,
                                  const COLORREF* cPalette, UINT uPaletteSize);
void wdDestroyImage(WD_HIMAGE hImage);

void wdGetImageSize(WD_HIMAGE hImage, UINT* puWidth, UINT* puHeight);

/*********************************
 ***  Cached Image Management  ***
 *********************************/

/* All these functions are usable only if the library has been initialized with
 * the flag WD_INIT_IMAGEAPI.
 *
 * Cached image is an image which is converted to the right pixel format for
 * faster rendering on the given canvas. It can only be used for the canvas
 * it has been created for.
 *
 * In other words, you may see WD_HCACHEDIMAGE as a counterpart to device
 * dependent bitmap, and WD_HIMAGE as a counterpart to device-independent
 * bitmap.
 *
 * In short WD_HIMAGE is more flexible and easier to use, while WD_HCACHEDIMAGE
 * requires more care from the developer but provides better performance,
 * especially when used repeatedly.
 *
 * All these functions are usable only if the library has been initialized with
 * the flag WD_INIT_IMAGEAPI.
 */

WD_HCACHEDIMAGE wdCreateCachedImage(WD_HCANVAS hCanvas, WD_HIMAGE hImage);
void wdDestroyCachedImage(WD_HCACHEDIMAGE hCachedImage);

/**************************
 ***  Brush Management  ***
 **************************/

/* Brush is an object used for drawing operations. Note the brush can only
 * be used for the canvas it has been created for. */

WD_HBRUSH wdCreateSolidBrush(WD_HCANVAS hCanvas, WD_COLOR color);
WD_HBRUSH wdCreateLinearGradientBrushEx(WD_HCANVAS hCanvas, float x0, float y0, float x1, float y1,
                                        const WD_COLOR* colors, const float* offsets, UINT numStops);
WD_HBRUSH wdCreateLinearGradientBrush(WD_HCANVAS hCanvas, float x0, float y0, WD_COLOR color0, float x1, float y1,
                                      WD_COLOR color1);
WD_HBRUSH wdCreateRadialGradientBrushEx(WD_HCANVAS hCanvas, float cx, float cy, float r, float fx, float fy,
                                        const WD_COLOR* colors, const float* offsets, UINT numStops);
WD_HBRUSH wdCreateRadialGradientBrush(WD_HCANVAS hCanvas, float cx, float cy, float r, WD_COLOR color0,
                                      WD_COLOR color1);
void wdDestroyBrush(WD_HBRUSH hBrush);

/* Can be only called for brushes created with wdCreateSolidBrush(). */
void wdSetSolidBrushColor(WD_HBRUSH hBrush, WD_COLOR color);

/*********************************
 ***  Stroke Style Management  ***
 ********************************/

/* Stroke Style is an object used for drawing operations.
   All drawing functions accept NULL as the stroke style parameter.
*/

#define WD_DASHSTYLE_SOLID 0 /* default */
#define WD_DASHSTYLE_DASH 1
#define WD_DASHSTYLE_DOT 2
#define WD_DASHSTYLE_DASHDOT 3
#define WD_DASHSTYLE_DASHDOTDOT 4

#define WD_LINECAP_FLAT 0 /* default */
#define WD_LINECAP_SQUARE 1
#define WD_LINECAP_ROUND 2
#define WD_LINECAP_TRIANGLE 3

#define WD_LINEJOIN_MITER 0 /* default */
#define WD_LINEJOIN_BEVEL 1
#define WD_LINEJOIN_ROUND 2

WD_HSTROKESTYLE wdCreateStrokeStyle(UINT dashStyle, UINT lineCap, UINT lineJoin);
WD_HSTROKESTYLE wdCreateStrokeStyleCustom(const float* dashes, UINT dashesCount, UINT lineCap, UINT lineJoin);
void wdDestroyStrokeStyle(WD_HSTROKESTYLE hStrokeStyle);

/*************************
 ***  Path Management  ***
 *************************/

/* Path is an object representing more complex and reusable shapes which can
 * be painted at once. Note the path can only be used for the canvas it has
 * been created for. */

WD_HPATH wdCreatePath(WD_HCANVAS hCanvas);
WD_HPATH wdCreatePolygonPath(WD_HCANVAS hCanvas, const WD_POINT* pPoints, UINT uCount);
WD_HPATH wdCreateRoundedRectPath(WD_HCANVAS hCanvas, const WD_RECT* prc, float r);
void wdDestroyPath(WD_HPATH hPath);

typedef struct WD_PATHSINK_tag WD_PATHSINK;
struct WD_PATHSINK_tag {
    void* pData;
    WD_POINT ptEnd;
};

BOOL wdOpenPathSink(WD_PATHSINK* pSink, WD_HPATH hPath);
void wdClosePathSink(WD_PATHSINK* pSink);

void wdBeginFigure(WD_PATHSINK* pSink, float x, float y);
void wdEndFigure(WD_PATHSINK* pSink, BOOL bCloseFigure);

void wdAddLine(WD_PATHSINK* pSink, float x, float y);
void wdAddArc(WD_PATHSINK* pSink, float cx, float cy, float fSweepAngle);
void wdAddBezier(WD_PATHSINK* pSink, float x0, float y0, float x1, float y1, float x2, float y2);

/*************************
 ***  Font Management  ***
 *************************/

/* All these functions are usable only if the library has been initialized with
 * the flag WD_INIT_DRAWSTRINGAPI.
 *
 * Also note that usage of non-TrueType fonts is not supported by GDI+
 * so attempt to create such WD_HFONT will fall back to a default GUI font.
 */

WD_HFONT wdCreateFont(const LOGFONTW* pLogFont);
WD_HFONT wdCreateFontWithGdiHandle(HFONT hGdiFont);
void wdDestroyFont(WD_HFONT hFont);

/* Structure describing metrics of the font. */
typedef struct WD_FONTMETRICS_tag WD_FONTMETRICS;
struct WD_FONTMETRICS_tag {
    float fEmHeight; /* Typically height of letter 'M' or 'H' */
    float fAscent;   /* Height of char cell above the base line. */
    float fDescent;  /* Height of char cell below the base line. */
    float fLeading;  /* Distance of two base lines in multi-line text. */

    /* Usually: fEmHeight < fAscent + fDescent <= fLeading */
};

void wdFontMetrics(WD_HFONT hFont, WD_FONTMETRICS* pMetrics);

/*************************
 ***  Draw Operations  ***
 *************************/

void wdDrawEllipseArcStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                            float fBaseAngle, float fSweepAngle, float fStrokeWidth, WD_HSTROKESTYLE hStrokeStyle);
void wdDrawEllipsePieStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                            float fBaseAngle, float fSweepAngle, float fStrokeWidth, WD_HSTROKESTYLE hStrokeStyle);
void wdDrawEllipseStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                         float fStrokeWidth, WD_HSTROKESTYLE hStrokeStyle);
void wdDrawLineStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float x0, float y0, float x1, float y1, float fStrokeWidth,
                      WD_HSTROKESTYLE hStrokeStyle);
void wdDrawPathStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_HPATH hPath, float fStrokeWidth,
                      WD_HSTROKESTYLE hStrokeStyle);
void wdDrawRectStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float x0, float y0, float x1, float y1, float fStrokeWidth,
                      WD_HSTROKESTYLE hStrokeStyle);

WD_INLINE void wdDrawArcStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r, float fBaseAngle,
                               float fSweepAngle, float fStrokeWidth, WD_HSTROKESTYLE hStrokeStyle) {
    wdDrawEllipseArcStyled(hCanvas, hBrush, cx, cy, r, r, fBaseAngle, fSweepAngle, fStrokeWidth, hStrokeStyle);
}

WD_INLINE void wdDrawCircleStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r, float fStrokeWidth,
                                  WD_HSTROKESTYLE hStrokeStyle) {
    wdDrawEllipseStyled(hCanvas, hBrush, cx, cy, r, r, fStrokeWidth, hStrokeStyle);
}

WD_INLINE void wdDrawPieStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r, float fBaseAngle,
                               float fSweepAngle, float fStrokeWidth, WD_HSTROKESTYLE hStrokeStyle) {
    wdDrawEllipsePieStyled(hCanvas, hBrush, cx, cy, r, r, fBaseAngle, fSweepAngle, fStrokeWidth, hStrokeStyle);
}

WD_INLINE void wdDrawArc(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r, float fBaseAngle,
                         float fSweepAngle, float fStrokeWidth) {
    wdDrawArcStyled(hCanvas, hBrush, cx, cy, r, fBaseAngle, fSweepAngle, fStrokeWidth, NULL);
}

WD_INLINE void wdDrawCircle(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r, float fStrokeWidth) {
    wdDrawCircleStyled(hCanvas, hBrush, cx, cy, r, fStrokeWidth, NULL);
}

WD_INLINE void wdDrawEllipse(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                             float fStrokeWidth) {
    wdDrawEllipseStyled(hCanvas, hBrush, cx, cy, rx, ry, fStrokeWidth, NULL);
}

WD_INLINE void wdDrawEllipseArc(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                                float fBaseAngle, float fSweepAngle, float fStrokeWidth) {
    wdDrawEllipseArcStyled(hCanvas, hBrush, cx, cy, rx, ry, fBaseAngle, fSweepAngle, fStrokeWidth, NULL);
}

WD_INLINE void wdDrawEllipsePie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                                float fBaseAngle, float fSweepAngle, float fStrokeWidth) {
    wdDrawEllipsePieStyled(hCanvas, hBrush, cx, cy, rx, ry, fBaseAngle, fSweepAngle, fStrokeWidth, NULL);
}

WD_INLINE void wdDrawLine(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float x0, float y0, float x1, float y1,
                          float fStrokeWidth) {
    wdDrawLineStyled(hCanvas, hBrush, x0, y0, x1, y1, fStrokeWidth, NULL);
}

WD_INLINE void wdDrawPath(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_HPATH hPath, float fStrokeWidth) {
    wdDrawPathStyled(hCanvas, hBrush, hPath, fStrokeWidth, NULL);
}

WD_INLINE void wdDrawPie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r, float fBaseAngle,
                         float fSweepAngle, float fStrokeWidth) {
    wdDrawPieStyled(hCanvas, hBrush, cx, cy, r, fBaseAngle, fSweepAngle, fStrokeWidth, NULL);
}

WD_INLINE void wdDrawRect(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float x0, float y0, float x1, float y1,
                          float fStrokeWidth) {
    wdDrawRectStyled(hCanvas, hBrush, x0, y0, x1, y1, fStrokeWidth, NULL);
}

/*************************
 ***  Fill Operations  ***
 *************************/

void wdFillEllipse(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry);
void wdFillEllipsePie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry, float fBaseAngle,
                      float fSweepAngle);
void wdFillPath(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_HPATH hPath);
void wdFillRect(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float x0, float y0, float x1, float y1);

WD_INLINE void wdFillCircle(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r) {
    wdFillEllipse(hCanvas, hBrush, cx, cy, r, r);
}

WD_INLINE void wdFillPie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float r, float fBaseAngle,
                         float fSweepAngle) {
    wdFillEllipsePie(hCanvas, hBrush, cx, cy, r, r, fBaseAngle, fSweepAngle);
}

/*****************************
 ***  Bit-Blit Operations  ***
 *****************************/

/* All these functions are usable only if the library has been initialized
 * with the flag WD_INIT_IMAGEAPI.
 *
 * These functions are capable of bit-blit operation from some source image
 * to a destination canvas. If source and target rectangles gave different
 * dimensions, the functions scale the image during the operation.
 *
 * Note the destination rectangle has to be always specified. Source rectangle
 * is optional: If NULL, whole source image is taken.
 *
 */
void wdBitBltImage(WD_HCANVAS hCanvas, const WD_HIMAGE hImage, const WD_RECT* pDestRect, const WD_RECT* pSourceRect);
void wdBitBltCachedImage(WD_HCANVAS hCanvas, const WD_HCACHEDIMAGE hCachedImage, float x, float y);
void wdBitBltHICON(WD_HCANVAS hCanvas, HICON hIcon, const WD_RECT* pDestRect, const WD_RECT* pSourceRect);

/****************************
 ***  Simple Text Output  ***
 ****************************/

/* Functions for basic string output. Note the functions operate strictly with
 * Unicode strings.
 *
 * All these functions are usable only if the library has been initialized with
 * the flag WD_INIT_DRAWSTRINGAPI.
 */

/* Flags specifying alignment and various rendering options.
 *
 * Note GDI+ back-end does not support ellipses in case of multi-line string,
 * so the ellipsis flags should be only used together with WD_STR_NOWRAP.
 */
#define WD_STR_LEFTALIGN 0x0000
#define WD_STR_CENTERALIGN 0x0001
#define WD_STR_RIGHTALIGN 0x0002
#define WD_STR_TOPALIGN 0x0000
#define WD_STR_MIDDLEALIGN 0x0004
#define WD_STR_BOTTOMALIGN 0x0008
#define WD_STR_NOCLIP 0x0010
#define WD_STR_NOWRAP 0x0020
#define WD_STR_ENDELLIPSIS 0x0040
#define WD_STR_WORDELLIPSIS 0x0080
#define WD_STR_PATHELLIPSIS 0x0100

#define WD_STR_ALIGNMASK (WD_STR_LEFTALIGN | WD_STR_CENTERALIGN | WD_STR_RIGHTALIGN)
#define WD_STR_VALIGNMASK (WD_STR_TOPALIGN | WD_STR_MIDDLEALIGN | WD_STR_BOTTOMALIGN)
#define WD_STR_ELLIPSISMASK (WD_STR_ENDELLIPSIS | WD_STR_WORDELLIPSIS | WD_STR_PATHELLIPSIS)

void wdDrawString(WD_HCANVAS hCanvas, WD_HFONT hFont, const WD_RECT* pRect, const WCHAR* pszText, int iTextLength,
                  WD_HBRUSH hBrush, DWORD dwFlags);

/* Note hCanvas here is optional. If hCanvas == NULL, GDI+ uses screen
 * for the computation; D2D back-end ignores that parameter altogether.
 */
void wdMeasureString(WD_HCANVAS hCanvas, WD_HFONT hFont, const WD_RECT* pRect, const WCHAR* pszText, int iTextLength,
                     WD_RECT* pResult, DWORD dwFlags);

/* Convenient wdMeasureString() wrapper. */
float wdStringWidth(WD_HCANVAS hCanvas, WD_HFONT hFont, const WCHAR* pszText);
float wdStringHeight(WD_HFONT hFont, const WCHAR* pszText);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WDL_H */
