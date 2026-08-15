/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SumatraPDF_GuiMacBridge_h
#define SumatraPDF_GuiMacBridge_h

#include <stddef.h>
#include <stdint.h>

struct MacGuiRect {
    int x;
    int y;
    int dx;
    int dy;
};

struct MacGuiPointerEvent {
    int type;
    int x;
    int y;
    int button;
    bool isCtrl;
    bool isShift;
    bool isAlt;
};

struct MacGuiKeyEvent {
    int codepoint;
    bool isCtrl;
    bool isShift;
    bool isAlt;
};

using MacGuiPaintCallback = void (*)(void*, void*, int, int);
using MacGuiPointerCallback = bool (*)(void*, const MacGuiPointerEvent*);
using MacGuiKeyCallback = bool (*)(void*, const MacGuiKeyEvent*);
using MacGuiCloseCallback = void (*)(void*);

struct MacGuiWindowCallbacks {
    MacGuiPaintCallback paint;
    MacGuiPointerCallback pointer;
    MacGuiKeyCallback key;
    MacGuiCloseCallback close;
};

enum MacGuiTextFlags : uint32_t {
    MacGuiTextCenter = 1 << 0,
    MacGuiTextRight = 1 << 1,
    MacGuiTextEllipsis = 1 << 2,
    MacGuiTextPathEllipsis = 1 << 3,
    MacGuiTextSingleLine = 1 << 4,
    MacGuiTextVCenter = 1 << 5,
    MacGuiTextNoClip = 1 << 6,
    MacGuiTextWrap = 1 << 7,
};

void* MacGuiFontCreate(const char* name, int nameLen, float sizePt, bool bold, bool italic);
void MacGuiFontMeasure(void* font, const char* text, int textLen, int maxDx, int* dx, int* dy);
float MacGuiDefaultFontSize();

void* MacGuiWindowCreate(void* parent, const char* title, int titleLen, int dx, int dy, bool visible, bool frameless,
                         bool resizable, void* userData, const MacGuiWindowCallbacks* callbacks);
void MacGuiWindowDestroy(void* window);
MacGuiRect MacGuiWindowClientRect(void* window);
MacGuiRect MacGuiWindowScreenRect(void* window);
MacGuiRect MacGuiWindowWorkArea(void* window);
void MacGuiWindowSetBounds(void* window, MacGuiRect rect);
void MacGuiWindowShow(void* window, bool show);
void MacGuiWindowFocus(void* window);
void MacGuiWindowInvalidate(void* window);
void MacGuiWindowSetCursor(void* window, int cursor);
void MacGuiWindowBeginMove(void* window);
bool MacGuiWindowIsMaximized(void* window);
void MacGuiWindowActivateIfForeground(void* window);
void MacGuiPostTask(void (*fn)(void*), void* data);

void MacGuiFillRect(void* ctx, MacGuiRect rect, uint32_t color, uint8_t alpha);
void MacGuiDrawRect(void* ctx, MacGuiRect rect, uint32_t color, int thickness, bool dashed);
void MacGuiFillRoundedRect(void* ctx, MacGuiRect rect, int radius, uint32_t fill, bool hasFill, uint32_t border,
                           bool hasBorder);
void MacGuiFillEllipse(void* ctx, MacGuiRect rect, uint32_t color, uint8_t alpha);
void MacGuiDrawLine(void* ctx, int x1, int y1, int x2, int y2, uint32_t color, float thickness, uint8_t alpha);
void MacGuiDrawText(void* ctx, const char* text, int textLen, MacGuiRect rect, uint32_t flags, void* font,
                    uint32_t color);
void MacGuiDrawPixmap(void* ctx, const uint8_t* data, int width, int height, int stride, MacGuiRect rect);
void MacGuiPushClip(void* ctx, MacGuiRect rect);
void MacGuiPopClip(void* ctx);
bool MacGuiSetMirrored(void* ctx, bool mirrored);

#endif
