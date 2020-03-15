#define LICE_TEXT_NO_DECLARE_CACHEDFONT

#ifdef LICE_IMPORT_INTERFACE_ONLY
#define LICE_FUNC_DEF_DECL extern
#else
#define LICE_FUNC_DEF_DECL
#endif

#include "lice_text.h"
LICE_FUNC_DEF_DECL LICE_IBitmap *(*__LICE_CreateBitmap)(int, int, int);
LICE_FUNC_DEF_DECL void (*__LICE_PutPixel)(LICE_IBitmap* dest, int x, int y, LICE_pixel color, float alpha, int mode);
LICE_FUNC_DEF_DECL void (*__LICE_Line)(LICE_IBitmap *dest, int x1, int y1, int x2, int y2, LICE_pixel color, float alpha, int mode, bool aa);
LICE_FUNC_DEF_DECL void (*__LICE_FLine)(LICE_IBitmap *dest, float x1, float y1, float x2, float y2, LICE_pixel color, float alpha, int mode, bool aa);
LICE_FUNC_DEF_DECL void (*__LICE_FillRect)(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel color, float alpha , int mode);
LICE_FUNC_DEF_DECL void (*__LICE_DrawRect)(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel color, float alpha , int mode);
LICE_FUNC_DEF_DECL void (*__LICE_BorderedRect)(LICE_IBitmap *dest, int x, int y, int w, int h, LICE_pixel bgcolor, LICE_pixel fgcolor, float alpha, int mode);
LICE_FUNC_DEF_DECL void (*__LICE_Circle)(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha, int mode, bool aa);
LICE_FUNC_DEF_DECL void (*__LICE_FillCircle)(LICE_IBitmap* dest, float cx, float cy, float r, LICE_pixel color, float alpha, int mode, bool aa);
LICE_FUNC_DEF_DECL void (*__LICE_Clear)(LICE_IBitmap *dest, LICE_pixel color);
LICE_FUNC_DEF_DECL void (*__LICE_Blit)(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int srcx, int srcy, int srcw, int srch, float alpha, int mode);
LICE_FUNC_DEF_DECL void (*__LICE_RotatedBlit)(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int dstw, int dsth, float srcx, float srcy, float srcw, float srch, float angle, bool cliptosourcerect, float alpha, int mode, float rotxcent, float rotycent);
LICE_FUNC_DEF_DECL void (*__LICE_DrawGlyph)(LICE_IBitmap* dest, int x, int y, LICE_pixel color, LICE_pixel_chan* glyph, int glyph_w, int glyph_h, float alpha, int mode);
LICE_FUNC_DEF_DECL void (*__LICE_FillTriangle)(LICE_IBitmap *dest, int x1, int y1, int x2, int y2, int x3, int y3, LICE_pixel color, float alpha, int mode);
LICE_FUNC_DEF_DECL void (*__LICE_Arc)(LICE_IBitmap* dest, float cx, float cy, float r, float alo, float ahi, LICE_pixel color, float alpha, int mode, bool aa);
LICE_FUNC_DEF_DECL void (*__LICE_FillTrapezoid)(LICE_IBitmap* dest, int x1a, int x1b, int y1, int x2a, int x2b, int y2, LICE_pixel color, float alpha, int mode);
LICE_FUNC_DEF_DECL void (*__LICE_FillConvexPolygon)(LICE_IBitmap* dest, int* x, int* y, int npoints, LICE_pixel color, float alpha, int mode);
LICE_FUNC_DEF_DECL void (*__LICE_Copy)(LICE_IBitmap* dest, LICE_IBitmap* src);
LICE_FUNC_DEF_DECL void (*__LICE_DrawText)(LICE_IBitmap *bm, int x, int y, const char *string, LICE_pixel color, float alpha, int mode);
LICE_FUNC_DEF_DECL void (*__LICE_MeasureText)(const char *string, int *w, int *h);
LICE_FUNC_DEF_DECL void (*__LICE_ScaledBlit)(LICE_IBitmap *dest, LICE_IBitmap *src, int dstx, int dsty, int dstw, int dsth, float srcx, float srcy, float srcw, float srch, float alpha, int mode);

LICE_FUNC_DEF_DECL void * (*LICE_CreateFont)();
#define LICE_PutPixel __LICE_PutPixel
#define LICE_Line __LICE_Line
#define LICE_FLine __LICE_FLine
#define LICE_FillRect __LICE_FillRect
#define LICE_DrawRect __LICE_DrawRect
#define LICE_Circle __LICE_Circle
#define LICE_Clear __LICE_Clear
#define LICE_Blit __LICE_Blit
#define LICE_RotatedBlit __LICE_RotatedBlit
#define LICE_DrawGlyph __LICE_DrawGlyph
#define LICE_FillCircle __LICE_FillCircle
#define LICE_BorderedRect __LICE_BorderedRect
#define LICE_FillTriangle __LICE_FillTriangle
#define LICE_Arc __LICE_Arc
#define LICE_FillTrapezoid __LICE_FillTrapezoid
#define LICE_FillConvexPolygon __LICE_FillConvexPolygon
#define LICE_Copy __LICE_Copy
#define LICE_DrawText __LICE_DrawText
#define LICE_MeasureText __LICE_MeasureText
#define LICE_ScaledBlit __LICE_ScaledBlit
#define LICE_CreateMemBitmap(w,h) (__LICE_CreateBitmap ? __LICE_CreateBitmap(0,w,h) : 0)
#define LICE_CreateSysBitmap(w,h) (__LICE_CreateBitmap ? __LICE_CreateBitmap(1,w,h) : 0)
#define LICE_CreateTextCache() ((LICE_IFont*)(LICE_CreateFont?LICE_CreateFont():0))
#undef LICE_FUNC_DEF_DECL

#define IMPORT_LICE_FUNCS(IMPORT_FUNC) \
    IMPORT_FUNC(__LICE_CreateBitmap,"LICE_CreateBitmap") \
    IMPORT_FUNC(__LICE_PutPixel,"LICE_PutPixel") \
    IMPORT_FUNC(__LICE_Line,"LICE_LineInt") \
    IMPORT_FUNC(__LICE_FLine,"LICE_Line") \
    IMPORT_FUNC(__LICE_Circle,"LICE_Circle") \
    IMPORT_FUNC(__LICE_FillCircle,"LICE_FillCircle") \
    IMPORT_FUNC(__LICE_FillRect,"LICE_FillRect") \
    IMPORT_FUNC(__LICE_DrawRect,"LICE_DrawRect") \
    IMPORT_FUNC(__LICE_BorderedRect,"LICE_BorderedRect") \
    IMPORT_FUNC(__LICE_Clear,"LICE_Clear") \
    IMPORT_FUNC(__LICE_Blit,"LICE_Blit") \
    IMPORT_FUNC(__LICE_RotatedBlit,"LICE_RotatedBlit") \
    IMPORT_FUNC(__LICE_DrawGlyph,"LICE_DrawGlyph") \
    IMPORT_FUNC(LICE_CreateFont,"LICE_CreateFont") \
    IMPORT_FUNC(LICE_FillTriangle,"LICE_FillTriangle") \
    IMPORT_FUNC(LICE_Arc,"LICE_Arc") \
    IMPORT_FUNC(LICE_FillTrapezoid,"LICE_FillTrapezoid") \
    IMPORT_FUNC(LICE_FillConvexPolygon,"LICE_FillConvexPolygon") \
    IMPORT_FUNC(LICE_Copy,"LICE_Copy") \
    IMPORT_FUNC(__LICE_ScaledBlit,"LICE_ScaledBlit") \
    IMPORT_FUNC(__LICE_MeasureText,"LICE_MeasureText") \
    IMPORT_FUNC(__LICE_DrawText,"LICE_DrawText")

