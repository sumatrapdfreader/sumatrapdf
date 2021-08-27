#include "windrawlib.h"

#if !defined _MSC_VER || (defined _MSC_VER && _MSC_VER > 1200)
#define no_log(...) \
    do {            \
    } while (0)
#else
static void no_log(const char* fmt, ...) {
}
#endif

#ifdef DEBUG
void wd_log(const char* fmt, ...);
#define WD_TRACE wd_log
#else
#define WD_TRACE no_log
#endif

#define WD_TRACE_ERR_(msg, err) WD_TRACE(msg " [%lu]", (err))
#define WD_TRACE_ERR(msg) WD_TRACE(msg " [%lu]", GetLastError())
#define WD_TRACE_HR_(msg, hr) WD_TRACE(msg " [0x%lx]", (hr))
#define WD_TRACE_HR(msg) WD_TRACE(msg " [0x%lx]", hr)


/* -------------------------------------- c-gdiplus.h ----------------------------------- */

/* MSDN documentation for <gdiplus/gdiplusflat.h> sucks. This one is better:
 * http://www.jose.it-berater.org/gdiplus/iframe/index.htm
 */

/* Note we don't declare any functions here: We load them dynamically anyway.
 */

typedef DWORD c_ARGB;

typedef INT c_GpPixelFormat;
#define c_PixelFormatGDI 0x00020000    /* Is a GDI-supported format */
#define c_PixelFormatAlpha 0x00040000  /* Has an alpha component */
#define c_PixelFormatPAlpha 0x00080000 /* Pre-multiplied alpha */
#define c_PixelFormatCanonical 0x00200000
#define c_PixelFormat24bppRGB (8 | (24 << 8) | c_PixelFormatGDI)
#define c_PixelFormat32bppARGB (10 | (32 << 8) | c_PixelFormatAlpha | c_PixelFormatGDI | c_PixelFormatCanonical)
#define c_PixelFormat32bppPARGB (11 | (32 << 8) | c_PixelFormatAlpha | c_PixelFormatPAlpha | c_PixelFormatGDI)

#define c_ImageLockModeWrite 2

/*****************************
 ***  Helper Enumerations  ***
 *****************************/

typedef enum c_GpMatrixOrder_tag c_GpMatrixOrder;
enum c_GpMatrixOrder_tag { c_MatrixOrderPrepend = 0, c_MatrixOrderAppend = 1 };

typedef enum c_GpCombineMode_tag c_GpCombineMode;
enum c_GpCombineMode_tag {
    c_CombineModeReplace = 0,
    c_CombineModeIntersect = 1,
    c_CombineModeUnion = 2,
    c_CombineModeXor = 3,
    c_CombineModeExclude = 4,
    c_CombineModeComplement = 5
};

typedef enum c_GpPixelOffsetMode_tag c_GpPixelOffsetMode;
enum c_GpPixelOffsetMode_tag {
    c_PixelOffsetModeInvalid = -1,
    c_PixelOffsetModeDefault = 0,
    c_PixelOffsetModeHighSpeed = 1,
    c_PixelOffsetModeHighQuality = 2,
    c_PixelOffsetModeNone = 3,
    c_PixelOffsetModeHalf = 4
};

typedef enum c_GpSmoothingMode_tag c_GpSmoothingMode;
enum c_GpSmoothingMode_tag {
    c_SmoothingModeInvalid = -1,
    c_SmoothingModeDefault = 0,
    c_SmoothingModeHighSpeed = 1,
    c_SmoothingModeHighQuality = 2,
    c_SmoothingModeNone = 3,
    c_SmoothingModeAntiAlias8x4 = 4,
    c_SmoothingModeAntiAlias = 4,
    c_SmoothingModeAntiAlias8x8 = 5
};

typedef enum c_GpUnit_tag c_GpUnit;
enum c_GpUnit_tag {
    c_UnitWorld = 0,
    c_UnitDisplay = 1,
    c_UnitPixel = 2,
    c_UnitPoint = 3,
    c_UnitInch = 4,
    c_UnitDocument = 5,
    c_UnitMillimeter = 6
};

typedef enum c_GpFillMode_tag c_GpFillMode;
enum c_GpFillMode_tag { c_FillModeAlternate = 0, c_FillModeWinding = 1 };

typedef enum c_GpStringAlignment_tag c_GpStringAlignment;
enum c_GpStringAlignment_tag { c_StringAlignmentNear = 0, c_StringAlignmentCenter = 1, c_StringAlignmentFar = 2 };

typedef enum c_GpStringFormatFlags_tag c_GpStringFormatFlags;
enum c_GpStringFormatFlags_tag {
    c_StringFormatFlagsDirectionRightToLeft = 0x00000001,
    c_StringFormatFlagsDirectionVertical = 0x00000002,
    c_StringFormatFlagsNoFitBlackBox = 0x00000004,
    c_StringFormatFlagsDisplayFormatControl = 0x00000020,
    c_StringFormatFlagsNoFontFallback = 0x00000400,
    c_StringFormatFlagsMeasureTrailingSpaces = 0x00000800,
    c_StringFormatFlagsNoWrap = 0x00001000,
    c_StringFormatFlagsLineLimit = 0x00002000,
    c_StringFormatFlagsNoClip = 0x00004000
};

typedef enum c_GpStringTrimming_tag c_GpStringTrimming;
enum c_GpStringTrimming_tag {
    c_StringTrimmingNone = 0,
    c_StringTrimmingCharacter = 1,
    c_StringTrimmingWord = 2,
    c_StringTrimmingEllipsisCharacter = 3,
    c_StringTrimmingEllipsisWord = 4,
    c_StringTrimmingEllipsisPath = 5
};

typedef enum c_GpLineCap_tag c_GpLineCap;
enum c_GpLineCap_tag {
    c_LineCapFlat = 0,
    c_LineCapSquare = 1,
    c_LineCapRound = 2,
    c_LineCapTriangle = 3,
};

typedef enum c_GpLineJoin_tag c_GpLineJoin;
enum c_GpLineJoin_tag { c_LineJoinMiter = 0, c_LineJoinBevel = 1, c_LineJoinRound = 2 };

typedef enum c_GpDashStyle_tag c_GpDashStyle;
enum c_GpDashStyle_tag {
    c_DashStyleSolid = 0,
    c_DashStyleDash = 1,
    c_DashStyleDot = 2,
    c_DashStyleDashDot = 3,
    c_DashStyleDashDotDot = 4,
    c_DashStyleCustom = 5
};

typedef enum c_GpWrapMode_tag c_GpWrapMode;
enum c_GpWrapMode_tag {
    c_WrapModeTile = 0,
    c_WrapModeTileFlipX = 1,
    c_WrapModeTileFlipY = 2,
    c_WrapModeTileFlipXY = 3,
    c_WrapModeClamp = 4
};

/***************************
 ***  Helper Structures  ***
 ***************************/

typedef struct c_GpStartupInput_tag c_GpStartupInput;
struct c_GpStartupInput_tag {
    UINT32 GdiplusVersion;
    void* DebugEventCallback; /* DebugEventProc (not used) */
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
};

typedef struct c_GpPointF_tag c_GpPointF;
struct c_GpPointF_tag {
    float x;
    float y;
};

typedef struct c_GpRectF_tag c_GpRectF;
struct c_GpRectF_tag {
    float x;
    float y;
    float w;
    float h;
};

typedef struct c_GpRectI_tag c_GpRectI;
struct c_GpRectI_tag {
    INT x;
    INT y;
    INT w;
    INT h;
};

typedef struct c_GpBitmapData_tag c_GpBitmapData;
struct c_GpBitmapData_tag {
    UINT width;
    UINT height;
    INT Stride;
    c_GpPixelFormat PixelFormat;
    void* Scan0;
    UINT_PTR Reserved;
};

/**********************
 ***  GDI+ Objects  ***
 **********************/

typedef struct c_GpBrush_tag c_GpBrush;
typedef struct c_GpCachedBitmap_tag c_GpCachedBitmap;
typedef struct c_GpFont_tag c_GpFont;
typedef struct c_GpGraphics_tag c_GpGraphics;
typedef struct c_GpImage_tag c_GpImage;
typedef struct c_GpPath_tag c_GpPath;
typedef struct c_GpPen_tag c_GpPen;
typedef struct c_GpStringFormat_tag c_GpStringFormat;
typedef struct c_GpMatrix_tag c_GpMatrix;

/* These are "derived" from the types aboves (more specialized). */
typedef struct c_GpImage_tag c_GpBitmap;
typedef struct c_GpBrush_tag c_GpSolidFill;
typedef struct c_GpBrush_tag c_GpLineGradient;
typedef struct c_GpBrush_tag c_GpPathGradient;

/* ------------------------------------- c-d2d1.h --------------------------------------- */


/* ------------------------------------ c-dwrite.h ------------------------------------- */


static const GUID c_IID_IDWriteFactory = {0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};
static const GUID c_IID_IDWritePixelSnapping = {0xeaf3a2da,
                                                0xecf4,
                                                0x4d24,
                                                {0xb6, 0x44, 0xb3, 0x4f, 0x68, 0x42, 0x02, 0x4b}};
static const GUID c_IID_IDWriteTextRenderer = {0xef8a8135,
                                               0x5cc6,
                                               0x45fe,
                                               {0x88, 0x25, 0xc5, 0xa0, 0x72, 0x4e, 0xb8, 0x19}};

/******************************
 ***  Forward declarations  ***
 ******************************/

typedef struct c_IDWriteFactory_tag c_IDWriteFactory;
typedef struct c_IDWriteFont_tag c_IDWriteFont;
typedef struct c_IDWriteFontFace_tag c_IDWriteFontFace;
typedef struct c_IDWriteFontCollection_tag c_IDWriteFontCollection;
typedef struct c_IDWriteFontFamily_tag c_IDWriteFontFamily;
typedef struct c_IDWriteGdiInterop_tag c_IDWriteGdiInterop;
typedef struct c_IDWriteInlineObject_tag c_IDWriteInlineObject;
typedef struct c_IDWriteLocalizedStrings_tag c_IDWriteLocalizedStrings;
typedef struct c_IDWriteTextFormat_tag c_IDWriteTextFormat;
typedef struct c_IDWriteTextLayout_tag c_IDWriteTextLayout;
typedef struct c_IDWriteTextRenderer_tag c_IDWriteTextRenderer;

/*****************************
 ***  Helper Enumerations  ***
 *****************************/

typedef enum c_DWRITE_FACTORY_TYPE_tag c_DWRITE_FACTORY_TYPE;
enum c_DWRITE_FACTORY_TYPE_tag { c_DWRITE_FACTORY_TYPE_SHARED = 0, c_DWRITE_FACTORY_TYPE_ISOLATED };

typedef enum c_DWRITE_FONT_WEIGHT_tag c_DWRITE_FONT_WEIGHT;
enum c_DWRITE_FONT_WEIGHT_tag {
    c_DWRITE_FONT_WEIGHT_THIN = 100,
    c_DWRITE_FONT_WEIGHT_EXTRA_LIGHT = 200,
    c_DWRITE_FONT_WEIGHT_ULTRA_LIGHT = 200,
    c_DWRITE_FONT_WEIGHT_LIGHT = 300,
    c_DWRITE_FONT_WEIGHT_SEMI_LIGHT = 350,
    c_DWRITE_FONT_WEIGHT_NORMAL = 400,
    c_DWRITE_FONT_WEIGHT_REGULAR = 400,
    c_DWRITE_FONT_WEIGHT_MEDIUM = 500,
    c_DWRITE_FONT_WEIGHT_DEMI_BOLD = 600,
    c_DWRITE_FONT_WEIGHT_SEMI_BOLD = 600,
    c_DWRITE_FONT_WEIGHT_BOLD = 700,
    c_DWRITE_FONT_WEIGHT_EXTRA_BOLD = 800,
    c_DWRITE_FONT_WEIGHT_ULTRA_BOLD = 800,
    c_DWRITE_FONT_WEIGHT_BLACK = 900,
    c_DWRITE_FONT_WEIGHT_HEAVY = 900,
    c_DWRITE_FONT_WEIGHT_EXTRA_BLACK = 950,
    c_DWRITE_FONT_WEIGHT_ULTRA_BLACK = 950
};

typedef enum c_DWRITE_FONT_STYLE_tag c_DWRITE_FONT_STYLE;
enum c_DWRITE_FONT_STYLE_tag {
    c_DWRITE_FONT_STYLE_NORMAL = 0,
    c_DWRITE_FONT_STYLE_OBLIQUE,
    c_DWRITE_FONT_STYLE_ITALIC
};

typedef enum c_DWRITE_FONT_STRETCH_tag c_DWRITE_FONT_STRETCH;
enum c_DWRITE_FONT_STRETCH_tag {
    c_DWRITE_FONT_STRETCH_UNDEFINED = 0,
    c_DWRITE_FONT_STRETCH_ULTRA_CONDENSED,
    c_DWRITE_FONT_STRETCH_EXTRA_CONDENSED,
    c_DWRITE_FONT_STRETCH_CONDENSED,
    c_DWRITE_FONT_STRETCH_SEMI_CONDENSED,
    c_DWRITE_FONT_STRETCH_NORMAL,
    c_DWRITE_FONT_STRETCH_MEDIUM = c_DWRITE_FONT_STRETCH_NORMAL,
    c_DWRITE_FONT_STRETCH_SEMI_EXPANDED,
    c_DWRITE_FONT_STRETCH_EXPANDED,
    c_DWRITE_FONT_STRETCH_EXTRA_EXPANDED,
    c_DWRITE_FONT_STRETCH_ULTRA_EXPANDED
};

typedef enum c_DWRITE_READING_DIRECTION_tag c_DWRITE_READING_DIRECTION;
enum c_DWRITE_READING_DIRECTION_tag {
    c_DWRITE_READING_DIRECTION_LEFT_TO_RIGHT = 0,
    c_DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
};

typedef enum c_DWRITE_FLOW_DIRECTION_tag c_DWRITE_FLOW_DIRECTION;
enum c_DWRITE_FLOW_DIRECTION_tag {
    c_DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM = 0,
    c_DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP,
    c_DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT,
    c_DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT
};

typedef enum c_DWRITE_WORD_WRAPPING_tag c_DWRITE_WORD_WRAPPING;
enum c_DWRITE_WORD_WRAPPING_tag { c_DWRITE_WORD_WRAPPING_WRAP = 0, c_DWRITE_WORD_WRAPPING_NO_WRAP };

typedef enum c_DWRITE_TEXT_ALIGNMENT_tag c_DWRITE_TEXT_ALIGNMENT;
enum c_DWRITE_TEXT_ALIGNMENT_tag {
    c_DWRITE_TEXT_ALIGNMENT_LEADING = 0,
    c_DWRITE_TEXT_ALIGNMENT_TRAILING,
    c_DWRITE_TEXT_ALIGNMENT_CENTER,
    c_DWRITE_TEXT_ALIGNMENT_JUSTIFIED
};

typedef enum c_DWRITE_PARAGRAPH_ALIGNMENT_tag c_DWRITE_PARAGRAPH_ALIGNMENT;
enum c_DWRITE_PARAGRAPH_ALIGNMENT_tag {
    c_DWRITE_PARAGRAPH_ALIGNMENT_NEAR = 0,
    c_DWRITE_PARAGRAPH_ALIGNMENT_FAR,
    c_DWRITE_PARAGRAPH_ALIGNMENT_CENTER
};

typedef enum c_DWRITE_TRIMMING_GRANULARITY_tag c_DWRITE_TRIMMING_GRANULARITY;
enum c_DWRITE_TRIMMING_GRANULARITY_tag {
    c_DWRITE_TRIMMING_GRANULARITY_NONE = 0,
    c_DWRITE_TRIMMING_GRANULARITY_CHARACTER,
    c_DWRITE_TRIMMING_GRANULARITY_WORD
};

typedef enum c_DWRITE_MEASURING_MODE_tag c_DWRITE_MEASURING_MODE;
enum c_DWRITE_MEASURING_MODE_tag {
    c_DWRITE_MEASURING_MODE_NATURAL = 0,
    c_DWRITE_MEASURING_MODE_GDI_CLASSIC,
    c_DWRITE_MEASURING_MODE_GDI_NATURAL
};

/***************************
 ***  Helper Structures  ***
 ***************************/

typedef struct c_DWRITE_TRIMMING_tag c_DWRITE_TRIMMING;
struct c_DWRITE_TRIMMING_tag {
    c_DWRITE_TRIMMING_GRANULARITY granularity;
    UINT32 delimiter;
    UINT32 delimiterCount;
};

typedef struct c_DWRITE_FONT_METRICS_tag c_DWRITE_FONT_METRICS;
struct c_DWRITE_FONT_METRICS_tag {
    UINT16 designUnitsPerEm;
    UINT16 ascent;
    UINT16 descent;
    INT16 lineGap;
    UINT16 capHeight;
    UINT16 xHeight;
    INT16 underlinePosition;
    UINT16 underlineThickness;
    INT16 strikethroughPosition;
    UINT16 strikethroughThickness;
};

typedef struct c_DWRITE_TEXT_METRICS_tag c_DWRITE_TEXT_METRICS;
struct c_DWRITE_TEXT_METRICS_tag {
    FLOAT left;
    FLOAT top;
    FLOAT width;
    FLOAT widthIncludingTrailingWhitespace;
    FLOAT height;
    FLOAT layoutWidth;
    FLOAT layoutHeight;
    UINT32 maxBidiReorderingDepth;
    UINT32 lineCount;
};

typedef struct c_DWRITE_HIT_TEST_METRICS_tag c_DWRITE_HIT_TEST_METRICS;
struct c_DWRITE_HIT_TEST_METRICS_tag {
    UINT32 textPosition;
    UINT32 length;
    FLOAT left;
    FLOAT top;
    FLOAT width;
    FLOAT height;
    UINT32 bidiLevel;
    BOOL isText;
    BOOL isTrimmed;
};

typedef struct c_DWRITE_LINE_METRICS_tag c_DWRITE_LINE_METRICS;
struct c_DWRITE_LINE_METRICS_tag {
    UINT32 length;
    UINT32 trailingWhitespaceLength;
    UINT32 newlineLength;
    FLOAT height;
    FLOAT baseline;
    BOOL isTrimmed;
};

typedef struct c_DWRITE_TEXT_RANGE_tag c_DWRITE_TEXT_RANGE;
struct c_DWRITE_TEXT_RANGE_tag {
    UINT32 startPosition;
    UINT32 length;
};

typedef struct c_DWRITE_MATRIX_tag c_DWRITE_MATRIX;
struct c_DWRITE_MATRIX_tag {
    FLOAT m11;
    FLOAT m12;
    FLOAT m21;
    FLOAT m22;
    FLOAT dx;
    FLOAT dy;
};

typedef struct c_DWRITE_GLYPH_OFFSET_tag c_DWRITE_GLYPH_OFFSET;
struct c_DWRITE_GLYPH_OFFSET_tag {
    FLOAT advanceOffset;
    FLOAT ascenderOffset;
};

typedef struct c_DWRITE_GLYPH_RUN_tag c_DWRITE_GLYPH_RUN;
struct c_DWRITE_GLYPH_RUN_tag {
    c_IDWriteFontFace* fontFace;
    FLOAT fontEmSize;
    UINT32 glyphCount;
    UINT16 const* glyphIndices;
    FLOAT const* glyphAdvances;
    c_DWRITE_GLYPH_OFFSET const* glyphOffsets;
    BOOL isSideways;
    UINT32 bidiLevel;
};

typedef struct c_DWRITE_GLYPH_RUN_DESCRIPTION_tag c_DWRITE_GLYPH_RUN_DESCRIPTION;
struct c_DWRITE_GLYPH_RUN_DESCRIPTION_tag {
    WCHAR const* localeName;
    WCHAR const* string;
    UINT32 stringLength;
    UINT16 const* clusterMap;
    UINT32 textPosition;
};

typedef struct c_DWRITE_UNDERLINE_tag c_DWRITE_UNDERLINE;
struct c_DWRITE_UNDERLINE_tag {
    FLOAT width;
    FLOAT thickness;
    FLOAT offset;
    FLOAT runHeight;
    c_DWRITE_READING_DIRECTION readingDirection;
    c_DWRITE_FLOW_DIRECTION flowDirection;
    WCHAR const* localeName;
    c_DWRITE_MEASURING_MODE measuringMode;
};

typedef struct c_DWRITE_STRIKETHROUGH_tag c_DWRITE_STRIKETHROUGH;
struct c_DWRITE_STRIKETHROUGH_tag {
    FLOAT width;
    FLOAT thickness;
    FLOAT offset;
    c_DWRITE_READING_DIRECTION readingDirection;
    c_DWRITE_FLOW_DIRECTION flowDirection;
    WCHAR const* localeName;
    c_DWRITE_MEASURING_MODE measuringMode;
};

/**********************************
 ***  Interface IDWriteFactory  ***
 **********************************/

typedef struct c_IDWriteFactoryVtbl_tag c_IDWriteFactoryVtbl;
struct c_IDWriteFactoryVtbl_tag {
    /* IUknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFactory*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFactory*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFactory*);

    /* IDWriteFactory methods */
    STDMETHOD(dummy_GetSystemFontCollection)(void);
    STDMETHOD(dummy_CreateCustomFontCollection)(void);
    STDMETHOD(dummy_RegisterFontCollectionLoader)(void);
    STDMETHOD(dummy_UnregisterFontCollectionLoader)(void);
    STDMETHOD(dummy_CreateFontFileReference)(void);
    STDMETHOD(dummy_CreateCustomFontFileReference)(void);
    STDMETHOD(dummy_CreateFontFace)(void);
    STDMETHOD(dummy_CreateRenderingParams)(void);
    STDMETHOD(dummy_CreateMonitorRenderingParams)(void);
    STDMETHOD(dummy_CreateCustomRenderingParams)(void);
    STDMETHOD(dummy_RegisterFontFileLoader)(void);
    STDMETHOD(dummy_UnregisterFontFileLoader)(void);
    STDMETHOD(CreateTextFormat)
    (c_IDWriteFactory*, WCHAR const*, void*, c_DWRITE_FONT_WEIGHT, c_DWRITE_FONT_STYLE, c_DWRITE_FONT_STRETCH, FLOAT,
     WCHAR const*, c_IDWriteTextFormat**);
    STDMETHOD(dummy_CreateTypography)(void);
    STDMETHOD(GetGdiInterop)(c_IDWriteFactory*, c_IDWriteGdiInterop**);
    STDMETHOD(CreateTextLayout)
    (c_IDWriteFactory*, WCHAR const*, UINT32, c_IDWriteTextFormat*, FLOAT, FLOAT, c_IDWriteTextLayout**);
    STDMETHOD(dummy_CreateGdiCompatibleTextLayout)(void);
    STDMETHOD(CreateEllipsisTrimmingSign)(c_IDWriteFactory*, c_IDWriteTextFormat*, c_IDWriteInlineObject**);
    STDMETHOD(dummy_CreateTextAnalyzer)(void);
    STDMETHOD(dummy_CreateNumberSubstitution)(void);
    STDMETHOD(dummy_CreateGlyphRunAnalysis)(void);
};

struct c_IDWriteFactory_tag {
    c_IDWriteFactoryVtbl* vtbl;
};

#define c_IDWriteFactory_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteFactory_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteFactory_Release(self) (self)->vtbl->Release(self)
#define c_IDWriteFactory_GetSystemFontCollection(self, a, b) (self)->vtbl->GetSystemFontCollection(self, a, b)
#define c_IDWriteFactory_GetGdiInterop(self, a) (self)->vtbl->GetGdiInterop(self, a)
#define c_IDWriteFactory_CreateTextFormat(self, a, b, c, d, e, f, g, h) \
    (self)->vtbl->CreateTextFormat(self, a, b, c, d, e, f, g, h)
#define c_IDWriteFactory_CreateTextLayout(self, a, b, c, d, e, f) (self)->vtbl->CreateTextLayout(self, a, b, c, d, e, f)
#define c_IDWriteFactory_CreateEllipsisTrimmingSign(self, a, b) (self)->vtbl->CreateEllipsisTrimmingSign(self, a, b)

/*******************************
 ***  Interface IDWriteFont  ***
 *******************************/

typedef struct c_IDWriteFontVtbl_tag c_IDWriteFontVtbl;
struct c_IDWriteFontVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFont*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFont*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFont*);

    /* IDWriteFont methods */
    STDMETHOD(GetFontFamily)(c_IDWriteFont*, c_IDWriteFontFamily**);
    STDMETHOD_(c_DWRITE_FONT_WEIGHT, GetWeight)(c_IDWriteFont*);
    STDMETHOD_(c_DWRITE_FONT_STRETCH, GetStretch)(c_IDWriteFont*);
    STDMETHOD_(c_DWRITE_FONT_STYLE, GetStyle)(c_IDWriteFont*);
    STDMETHOD(dummy_IsSymbolFont)(void);
    STDMETHOD(dummy_GetFaceNames)(void);
    STDMETHOD(dummy_GetInformationalStrings)(void);
    STDMETHOD(dummy_GetSimulations)(void);
    STDMETHOD_(void, GetMetrics)(c_IDWriteFont*, c_DWRITE_FONT_METRICS*);
    STDMETHOD(dummy_HasCharacter)(void);
    STDMETHOD(dummy_CreateFontFace)(void);
};

struct c_IDWriteFont_tag {
    c_IDWriteFontVtbl* vtbl;
};

#define c_IDWriteFont_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteFont_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteFont_Release(self) (self)->vtbl->Release(self)
#define c_IDWriteFont_GetWeight(self) (self)->vtbl->GetWeight(self)
#define c_IDWriteFont_GetStretch(self) (self)->vtbl->GetStretch(self)
#define c_IDWriteFont_GetStyle(self) (self)->vtbl->GetStyle(self)
#define c_IDWriteFont_GetMetrics(self, a) (self)->vtbl->GetMetrics(self, a)
#define c_IDWriteFont_GetFontFamily(self, a) (self)->vtbl->GetFontFamily(self, a)

/***********************************
 ***  Interface IDWriteFontFace  ***
 ***********************************/

typedef struct c_IDWriteFontFaceVtbl_tag c_IDWriteFontFaceVtbl;
struct c_IDWriteFontFaceVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFontFace*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFontFace*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFontFace*);

    /* IDWriteFontFace methods */
    STDMETHOD(dummy_GetType)(void);
    STDMETHOD(dummy_GetFiles)(void);
    STDMETHOD(dummy_GetIndex)(void);
    STDMETHOD(dummy_GetSimulations)(void);
    STDMETHOD(dummy_IsSymbolFont)(void);
    STDMETHOD(dummy_GetMetrics)(void);
    STDMETHOD(dummy_GetGlyphCount)(void);
    STDMETHOD(dummy_GetDesignGlyphMetrics)(void);
    STDMETHOD(dummy_GetGlyphIndices)(void);
    STDMETHOD(dummy_TryGetFontTable)(void);
    STDMETHOD(dummy_ReleaseFontTable)(void);
    STDMETHOD(dummy_GetGlyphRunOutline)(void);
    STDMETHOD(dummy_GetRecommendedRenderingMode)(void);
    STDMETHOD(dummy_GetGdiCompatibleMetrics)(void);
    STDMETHOD(dummy_GetGdiCompatibleGlyphMetrics)(void);
};

struct c_IDWriteFontFace_tag {
    c_IDWriteFontFaceVtbl* vtbl;
};

#define c_IDWriteFontFace_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteFontFace_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteFontFace_Release(self) (self)->vtbl->Release(self)

/*****************************************
 ***  Interface IDWriteFontCollection  ***
 *****************************************/

typedef struct c_IDWriteFontCollectionVtbl_tag c_IDWriteFontCollectionVtbl;
struct c_IDWriteFontCollectionVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFontCollection*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFontCollection*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFontCollection*);

    /* IDWriteFontCollection methods */
    STDMETHOD(dummy_GetFontFamilyCount)(void);
    STDMETHOD(GetFontFamily)(c_IDWriteFontCollection*, UINT32, c_IDWriteFontFamily**);
    STDMETHOD(FindFamilyName)(c_IDWriteFontCollection*, WCHAR*, UINT32*, BOOL*);
    STDMETHOD(dummy_GetFontFromFontFace)(void);
};

struct c_IDWriteFontCollection_tag {
    c_IDWriteFontCollectionVtbl* vtbl;
};

#define c_IDWriteFontCollection_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteFontCollection_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteFontCollection_Release(self) (self)->vtbl->Release(self)
#define c_IDWriteFontCollection_GetFontFamilyCount(self) (self)->vtbl->GetFontFamilyCount(self)
#define c_IDWriteFontCollection_GetFontFamily(self, a, b) (self)->vtbl->GetFontFamily(self, a, b)
#define c_IDWriteFontCollection_FindFamilyName(self, a, b, c) (self)->vtbl->FindFamilyName(self, a, b, c)

/*************************************
 ***  Interface IDWriteFontFamily  ***
 *************************************/

typedef struct c_IDWriteFontFamilyVtbl_tag c_IDWriteFontFamilyVtbl;
struct c_IDWriteFontFamilyVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteFontFamily*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteFontFamily*);
    STDMETHOD_(ULONG, Release)(c_IDWriteFontFamily*);

    /* IDWriteFontList methods */
    STDMETHOD(dummy_GetFontCollection)(void);
    STDMETHOD(dummy_GetFontCount)(void);
    STDMETHOD(dummy_GetFont)(void);

    /* IDWriteFontFamily methods */
    STDMETHOD(GetFamilyNames)(c_IDWriteFontFamily*, c_IDWriteLocalizedStrings**);
    STDMETHOD(GetFirstMatchingFont)
    (c_IDWriteFontFamily*, c_DWRITE_FONT_WEIGHT, c_DWRITE_FONT_STRETCH, c_DWRITE_FONT_STYLE, c_IDWriteFont**);
    STDMETHOD(dummy_GetMatchingFonts)(void);
};

struct c_IDWriteFontFamily_tag {
    c_IDWriteFontFamilyVtbl* vtbl;
};

#define c_IDWriteFontFamily_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteFontFamily_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteFontFamily_Release(self) (self)->vtbl->Release(self)
#define c_IDWriteFontFamily_GetFirstMatchingFont(self, a, b, c, d) (self)->vtbl->GetFirstMatchingFont(self, a, b, c, d)
#define c_IDWriteFontFamily_GetFamilyNames(self, a) (self)->vtbl->GetFamilyNames(self, a)

/*************************************
 ***  Interface IDWriteGdiInterop  ***
 *************************************/

typedef struct c_IDWriteGdiInteropVtbl_tag c_IDWriteGdiInteropVtbl;
struct c_IDWriteGdiInteropVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteGdiInterop*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteGdiInterop*);
    STDMETHOD_(ULONG, Release)(c_IDWriteGdiInterop*);

    /* IDWriteGdiInterop methods */
    STDMETHOD(CreateFontFromLOGFONT)(c_IDWriteGdiInterop*, LOGFONTW const*, c_IDWriteFont**);
    STDMETHOD(dummy_ConvertFontToLOGFONT)(void);
    STDMETHOD(dummy_ConvertFontFaceToLOGFONT)(void);
    STDMETHOD(dummy_CreateFontFaceFromHdc)(void);
    STDMETHOD(dummy_CreateBitmapRenderTarget)(void);
};

struct c_IDWriteGdiInterop_tag {
    c_IDWriteGdiInteropVtbl* vtbl;
};

#define c_IDWriteGdiInterop_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteGdiInterop_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteGdiInterop_Release(self) (self)->vtbl->Release(self)
#define c_IDWriteGdiInterop_CreateFontFromLOGFONT(self, a, b) (self)->vtbl->CreateFontFromLOGFONT(self, a, b)

/*******************************************
 ***  Interface IDWriteLocalizedStrings  ***
 *******************************************/

typedef struct c_IDWriteLocalizedStringsVtbl_tag c_IDWriteLocalizedStringsVtbl;
struct c_IDWriteLocalizedStringsVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteLocalizedStrings*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteLocalizedStrings*);
    STDMETHOD_(ULONG, Release)(c_IDWriteLocalizedStrings*);

    /* IDWriteLocalizedStrings methods */
    STDMETHOD(dummy_GetCount)(void);
    STDMETHOD(dummy_FindLocaleName)(void);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);
    STDMETHOD(GetStringLength)(c_IDWriteLocalizedStrings*, UINT32, UINT32*);
    STDMETHOD(GetString)(c_IDWriteLocalizedStrings*, UINT32, WCHAR*, UINT32);
};

struct c_IDWriteLocalizedStrings_tag {
    c_IDWriteLocalizedStringsVtbl* vtbl;
};

#define c_IDWriteLocalizedStrings_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteLocalizedStrings_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteLocalizedStrings_Release(self) (self)->vtbl->Release(self)
#define c_IDWriteLocalizedStrings_GetStringLength(self, a, b) (self)->vtbl->GetStringLength(self, a, b)
#define c_IDWriteLocalizedStrings_GetString(self, a, b, c) (self)->vtbl->GetString(self, a, b, c)

/***************************************
 ***  Interface IDWriteInlineObject  ***
 ***************************************/

typedef struct c_IDWriteInlineObjectVtbl_tag c_IDWriteInlineObjectVtbl;
struct c_IDWriteInlineObjectVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteInlineObject*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteInlineObject*);
    STDMETHOD_(ULONG, Release)(c_IDWriteInlineObject*);

    /* IDWriteInlineObject methods */
    STDMETHOD(dummy_Draw)(void);
    STDMETHOD(dummy_GetMetrics)(void);
    STDMETHOD(dummy_GetOverhangMetrics)(void);
    STDMETHOD(dummy_GetBreakConditions)(void);
};

struct c_IDWriteInlineObject_tag {
    c_IDWriteInlineObjectVtbl* vtbl;
};

#define c_IDWriteInlineObject_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteInlineObject_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteInlineObject_Release(self) (self)->vtbl->Release(self)

/*************************************
 ***  Interface IDWriteTextFormat  ***
 *************************************/

typedef struct c_IDWriteTextFormatVtbl_tag c_IDWriteTextFormatVtbl;
struct c_IDWriteTextFormatVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteTextFormat*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteTextFormat*);
    STDMETHOD_(ULONG, Release)(c_IDWriteTextFormat*);

    /* IDWriteTextFormat methods */
    STDMETHOD(dummy_SetTextAlignment)(void);
    STDMETHOD(dummy_SetParagraphAlignment)(void);
    STDMETHOD(dummy_SetWordWrapping)(void);
    STDMETHOD(dummy_SetReadingDirection)(void);
    STDMETHOD(dummy_SetFlowDirection)(void);
    STDMETHOD(dummy_SetIncrementalTabStop)(void);
    STDMETHOD(dummy_SetTrimming)(void);
    STDMETHOD(dummy_SetLineSpacing)(void);
    STDMETHOD(dummy_GetTextAlignment)(void);
    STDMETHOD(dummy_GetParagraphAlignment)(void);
    STDMETHOD(dummy_GetWordWrapping)(void);
    STDMETHOD(dummy_GetReadingDirection)(void);
    STDMETHOD(dummy_GetFlowDirection)(void);
    STDMETHOD(dummy_GetIncrementalTabStop)(void);
    STDMETHOD(dummy_GetTrimming)(void);
    STDMETHOD(dummy_GetLineSpacing)(void);
    STDMETHOD(GetFontCollection)(c_IDWriteTextFormat*, c_IDWriteFontCollection**);
    STDMETHOD_(UINT32, GetFontFamilyNameLength)(c_IDWriteTextFormat*);
    STDMETHOD(GetFontFamilyName)(c_IDWriteTextFormat*, WCHAR*, UINT32);
    STDMETHOD_(c_DWRITE_FONT_WEIGHT, GetFontWeight)(c_IDWriteTextFormat*);
    STDMETHOD_(c_DWRITE_FONT_STYLE, GetFontStyle)(c_IDWriteTextFormat*);
    STDMETHOD_(c_DWRITE_FONT_STRETCH, GetFontStretch)(c_IDWriteTextFormat*);
    STDMETHOD_(FLOAT, GetFontSize)(c_IDWriteTextFormat*);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);
};

struct c_IDWriteTextFormat_tag {
    c_IDWriteTextFormatVtbl* vtbl;
};

#define c_IDWriteTextFormat_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteTextFormat_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteTextFormat_Release(self) (self)->vtbl->Release(self)
#define c_IDWriteTextFormat_GetFontCollection(self, a) (self)->vtbl->GetFontCollection(self, a)
#define c_IDWriteTextFormat_GetFontFamilyNameLength(self) (self)->vtbl->GetFontFamilyNameLength(self)
#define c_IDWriteTextFormat_GetFontFamilyName(self, a, b) (self)->vtbl->GetFontFamilyName(self, a, b)
#define c_IDWriteTextFormat_GetFontWeight(self) (self)->vtbl->GetFontWeight(self)
#define c_IDWriteTextFormat_GetFontStretch(self) (self)->vtbl->GetFontStretch(self)
#define c_IDWriteTextFormat_GetFontStyle(self) (self)->vtbl->GetFontStyle(self)
#define c_IDWriteTextFormat_GetFontSize(self) (self)->vtbl->GetFontSize(self)

/*************************************
 ***  Interface IDWriteTextLayout  ***
 *************************************/

typedef struct c_IDWriteTextLayoutVtbl_tag c_IDWriteTextLayoutVtbl;
struct c_IDWriteTextLayoutVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteTextLayout*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteTextLayout*);
    STDMETHOD_(ULONG, Release)(c_IDWriteTextLayout*);

    /* IDWriteTextFormat methods */
    STDMETHOD(SetTextAlignment)(c_IDWriteTextLayout*, c_DWRITE_TEXT_ALIGNMENT);
    STDMETHOD(SetParagraphAlignment)(c_IDWriteTextLayout*, c_DWRITE_PARAGRAPH_ALIGNMENT);
    STDMETHOD(SetWordWrapping)(c_IDWriteTextLayout*, c_DWRITE_WORD_WRAPPING);
    STDMETHOD(SetReadingDirection)(c_IDWriteTextLayout*, c_DWRITE_READING_DIRECTION);
    STDMETHOD(dummy_SetFlowDirection)(void);
    STDMETHOD(dummy_SetIncrementalTabStop)(void);
    STDMETHOD(SetTrimming)(c_IDWriteTextLayout*, const c_DWRITE_TRIMMING*, c_IDWriteInlineObject*);
    STDMETHOD(dummy_SetLineSpacing)(void);
    STDMETHOD(dummy_GetTextAlignment)(void);
    STDMETHOD(dummy_GetParagraphAlignment)(void);
    STDMETHOD(dummy_GetWordWrapping)(void);
    STDMETHOD(dummy_GetReadingDirection)(void);
    STDMETHOD(dummy_GetFlowDirection)(void);
    STDMETHOD(dummy_GetIncrementalTabStop)(void);
    STDMETHOD(dummy_GetTrimming)(void);
    STDMETHOD(dummy_GetLineSpacing)(void);
    STDMETHOD(dummy_GetFontCollection)(void);
    STDMETHOD(dummy_GetFontFamilyNameLength)(void);
    STDMETHOD(dummy_GetFontFamilyName)(void);
    STDMETHOD(dummy_GetFontWeight)(void);
    STDMETHOD(dummy_GetFontStyle)(void);
    STDMETHOD(dummy_GetFontStretch)(void);
    STDMETHOD_(FLOAT, GetFontSize)(c_IDWriteTextLayout*);
    STDMETHOD(dummy_GetLocaleNameLength)(void);
    STDMETHOD(dummy_GetLocaleName)(void);

    /* IDWriteTextLayout methods */
    STDMETHOD(SetMaxWidth)(c_IDWriteTextLayout*, FLOAT);
    STDMETHOD(SetMaxHeight)(c_IDWriteTextLayout*, FLOAT);
    STDMETHOD(dummy_SetFontCollection)(void);
    STDMETHOD(SetFontFamilyName)(c_IDWriteTextLayout*, const WCHAR*, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetFontWeight)(c_IDWriteTextLayout*, c_DWRITE_FONT_WEIGHT, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetFontStyle)(c_IDWriteTextLayout*, c_DWRITE_FONT_STYLE, c_DWRITE_TEXT_RANGE);
    STDMETHOD(dummy_SetFontStretch)(void);
    STDMETHOD(SetFontSize)(c_IDWriteTextLayout*, FLOAT, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetUnderline)(c_IDWriteTextLayout*, BOOL, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetStrikethrough)(c_IDWriteTextLayout*, BOOL, c_DWRITE_TEXT_RANGE);
    STDMETHOD(SetDrawingEffect)(c_IDWriteTextLayout*, IUnknown*, c_DWRITE_TEXT_RANGE);
    STDMETHOD(dummy_SetInlineObject)(void);
    STDMETHOD(dummy_SetTypography)(void);
    STDMETHOD(dummy_SetLocaleName)(void);
    STDMETHOD_(FLOAT, GetMaxWidth)(c_IDWriteTextLayout*);
    STDMETHOD(dummy_GetMaxHeight)(void);
    STDMETHOD(dummy_GetFontCollection2)(void);
    STDMETHOD(dummy_GetFontFamilyNameLength2)(void);
    STDMETHOD(dummy_GetFontFamilyName2)(void);
    STDMETHOD(dummy_GetFontWeight2)(void);
    STDMETHOD(dummy_GetFontStyle2)(void);
    STDMETHOD(dummy_GetFontStretch2)(void);
    STDMETHOD(GetFontSize2)(c_IDWriteTextLayout*, UINT32, FLOAT*, c_DWRITE_TEXT_RANGE*);
    STDMETHOD(dummy_GetUnderline)(void);
    STDMETHOD(dummy_GetStrikethrough)(void);
    STDMETHOD(dummy_GetDrawingEffect)(void);
    STDMETHOD(dummy_GetInlineObject)(void);
    STDMETHOD(dummy_GetTypography)(void);
    STDMETHOD(dummy_GetLocaleNameLength2)(void);
    STDMETHOD(dummy_GetLocaleName2)(void);
    STDMETHOD(Draw)(c_IDWriteTextLayout*, void*, c_IDWriteTextRenderer*, FLOAT, FLOAT);
    STDMETHOD(GetLineMetrics)(c_IDWriteTextLayout*, c_DWRITE_LINE_METRICS*, UINT32, UINT32*);
    STDMETHOD(GetMetrics)(c_IDWriteTextLayout*, c_DWRITE_TEXT_METRICS*);
    STDMETHOD(dummy_GetOverhangMetrics)(void);
    STDMETHOD(dummy_GetClusterMetrics)(void);
    STDMETHOD(DetermineMinWidth)(c_IDWriteTextLayout*, FLOAT*);
    STDMETHOD(HitTestPoint)(c_IDWriteTextLayout*, FLOAT, FLOAT, BOOL*, BOOL*, c_DWRITE_HIT_TEST_METRICS*);
    STDMETHOD(dummy_HitTestTextPosition)(void);
    STDMETHOD(dummy_HitTestTextRange)(void);
};

struct c_IDWriteTextLayout_tag {
    c_IDWriteTextLayoutVtbl* vtbl;
};

#define c_IDWriteTextLayout_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_IDWriteTextLayout_AddRef(self) (self)->vtbl->AddRef(self)
#define c_IDWriteTextLayout_Release(self) (self)->vtbl->Release(self)
#define c_IDWriteTextLayout_SetTextAlignment(self, a) (self)->vtbl->SetTextAlignment(self, a)
#define c_IDWriteTextLayout_SetParagraphAlignment(self, a) (self)->vtbl->SetParagraphAlignment(self, a)
#define c_IDWriteTextLayout_SetWordWrapping(self, a) (self)->vtbl->SetWordWrapping(self, a)
#define c_IDWriteTextLayout_SetReadingDirection(self, a) (self)->vtbl->SetReadingDirection(self, a)
#define c_IDWriteTextLayout_SetTrimming(self, a, b) (self)->vtbl->SetTrimming(self, a, b)
#define c_IDWriteTextLayout_GetFontSize(self) (self)->vtbl->GetFontSize(self)
#define c_IDWriteTextLayout_SetMaxWidth(self, a) (self)->vtbl->SetMaxWidth(self, a)
#define c_IDWriteTextLayout_SetMaxHeight(self, a) (self)->vtbl->SetMaxHeight(self, a)
#define c_IDWriteTextLayout_SetFontFamilyName(self, a, b) (self)->vtbl->SetFontFamilyName(self, a, b)
#define c_IDWriteTextLayout_SetFontWeight(self, a, b) (self)->vtbl->SetFontWeight(self, a, b)
#define c_IDWriteTextLayout_SetFontStyle(self, a, b) (self)->vtbl->SetFontStyle(self, a, b)
#define c_IDWriteTextLayout_SetFontSize(self, a, b) (self)->vtbl->SetFontSize(self, a, b)
#define c_IDWriteTextLayout_SetStrikethrough(self, a, b) (self)->vtbl->SetStrikethrough(self, a, b)
#define c_IDWriteTextLayout_SetDrawingEffect(self, a, b) (self)->vtbl->SetDrawingEffect(self, a, b)
#define c_IDWriteTextLayout_SetUnderline(self, a, b) (self)->vtbl->SetUnderline(self, a, b)
#define c_IDWriteTextLayout_GetMaxWidth(self) (self)->vtbl->GetMaxWidth(self)
#define c_IDWriteTextLayout_GetFontSize2(self, a, b, c) (self)->vtbl->GetFontSize2(self, a, b, c)
#define c_IDWriteTextLayout_Draw(self, a, b, c, d) (self)->vtbl->Draw(self, a, b, c, d)
#define c_IDWriteTextLayout_GetLineMetrics(self, a, b, c) (self)->vtbl->GetLineMetrics(self, a, b, c)
#define c_IDWriteTextLayout_GetMetrics(self, a) (self)->vtbl->GetMetrics(self, a)
#define c_IDWriteTextLayout_DetermineMinWidth(self, a) (self)->vtbl->DetermineMinWidth(self, a)
#define c_IDWriteTextLayout_HitTestPoint(self, a, b, c, d, e) (self)->vtbl->HitTestPoint(self, a, b, c, d, e)

/***************************************
 ***  Interface IDWriteTextRenderer  ***
 ***************************************/

typedef struct c_IDWriteTextRendererVtbl_tag c_IDWriteTextRendererVtbl;
struct c_IDWriteTextRendererVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_IDWriteTextRenderer*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_IDWriteTextRenderer*);
    STDMETHOD_(ULONG, Release)(c_IDWriteTextRenderer*);

    /* IDWritePixelSnapping methods */
    STDMETHOD(IsPixelSnappingDisabled)(c_IDWriteTextRenderer*, void*, BOOL*);
    STDMETHOD(GetCurrentTransform)(c_IDWriteTextRenderer*, void*, c_DWRITE_MATRIX*);
    STDMETHOD(GetPixelsPerDip)(c_IDWriteTextRenderer*, void*, FLOAT*);

    /* IDWriteTextRenderer methods */
    STDMETHOD(DrawGlyphRun)
    (c_IDWriteTextRenderer*, void*, FLOAT, FLOAT, c_DWRITE_MEASURING_MODE, c_DWRITE_GLYPH_RUN const*,
     c_DWRITE_GLYPH_RUN_DESCRIPTION const*, IUnknown*);
    STDMETHOD(DrawUnderline)(c_IDWriteTextRenderer*, void*, FLOAT, FLOAT, c_DWRITE_UNDERLINE const*, IUnknown*);
    STDMETHOD(DrawStrikethrough)(c_IDWriteTextRenderer*, void*, FLOAT, FLOAT, c_DWRITE_STRIKETHROUGH const*, IUnknown*);
    STDMETHOD(DrawInlineObject)
    (c_IDWriteTextRenderer*, void*, FLOAT, FLOAT, c_IDWriteInlineObject*, BOOL, BOOL, IUnknown*);
};

struct c_IDWriteTextRenderer_tag {
    c_IDWriteTextRendererVtbl* vtbl;
};

/* No need for the macro wrappers, because application is only supposed to
 * implement this interface and pass it into IDWriteTextLayout::Draw(), not
 * to call it directly. */

static const GUID c_IID_ID2D1Factory = {0x06152247, 0x6f50, 0x465a, {0x92, 0x45, 0x11, 0x8b, 0xfd, 0x3b, 0x60, 0x07}};

static const GUID c_IID_ID2D1GdiInteropRenderTarget = {0xe0db51c3,
                                                       0x6f77,
                                                       0x4bae,
                                                       {0xb3, 0xd5, 0xe4, 0x75, 0x09, 0xb3, 0x58, 0x38}};

/******************************
 ***  Forward declarations  ***
 ******************************/

typedef struct c_IDWriteTextLayout_tag c_IDWriteTextLayout;

typedef struct c_ID2D1Bitmap_tag c_ID2D1Bitmap;
typedef struct c_ID2D1BitmapRenderTarget_tag c_ID2D1BitmapRenderTarget;
typedef struct c_ID2D1Brush_tag c_ID2D1Brush;
typedef struct c_ID2D1StrokeStyle_tag c_ID2D1StrokeStyle;
typedef struct c_ID2D1DCRenderTarget_tag c_ID2D1DCRenderTarget;
typedef struct c_ID2D1Factory_tag c_ID2D1Factory;
typedef struct c_ID2D1GdiInteropRenderTarget_tag c_ID2D1GdiInteropRenderTarget;
typedef struct c_ID2D1Geometry_tag c_ID2D1Geometry;
typedef struct c_ID2D1GeometrySink_tag c_ID2D1GeometrySink;
typedef struct c_ID2D1HwndRenderTarget_tag c_ID2D1HwndRenderTarget;
typedef struct c_ID2D1Layer_tag c_ID2D1Layer;
typedef struct c_ID2D1PathGeometry_tag c_ID2D1PathGeometry;
typedef struct c_ID2D1RenderTarget_tag c_ID2D1RenderTarget;
typedef struct c_ID2D1SolidColorBrush_tag c_ID2D1SolidColorBrush;
typedef struct c_ID2D1LinearGradientBrush_tag c_ID2D1LinearGradientBrush;
typedef struct c_ID2D1RadialGradientBrush_tag c_ID2D1RadialGradientBrush;
typedef struct c_ID2D1GradientStopCollection_tag c_ID2D1GradientStopCollection;

/*****************************
 ***  Helper Enumerations  ***
 *****************************/

#define c_D2D1_DRAW_TEXT_OPTIONS_CLIP 0x00000002
#define c_D2D1_PRESENT_OPTIONS_NONE 0x00000000
#define c_D2D1_LAYER_OPTIONS_NONE 0x00000000
#define c_D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE 0x00000002

typedef enum c_D2D1_TEXT_ANTIALIAS_MODE_tag c_D2D1_TEXT_ANTIALIAS_MODE;
enum c_D2D1_TEXT_ANTIALIAS_MODE_tag { c_D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE = 1 };

typedef enum c_DXGI_FORMAT_tag c_DXGI_FORMAT;
enum c_DXGI_FORMAT_tag { c_DXGI_FORMAT_B8G8R8A8_UNORM = 87 };

typedef enum c_D2D1_ANTIALIAS_MODE_tag c_D2D1_ANTIALIAS_MODE;
enum c_D2D1_ANTIALIAS_MODE_tag { c_D2D1_ANTIALIAS_MODE_PER_PRIMITIVE = 0 };

typedef enum c_D2D1_ALPHA_MODE_tag c_D2D1_ALPHA_MODE;
enum c_D2D1_ALPHA_MODE_tag { c_D2D1_ALPHA_MODE_UNKNOWN = 0, c_D2D1_ALPHA_MODE_PREMULTIPLIED = 1 };

typedef enum c_D2D1_ARC_SIZE_tag c_D2D1_ARC_SIZE;
enum c_D2D1_ARC_SIZE_tag { c_D2D1_ARC_SIZE_SMALL = 0, c_D2D1_ARC_SIZE_LARGE = 1 };

typedef enum c_D2D1_DC_INITIALIZE_MODE_tag c_D2D1_DC_INITIALIZE_MODE;
enum c_D2D1_DC_INITIALIZE_MODE_tag { c_D2D1_DC_INITIALIZE_MODE_COPY = 0, c_D2D1_DC_INITIALIZE_MODE_CLEAR = 1 };

typedef enum c_D2D1_DEBUG_LEVEL_tag c_D2D1_DEBUG_LEVEL;
enum c_D2D1_DEBUG_LEVEL_tag { c_D2D1_DEBUG_LEVEL_NONE = 0 };

typedef enum c_D2D1_FACTORY_TYPE_tag c_D2D1_FACTORY_TYPE;
enum c_D2D1_FACTORY_TYPE_tag { c_D2D1_FACTORY_TYPE_SINGLE_THREADED = 0, c_D2D1_FACTORY_TYPE_MULTI_THREADED = 1 };

typedef enum c_D2D1_FEATURE_LEVEL_tag c_D2D1_FEATURE_LEVEL;
enum c_D2D1_FEATURE_LEVEL_tag { c_D2D1_FEATURE_LEVEL_DEFAULT = 0 };

typedef enum c_D2D1_FIGURE_BEGIN_tag c_D2D1_FIGURE_BEGIN;
enum c_D2D1_FIGURE_BEGIN_tag { c_D2D1_FIGURE_BEGIN_FILLED = 0, c_D2D1_FIGURE_BEGIN_HOLLOW = 1 };

typedef enum c_D2D1_FIGURE_END_tag c_D2D1_FIGURE_END;
enum c_D2D1_FIGURE_END_tag { c_D2D1_FIGURE_END_OPEN = 0, c_D2D1_FIGURE_END_CLOSED = 1 };

typedef enum c_D2D1_BITMAP_INTERPOLATION_MODE_tag c_D2D1_BITMAP_INTERPOLATION_MODE;
enum c_D2D1_BITMAP_INTERPOLATION_MODE_tag {
    c_D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR = 0,
    c_D2D1_BITMAP_INTERPOLATION_MODE_LINEAR = 1
};

typedef enum c_D2D1_RENDER_TARGET_TYPE_tag c_D2D1_RENDER_TARGET_TYPE;
enum c_D2D1_RENDER_TARGET_TYPE_tag { c_D2D1_RENDER_TARGET_TYPE_DEFAULT = 0 };

typedef enum c_D2D1_SWEEP_DIRECTION_tag c_D2D1_SWEEP_DIRECTION;
enum c_D2D1_SWEEP_DIRECTION_tag { c_D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE = 0, c_D2D1_SWEEP_DIRECTION_CLOCKWISE = 1 };

typedef enum c_D2D1_CAP_STYLE_tag c_D2D1_CAP_STYLE;
enum c_D2D1_CAP_STYLE_tag {
    c_D2D1_CAP_STYLE_FLAT = 0,
    c_D2D1_CAP_STYLE_SQUARE = 1,
    c_D2D1_CAP_STYLE_ROUND = 2,
    c_D2D1_CAP_STYLE_TRIANGLE = 3,
};

typedef enum c_D2D1_DASH_STYLE_tag c_D2D1_DASH_STYLE;
enum c_D2D1_DASH_STYLE_tag {
    c_D2D1_DASH_STYLE_SOLID = 0,
    c_D2D1_DASH_STYLE_DASH = 1,
    c_D2D1_DASH_STYLE_DOT = 2,
    c_D2D1_DASH_STYLE_DASH_DOT = 3,
    c_D2D1_DASH_STYLE_DASH_DOT_DOT = 4,
    c_D2D1_DASH_STYLE_CUSTOM = 5
};

typedef enum c_D2D1_LINE_JOIN_tag c_D2D1_LINE_JOIN;
enum c_D2D1_LINE_JOIN_tag { c_D2D1_LINE_JOIN_MITER = 0, c_D2D1_LINE_JOIN_BEVEL = 1, c_D2D1_LINE_JOIN_ROUND = 2 };

typedef enum c_D2D1_GAMMA_tag c_D2D1_GAMMA;
enum c_D2D1_GAMMA_tag { c_D2D1_GAMMA_2_2 = 0, c_D2D1_GAMMA_1_0 = 1, c_D2D1_GAMMA_FORCE_DWORD = 2 };

typedef enum c_D2D1_EXTEND_MODE_tag c_D2D1_EXTEND_MODE;
enum c_D2D1_EXTEND_MODE_tag {
    c_D2D1_EXTEND_MODE_CLAMP = 0,
    c_D2D1_EXTEND_MODE_WRAP = 1,
    c_D2D1_EXTEND_MODE_MIRROR = 2,
    c_D2D1_EXTEND_MODE_FORCE_DWORD = 3
};

/*************************
 ***  Helper Typedefs  ***
 *************************/

typedef struct c_D2D1_BITMAP_PROPERTIES_tag c_D2D1_BITMAP_PROPERTIES;
typedef struct c_D2D1_BRUSH_PROPERTIES_tag c_D2D1_BRUSH_PROPERTIES;
typedef D2D_COLOR_F c_D2D1_COLOR_F;
typedef struct D2D_MATRIX_3X2_F c_D2D1_MATRIX_3X2_F;
typedef struct D2D_POINT_2F c_D2D1_POINT_2F;
typedef struct D2D_RECT_F c_D2D1_RECT_F;
typedef struct D2D_SIZE_F c_D2D1_SIZE_F;
typedef struct D2D_SIZE_U c_D2D1_SIZE_U;

/***************************
 ***  Helper Structures  ***
 ***************************/

typedef struct c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES_tag c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES;
struct c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES_tag {
    c_D2D1_POINT_2F startPoint;
    c_D2D1_POINT_2F endPoint;
};

typedef struct c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES_tag c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES;
struct c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES_tag {
    c_D2D1_POINT_2F center;
    c_D2D1_POINT_2F gradientOriginOffset;
    FLOAT radiusX;
    FLOAT radiusY;
};

typedef struct c_D2D1_GRADIENT_STOP_tag c_D2D1_GRADIENT_STOP;
struct c_D2D1_GRADIENT_STOP_tag {
    FLOAT position;
    c_D2D1_COLOR_F color;
};

typedef struct c_D2D1_ARC_SEGMENT_tag c_D2D1_ARC_SEGMENT;
struct c_D2D1_ARC_SEGMENT_tag {
    c_D2D1_POINT_2F point;
    c_D2D1_SIZE_F size;
    FLOAT rotationAngle;
    c_D2D1_SWEEP_DIRECTION sweepDirection;
    c_D2D1_ARC_SIZE arcSize;
};

typedef struct c_D2D1_BEZIER_SEGMENT_tag c_D2D1_BEZIER_SEGMENT;
struct c_D2D1_BEZIER_SEGMENT_tag {
    c_D2D1_POINT_2F point1;
    c_D2D1_POINT_2F point2;
    c_D2D1_POINT_2F point3;
};

typedef struct c_D2D1_ELLIPSE_tag c_D2D1_ELLIPSE;
struct c_D2D1_ELLIPSE_tag {
    c_D2D1_POINT_2F point;
    FLOAT radiusX;
    FLOAT radiusY;
};

typedef struct c_D2D1_FACTORY_OPTIONS_tag c_D2D1_FACTORY_OPTIONS;
struct c_D2D1_FACTORY_OPTIONS_tag {
    c_D2D1_DEBUG_LEVEL debugLevel;
};

typedef struct c_D2D1_HWND_RENDER_TARGET_PROPERTIES_tag c_D2D1_HWND_RENDER_TARGET_PROPERTIES;
struct c_D2D1_HWND_RENDER_TARGET_PROPERTIES_tag {
    HWND hwnd;
    c_D2D1_SIZE_U pixelSize;
    unsigned presentOptions;
};

typedef struct c_D2D1_PIXEL_FORMAT_tag c_D2D1_PIXEL_FORMAT;
struct c_D2D1_PIXEL_FORMAT_tag {
    c_DXGI_FORMAT format;
    c_D2D1_ALPHA_MODE alphaMode;
};

typedef struct c_D2D1_RENDER_TARGET_PROPERTIES_tag c_D2D1_RENDER_TARGET_PROPERTIES;
struct c_D2D1_RENDER_TARGET_PROPERTIES_tag {
    c_D2D1_RENDER_TARGET_TYPE type;
    c_D2D1_PIXEL_FORMAT pixelFormat;
    FLOAT dpiX;
    FLOAT dpiY;
    unsigned usage;
    c_D2D1_FEATURE_LEVEL minLevel;
};

typedef struct c_D2D1_STROKE_STYLE_PROPERTIES_tag c_D2D1_STROKE_STYLE_PROPERTIES;
struct c_D2D1_STROKE_STYLE_PROPERTIES_tag {
    c_D2D1_CAP_STYLE startCap;
    c_D2D1_CAP_STYLE endCap;
    c_D2D1_CAP_STYLE dashCap;
    c_D2D1_LINE_JOIN lineJoin;
    FLOAT miterLimit;
    c_D2D1_DASH_STYLE dashStyle;
    FLOAT dashOffset;
};

typedef struct c_D2D1_LAYER_PARAMETERS_tag c_D2D1_LAYER_PARAMETERS;
struct c_D2D1_LAYER_PARAMETERS_tag {
    c_D2D1_RECT_F contentBounds;
    c_ID2D1Geometry* geometricMask;
    c_D2D1_ANTIALIAS_MODE maskAntialiasMode;
    c_D2D1_MATRIX_3X2_F maskTransform;
    FLOAT opacity;
    c_ID2D1Brush* opacityBrush;
    unsigned layerOptions;
};

/*******************************
 ***  Interface ID2D1Bitmap  ***
 *******************************/

typedef struct c_ID2D1BitmapVtbl_tag c_ID2D1BitmapVtbl;
struct c_ID2D1BitmapVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Bitmap*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Bitmap*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Bitmap*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Image methods */
    /* none */

    /* ID2D1Bitmap methods */
    STDMETHOD(dummy_GetSize)(void);
#if 0
    /* Original vanilla method prototype. But this seems to be problematic
     * as compiler use different ABI when returning aggregate types.
     *
     * When built with incompatible compiler, it usually results in crash.
     *
     * For gcc, it seems to be completely incompatible:
     *  -- https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64384
     *  -- https://sourceforge.net/p/mingw-w64/mailman/message/36238073/
     *  -- https://source.winehq.org/git/wine.git/commitdiff/b42a15513eaa973b40ab967014b311af64acbb98
     *  -- https://www.winehq.org/pipermail/wine-devel/2017-July/118470.html
     *  -- https://bugzilla.mozilla.org/show_bug.cgi?id=1411401
     *
     * For MSVC, it is compatible only when building as C++. In C, it crashes
     * as well.
     */
    STDMETHOD_(c_D2D1_SIZE_U, GetPixelSize)(c_ID2D1Bitmap*);
#else
    /* This prototype corresponds more literally to what the COM calling
     * convention is expecting from us.
     *
     * Tested with MSVC 2017 (64-bit build), gcc (32-bit build).
     */
    STDMETHOD_(void, GetPixelSize)(c_ID2D1Bitmap*, c_D2D1_SIZE_U*);
#endif
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD(dummy_GetDpi)(void);
    STDMETHOD(dummy_CopyFromBitmap)(void);
    STDMETHOD(dummy_CopyFromRenderTarget)(void);
    STDMETHOD(dummy_CopyFromMemory)(void);
};

struct c_ID2D1Bitmap_tag {
    c_ID2D1BitmapVtbl* vtbl;
};

#define c_ID2D1Bitmap_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1Bitmap_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1Bitmap_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1Bitmap_GetPixelSize(self, a) (self)->vtbl->GetPixelSize(self, a)

/*******************************************
 ***  Interface ID2D1BitmapRenderTarget  ***
 *******************************************/

typedef struct c_ID2D1BitmapRenderTargetVtbl_tag c_ID2D1BitmapRenderTargetVtbl;
struct c_ID2D1BitmapRenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1BitmapRenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1BitmapRenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1BitmapRenderTarget*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1RenderTarget methods */
    STDMETHOD(dummy_CreateBitmap)(void);
    STDMETHOD(dummy_CreateBitmapFromWicBitmap)(void);
    STDMETHOD(dummy_CreateSharedBitmap)(void);
    STDMETHOD(dummy_CreateBitmapBrush)(void);
    STDMETHOD(dummy_CreateSolidColorBrush)(void);
    STDMETHOD(dummy_CreateGradientStopCollection)(void);
    STDMETHOD(dummy_CreateLinearGradientBrush)(void);
    STDMETHOD(dummy_CreateRadialGradientBrush)(void);
    STDMETHOD(dummy_CreateCompatibleRenderTarget)(void);
    STDMETHOD(dummy_CreateLayer)(void);
    STDMETHOD(dummy_CreateMesh)(void);
    STDMETHOD(dummy_DrawLine)(void);
    STDMETHOD(dummy_DrawRectangle)(void);
    STDMETHOD(dummy_FillRectangle)(void);
    STDMETHOD(dummy_DrawRoundedRectangle)(void);
    STDMETHOD(dummy_FillRoundedRectangle)(void);
    STDMETHOD(dummy_DrawEllipse)(void);
    STDMETHOD(dummy_FillEllipse)(void);
    STDMETHOD(dummy_DrawGeometry)(void);
    STDMETHOD(dummy_FillGeometry)(void);
    STDMETHOD(dummy_FillMesh)(void);
    STDMETHOD(dummy_FillOpacityMask)(void);
    STDMETHOD(dummy_DrawBitmap)(void);
    STDMETHOD(dummy_DrawText)(void);
    STDMETHOD(dummy_DrawTextLayout)(void);
    STDMETHOD(dummy_DrawGlyphRun)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetTransform)(void);
    STDMETHOD(dummy_SetAntialiasMode)(void);
    STDMETHOD(dummy_GetAntialiasMode)(void);
    STDMETHOD(dummy_SetTextAntialiasMode)(void);
    STDMETHOD(dummy_GetTextAntialiasMode)(void);
    STDMETHOD(dummy_SetTextRenderingParams)(void);
    STDMETHOD(dummy_GetTextRenderingParams)(void);
    STDMETHOD(dummy_SetTags)(void);
    STDMETHOD(dummy_GetTags)(void);
    STDMETHOD(dummy_PushLayer)(void);
    STDMETHOD(dummy_PopLayer)(void);
    STDMETHOD(dummy_Flush)(void);
    STDMETHOD(dummy_SaveDrawingState)(void);
    STDMETHOD(dummy_RestoreDrawingState)(void);
    STDMETHOD(dummy_PushAxisAlignedClip)(void);
    STDMETHOD(dummy_PopAxisAlignedClip)(void);
    STDMETHOD(dummy_Clear)(void);
    STDMETHOD(dummy_BeginDraw)(void);
    STDMETHOD(dummy_EndDraw)(void);
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD(dummy_SetDpi)(void);
    STDMETHOD(dummy_GetDpi)(void);
    STDMETHOD(dummy_GetSize)(void);
    STDMETHOD(dummy_GetPixelSize)(void);
    STDMETHOD(dummy_GetMaximumBitmapSize)(void);
    STDMETHOD(dummy_IsSupported)(void);

    /* ID2D1BitmapRenderTarget methods */
    STDMETHOD(dummy_GetBitmap)(void);
};

struct c_ID2D1BitmapRenderTarget_tag {
    c_ID2D1BitmapRenderTargetVtbl* vtbl;
};

#define c_ID2D1BitmapRenderTarget_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1BitmapRenderTarget_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1BitmapRenderTarget_Release(self) (self)->vtbl->Release(self)

/******************************
 ***  Interface ID2D1Brush  ***
 ******************************/

typedef struct c_ID2D1BrushVtbl_tag c_ID2D1BrushVtbl;
struct c_ID2D1BrushVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Brush*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Brush*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Brush*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Brush methods */
    STDMETHOD(dummy_SetOpacity)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetOpacity)(void);
    STDMETHOD(dummy_GetTransform)(void);
};

struct c_ID2D1Brush_tag {
    c_ID2D1BrushVtbl* vtbl;
};

#define c_ID2D1Brush_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1Brush_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1Brush_Release(self) (self)->vtbl->Release(self)

/***********************************
***  Interface ID2D1StrokeStyle  ***
***********************************/

typedef struct c_ID2D1StrokeStyleVtbl_tag c_ID2D1StrokeStyleVtbl;
struct c_ID2D1StrokeStyleVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1StrokeStyle*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1StrokeStyle*);
    STDMETHOD_(ULONG, Release)(c_ID2D1StrokeStyle*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1StrokeStyle methods */
    STDMETHOD(dummy_GetStartCap)(void);
    STDMETHOD(dummy_GetEndCap)(void);
    STDMETHOD(dummy_GetDashCap)(void);
    STDMETHOD(dummy_GetMiterLimit)(void);
    STDMETHOD(dummy_GetLineJoin)(void);
    STDMETHOD(dummy_GetDashOffset)(void);
    STDMETHOD(dummy_GetDashStyle)(void);
    STDMETHOD(dummy_GetDashesCount)(void);
    STDMETHOD(dummy_GetDashes)(void);
};

struct c_ID2D1StrokeStyle_tag {
    c_ID2D1StrokeStyleVtbl* vtbl;
};

#define c_ID2D1StrokeStyle_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1StrokeStyle_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1StrokeStyle_Release(self) (self)->vtbl->Release(self)

/*****************************************
 ***  Interface ID2D1DCRenderTarget  ***
 *****************************************/

typedef struct c_ID2D1DCRenderTargetVtbl_tag c_ID2D1DCRenderTargetVtbl;
struct c_ID2D1DCRenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1DCRenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1DCRenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1DCRenderTarget*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1RenderTarget methods */
    STDMETHOD(dummy_CreateBitmap)(void);
    STDMETHOD(dummy_CreateBitmapFromWicBitmap)(void);
    STDMETHOD(dummy_CreateSharedBitmap)(void);
    STDMETHOD(dummy_CreateBitmapBrush)(void);
    STDMETHOD(dummy_CreateSolidColorBrush)(void);
    STDMETHOD(dummy_CreateGradientStopCollection)(void);
    STDMETHOD(dummy_CreateLinearGradientBrush)(void);
    STDMETHOD(dummy_CreateRadialGradientBrush)(void);
    STDMETHOD(dummy_CreateCompatibleRenderTarget)(void);
    STDMETHOD(dummy_CreateLayer)(void);
    STDMETHOD(dummy_CreateMesh)(void);
    STDMETHOD(dummy_DrawLine)(void);
    STDMETHOD(dummy_DrawRectangle)(void);
    STDMETHOD(dummy_FillRectangle)(void);
    STDMETHOD(dummy_DrawRoundedRectangle)(void);
    STDMETHOD(dummy_FillRoundedRectangle)(void);
    STDMETHOD(dummy_DrawEllipse)(void);
    STDMETHOD(dummy_FillEllipse)(void);
    STDMETHOD(dummy_DrawGeometry)(void);
    STDMETHOD(dummy_FillGeometry)(void);
    STDMETHOD(dummy_FillMesh)(void);
    STDMETHOD(dummy_FillOpacityMask)(void);
    STDMETHOD(dummy_DrawBitmap)(void);
    STDMETHOD(dummy_DrawText)(void);
    STDMETHOD(dummy_DrawTextLayout)(void);
    STDMETHOD(dummy_DrawGlyphRun)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetTransform)(void);
    STDMETHOD(dummy_SetAntialiasMode)(void);
    STDMETHOD(dummy_GetAntialiasMode)(void);
    STDMETHOD(dummy_SetTextAntialiasMode)(void);
    STDMETHOD(dummy_GetTextAntialiasMode)(void);
    STDMETHOD(dummy_SetTextRenderingParams)(void);
    STDMETHOD(dummy_GetTextRenderingParams)(void);
    STDMETHOD(dummy_SetTags)(void);
    STDMETHOD(dummy_GetTags)(void);
    STDMETHOD(dummy_PushLayer)(void);
    STDMETHOD(dummy_PopLayer)(void);
    STDMETHOD(dummy_Flush)(void);
    STDMETHOD(dummy_SaveDrawingState)(void);
    STDMETHOD(dummy_RestoreDrawingState)(void);
    STDMETHOD(dummy_PushAxisAlignedClip)(void);
    STDMETHOD(dummy_PopAxisAlignedClip)(void);
    STDMETHOD(dummy_Clear)(void);
    STDMETHOD(dummy_BeginDraw)(void);
    STDMETHOD(dummy_EndDraw)(void);
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD(dummy_SetDpi)(void);
    STDMETHOD(dummy_GetDpi)(void);
    STDMETHOD(dummy_GetSize)(void);
    STDMETHOD(dummy_GetPixelSize)(void);
    STDMETHOD(dummy_GetMaximumBitmapSize)(void);
    STDMETHOD(dummy_IsSupported)(void);

    /* ID2D1DCRenderTarget methods */
    STDMETHOD(BindDC)(c_ID2D1DCRenderTarget*, const HDC, const RECT*);
};

struct c_ID2D1DCRenderTarget_tag {
    c_ID2D1DCRenderTargetVtbl* vtbl;
};

#define c_ID2D1DCRenderTarget_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1DCRenderTarget_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1DCRenderTarget_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1DCRenderTarget_BindDC(self, a, b) (self)->vtbl->BindDC(self, a, b)

/********************************
 ***  Interface ID2D1Factory  ***
 ********************************/

typedef struct c_ID2D1FactoryVtbl_tag c_ID2D1FactoryVtbl;
struct c_ID2D1FactoryVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Factory*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Factory*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Factory*);

    /* ID2D1Factory methods */
    STDMETHOD(dummy_ReloadSystemMetrics)(void);
    STDMETHOD(dummy_GetDesktopDpi)(void);
    STDMETHOD(dummy_CreateRectangleGeometry)(void);
    STDMETHOD(dummy_CreateRoundedRectangleGeometry)(void);
    STDMETHOD(dummy_CreateEllipseGeometry)(void);
    STDMETHOD(dummy_CreateGeometryGroup)(void);
    STDMETHOD(dummy_CreateTransformedGeometry)(void);
    STDMETHOD(CreatePathGeometry)(c_ID2D1Factory*, c_ID2D1PathGeometry**);
    STDMETHOD(CreateStrokeStyle)
    (c_ID2D1Factory*, const c_D2D1_STROKE_STYLE_PROPERTIES*, const FLOAT*, UINT32, c_ID2D1StrokeStyle**);
    STDMETHOD(dummy_CreateDrawingStateBlock)(void);
    STDMETHOD(dummy_CreateWicBitmapRenderTarget)(void);
    STDMETHOD(CreateHwndRenderTarget)
    (c_ID2D1Factory*, const c_D2D1_RENDER_TARGET_PROPERTIES*, const c_D2D1_HWND_RENDER_TARGET_PROPERTIES*,
     c_ID2D1HwndRenderTarget**);
    STDMETHOD(dummy_CreateDxgiSurfaceRenderTarget)(void);
    STDMETHOD(CreateDCRenderTarget)(c_ID2D1Factory*, const c_D2D1_RENDER_TARGET_PROPERTIES*, c_ID2D1DCRenderTarget**);
};

struct c_ID2D1Factory_tag {
    c_ID2D1FactoryVtbl* vtbl;
};

#define c_ID2D1Factory_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1Factory_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1Factory_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1Factory_CreatePathGeometry(self, a) (self)->vtbl->CreatePathGeometry(self, a)
#define c_ID2D1Factory_CreateHwndRenderTarget(self, a, b, c) (self)->vtbl->CreateHwndRenderTarget(self, a, b, c)
#define c_ID2D1Factory_CreateDCRenderTarget(self, a, b) (self)->vtbl->CreateDCRenderTarget(self, a, b)
#define c_ID2D1Factory_CreateStrokeStyle(self, a, b, c, d) (self)->vtbl->CreateStrokeStyle(self, a, b, c, d)

/*****************************************************
 ***  Interface c_ID2D1GdiInteropRenderTarget  ***
 *****************************************************/

typedef struct c_ID2D1GdiInteropRenderTargetVtbl_tag c_ID2D1GdiInteropRenderTargetVtbl;
struct c_ID2D1GdiInteropRenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1GdiInteropRenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1GdiInteropRenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1GdiInteropRenderTarget*);

    /* ID2D1GdiInteropRenderTarget methods */
    STDMETHOD(GetDC)(c_ID2D1GdiInteropRenderTarget*, c_D2D1_DC_INITIALIZE_MODE, HDC*);
    STDMETHOD(ReleaseDC)(c_ID2D1GdiInteropRenderTarget*, const RECT*);
};

struct c_ID2D1GdiInteropRenderTarget_tag {
    c_ID2D1GdiInteropRenderTargetVtbl* vtbl;
};

#define c_ID2D1GdiInteropRenderTarget_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1GdiInteropRenderTarget_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1GdiInteropRenderTarget_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1GdiInteropRenderTarget_GetDC(self, a, b) (self)->vtbl->GetDC(self, a, b)
#define c_ID2D1GdiInteropRenderTarget_ReleaseDC(self, a) (self)->vtbl->ReleaseDC(self, a)

/*********************************
 ***  Interface ID2D1Geometry  ***
 *********************************/

typedef struct c_ID2D1GeometryVtbl_tag c_ID2D1GeometryVtbl;
struct c_ID2D1GeometryVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Geometry*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Geometry*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Geometry*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Geometry methods */
    STDMETHOD(dummy_GetBounds)(void);
    STDMETHOD(dummy_GetWidenedBounds)(void);
    STDMETHOD(dummy_StrokeContainsPoint)(void);
    STDMETHOD(dummy_FillContainsPoint)(void);
    STDMETHOD(dummy_CompareWithGeometry)(void);
    STDMETHOD(dummy_Simplify)(void);
    STDMETHOD(dummy_Tessellate)(void);
    STDMETHOD(dummy_CombineWithGeometry)(void);
    STDMETHOD(dummy_Outline)(void);
    STDMETHOD(dummy_ComputeArea)(void);
    STDMETHOD(dummy_ComputeLength)(void);
    STDMETHOD(dummy_ComputePointAtLength)(void);
    STDMETHOD(dummy_Widen)(void);
};

struct c_ID2D1Geometry_tag {
    c_ID2D1GeometryVtbl* vtbl;
};

#define c_ID2D1Geometry_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1Geometry_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1Geometry_Release(self) (self)->vtbl->Release(self)

/*************************************
 ***  Interface ID2D1GeometrySink  ***
 *************************************/

typedef struct c_ID2D1GeometrySinkVtbl_tag c_ID2D1GeometrySinkVtbl;
struct c_ID2D1GeometrySinkVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1GeometrySink*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1GeometrySink*);
    STDMETHOD_(ULONG, Release)(c_ID2D1GeometrySink*);

    /* ID2D1SimplifiedGeometrySink methods */
    STDMETHOD(dummy_SetFillMode)(void);
    STDMETHOD(dummy_SetSegmentFlags)(void);
    STDMETHOD_(void, BeginFigure)(c_ID2D1GeometrySink*, c_D2D1_POINT_2F, c_D2D1_FIGURE_BEGIN);
    STDMETHOD(dummy_AddLines)(void);
    STDMETHOD(dummy_AddBeziers)(void);
    STDMETHOD_(void, EndFigure)(c_ID2D1GeometrySink*, c_D2D1_FIGURE_END);
    STDMETHOD(Close)(c_ID2D1GeometrySink*) PURE;

    /* ID2D1GeometrySink methods */
    STDMETHOD_(void, AddLine)(c_ID2D1GeometrySink*, c_D2D1_POINT_2F point);
    STDMETHOD_(void, AddBezier)(c_ID2D1GeometrySink*, const c_D2D1_BEZIER_SEGMENT*);
    STDMETHOD(dummy_AddQuadraticBezier)(void);
    STDMETHOD(dummy_AddQuadraticBeziers)(void);
    STDMETHOD_(void, AddArc)(c_ID2D1GeometrySink*, const c_D2D1_ARC_SEGMENT*);
};

struct c_ID2D1GeometrySink_tag {
    c_ID2D1GeometrySinkVtbl* vtbl;
};

#define c_ID2D1GeometrySink_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1GeometrySink_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1GeometrySink_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1GeometrySink_BeginFigure(self, a, b) (self)->vtbl->BeginFigure(self, a, b)
#define c_ID2D1GeometrySink_EndFigure(self, a) (self)->vtbl->EndFigure(self, a)
#define c_ID2D1GeometrySink_Close(self) (self)->vtbl->Close(self)
#define c_ID2D1GeometrySink_AddLine(self, a) (self)->vtbl->AddLine(self, a)
#define c_ID2D1GeometrySink_AddArc(self, a) (self)->vtbl->AddArc(self, a)
#define c_ID2D1GeometrySink_AddBezier(self, a) (self)->vtbl->AddBezier(self, a)

/*****************************************
 ***  Interface ID2D1HwndRenderTarget  ***
 *****************************************/

typedef struct c_ID2D1HwndRenderTargetVtbl_tag c_ID2D1HwndRenderTargetVtbl;
struct c_ID2D1HwndRenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1HwndRenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1HwndRenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1HwndRenderTarget*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1RenderTarget methods */
    STDMETHOD(dummy_CreateBitmap)(void);
    STDMETHOD(dummy_CreateBitmapFromWicBitmap)(void);
    STDMETHOD(dummy_CreateSharedBitmap)(void);
    STDMETHOD(dummy_CreateBitmapBrush)(void);
    STDMETHOD(dummy_CreateSolidColorBrush)(void);
    STDMETHOD(dummy_CreateGradientStopCollection)(void);
    STDMETHOD(dummy_CreateLinearGradientBrush)(void);
    STDMETHOD(dummy_CreateRadialGradientBrush)(void);
    STDMETHOD(dummy_CreateCompatibleRenderTarget)(void);
    STDMETHOD(dummy_CreateLayer)(void);
    STDMETHOD(dummy_CreateMesh)(void);
    STDMETHOD(dummy_DrawLine)(void);
    STDMETHOD(dummy_DrawRectangle)(void);
    STDMETHOD(dummy_FillRectangle)(void);
    STDMETHOD(dummy_DrawRoundedRectangle)(void);
    STDMETHOD(dummy_FillRoundedRectangle)(void);
    STDMETHOD(dummy_DrawEllipse)(void);
    STDMETHOD(dummy_FillEllipse)(void);
    STDMETHOD(dummy_DrawGeometry)(void);
    STDMETHOD(dummy_FillGeometry)(void);
    STDMETHOD(dummy_FillMesh)(void);
    STDMETHOD(dummy_FillOpacityMask)(void);
    STDMETHOD(dummy_DrawBitmap)(void);
    STDMETHOD(dummy_DrawText)(void);
    STDMETHOD(dummy_DrawTextLayout)(void);
    STDMETHOD(dummy_DrawGlyphRun)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetTransform)(void);
    STDMETHOD(dummy_SetAntialiasMode)(void);
    STDMETHOD(dummy_GetAntialiasMode)(void);
    STDMETHOD(dummy_SetTextAntialiasMode)(void);
    STDMETHOD(dummy_GetTextAntialiasMode)(void);
    STDMETHOD(dummy_SetTextRenderingParams)(void);
    STDMETHOD(dummy_GetTextRenderingParams)(void);
    STDMETHOD(dummy_SetTags)(void);
    STDMETHOD(dummy_GetTags)(void);
    STDMETHOD(dummy_PushLayer)(void);
    STDMETHOD(dummy_PopLayer)(void);
    STDMETHOD(dummy_Flush)(void);
    STDMETHOD(dummy_SaveDrawingState)(void);
    STDMETHOD(dummy_RestoreDrawingState)(void);
    STDMETHOD(dummy_PushAxisAlignedClip)(void);
    STDMETHOD(dummy_PopAxisAlignedClip)(void);
    STDMETHOD(dummy_Clear)(void);
    STDMETHOD(dummy_BeginDraw)(void);
    STDMETHOD(dummy_EndDraw)(void);
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD(dummy_SetDpi)(void);
    STDMETHOD(dummy_GetDpi)(void);
    STDMETHOD(dummy_GetSize)(void);
    STDMETHOD(dummy_GetPixelSize)(void);
    STDMETHOD(dummy_GetMaximumBitmapSize)(void);
    STDMETHOD(dummy_IsSupported)(void);

    /* ID2D1HwndRenderTarget methods */
    STDMETHOD(dummy_CheckWindowState)(void);
    STDMETHOD(Resize)(c_ID2D1HwndRenderTarget*, const c_D2D1_SIZE_U*);
    STDMETHOD(dummy_GetHwnd)(void);
};

struct c_ID2D1HwndRenderTarget_tag {
    c_ID2D1HwndRenderTargetVtbl* vtbl;
};

#define c_ID2D1HwndRenderTarget_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1HwndRenderTarget_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1HwndRenderTarget_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1HwndRenderTarget_Resize(self, a) (self)->vtbl->Resize(self, a)

/******************************
 ***  Interface ID2D1Layer  ***
 ******************************/

typedef struct c_ID2D1LayerVtbl_tag c_ID2D1LayerVtbl;
struct c_ID2D1LayerVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1Layer*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1Layer*);
    STDMETHOD_(ULONG, Release)(c_ID2D1Layer*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Layer methods */
    STDMETHOD(dummy_GetSize)(void);
};

struct c_ID2D1Layer_tag {
    c_ID2D1LayerVtbl* vtbl;
};

#define c_ID2D1Layer_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1Layer_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1Layer_Release(self) (self)->vtbl->Release(self)

/*************************************
 ***  Interface ID2D1PathGeometry  ***
 *************************************/

typedef struct c_ID2D1PathGeometryVtbl_tag c_ID2D1PathGeometryVtbl;
struct c_ID2D1PathGeometryVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1PathGeometry*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1PathGeometry*);
    STDMETHOD_(ULONG, Release)(c_ID2D1PathGeometry*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Geometry methods */
    STDMETHOD(dummy_GetBounds)(void);
    STDMETHOD(dummy_GetWidenedBounds)(void);
    STDMETHOD(dummy_StrokeContainsPoint)(void);
    STDMETHOD(dummy_FillContainsPoint)(void);
    STDMETHOD(dummy_CompareWithGeometry)(void);
    STDMETHOD(dummy_Simplify)(void);
    STDMETHOD(dummy_Tessellate)(void);
    STDMETHOD(dummy_CombineWithGeometry)(void);
    STDMETHOD(dummy_Outline)(void);
    STDMETHOD(dummy_ComputeArea)(void);
    STDMETHOD(dummy_ComputeLength)(void);
    STDMETHOD(dummy_ComputePointAtLength)(void);
    STDMETHOD(dummy_Widen)(void);

    /* ID2D1PathGeometry methods */
    STDMETHOD(Open)(c_ID2D1PathGeometry*, c_ID2D1GeometrySink**);
    STDMETHOD(dummy_Stream)(void);
    STDMETHOD(dummy_GetSegmentCount)(void);
    STDMETHOD(dummy_GetFigureCount)(void);
};

struct c_ID2D1PathGeometry_tag {
    c_ID2D1PathGeometryVtbl* vtbl;
};

#define c_ID2D1PathGeometry_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1PathGeometry_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1PathGeometry_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1PathGeometry_Open(self, a) (self)->vtbl->Open(self, a)

/*************************************
 ***  Interface ID2D1RenderTarget  ***
 *************************************/

typedef struct c_ID2D1RenderTargetVtbl_tag c_ID2D1RenderTargetVtbl;
struct c_ID2D1RenderTargetVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1RenderTarget*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1RenderTarget*);
    STDMETHOD_(ULONG, Release)(c_ID2D1RenderTarget*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1RenderTarget methods */
    STDMETHOD(dummy_CreateBitmap)(void);
    STDMETHOD(CreateBitmapFromWicBitmap)
    (c_ID2D1RenderTarget*, IWICBitmapSource*, const c_D2D1_BITMAP_PROPERTIES*, c_ID2D1Bitmap**);
    STDMETHOD(dummy_CreateSharedBitmap)(void);
    STDMETHOD(dummy_CreateBitmapBrush)(void);
    STDMETHOD(CreateSolidColorBrush)
    (c_ID2D1RenderTarget*, const c_D2D1_COLOR_F*, const void*, c_ID2D1SolidColorBrush**);
    STDMETHOD(CreateGradientStopCollection)
    (c_ID2D1RenderTarget*, const c_D2D1_GRADIENT_STOP*, UINT32, c_D2D1_GAMMA, c_D2D1_EXTEND_MODE,
     c_ID2D1GradientStopCollection**);
    STDMETHOD(CreateLinearGradientBrush)
    (c_ID2D1RenderTarget*, const c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES*, const c_D2D1_BRUSH_PROPERTIES*,
     c_ID2D1GradientStopCollection*, c_ID2D1LinearGradientBrush**);
    STDMETHOD(CreateRadialGradientBrush)
    (c_ID2D1RenderTarget*, const c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES*, const c_D2D1_BRUSH_PROPERTIES*,
     c_ID2D1GradientStopCollection*, c_ID2D1RadialGradientBrush**);
    STDMETHOD(dummy_CreateCompatibleRenderTarget)(void);
    STDMETHOD(CreateLayer)(c_ID2D1RenderTarget*, const c_D2D1_SIZE_F*, c_ID2D1Layer**);
    STDMETHOD(dummy_CreateMesh)(void);
    STDMETHOD_(void, DrawLine)
    (c_ID2D1RenderTarget*, c_D2D1_POINT_2F, c_D2D1_POINT_2F, c_ID2D1Brush*, FLOAT, c_ID2D1StrokeStyle*);
    STDMETHOD_(void, DrawRectangle)
    (c_ID2D1RenderTarget*, const c_D2D1_RECT_F*, c_ID2D1Brush*, FLOAT, c_ID2D1StrokeStyle*);
    STDMETHOD_(void, FillRectangle)(c_ID2D1RenderTarget*, const c_D2D1_RECT_F*, c_ID2D1Brush*);
    STDMETHOD(dummy_DrawRoundedRectangle)(void);
    STDMETHOD(dummy_FillRoundedRectangle)(void);
    STDMETHOD_(void, DrawEllipse)
    (c_ID2D1RenderTarget*, const c_D2D1_ELLIPSE*, c_ID2D1Brush*, FLOAT, c_ID2D1StrokeStyle*);
    STDMETHOD_(void, FillEllipse)(c_ID2D1RenderTarget*, const c_D2D1_ELLIPSE*, c_ID2D1Brush*);
    STDMETHOD_(void, DrawGeometry)(c_ID2D1RenderTarget*, c_ID2D1Geometry*, c_ID2D1Brush*, FLOAT, c_ID2D1StrokeStyle*);
    STDMETHOD_(void, FillGeometry)(c_ID2D1RenderTarget*, c_ID2D1Geometry*, c_ID2D1Brush*, c_ID2D1Brush*);
    STDMETHOD(dummy_FillMesh)(void);
    STDMETHOD(dummy_FillOpacityMask)(void);
    STDMETHOD_(void, DrawBitmap)
    (c_ID2D1RenderTarget*, c_ID2D1Bitmap*, const c_D2D1_RECT_F*, FLOAT, c_D2D1_BITMAP_INTERPOLATION_MODE,
     const c_D2D1_RECT_F*);
    STDMETHOD(dummy_DrawText)(void);
    STDMETHOD_(void, DrawTextLayout)
    (c_ID2D1RenderTarget*, c_D2D1_POINT_2F, c_IDWriteTextLayout*, c_ID2D1Brush*, unsigned);
    STDMETHOD_(void, DrawGlyphRun)
    (c_ID2D1RenderTarget*, c_D2D1_POINT_2F, const c_DWRITE_GLYPH_RUN*, c_ID2D1Brush*, c_DWRITE_MEASURING_MODE);
    STDMETHOD_(void, SetTransform)(c_ID2D1RenderTarget*, const c_D2D1_MATRIX_3X2_F*);
    STDMETHOD_(void, GetTransform)(c_ID2D1RenderTarget*, c_D2D1_MATRIX_3X2_F*);
    STDMETHOD(dummy_SetAntialiasMode)(void);
    STDMETHOD(dummy_GetAntialiasMode)(void);
    STDMETHOD(SetTextAntialiasMode)(c_ID2D1RenderTarget*, c_D2D1_TEXT_ANTIALIAS_MODE);
    STDMETHOD(dummy_GetTextAntialiasMode)(void);
    STDMETHOD(dummy_SetTextRenderingParams)(void);
    STDMETHOD(dummy_GetTextRenderingParams)(void);
    STDMETHOD(dummy_SetTags)(void);
    STDMETHOD(dummy_GetTags)(void);
    STDMETHOD_(void, PushLayer)(c_ID2D1RenderTarget*, const c_D2D1_LAYER_PARAMETERS*, c_ID2D1Layer*);
    STDMETHOD_(void, PopLayer)(c_ID2D1RenderTarget*);
    STDMETHOD(dummy_Flush)(void);
    STDMETHOD(dummy_SaveDrawingState)(void);
    STDMETHOD(dummy_RestoreDrawingState)(void);
    STDMETHOD_(void, PushAxisAlignedClip)(c_ID2D1RenderTarget*, const c_D2D1_RECT_F*, c_D2D1_ANTIALIAS_MODE);
    STDMETHOD_(void, PopAxisAlignedClip)(c_ID2D1RenderTarget*);
    STDMETHOD_(void, Clear)(c_ID2D1RenderTarget*, const c_D2D1_COLOR_F*);
    STDMETHOD_(void, BeginDraw)(c_ID2D1RenderTarget*);
    STDMETHOD(EndDraw)(c_ID2D1RenderTarget*, void*, void*);
    STDMETHOD(dummy_GetPixelFormat)(void);
    STDMETHOD_(void, SetDpi)(c_ID2D1RenderTarget*, FLOAT, FLOAT);
    STDMETHOD_(void, GetDpi)(c_ID2D1RenderTarget*, FLOAT*, FLOAT*);
    STDMETHOD(dummy_GetSize)(void);
    STDMETHOD(dummy_GetPixelSize)(void);
    STDMETHOD(dummy_GetMaximumBitmapSize)(void);
    STDMETHOD(dummy_IsSupported)(void);
};

struct c_ID2D1RenderTarget_tag {
    c_ID2D1RenderTargetVtbl* vtbl;
};

#define c_ID2D1RenderTarget_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1RenderTarget_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1RenderTarget_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1RenderTarget_CreateBitmapFromWicBitmap(self, a, b, c) \
    (self)->vtbl->CreateBitmapFromWicBitmap(self, a, b, c)
#define c_ID2D1RenderTarget_CreateSolidColorBrush(self, a, b, c) (self)->vtbl->CreateSolidColorBrush(self, a, b, c)
#define c_ID2D1RenderTarget_CreateLinearGradientBrush(self, a, b, c, d) \
    (self)->vtbl->CreateLinearGradientBrush(self, a, b, c, d)
#define c_ID2D1RenderTarget_CreateRadialGradientBrush(self, a, b, c, d) \
    (self)->vtbl->CreateRadialGradientBrush(self, a, b, c, d)
#define c_ID2D1RenderTarget_CreateGradientStopCollection(self, a, b, c, d, e) \
    (self)->vtbl->CreateGradientStopCollection(self, a, b, c, d, e)
#define c_ID2D1RenderTarget_CreateLayer(self, a, b) (self)->vtbl->CreateLayer(self, a, b)
#define c_ID2D1RenderTarget_DrawLine(self, a, b, c, d, e) (self)->vtbl->DrawLine(self, a, b, c, d, e)
#define c_ID2D1RenderTarget_DrawRectangle(self, a, b, c, d) (self)->vtbl->DrawRectangle(self, a, b, c, d)
#define c_ID2D1RenderTarget_FillRectangle(self, a, b) (self)->vtbl->FillRectangle(self, a, b)
#define c_ID2D1RenderTarget_DrawEllipse(self, a, b, c, d) (self)->vtbl->DrawEllipse(self, a, b, c, d)
#define c_ID2D1RenderTarget_FillEllipse(self, a, b) (self)->vtbl->FillEllipse(self, a, b)
#define c_ID2D1RenderTarget_DrawGeometry(self, a, b, c, d) (self)->vtbl->DrawGeometry(self, a, b, c, d)
#define c_ID2D1RenderTarget_FillGeometry(self, a, b, c) (self)->vtbl->FillGeometry(self, a, b, c)
#define c_ID2D1RenderTarget_DrawBitmap(self, a, b, c, d, e) (self)->vtbl->DrawBitmap(self, a, b, c, d, e)
#define c_ID2D1RenderTarget_DrawTextLayout(self, a, b, c, d) (self)->vtbl->DrawTextLayout(self, a, b, c, d)
#define c_ID2D1RenderTarget_DrawGlyphRun(self, a, b, c, d) (self)->vtbl->DrawGlyphRun(self, a, b, c, d)
#define c_ID2D1RenderTarget_SetTransform(self, a) (self)->vtbl->SetTransform(self, a)
#define c_ID2D1RenderTarget_GetTransform(self, a) (self)->vtbl->GetTransform(self, a)
#define c_ID2D1RenderTarget_SetTextAntialiasMode(self, a) (self)->vtbl->SetTextAntialiasMode(self, a)
#define c_ID2D1RenderTarget_PushLayer(self, a, b) (self)->vtbl->PushLayer(self, a, b)
#define c_ID2D1RenderTarget_PopLayer(self) (self)->vtbl->PopLayer(self)
#define c_ID2D1RenderTarget_PushAxisAlignedClip(self, a, b) (self)->vtbl->PushAxisAlignedClip(self, a, b)
#define c_ID2D1RenderTarget_PopAxisAlignedClip(self) (self)->vtbl->PopAxisAlignedClip(self)
#define c_ID2D1RenderTarget_Clear(self, a) (self)->vtbl->Clear(self, a)
#define c_ID2D1RenderTarget_BeginDraw(self) (self)->vtbl->BeginDraw(self)
#define c_ID2D1RenderTarget_EndDraw(self, a, b) (self)->vtbl->EndDraw(self, a, b)
#define c_ID2D1RenderTarget_SetDpi(self, a, b) (self)->vtbl->SetDpi(self, a, b)
#define c_ID2D1RenderTarget_GetDpi(self, a, b) (self)->vtbl->GetDpi(self, a, b)

/*********************************************
 ***  Interface ID2D1SolidColorBrushBrush  ***
 *********************************************/

typedef struct c_ID2D1SolidColorBrushVtbl_tag c_ID2D1SolidColorBrushVtbl;
struct c_ID2D1SolidColorBrushVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1SolidColorBrush*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1SolidColorBrush*);
    STDMETHOD_(ULONG, Release)(c_ID2D1SolidColorBrush*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Brush methods */
    STDMETHOD(dummy_SetOpacity)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetOpacity)(void);
    STDMETHOD(dummy_GetTransform)(void);

    /* ID2D1SolidColorBrushBrush methods */
    STDMETHOD_(void, SetColor)(c_ID2D1SolidColorBrush*, const c_D2D1_COLOR_F*);
    STDMETHOD(dummy_GetColor)(void);
};

struct c_ID2D1SolidColorBrush_tag {
    c_ID2D1SolidColorBrushVtbl* vtbl;
};

#define c_ID2D1SolidColorBrush_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1SolidColorBrush_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1SolidColorBrush_Release(self) (self)->vtbl->Release(self)
#define c_ID2D1SolidColorBrush_SetColor(self, a) (self)->vtbl->SetColor(self, a)

/*********************************************
 ***  Interface ID2D1LinearGradientBrush   ***
 *********************************************/

typedef struct c_ID2D1LinearGradientBrushVtbl_tag c_ID2D1LinearGradientBrushVtbl;
struct c_ID2D1LinearGradientBrushVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1LinearGradientBrush*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1LinearGradientBrush*);
    STDMETHOD_(ULONG, Release)(c_ID2D1LinearGradientBrush*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Brush methods */
    STDMETHOD(dummy_SetOpacity)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetOpacity)(void);
    STDMETHOD(dummy_GetTransform)(void);

    /* ID2D1LinearGradientBrush methods */
    STDMETHOD(dummy_SetStartPoint)(void);
    STDMETHOD(dummy_SetEndPoint)(void);
    STDMETHOD(dummy_GetStartPoint)(void);
    STDMETHOD(dummy_GetEndPoint)(void);
    STDMETHOD(dummy_GetGradientStopCollection)(void);
};

struct c_ID2D1LinearGradientBrush_tag {
    c_ID2D1LinearGradientBrushVtbl* vtbl;
};

#define c_ID2D1LinearGradientBrush_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1LinearGradientBrush_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1LinearGradientBrush_Release(self) (self)->vtbl->Release(self)

/*********************************************
 ***  Interface ID2D1RadialGradientBrush   ***
 *********************************************/

typedef struct c_ID2D1RadialGradientBrushVtbl_tag c_ID2D1RadialGradientBrushVtbl;
struct c_ID2D1RadialGradientBrushVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1RadialGradientBrush*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1RadialGradientBrush*);
    STDMETHOD_(ULONG, Release)(c_ID2D1RadialGradientBrush*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1Brush methods */
    STDMETHOD(dummy_SetOpacity)(void);
    STDMETHOD(dummy_SetTransform)(void);
    STDMETHOD(dummy_GetOpacity)(void);
    STDMETHOD(dummy_GetTransform)(void);

    /* ID2D1RadialGradientBrush methods */
    STDMETHOD(dummy_SetCenter)(void);
    STDMETHOD(dummy_SetGradientOriginOffset)(void);
    STDMETHOD(dummy_SetRadiusX)(void);
    STDMETHOD(dummy_SetRadiusY)(void);
    STDMETHOD(dummy_GetCenter)(void);
    STDMETHOD(dummy_GetGradientOriginOffset)(void);
    STDMETHOD(dummy_GetRadiusX)(void);
    STDMETHOD(dummy_GetRadiusY)(void);
    STDMETHOD(dummy_GetGradientStopCollection)(void);
};

struct c_ID2D1RadialGradientBrush_tag {
    c_ID2D1RadialGradientBrushVtbl* vtbl;
};

#define c_ID2D1RadialGradientBrush_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1RadialGradientBrush_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1RadialGradientBrush_Release(self) (self)->vtbl->Release(self)

/************************************************
 ***  Interface ID2D1GradientStopCollection   ***
 ************************************************/

typedef struct c_ID2D1GradientStopCollectionVtbl_tag c_ID2D1GradientStopCollectionVtbl;
struct c_ID2D1GradientStopCollectionVtbl_tag {
    /* IUnknown methods */
    STDMETHOD(QueryInterface)(c_ID2D1GradientStopCollection*, REFIID, void**);
    STDMETHOD_(ULONG, AddRef)(c_ID2D1GradientStopCollection*);
    STDMETHOD_(ULONG, Release)(c_ID2D1GradientStopCollection*);

    /* ID2D1Resource methods */
    STDMETHOD(dummy_GetFactory)(void);

    /* ID2D1GradientStopCollection methods */
    STDMETHOD(dummy_GetGradientStopCount)(void);
    STDMETHOD(dummy_GetGradientStops)(void);
    STDMETHOD(dummy_GetColorInterpolationGamma)(void);
    STDMETHOD(dummy_GetExtendMode)(void);
};

struct c_ID2D1GradientStopCollection_tag {
    c_ID2D1GradientStopCollectionVtbl* vtbl;
};

#define c_ID2D1GradientStopCollection_QueryInterface(self, a, b) (self)->vtbl->QueryInterface(self, a, b)
#define c_ID2D1GradientStopCollection_AddRef(self) (self)->vtbl->AddRef(self)
#define c_ID2D1GradientStopCollection_Release(self) (self)->vtbl->Release(self)

/* ------------------------------------- backend-d2d.h ---------------------------------- */
#define D2D_CANVASTYPE_BITMAP 0
#define D2D_CANVASTYPE_DC 1
#define D2D_CANVASTYPE_HWND 2

#define D2D_CANVASFLAG_RECTCLIP 0x1
#define D2D_CANVASFLAG_RTL 0x2

#define D2D_BASEDELTA_X 0.5f
#define D2D_BASEDELTA_Y 0.5f

typedef struct d2d_canvas_tag d2d_canvas_t;
struct d2d_canvas_tag {
    WORD type;
    WORD flags;
    UINT width;
    union {
        c_ID2D1RenderTarget* target;
        c_ID2D1BitmapRenderTarget* bmp_target;
        c_ID2D1HwndRenderTarget* hwnd_target;
    };
    c_ID2D1GdiInteropRenderTarget* gdi_interop;
    c_ID2D1Layer* clip_layer;
};

extern c_ID2D1Factory* d2d_factory;

static inline BOOL d2d_enabled(void) {
    return (d2d_factory != NULL);
}

static inline void d2d_init_color(c_D2D1_COLOR_F* c, WD_COLOR color) {
    c->r = WD_RVALUE(color) / 255.0f;
    c->g = WD_GVALUE(color) / 255.0f;
    c->b = WD_BVALUE(color) / 255.0f;
    c->a = WD_AVALUE(color) / 255.0f;
}

int d2d_init(void);
void d2d_fini(void);

d2d_canvas_t* d2d_canvas_alloc(c_ID2D1RenderTarget* target, WORD type, UINT width, BOOL rtl);

void d2d_reset_clip(d2d_canvas_t* c);

void d2d_reset_transform(d2d_canvas_t* c);
void d2d_apply_transform(d2d_canvas_t* c, const c_D2D1_MATRIX_3X2_F* matrix);

/* Note: Can be called only if D2D_CANVASFLAG_RTL, and have to reinstall
 * the original transformation then-after. */
void d2d_disable_rtl_transform(d2d_canvas_t* c, c_D2D1_MATRIX_3X2_F* old_matrix);

void d2d_setup_arc_segment(c_D2D1_ARC_SEGMENT* arc_seg, float cx, float cy, float rx, float ry, float base_angle,
                           float sweep_angle);
void d2d_setup_bezier_segment(c_D2D1_BEZIER_SEGMENT* bezier_seg, float x0, float y0, float x1, float y1, float x2,
                              float y2);
c_ID2D1Geometry* d2d_create_arc_geometry(float cx, float cy, float rx, float ry, float base_angle, float sweep_angle,
                                         BOOL pie);

/* --------------------------------------- backend-d2d.c -------------------------- */

static HMODULE d2d_dll = NULL;

c_ID2D1Factory* d2d_factory = NULL;

static inline void d2d_matrix_mult(c_D2D1_MATRIX_3X2_F* res, const c_D2D1_MATRIX_3X2_F* a,
                                   const c_D2D1_MATRIX_3X2_F* b) {
    res->_11 = a->_11 * b->_11 + a->_12 * b->_21;
    res->_12 = a->_11 * b->_12 + a->_12 * b->_22;
    res->_21 = a->_21 * b->_11 + a->_22 * b->_21;
    res->_22 = a->_21 * b->_12 + a->_22 * b->_22;
    res->_31 = a->_31 * b->_11 + a->_32 * b->_21 + b->_31;
    res->_32 = a->_31 * b->_12 + a->_32 * b->_22 + b->_32;
}


void d2d_fini(void) {
    c_ID2D1Factory_Release(d2d_factory);
    FreeLibrary(d2d_dll);
    d2d_dll = NULL;
}

d2d_canvas_t* d2d_canvas_alloc(c_ID2D1RenderTarget* target, WORD type, UINT width, BOOL rtl) {
    d2d_canvas_t* c;

    c = (d2d_canvas_t*)malloc(sizeof(d2d_canvas_t));
    if (c == NULL) {
        WD_TRACE("d2d_canvas_alloc: malloc() failed.");
        return NULL;
    }

    memset(c, 0, sizeof(d2d_canvas_t));

    c->type = type;
    c->flags = (rtl ? D2D_CANVASFLAG_RTL : 0);
    c->width = width;
    c->target = target;

    /* We use raw pixels as units. D2D by default works with DIPs ("device
     * independent pixels"), which map 1:1 to physical pixels when DPI is 96.
     * So we enforce the render target to think we have this DPI. */
    c_ID2D1RenderTarget_SetDpi(c->target, 96.0f, 96.0f);

    d2d_reset_transform(c);

    return c;
}

void d2d_reset_clip(d2d_canvas_t* c) {
    if (c->clip_layer != NULL) {
        c_ID2D1RenderTarget_PopLayer(c->target);
        c_ID2D1Layer_Release(c->clip_layer);
        c->clip_layer = NULL;
    }
    if (c->flags & D2D_CANVASFLAG_RECTCLIP) {
        c_ID2D1RenderTarget_PopAxisAlignedClip(c->target);
        c->flags &= ~D2D_CANVASFLAG_RECTCLIP;
    }
}

void d2d_reset_transform(d2d_canvas_t* c) {
    c_D2D1_MATRIX_3X2_F m;

    if (c->flags & D2D_CANVASFLAG_RTL) {
        m._11 = -1.0f;
        m._12 = 0.0f;
        m._21 = 0.0f;
        m._22 = 1.0f;
        m._31 = (float)c->width - 1.0f + D2D_BASEDELTA_X;
        m._32 = D2D_BASEDELTA_Y;
    } else {
        m._11 = 1.0f;
        m._12 = 0.0f;
        m._21 = 0.0f;
        m._22 = 1.0f;
        m._31 = D2D_BASEDELTA_X;
        m._32 = D2D_BASEDELTA_Y;
    }

    c_ID2D1RenderTarget_SetTransform(c->target, &m);
}

void d2d_apply_transform(d2d_canvas_t* c, const c_D2D1_MATRIX_3X2_F* matrix) {
    c_D2D1_MATRIX_3X2_F res;
    c_D2D1_MATRIX_3X2_F old_matrix;

    c_ID2D1RenderTarget_GetTransform(c->target, &old_matrix);
    d2d_matrix_mult(&res, matrix, &old_matrix);
    c_ID2D1RenderTarget_SetTransform(c->target, &res);
}

void d2d_disable_rtl_transform(d2d_canvas_t* c, c_D2D1_MATRIX_3X2_F* old_matrix) {
    c_D2D1_MATRIX_3X2_F r;  /* Reflection + transition for WD_CANVAS_LAYOUTRTL. */
    c_D2D1_MATRIX_3X2_F ur; /* R * user's transformation. */
    c_D2D1_MATRIX_3X2_F u;  /* Only user's transformation. */

    r._11 = -1.0f;
    r._12 = 0.0f;
    r._21 = 0.0f;
    r._22 = 1.0f;
    r._31 = (float)c->width;
    r._32 = 0.0f;

    c_ID2D1RenderTarget_GetTransform(c->target, &ur);
    if (old_matrix != NULL)
        memcpy(old_matrix, &ur, sizeof(c_D2D1_MATRIX_3X2_F));
    ur._31 += D2D_BASEDELTA_X;
    ur._32 -= D2D_BASEDELTA_Y;

    /* Note R is inverse to itself. */
    d2d_matrix_mult(&u, &ur, &r);

    c_ID2D1RenderTarget_SetTransform(c->target, &u);
}

void d2d_setup_arc_segment(c_D2D1_ARC_SEGMENT* arc_seg, float cx, float cy, float rx, float ry, float base_angle,
                           float sweep_angle) {
    float sweep_rads = (base_angle + sweep_angle) * (WD_PI / 180.0f);

    arc_seg->point.x = cx + rx * cosf(sweep_rads);
    arc_seg->point.y = cy + ry * sinf(sweep_rads);
    arc_seg->size.width = rx;
    arc_seg->size.height = ry;
    arc_seg->rotationAngle = 0.0f;

    if (sweep_angle >= 0.0f)
        arc_seg->sweepDirection = c_D2D1_SWEEP_DIRECTION_CLOCKWISE;
    else
        arc_seg->sweepDirection = c_D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;

    if (sweep_angle >= 180.0f)
        arc_seg->arcSize = c_D2D1_ARC_SIZE_LARGE;
    else
        arc_seg->arcSize = c_D2D1_ARC_SIZE_SMALL;
}

void d2d_setup_bezier_segment(c_D2D1_BEZIER_SEGMENT* bezier_seg, float x0, float y0, float x1, float y1, float x2,
                              float y2) {
    bezier_seg->point1.x = x0;
    bezier_seg->point1.y = y0;
    bezier_seg->point2.x = x1;
    bezier_seg->point2.y = y1;
    bezier_seg->point3.x = x2;
    bezier_seg->point3.y = y2;
}

c_ID2D1Geometry* d2d_create_arc_geometry(float cx, float cy, float rx, float ry, float base_angle, float sweep_angle,
                                         BOOL pie) {
    c_ID2D1PathGeometry* g = NULL;
    c_ID2D1GeometrySink* s;
    HRESULT hr;
    float base_rads = base_angle * (WD_PI / 180.0f);
    c_D2D1_POINT_2F pt;
    c_D2D1_ARC_SEGMENT arc_seg;

    wd_lock();
    hr = c_ID2D1Factory_CreatePathGeometry(d2d_factory, &g);
    wd_unlock();
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "d2d_create_arc_geometry: "
            "ID2D1Factory::CreatePathGeometry() failed.");
        return NULL;
    }
    hr = c_ID2D1PathGeometry_Open(g, &s);
    if (FAILED(hr)) {
        WD_TRACE_HR("d2d_create_arc_geometry: ID2D1PathGeometry::Open() failed.");
        c_ID2D1PathGeometry_Release(g);
        return NULL;
    }

    pt.x = cx + rx * cosf(base_rads);
    pt.y = cy + ry * sinf(base_rads);
    c_ID2D1GeometrySink_BeginFigure(s, pt, c_D2D1_FIGURE_BEGIN_FILLED);

    d2d_setup_arc_segment(&arc_seg, cx, cy, rx, ry, base_angle, sweep_angle);
    c_ID2D1GeometrySink_AddArc(s, &arc_seg);

    if (pie) {
        pt.x = cx;
        pt.y = cy;
        c_ID2D1GeometrySink_AddLine(s, pt);
        c_ID2D1GeometrySink_EndFigure(s, c_D2D1_FIGURE_END_CLOSED);
    } else {
        c_ID2D1GeometrySink_EndFigure(s, c_D2D1_FIGURE_END_OPEN);
    }

    c_ID2D1GeometrySink_Close(s);
    c_ID2D1GeometrySink_Release(s);

    return (c_ID2D1Geometry*)g;
}

/* --------------------------------- backend-dwrite.h ------------------------------ */


extern c_IDWriteFactory* dwrite_factory;

typedef struct dwrite_font_tag dwrite_font_t;
struct dwrite_font_tag {
    c_IDWriteTextFormat* tf;
    c_DWRITE_FONT_METRICS metrics;
};

int dwrite_init(void);
void dwrite_fini(void);

void dwrite_default_user_locale(WCHAR buffer[LOCALE_NAME_MAX_LENGTH]);

c_IDWriteTextFormat* dwrite_create_text_format(const WCHAR* locale_name, const LOGFONTW* logfont,
                                               c_DWRITE_FONT_METRICS* metrics);

c_IDWriteTextLayout* dwrite_create_text_layout(c_IDWriteTextFormat* tf, const WD_RECT* rect, const WCHAR* str, int len,
                                               DWORD flags);

/* --------------------------------- backend-dwrite.c ------------------------------ */


static HMODULE dwrite_dll;

c_IDWriteFactory* dwrite_factory = NULL;

static int(WINAPI* fn_GetUserDefaultLocaleName)(WCHAR*, int) = NULL;

int dwrite_init(void) {
    HMODULE dll_kernel32;
    HRESULT(WINAPI * fn_DWriteCreateFactory)(int, REFIID, void**);
    HRESULT hr;

    dwrite_dll = wd_load_system_dll(_T("DWRITE.DLL"));
    if (dwrite_dll == NULL) {
        WD_TRACE_ERR("dwrite_init: LoadLibrary('DWRITE.DLL') failed.");
        goto err_LoadLibrary;
    }

    fn_DWriteCreateFactory = (HRESULT(WINAPI*)(int, REFIID, void**))GetProcAddress(dwrite_dll, "DWriteCreateFactory");
    if (fn_DWriteCreateFactory == NULL) {
        WD_TRACE_ERR(
            "dwrite_init: "
            "GetProcAddress('DWriteCreateFactory') failed.");
        goto err_GetProcAddress;
    }

    hr = fn_DWriteCreateFactory(c_DWRITE_FACTORY_TYPE_SHARED, &c_IID_IDWriteFactory, (void**)&dwrite_factory);
    if (FAILED(hr)) {
        WD_TRACE_HR("dwrite_init: DWriteCreateFactory() failed.");
        goto err_DWriteCreateFactory;
    }

    /* We need locale name for creation of c_IDWriteTextFormat. This
     * functions is available since Vista (which covers all systems with
     * Direct2D and DirectWrite). */
    dll_kernel32 = GetModuleHandle(_T("KERNEL32.DLL"));
    if (dll_kernel32 != NULL) {
        fn_GetUserDefaultLocaleName =
            (int(WINAPI*)(WCHAR*, int))GetProcAddress(dll_kernel32, "GetUserDefaultLocaleName");
    }

    /* Success. */
    return 0;

    /* Error path unwinding. */
err_DWriteCreateFactory:
err_GetProcAddress:
    FreeLibrary(dwrite_dll);
err_LoadLibrary:
    return -1;
}

void dwrite_fini(void) {
    c_IDWriteFactory_Release(dwrite_factory);
    FreeLibrary(dwrite_dll);
    dwrite_factory = NULL;
}

void dwrite_default_user_locale(WCHAR buffer[LOCALE_NAME_MAX_LENGTH]) {
    if (fn_GetUserDefaultLocaleName != NULL) {
        if (fn_GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH) > 0)
            return;
        WD_TRACE_ERR(
            "dwrite_default_user_locale: "
            "GetUserDefaultLocaleName() failed.");
    } else {
        WD_TRACE_ERR(
            "dwrite_default_user_locale: "
            "function GetUserDefaultLocaleName() not available.");
    }

    buffer[0] = L'\0';
}

c_IDWriteTextFormat* dwrite_create_text_format(const WCHAR* locale_name, const LOGFONTW* logfont,
                                               c_DWRITE_FONT_METRICS* metrics) {
    /* See
     * https://github.com/Microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/multimedia/DirectWrite/RenderTest/TextHelpers.cpp
     */

    c_IDWriteTextFormat* tf = NULL;
    c_IDWriteGdiInterop* gdi_interop;
    c_IDWriteFont* font;
    c_IDWriteFontFamily* family;
    c_IDWriteLocalizedStrings* family_names;
    UINT32 family_name_buffer_size;
    WCHAR* family_name_buffer;
    float font_size;
    HRESULT hr;

    hr = c_IDWriteFactory_GetGdiInterop(dwrite_factory, &gdi_interop);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "dwrite_create_text_format: "
            "IDWriteFactory::GetGdiInterop() failed.");
        goto err_IDWriteFactory_GetGdiInterop;
    }

    hr = c_IDWriteGdiInterop_CreateFontFromLOGFONT(gdi_interop, logfont, &font);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "dwrite_create_text_format: "
            "IDWriteGdiInterop::CreateFontFromLOGFONT() failed.");
        goto err_IDWriteGdiInterop_CreateFontFromLOGFONT;
    }

    c_IDWriteFont_GetMetrics(font, metrics);

    hr = c_IDWriteFont_GetFontFamily(font, &family);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "dwrite_create_text_format: "
            "IDWriteFont::GetFontFamily() failed.");
        goto err_IDWriteFont_GetFontFamily;
    }

    hr = c_IDWriteFontFamily_GetFamilyNames(family, &family_names);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "dwrite_create_text_format: "
            "IDWriteFontFamily::GetFamilyNames() failed.");
        goto err_IDWriteFontFamily_GetFamilyNames;
    }

    hr = c_IDWriteLocalizedStrings_GetStringLength(family_names, 0, &family_name_buffer_size);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "dwrite_create_text_format: "
            "IDWriteLocalizedStrings::GetStringLength() failed.");
        goto err_IDWriteLocalizedStrings_GetStringLength;
    }

    family_name_buffer = (WCHAR*)_malloca(sizeof(WCHAR) * (family_name_buffer_size + 1));
    if (family_name_buffer == NULL) {
        WD_TRACE("dwrite_create_text_format: _malloca() failed.");
        goto err_malloca;
    }

    hr = c_IDWriteLocalizedStrings_GetString(family_names, 0, family_name_buffer, family_name_buffer_size + 1);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "dwrite_create_text_format: "
            "IDWriteLocalizedStrings::GetString() failed.");
        goto err_IDWriteLocalizedStrings_GetString;
    }

    if (logfont->lfHeight < 0) {
        font_size = (float)-logfont->lfHeight;
    } else if (logfont->lfHeight > 0) {
        font_size =
            (float)logfont->lfHeight * (float)metrics->designUnitsPerEm / (float)(metrics->ascent + metrics->descent);
    } else {
        font_size = 12.0f;
    }

    hr = c_IDWriteFactory_CreateTextFormat(dwrite_factory, family_name_buffer, NULL, c_IDWriteFont_GetWeight(font),
                                           c_IDWriteFont_GetStyle(font), c_IDWriteFont_GetStretch(font), font_size,
                                           locale_name, &tf);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "dwrite_create_text_format: "
            "IDWriteFactory::CreateTextFormat() failed.");
        goto err_IDWriteFactory_CreateTextFormat;
    }

err_IDWriteFactory_CreateTextFormat:
err_IDWriteLocalizedStrings_GetString:
    _freea(family_name_buffer);
err_malloca:
err_IDWriteLocalizedStrings_GetStringLength:
    c_IDWriteLocalizedStrings_Release(family_names);
err_IDWriteFontFamily_GetFamilyNames:
    c_IDWriteFontFamily_Release(family);
err_IDWriteFont_GetFontFamily:
    c_IDWriteFont_Release(font);
err_IDWriteGdiInterop_CreateFontFromLOGFONT:
    c_IDWriteGdiInterop_Release(gdi_interop);
err_IDWriteFactory_GetGdiInterop:
    return tf;
}

c_IDWriteTextLayout* dwrite_create_text_layout(c_IDWriteTextFormat* tf, const WD_RECT* rect, const WCHAR* str, int len,
                                               DWORD flags) {
    c_IDWriteTextLayout* layout;
    HRESULT hr;
    int tla;

    if (len < 0)
        len = wcslen(str);

    hr = c_IDWriteFactory_CreateTextLayout(dwrite_factory, str, len, tf, rect->x1 - rect->x0, rect->y1 - rect->y0,
                                           &layout);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "dwrite_create_text_layout: "
            "IDWriteFactory::CreateTextLayout() failed.");
        return NULL;
    }

    if (flags & WD_STR_RIGHTALIGN)
        tla = c_DWRITE_TEXT_ALIGNMENT_TRAILING;
    else if (flags & WD_STR_CENTERALIGN)
        tla = c_DWRITE_TEXT_ALIGNMENT_CENTER;
    else
        tla = c_DWRITE_TEXT_ALIGNMENT_LEADING;
    c_IDWriteTextLayout_SetTextAlignment(layout, tla);

    if (flags & WD_STR_BOTTOMALIGN)
        tla = c_DWRITE_PARAGRAPH_ALIGNMENT_FAR;
    else if (flags & WD_STR_MIDDLEALIGN)
        tla = c_DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    else
        tla = c_DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    c_IDWriteTextLayout_SetParagraphAlignment(layout, tla);

    if (flags & WD_STR_NOWRAP)
        c_IDWriteTextLayout_SetWordWrapping(layout, c_DWRITE_WORD_WRAPPING_NO_WRAP);

    if ((flags & WD_STR_ELLIPSISMASK) != 0) {
        static const c_DWRITE_TRIMMING trim_end = {c_DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        static const c_DWRITE_TRIMMING trim_word = {c_DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0};
        static const c_DWRITE_TRIMMING trim_path = {c_DWRITE_TRIMMING_GRANULARITY_WORD, L'\\', 1};

        const c_DWRITE_TRIMMING* trim_options = NULL;
        c_IDWriteInlineObject* trim_sign;

        hr = c_IDWriteFactory_CreateEllipsisTrimmingSign(dwrite_factory, tf, &trim_sign);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "dwrite_create_text_layout: "
                "IDWriteFactory::CreateEllipsisTrimmingSign() failed.");
            goto err_CreateEllipsisTrimmingSign;
        }

        switch (flags & WD_STR_ELLIPSISMASK) {
            case WD_STR_ENDELLIPSIS:
                trim_options = &trim_end;
                break;
            case WD_STR_WORDELLIPSIS:
                trim_options = &trim_word;
                break;
            case WD_STR_PATHELLIPSIS:
                trim_options = &trim_path;
                break;
        }

        if (trim_options != NULL)
            c_IDWriteTextLayout_SetTrimming(layout, trim_options, trim_sign);

        c_IDWriteInlineObject_Release(trim_sign);
    }

err_CreateEllipsisTrimmingSign:
    return layout;
}

/* ----------------------------- backend-gdix.h ------------------------------------  */


typedef struct gdix_strokestyle_tag gdix_strokestyle_t;
struct gdix_strokestyle_tag {
    c_GpLineCap lineCap;
    c_GpLineJoin lineJoin;
    c_GpDashStyle dashStyle;
    UINT dashesCount;
    float dashes[1];
};

typedef struct gdix_canvas_tag gdix_canvas_t;
struct gdix_canvas_tag {
    HDC dc;
    c_GpGraphics* graphics;
    c_GpPen* pen;
    c_GpStringFormat* string_format;
    int dc_layout;
    UINT width : 31;
    UINT rtl : 1;

    HDC real_dc; /* non-NULL if double buffering is enabled. */
    HBITMAP orig_bmp;
    int x;
    int y;
    int cx;
    int cy;
};

typedef struct gdix_vtable_tag gdix_vtable_t;
struct gdix_vtable_tag {
    /* Graphics functions */
    int(WINAPI* fn_CreateFromHDC)(HDC, c_GpGraphics**);
    int(WINAPI* fn_DeleteGraphics)(c_GpGraphics*);
    int(WINAPI* fn_GraphicsClear)(c_GpGraphics*, c_ARGB);
    int(WINAPI* fn_GetDC)(c_GpGraphics*, HDC*);
    int(WINAPI* fn_ReleaseDC)(c_GpGraphics*, HDC);
    int(WINAPI* fn_ResetClip)(c_GpGraphics*);
    int(WINAPI* fn_ResetWorldTransform)(c_GpGraphics*);
    int(WINAPI* fn_RotateWorldTransform)(c_GpGraphics*, float, c_GpMatrixOrder);
    int(WINAPI* fn_ScaleWorldTransform)(c_GpGraphics*, float, float, c_GpMatrixOrder);
    int(WINAPI* fn_SetClipPath)(c_GpGraphics*, c_GpPath*, c_GpCombineMode);
    int(WINAPI* fn_SetClipRect)(c_GpGraphics*, float, float, float, float, c_GpCombineMode);
    int(WINAPI* fn_SetPageUnit)(c_GpGraphics*, c_GpUnit);
    int(WINAPI* fn_SetPixelOffsetMode)(c_GpGraphics*, c_GpPixelOffsetMode);
    int(WINAPI* fn_SetSmoothingMode)(c_GpGraphics*, c_GpSmoothingMode);
    int(WINAPI* fn_TranslateWorldTransform)(c_GpGraphics*, float, float, c_GpMatrixOrder);
    int(WINAPI* fn_MultiplyWorldTransform)(c_GpGraphics*, c_GpMatrix*, c_GpMatrixOrder);
    int(WINAPI* fn_CreateMatrix2)(float, float, float, float, float, float, c_GpMatrix**);
    int(WINAPI* fn_DeleteMatrix)(c_GpMatrix*);

    /* Brush functions */
    int(WINAPI* fn_CreateSolidFill)(c_ARGB, c_GpSolidFill**);
    int(WINAPI* fn_DeleteBrush)(c_GpBrush*);
    int(WINAPI* fn_SetSolidFillColor)(c_GpSolidFill*, c_ARGB);
    int(WINAPI* fn_CreateLineBrush)(const c_GpPointF*, const c_GpPointF*, c_ARGB, c_ARGB, c_GpWrapMode,
                                    c_GpLineGradient**);
    int(WINAPI* fn_CreatePathGradientFromPath)(const c_GpPath*, c_GpPathGradient**);
    int(WINAPI* fn_SetLinePresetBlend)(c_GpLineGradient*, const c_ARGB*, const float*, INT);
    int(WINAPI* fn_SetPathGradientPresetBlend)(c_GpPathGradient*, const c_ARGB*, const float*, INT);
    int(WINAPI* fn_SetPathGradientCenterPoint)(c_GpPathGradient*, const c_GpPointF*);

    /* Pen functions */
    int(WINAPI* fn_CreatePen1)(c_ARGB, float, c_GpUnit, c_GpPen**);
    int(WINAPI* fn_DeletePen)(c_GpPen*);
    int(WINAPI* fn_SetPenBrushFill)(c_GpPen*, c_GpBrush*);
    int(WINAPI* fn_SetPenWidth)(c_GpPen*, float);
    int(WINAPI* fn_SetPenStartCap)(c_GpPen*, c_GpLineCap);
    int(WINAPI* fn_SetPenEndCap)(c_GpPen*, c_GpLineCap);
    int(WINAPI* fn_SetPenLineJoin)(c_GpPen*, c_GpLineJoin);
    int(WINAPI* fn_SetPenMiterLimit)(c_GpPen*, float);
    int(WINAPI* fn_SetPenDashStyle)(c_GpPen*, c_GpDashStyle);
    int(WINAPI* fn_SetPenDashArray)(c_GpPen*, const float*, INT);

    /* Path functions */
    int(WINAPI* fn_CreatePath)(c_GpFillMode, c_GpPath**);
    int(WINAPI* fn_DeletePath)(c_GpPath*);
    int(WINAPI* fn_ClosePathFigure)(c_GpPath*);
    int(WINAPI* fn_StartPathFigure)(c_GpPath*);
    int(WINAPI* fn_GetPathLastPoint)(c_GpPath*, c_GpPointF*);
    int(WINAPI* fn_AddPathArc)(c_GpPath*, float, float, float, float, float, float);
    int(WINAPI* fn_AddPathBezier)(c_GpPath*, float, float, float, float, float, float, float, float);
    int(WINAPI* fn_AddPathLine)(c_GpPath*, float, float, float, float);

    /* Font functions */
    int(WINAPI* fn_CreateFontFromLogfontW)(HDC, const LOGFONTW*, c_GpFont**);
    int(WINAPI* fn_DeleteFont)(c_GpFont*);
    int(WINAPI* fn_DeleteFontFamily)(c_GpFont*);
    int(WINAPI* fn_GetCellAscent)(const c_GpFont*, int, UINT16*);
    int(WINAPI* fn_GetCellDescent)(const c_GpFont*, int, UINT16*);
    int(WINAPI* fn_GetEmHeight)(const c_GpFont*, int, UINT16*);
    int(WINAPI* fn_GetFamily)(c_GpFont*, void**);
    int(WINAPI* fn_GetFontSize)(c_GpFont*, float*);
    int(WINAPI* fn_GetFontStyle)(c_GpFont*, int*);
    int(WINAPI* fn_GetLineSpacing)(const c_GpFont*, int, UINT16*);

    /* Image & bitmap functions */
    int(WINAPI* fn_LoadImageFromFile)(const WCHAR*, c_GpImage**);
    int(WINAPI* fn_LoadImageFromStream)(IStream*, c_GpImage**);
    int(WINAPI* fn_CreateBitmapFromHBITMAP)(HBITMAP, HPALETTE, c_GpBitmap**);
    int(WINAPI* fn_CreateBitmapFromHICON)(HICON, c_GpBitmap**);
    int(WINAPI* fn_DisposeImage)(c_GpImage*);
    int(WINAPI* fn_GetImageWidth)(c_GpImage*, UINT*);
    int(WINAPI* fn_GetImageHeight)(c_GpImage*, UINT*);
    int(WINAPI* fn_CreateBitmapFromScan0)(UINT, UINT, INT, c_GpPixelFormat, BYTE*, c_GpBitmap**);
    int(WINAPI* fn_BitmapLockBits)(c_GpBitmap*, const c_GpRectI*, UINT, c_GpPixelFormat, c_GpBitmapData*);
    int(WINAPI* fn_BitmapUnlockBits)(c_GpBitmap*, c_GpBitmapData*);
    int(WINAPI* fn_CreateBitmapFromGdiDib)(const BITMAPINFO*, void*, c_GpBitmap**);

    /* Cached bitmap functions */
    int(WINAPI* fn_CreateCachedBitmap)(c_GpBitmap*, c_GpGraphics*, c_GpCachedBitmap**);
    int(WINAPI* fn_DeleteCachedBitmap)(c_GpCachedBitmap*);
    int(WINAPI* fn_DrawCachedBitmap)(c_GpGraphics*, c_GpCachedBitmap*, INT, INT);

    /* String format functions */
    int(WINAPI* fn_CreateStringFormat)(int, LANGID, c_GpStringFormat**);
    int(WINAPI* fn_DeleteStringFormat)(c_GpStringFormat*);
    int(WINAPI* fn_SetStringFormatAlign)(c_GpStringFormat*, c_GpStringAlignment);
    int(WINAPI* fn_SetStringFormatLineAlign)(c_GpStringFormat*, c_GpStringAlignment);
    int(WINAPI* fn_SetStringFormatFlags)(c_GpStringFormat*, int);
    int(WINAPI* fn_SetStringFormatTrimming)(c_GpStringFormat*, c_GpStringTrimming);

    /* Draw/fill functions */
    int(WINAPI* fn_DrawArc)(c_GpGraphics*, c_GpPen*, float, float, float, float, float, float);
    int(WINAPI* fn_DrawImageRectRect)(c_GpGraphics*, c_GpImage*, float, float, float, float, float, float, float, float,
                                      c_GpUnit, const void*, void*, void*);
    int(WINAPI* fn_DrawEllipse)(c_GpGraphics*, c_GpPen*, float, float, float, float);
    int(WINAPI* fn_DrawLine)(c_GpGraphics*, c_GpPen*, float, float, float, float);
    int(WINAPI* fn_DrawBezier)(c_GpGraphics*, c_GpPen*, float, float, float, float, float, float, float, float);
    int(WINAPI* fn_DrawPath)(c_GpGraphics*, c_GpPen*, c_GpPath*);
    int(WINAPI* fn_DrawPie)(c_GpGraphics*, c_GpPen*, float, float, float, float, float, float);
    int(WINAPI* fn_DrawRectangle)(c_GpGraphics*, void*, float, float, float, float);
    int(WINAPI* fn_DrawString)(c_GpGraphics*, const WCHAR*, int, const c_GpFont*, const c_GpRectF*,
                               const c_GpStringFormat*, const c_GpBrush*);
    int(WINAPI* fn_FillEllipse)(c_GpGraphics*, c_GpBrush*, float, float, float, float);
    int(WINAPI* fn_FillPath)(c_GpGraphics*, c_GpBrush*, c_GpPath*);
    int(WINAPI* fn_FillPie)(c_GpGraphics*, c_GpBrush*, float, float, float, float, float, float);
    int(WINAPI* fn_FillRectangle)(c_GpGraphics*, void*, float, float, float, float);
    int(WINAPI* fn_MeasureString)(c_GpGraphics*, const WCHAR*, int, const c_GpFont*, const c_GpRectF*,
                                  const c_GpStringFormat*, c_GpRectF*, int*, int*);
};

extern gdix_vtable_t* gdix_vtable;

static inline BOOL gdix_enabled(void) {
    return (gdix_vtable != NULL);
}

int gdix_init(void);
void gdix_fini(void);

/* Helpers */
gdix_canvas_t* gdix_canvas_alloc(HDC dc, const RECT* doublebuffer_rect, UINT width, BOOL rtl);
void gdix_canvas_free(gdix_canvas_t* c);
void gdix_rtl_transform(gdix_canvas_t* c);
void gdix_reset_transform(gdix_canvas_t* c);
void gdix_delete_matrix(c_GpMatrix* m);
void gdix_canvas_apply_string_flags(gdix_canvas_t* c, DWORD flags);
void gdix_setpen(c_GpPen* pen, c_GpBrush* brush, float width, gdix_strokestyle_t* style);
c_GpBitmap* gdix_bitmap_from_HBITMAP_with_alpha(HBITMAP hBmp, BOOL has_premultiplied_alpha);

/* ------------------------------- backend-gdix.c --------------------------------------- */


#ifdef _MSC_VER
/* warning C4996: 'GetVersionExW': was declared deprecated */
#pragma warning(disable : 4996)
#endif

static HMODULE gdix_dll = NULL;

static ULONG_PTR gdix_token;
static void(WINAPI* gdix_Shutdown)(ULONG_PTR);

gdix_vtable_t* gdix_vtable = NULL;

int gdix_init(void) {
    int(WINAPI * gdix_Startup)(ULONG_PTR*, const c_GpStartupInput*, void*);
    c_GpStartupInput input = {0};
    int status;

    gdix_dll = wd_load_system_dll(_T("GDIPLUS.DLL"));
    if (gdix_dll == NULL) {
        /* On Windows 2000, we may need to use redistributable version of
         * GDIPLUS.DLL packaged with the application as GDI+.DLL is not part
         * of vanilla system. (However it MAY be present. Some later updates,
         * versions of MSIE or other software by Microsoft install it.) */
        OSVERSIONINFO version;
        version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&version);
        if (version.dwMajorVersion == 5 && version.dwMinorVersion == 0) {
            gdix_dll = LoadLibrary(_T("GDIPLUS.DLL"));
            if (gdix_dll == NULL) {
                WD_TRACE_ERR("gdix_init: LoadLibrary(GDIPLUS.DLL) failed");
                goto err_LoadLibrary;
            }
        } else {
            WD_TRACE_ERR("gdix_init: wd_load_system_dll(GDIPLUS.DLL) failed");
            goto err_LoadLibrary;
        }
    }

    gdix_vtable = (gdix_vtable_t*)malloc(sizeof(gdix_vtable_t));
    if (gdix_vtable == NULL) {
        WD_TRACE("gdix_init: malloc() failed.");
        goto err_malloc;
    }

    gdix_Startup = (int(WINAPI*)(ULONG_PTR*, const c_GpStartupInput*, void*))GetProcAddress(gdix_dll, "GdiplusStartup");
    if (gdix_Startup == NULL) {
        WD_TRACE_ERR("gdix_init: GetProcAddress(GdiplusStartup) failed");
        goto err_GetProcAddress;
    }

    gdix_Shutdown = (void(WINAPI*)(ULONG_PTR))GetProcAddress(gdix_dll, "GdiplusShutdown");
    if (gdix_Shutdown == NULL) {
        WD_TRACE_ERR("gdix_init: GetProcAddress(GdiplusShutdown) failed");
        goto err_GetProcAddress;
    }

#define GPA(name, params)                                                                     \
    do {                                                                                      \
        gdix_vtable->fn_##name = (int(WINAPI*) params)GetProcAddress(gdix_dll, "Gdip" #name); \
        if (gdix_vtable->fn_##name == NULL) {                                                 \
            WD_TRACE_ERR("gdix_init: GetProcAddress(Gdip" #name ") failed");                  \
            goto err_GetProcAddress;                                                          \
        }                                                                                     \
    } while (0)

    /* Graphics functions */
    GPA(CreateFromHDC, (HDC, c_GpGraphics**));
    GPA(DeleteGraphics, (c_GpGraphics*));
    GPA(GraphicsClear, (c_GpGraphics*, c_ARGB));
    GPA(GetDC, (c_GpGraphics*, HDC*));
    GPA(ReleaseDC, (c_GpGraphics*, HDC));
    GPA(ResetClip, (c_GpGraphics*));
    GPA(ResetWorldTransform, (c_GpGraphics*));
    GPA(RotateWorldTransform, (c_GpGraphics*, float, c_GpMatrixOrder));
    GPA(ScaleWorldTransform, (c_GpGraphics*, float, float, c_GpMatrixOrder));
    GPA(SetClipPath, (c_GpGraphics*, c_GpPath*, c_GpCombineMode));
    GPA(SetClipRect, (c_GpGraphics*, float, float, float, float, c_GpCombineMode));
    GPA(SetPageUnit, (c_GpGraphics*, c_GpUnit));
    GPA(SetPixelOffsetMode, (c_GpGraphics*, c_GpPixelOffsetMode));
    GPA(SetSmoothingMode, (c_GpGraphics*, c_GpSmoothingMode));
    GPA(TranslateWorldTransform, (c_GpGraphics*, float, float, c_GpMatrixOrder));
    GPA(MultiplyWorldTransform, (c_GpGraphics*, c_GpMatrix*, c_GpMatrixOrder));
    GPA(CreateMatrix2, (float, float, float, float, float, float, c_GpMatrix**));
    GPA(DeleteMatrix, (c_GpMatrix*));

    /* Brush functions */
    GPA(CreateSolidFill, (c_ARGB, c_GpSolidFill**));
    GPA(DeleteBrush, (c_GpBrush*));
    GPA(SetSolidFillColor, (c_GpSolidFill*, c_ARGB));
    GPA(CreateLineBrush, (const c_GpPointF*, const c_GpPointF*, c_ARGB, c_ARGB, c_GpWrapMode, c_GpLineGradient**));
    GPA(CreatePathGradientFromPath, (const c_GpPath*, c_GpPathGradient**));
    GPA(SetLinePresetBlend, (c_GpLineGradient*, const c_ARGB*, const float*, INT));
    GPA(SetPathGradientPresetBlend, (c_GpPathGradient*, const c_ARGB*, const float*, INT));
    GPA(SetPathGradientCenterPoint, (c_GpPathGradient*, const c_GpPointF*));

    /* Pen functions */
    GPA(CreatePen1, (DWORD, float, c_GpUnit, c_GpPen**));
    GPA(DeletePen, (c_GpPen*));
    GPA(SetPenBrushFill, (c_GpPen*, c_GpBrush*));
    GPA(SetPenWidth, (c_GpPen*, float));
    GPA(SetPenStartCap, (c_GpPen*, c_GpLineCap));
    GPA(SetPenEndCap, (c_GpPen*, c_GpLineCap));
    GPA(SetPenLineJoin, (c_GpPen*, c_GpLineJoin));
    GPA(SetPenMiterLimit, (c_GpPen*, float));
    GPA(SetPenDashStyle, (c_GpPen*, c_GpDashStyle));
    GPA(SetPenDashArray, (c_GpPen*, const float*, INT));

    /* Path functions */
    GPA(CreatePath, (c_GpFillMode, c_GpPath**));
    GPA(DeletePath, (c_GpPath*));
    GPA(ClosePathFigure, (c_GpPath*));
    GPA(StartPathFigure, (c_GpPath*));
    GPA(GetPathLastPoint, (c_GpPath*, c_GpPointF*));
    GPA(AddPathArc, (c_GpPath*, float, float, float, float, float, float));
    GPA(AddPathLine, (c_GpPath*, float, float, float, float));
    GPA(AddPathBezier, (c_GpPath*, float, float, float, float, float, float, float, float));

    /* Font functions */
    GPA(CreateFontFromLogfontW, (HDC, const LOGFONTW*, c_GpFont**));
    GPA(DeleteFont, (c_GpFont*));
    GPA(DeleteFontFamily, (c_GpFont*));
    GPA(GetCellAscent, (const c_GpFont*, int, UINT16*));
    GPA(GetCellDescent, (const c_GpFont*, int, UINT16*));
    GPA(GetEmHeight, (const c_GpFont*, int, UINT16*));
    GPA(GetFamily, (c_GpFont*, void**));
    GPA(GetFontSize, (c_GpFont*, float*));
    GPA(GetFontStyle, (c_GpFont*, int*));
    GPA(GetLineSpacing, (const c_GpFont*, int, UINT16*));

    /* Image & bitmap functions */
    GPA(LoadImageFromFile, (const WCHAR*, c_GpImage**));
    GPA(LoadImageFromStream, (IStream*, c_GpImage**));
    GPA(CreateBitmapFromHBITMAP, (HBITMAP, HPALETTE, c_GpBitmap**));
    GPA(CreateBitmapFromHICON, (HICON, c_GpBitmap**));
    GPA(DisposeImage, (c_GpImage*));
    GPA(GetImageWidth, (c_GpImage*, UINT*));
    GPA(GetImageHeight, (c_GpImage*, UINT*));
    GPA(CreateBitmapFromScan0, (UINT, UINT, INT, c_GpPixelFormat format, BYTE*, c_GpBitmap**));
    GPA(BitmapLockBits, (c_GpBitmap*, const c_GpRectI*, UINT, c_GpPixelFormat, c_GpBitmapData*));
    GPA(BitmapUnlockBits, (c_GpBitmap*, c_GpBitmapData*));
    GPA(CreateBitmapFromGdiDib, (const BITMAPINFO*, void*, c_GpBitmap**));

    /* Cached bitmap functions */
    GPA(CreateCachedBitmap, (c_GpBitmap*, c_GpGraphics*, c_GpCachedBitmap**));
    GPA(DeleteCachedBitmap, (c_GpCachedBitmap*));
    GPA(DrawCachedBitmap, (c_GpGraphics*, c_GpCachedBitmap*, INT, INT));

    /* String format functions */
    GPA(CreateStringFormat, (int, LANGID, c_GpStringFormat**));
    GPA(DeleteStringFormat, (c_GpStringFormat*));
    GPA(SetStringFormatAlign, (c_GpStringFormat*, c_GpStringAlignment));
    GPA(SetStringFormatLineAlign, (c_GpStringFormat*, c_GpStringAlignment));
    GPA(SetStringFormatFlags, (c_GpStringFormat*, int));
    GPA(SetStringFormatTrimming, (c_GpStringFormat*, c_GpStringTrimming));

    /* Draw/fill functions */
    GPA(DrawArc, (c_GpGraphics*, c_GpPen*, float, float, float, float, float, float));
    GPA(DrawImageRectRect, (c_GpGraphics*, c_GpImage*, float, float, float, float, float, float, float, float, c_GpUnit,
                            const void*, void*, void*));
    GPA(DrawEllipse, (c_GpGraphics*, c_GpPen*, float, float, float, float));
    GPA(DrawLine, (c_GpGraphics*, c_GpPen*, float, float, float, float));
    GPA(DrawPath, (c_GpGraphics*, c_GpPen*, c_GpPath*));
    GPA(DrawPie, (c_GpGraphics*, c_GpPen*, float, float, float, float, float, float));
    GPA(DrawRectangle, (c_GpGraphics*, void*, float, float, float, float));
    GPA(DrawString, (c_GpGraphics*, const WCHAR*, int, const c_GpFont*, const c_GpRectF*, const c_GpStringFormat*,
                     const c_GpBrush*));
    GPA(FillEllipse, (c_GpGraphics*, c_GpBrush*, float, float, float, float));
    GPA(FillPath, (c_GpGraphics*, c_GpBrush*, c_GpPath*));
    GPA(FillPie, (c_GpGraphics*, c_GpBrush*, float, float, float, float, float, float));
    GPA(FillRectangle, (c_GpGraphics*, void*, float, float, float, float));
    GPA(MeasureString, (c_GpGraphics*, const WCHAR*, int, const c_GpFont*, const c_GpRectF*, const c_GpStringFormat*,
                        c_GpRectF*, int*, int*));

#undef GPA

    input.GdiplusVersion = 1;
    input.SuppressExternalCodecs = TRUE;
    status = gdix_Startup(&gdix_token, &input, NULL);
    if (status != 0) {
        WD_TRACE("GdiplusStartup() failed. [%d]", status);
        goto err_Startup;
    }

    /* Success */
    return 0;

    /* Error path */
err_Startup:
err_GetProcAddress:
    free(gdix_vtable);
    gdix_vtable = NULL;
err_malloc:
    FreeLibrary(gdix_dll);
    gdix_dll = NULL;
err_LoadLibrary:
    return -1;
}

void gdix_fini(void) {
    free(gdix_vtable);
    gdix_vtable = NULL;

    gdix_Shutdown(gdix_token);

    FreeLibrary(gdix_dll);
    gdix_dll = NULL;
}

gdix_canvas_t* gdix_canvas_alloc(HDC dc, const RECT* doublebuffer_rect, UINT width, BOOL rtl) {
    gdix_canvas_t* c;
    int status;

    c = (gdix_canvas_t*)malloc(sizeof(gdix_canvas_t));
    if (c == NULL) {
        WD_TRACE("gdix_canvas_alloc: malloc() failed.");
        goto err_malloc;
    }

    memset(c, 0, sizeof(gdix_canvas_t));
    c->width = width;
    c->rtl = (rtl ? TRUE : FALSE);

    if (doublebuffer_rect != NULL) {
        int cx = doublebuffer_rect->right - doublebuffer_rect->left;
        int cy = doublebuffer_rect->bottom - doublebuffer_rect->top;
        HDC mem_dc;
        HBITMAP mem_bmp;

        mem_dc = CreateCompatibleDC(dc);
        if (mem_dc == NULL) {
            WD_TRACE_ERR("gdix_canvas_alloc: CreateCompatibleDC() failed.");
            DeleteDC(mem_dc);
            goto no_doublebuffer;
        }
        SetLayout(mem_dc, 0);
        mem_bmp = CreateCompatibleBitmap(dc, cx, cy);
        if (mem_bmp == NULL) {
            DeleteObject(mem_dc);
            WD_TRACE_ERR("gdix_canvas_alloc: CreateCompatibleBitmap() failed.");
            goto no_doublebuffer;
        }

        c->dc = mem_dc;
        c->real_dc = dc;
        c->orig_bmp = SelectObject(mem_dc, mem_bmp);
        c->x = (GetLayout(dc) & LAYOUT_RTL) ? width - 1 - doublebuffer_rect->right : doublebuffer_rect->left;
        c->y = doublebuffer_rect->top;
        c->cx = doublebuffer_rect->right - doublebuffer_rect->left;
        c->cy = doublebuffer_rect->bottom - doublebuffer_rect->top;
        SetViewportOrgEx(mem_dc, -c->x, -c->y, NULL);
    } else {
    no_doublebuffer:
        c->dc = dc;
    }

    /* Different GDIPLUS.DLL versions treat RTL very differently.
     *
     * E.g. on Win 2000 and XP, it installs a reflection transformation is
     * that origin is at right top corner of the window, but this reflection
     * applies also on text (wdDrawString()).
     *
     * Windows 7 or 10 seem to ignore the RTL layout altogether.
     *
     * Hence we enforce left-to-right layout to get consistent behavior from
     * GDIPLUS.DLL and implement RTL manually on top of it.
     */
    c->dc_layout = SetLayout(dc, 0);

    status = gdix_vtable->fn_CreateFromHDC(c->dc, &c->graphics);
    if (status != 0) {
        WD_TRACE_ERR_("gdix_canvas_alloc: GdipCreateFromHDC() failed.", status);
        goto err_creategraphics;
    }

    status = gdix_vtable->fn_SetPageUnit(c->graphics, c_UnitPixel);
    if (status != 0) {
        WD_TRACE_ERR_("gdix_canvas_alloc: GdipSetPageUnit() failed.", status);
        goto err_setpageunit;
    }

    status = gdix_vtable->fn_SetSmoothingMode(c->graphics, /* GDI+ 1.1 */
                                              c_SmoothingModeAntiAlias8x8);
    if (status != 0) {
        gdix_vtable->fn_SetSmoothingMode(c->graphics, /* GDI+ 1.0 */
                                         c_SmoothingModeHighQuality);
    }

    /* GDI+ has, unlike D2D, a concept of pens, which are used for "draw"
     * operations, while brushes are used for "fill" operations.
     *
     * Our interface works only with brushes as D2D does. Hence we create
     * a pen as part of GDI+ canvas and we update it with GdipSetPenBrushFill()
     * and GdipSetPenWidth() every time whenever we need to use a pen. */
    status = gdix_vtable->fn_CreatePen1(0, 1.0f, c_UnitPixel, &c->pen);
    if (status != 0) {
        WD_TRACE_ERR_("gdix_canvas_alloc: GdipCreatePen1() failed.", status);
        goto err_createpen;
    }

    /* Needed for wdDrawString() and wdMeasureString() */
    status = gdix_vtable->fn_CreateStringFormat(0, LANG_NEUTRAL, &c->string_format);
    if (status != 0) {
        WD_TRACE(
            "gdix_canvas_alloc: "
            "GdipCreateStringFormat() failed. [%d]",
            status);
        goto err_createstringformat;
    }

    gdix_reset_transform(c);
    return c;

    /* Error path */
err_createstringformat:
    gdix_vtable->fn_DeletePen(c->pen);
err_createpen:
err_setpageunit:
    gdix_vtable->fn_DeleteGraphics(c->graphics);
err_creategraphics:
    if (c->real_dc != NULL) {
        HBITMAP mem_bmp = SelectObject(c->dc, c->orig_bmp);
        DeleteObject(mem_bmp);
        DeleteDC(c->dc);
    }
    SetLayout(dc, c->dc_layout);
    free(c);
err_malloc:
    return NULL;
}

void gdix_canvas_free(gdix_canvas_t* c) {
    gdix_vtable->fn_DeleteStringFormat(c->string_format);
    gdix_vtable->fn_DeletePen(c->pen);
    gdix_vtable->fn_DeleteGraphics(c->graphics);

    if (c->real_dc != NULL) {
        HBITMAP mem_bmp;

        mem_bmp = SelectObject(c->dc, c->orig_bmp);
        DeleteObject(mem_bmp);
        DeleteObject(c->dc);
    }

    free(c);
}

void gdix_rtl_transform(gdix_canvas_t* c) {
    gdix_vtable->fn_ScaleWorldTransform(c->graphics, -1.0f, 1.0f, c_MatrixOrderAppend);
    gdix_vtable->fn_TranslateWorldTransform(c->graphics, (float)(c->width - 1), 0.0f, c_MatrixOrderAppend);
}

void gdix_reset_transform(gdix_canvas_t* c) {
    gdix_vtable->fn_ResetWorldTransform(c->graphics);
    if (c->rtl)
        gdix_rtl_transform(c);
}

void gdix_delete_matrix(c_GpMatrix* m) {
    int status = gdix_vtable->fn_DeleteMatrix(m);
    if (status != 0) {
        WD_TRACE_ERR_("wdSetWorldTransform: Could not delete matrix", status);
        return;
    }
}

void gdix_canvas_apply_string_flags(gdix_canvas_t* c, DWORD flags) {
    int sfa;
    int sff;
    int trim;

    if (flags & WD_STR_RIGHTALIGN)
        sfa = c_StringAlignmentFar;
    else if (flags & WD_STR_CENTERALIGN)
        sfa = c_StringAlignmentCenter;
    else
        sfa = c_StringAlignmentNear;
    gdix_vtable->fn_SetStringFormatAlign(c->string_format, sfa);

    if (flags & WD_STR_BOTTOMALIGN)
        sfa = c_StringAlignmentFar;
    else if (flags & WD_STR_MIDDLEALIGN)
        sfa = c_StringAlignmentCenter;
    else
        sfa = c_StringAlignmentNear;
    gdix_vtable->fn_SetStringFormatLineAlign(c->string_format, sfa);

    sff = 0;
    if (c->rtl)
        sff |= c_StringFormatFlagsDirectionRightToLeft;
    if (flags & WD_STR_NOWRAP)
        sff |= c_StringFormatFlagsNoWrap;
    if (flags & WD_STR_NOCLIP)
        sff |= c_StringFormatFlagsNoClip;
    gdix_vtable->fn_SetStringFormatFlags(c->string_format, sff);

    switch (flags & WD_STR_ELLIPSISMASK) {
        case WD_STR_ENDELLIPSIS:
            trim = c_StringTrimmingEllipsisCharacter;
            break;
        case WD_STR_WORDELLIPSIS:
            trim = c_StringTrimmingEllipsisWord;
            break;
        case WD_STR_PATHELLIPSIS:
            trim = c_StringTrimmingEllipsisPath;
            break;
        default:
            trim = c_StringTrimmingNone;
            break;
    }
    gdix_vtable->fn_SetStringFormatTrimming(c->string_format, trim);
}

void gdix_setpen(c_GpPen* pen, c_GpBrush* brush, float width, gdix_strokestyle_t* style) {
    if (style) {
        if (style->dashesCount > 0) {
            gdix_vtable->fn_SetPenDashArray(pen, style->dashes, style->dashesCount);
        }

        gdix_vtable->fn_SetPenDashStyle(pen, style->dashStyle);
        gdix_vtable->fn_SetPenStartCap(pen, style->lineCap);
        gdix_vtable->fn_SetPenEndCap(pen, style->lineCap);
        gdix_vtable->fn_SetPenLineJoin(pen, style->lineJoin);
    }

    gdix_vtable->fn_SetPenBrushFill(pen, brush);
    gdix_vtable->fn_SetPenWidth(pen, width);
}

c_GpBitmap* gdix_bitmap_from_HBITMAP_with_alpha(HBITMAP bmp, BOOL has_premultiplied_alpha) {
    c_GpBitmap* b;
    BITMAP bmp_desc;
    UINT stride;
    BYTE bmp_info_buffer[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 3];
    BITMAPINFO* bmp_info = (BITMAPINFO*)&bmp_info_buffer;
    HDC dc;
    BYTE* bits;
    int pixel_format;

    GetObject(bmp, sizeof(BITMAP), &bmp_desc);

    if (bmp_desc.bmBitsPixel != 32) {
        WD_TRACE("gdix_bitmap_from_HBITMAP_with_alpha: Unsupported pixel format.");
        return NULL;
    }

    stride = ((bmp_desc.bmWidth * bmp_desc.bmBitsPixel + 31) / 32) * 4;

    memset(bmp_info, 0, sizeof(BITMAPINFOHEADER));
    bmp_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    dc = GetDC(NULL);
    GetDIBits(dc, bmp, 0, (UINT)bmp_desc.bmHeight, NULL, bmp_info, DIB_RGB_COLORS);
    bits = (BYTE*)malloc(bmp_info->bmiHeader.biSizeImage);
    if (bits == NULL) {
        ReleaseDC(NULL, dc);
        return NULL;
    }
    GetDIBits(dc, bmp, 0, (UINT)bmp_desc.bmHeight, bits, bmp_info, DIB_RGB_COLORS);
    ReleaseDC(NULL, dc);

    if (has_premultiplied_alpha)
        pixel_format = WD_PIXELFORMAT_B8G8R8A8_PREMULTIPLIED;
    else
        pixel_format = WD_PIXELFORMAT_B8G8R8A8;

    b = (c_GpBitmap*)wdCreateImageFromBuffer(bmp_desc.bmWidth, bmp_desc.bmHeight, stride, bits, pixel_format, NULL, 0);

    free(bits);
    return b;
}

/* -------------------------------------- backend-wic.h --------------------------------- */


extern IWICImagingFactory* wic_factory;

extern const GUID wic_pixel_format;

int wic_init(void);
void wic_fini(void);

IWICBitmapSource* wic_convert_bitmap(IWICBitmapSource* bitmap);

/* -------------------------------------- backend-wic.c --------------------------------- */


static HMODULE wic_dll = NULL;

IWICImagingFactory* wic_factory = NULL;

/* According to MSDN, GUID_WICPixelFormat32bppPBGRA is the recommended pixel
 * format for cooperation with Direct2D. Note we define it here manually to
 * avoid need to link with UUID.LIB. */
const GUID wic_pixel_format = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10}};

int wic_init(void) {
#ifdef WINCODEC_SDK_VERSION1
    /* This is only available in newer SDK headers. */
    static const UINT wic_version = WINCODEC_SDK_VERSION1;
#else
    static const UINT wic_version = WINCODEC_SDK_VERSION;
#endif

    HRESULT(WINAPI * fn_WICCreateImagingFactory_Proxy)(UINT, IWICImagingFactory**);
    HRESULT hr;

    wic_dll = wd_load_system_dll(_T("WINDOWSCODECS.DLL"));
    if (wic_dll == NULL) {
        WD_TRACE_ERR("wic_init: wd_load_system_dll(WINDOWSCODECS.DLL) failed.");
        goto err_LoadLibrary;
    }

    fn_WICCreateImagingFactory_Proxy =
        (HRESULT(WINAPI*)(UINT, IWICImagingFactory**))GetProcAddress(wic_dll, "WICCreateImagingFactory_Proxy");
    if (fn_WICCreateImagingFactory_Proxy == NULL) {
        WD_TRACE_ERR("wic_init: GetProcAddress(WICCreateImagingFactory_Proxy) failed.");
        goto err_GetProcAddress;
    }

    hr = fn_WICCreateImagingFactory_Proxy(wic_version, &wic_factory);
    if (FAILED(hr)) {
        WD_TRACE_HR("wic_init: WICCreateImagingFactory_Proxy() failed.");
        goto err_WICCreateImagingFactory_Proxy;
    }

    /* Success. */
    return 0;

    /* Error path unwinding. */
err_WICCreateImagingFactory_Proxy:
err_GetProcAddress:
    FreeLibrary(wic_dll);
    wic_dll = NULL;
err_LoadLibrary:
    return -1;
}

void wic_fini(void) {
    IWICImagingFactory_Release(wic_factory);
    wic_factory = NULL;

    FreeLibrary(wic_dll);
    wic_dll = NULL;
}

IWICBitmapSource* wic_convert_bitmap(IWICBitmapSource* bitmap) {
    GUID pixel_format;
    IWICFormatConverter* converter;
    HRESULT hr;

    hr = IWICBitmapSource_GetPixelFormat(bitmap, &pixel_format);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "wc_convert_bitmap: "
            "IWICBitmapSource::GetPixelFormat() failed.");
        return NULL;
    }

    if (IsEqualGUID(&pixel_format, &wic_pixel_format)) {
        /* No conversion needed. */
        IWICBitmapSource_AddRef(bitmap);
        return bitmap;
    }

    hr = IWICImagingFactory_CreateFormatConverter(wic_factory, &converter);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "wc_convert_bitmap: "
            "IWICImagingFactory::CreateFormatConverter() failed.");
        return NULL;
    }

    hr = IWICFormatConverter_Initialize(converter, bitmap, &wic_pixel_format, WICBitmapDitherTypeNone, NULL, 0.0f,
                                        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "wc_convert_bitmap: "
            "IWICFormatConverter::Initialize() failed.");
        IWICFormatConverter_Release(converter);
        return NULL;
    }

    return (IWICBitmapSource*)converter;
}

/* --------------------------------------- init.c ---------------------------------- */




static void wd_fini_core_api(void) {
    if (d2d_enabled())
        d2d_fini();
    else
        gdix_fini();
}

static int wd_init_image_api(void) {
    if (d2d_enabled()) {
        return wic_init();
    } else {
        /* noop */
        return 0;
    }
}

static void wd_fini_image_api(void) {
    if (d2d_enabled()) {
        wic_fini();
    } else {
        /* noop */
    }
}

static int wd_init_string_api(void) {
    if (d2d_enabled()) {
        return dwrite_init();
    } else {
        /* noop */
        return 0;
    }
}

static void wd_fini_string_api(void) {
    if (d2d_enabled()) {
        dwrite_fini();
    } else {
        /* noop */
    }
}

static const struct {
    int (*fn_init)(void);
    void (*fn_fini)(void);
} wd_modules[] = {{wd_init_core_api, wd_fini_core_api},
                  {wd_init_image_api, wd_fini_image_api},
                  {wd_init_string_api, wd_fini_string_api}};

#define WD_MOD_COUNT (sizeof(wd_modules) / sizeof(wd_modules[0]))

#define WD_MOD_COREAPI 0
#define WD_MOD_IMAGEAPI 1
#define WD_MOD_STRINGAPI 2

static UINT wd_init_counter[WD_MOD_COUNT] = {0};

BOOL wdInitialize(DWORD dwFlags) {
    BOOL want_init[WD_MOD_COUNT];
    int i;

    want_init[WD_MOD_COREAPI] = TRUE;
    want_init[WD_MOD_IMAGEAPI] = (dwFlags & WD_INIT_IMAGEAPI);
    want_init[WD_MOD_STRINGAPI] = (dwFlags & WD_INIT_STRINGAPI);

    wd_lock();

    for (i = 0; i < WD_MOD_COUNT; i++) {
        if (!want_init[i])
            continue;

        wd_init_counter[i]++;
        if (wd_init_counter[i] > 0) {
            if (wd_modules[i].fn_init() != 0)
                goto fail;
        }
    }

    wd_unlock();
    return TRUE;

fail:
    /* Undo initializations from successful iterations. */
    while (--i >= 0) {
        if (want_init[i]) {
            wd_init_counter[i]--;
            if (wd_init_counter[i] == 0)
                wd_modules[i].fn_fini();
        }
    }

    wd_unlock();
    return FALSE;
}

void wdTerminate(DWORD dwFlags) {
    BOOL want_fini[WD_MOD_COUNT];
    int i;

    want_fini[WD_MOD_COREAPI] = TRUE;
    want_fini[WD_MOD_IMAGEAPI] = (dwFlags & WD_INIT_IMAGEAPI);
    want_fini[WD_MOD_STRINGAPI] = (dwFlags & WD_INIT_STRINGAPI);

    wd_lock();

    for (i = WD_MOD_COUNT - 1; i >= 0; i--) {
        if (!want_fini[i])
            continue;

        wd_init_counter[i]--;
        if (wd_init_counter[i] == 0)
            wd_modules[i].fn_fini();
    }

    /* If core module counter has dropped to zero, caller likely forgot to
     * terminate some optional module (i.e. mismatching flags for wdTerminate()
     * somewhere. So lets kill all those modules forcefully now anyway even
     * though well behaving applications should never do that...
     */
    if (wd_init_counter[WD_MOD_COREAPI] == 0) {
        for (i = WD_MOD_COUNT - 1; i >= 0; i--) {
            if (wd_init_counter[i] > 0) {
                WD_TRACE("wdTerminate: Forcefully terminating module %d.", i);
                wd_modules[i].fn_fini();
                wd_init_counter[i] = 0;
            }
        }
    }

    wd_unlock();
}

int wdBackend(void) {
    if (d2d_enabled()) {
        return WD_BACKEND_D2D;
    }

    if (gdix_enabled()) {
        return WD_BACKEND_GDIPLUS;
    }

    return -1;
}

/* ----------------------------------- string.c --------------------------------- */

void wdDrawString(WD_HCANVAS hCanvas, WD_HFONT hFont, const WD_RECT* pRect, const WCHAR* pszText, int iTextLength,
                  WD_HBRUSH hBrush, DWORD dwFlags) {
    if (d2d_enabled()) {
        dwrite_font_t* font = (dwrite_font_t*)hFont;
        c_D2D1_POINT_2F origin = {pRect->x0, pRect->y0};
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_IDWriteTextLayout* layout;
        c_D2D1_MATRIX_3X2_F old_matrix;

        layout = dwrite_create_text_layout(font->tf, pRect, pszText, iTextLength, dwFlags);
        if (layout == NULL) {
            WD_TRACE("wdDrawString: dwrite_create_text_layout() failed.");
            return;
        }

        if (c->flags & D2D_CANVASFLAG_RTL) {
            d2d_disable_rtl_transform(c, &old_matrix);
            origin.x = (float)c->width - pRect->x1;

            c_IDWriteTextLayout_SetReadingDirection(layout, c_DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        }

        c_ID2D1RenderTarget_DrawTextLayout(c->target, origin, layout, b,
                                           (dwFlags & WD_STR_NOCLIP) ? 0 : c_D2D1_DRAW_TEXT_OPTIONS_CLIP);

        c_IDWriteTextLayout_Release(layout);

        if (c->flags & D2D_CANVASFLAG_RTL) {
            c_ID2D1RenderTarget_SetTransform(c->target, &old_matrix);
        }
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        c_GpRectF r;
        c_GpFont* f = (c_GpFont*)hFont;
        c_GpBrush* b = (c_GpBrush*)hBrush;

        if (c->rtl) {
            gdix_rtl_transform(c);
            r.x = (float)(c->width - 1) - pRect->x1;
        } else {
            r.x = pRect->x0;
        }
        r.y = pRect->y0;
        r.w = pRect->x1 - pRect->x0;
        r.h = pRect->y1 - pRect->y0;

        gdix_canvas_apply_string_flags(c, dwFlags);
        gdix_vtable->fn_DrawString(c->graphics, pszText, iTextLength, f, &r, c->string_format, b);

        if (c->rtl)
            gdix_rtl_transform(c);
    }
}

void wdMeasureString(WD_HCANVAS hCanvas, WD_HFONT hFont, const WD_RECT* pRect, const WCHAR* pszText, int iTextLength,
                     WD_RECT* pResult, DWORD dwFlags) {
    if (d2d_enabled()) {
        dwrite_font_t* font = (dwrite_font_t*)hFont;
        c_IDWriteTextLayout* layout;
        c_DWRITE_TEXT_METRICS tm;

        layout = dwrite_create_text_layout(font->tf, pRect, pszText, iTextLength, dwFlags);
        if (layout == NULL) {
            WD_TRACE("wdMeasureString: dwrite_create_text_layout() failed.");
            return;
        }

        c_IDWriteTextLayout_GetMetrics(layout, &tm);

        pResult->x0 = pRect->x0 + tm.left;
        pResult->y0 = pRect->y0 + tm.top;
        pResult->x1 = pResult->x0 + tm.width;
        pResult->y1 = pResult->y0 + tm.height;

        c_IDWriteTextLayout_Release(layout);
    } else {
        HDC screen_dc = NULL;
        gdix_canvas_t* c;
        c_GpRectF r;
        c_GpFont* f = (c_GpFont*)hFont;
        c_GpRectF br;

        if (hCanvas != NULL) {
            c = (gdix_canvas_t*)hCanvas;
        } else {
            screen_dc = GetDCEx(NULL, NULL, DCX_CACHE);
            c = gdix_canvas_alloc(screen_dc, NULL, (UINT)(pRect->x1 - pRect->x0), FALSE);
            if (c == NULL) {
                WD_TRACE("wdMeasureString: gdix_canvas_alloc() failed.");
                pResult->x0 = 0.0f;
                pResult->y0 = 0.0f;
                pResult->x1 = 0.0f;
                pResult->y1 = 0.0f;
                return;
            }
        }

        if (c->rtl) {
            gdix_rtl_transform(c);
            r.x = (float)(c->width - 1) - pRect->x1;
        } else {
            r.x = pRect->x0;
        }
        r.y = pRect->y0;
        r.w = pRect->x1 - pRect->x0;
        r.h = pRect->y1 - pRect->y0;

        gdix_canvas_apply_string_flags(c, dwFlags);
        gdix_vtable->fn_MeasureString(c->graphics, pszText, iTextLength, f, &r, c->string_format, &br, NULL, NULL);

        if (c->rtl)
            gdix_rtl_transform(c);

        if (hCanvas == NULL) {
            gdix_canvas_free(c);
            ReleaseDC(NULL, screen_dc);
        }

        pResult->x0 = br.x;
        pResult->y0 = br.y;
        pResult->x1 = br.x + br.w;
        pResult->y1 = br.y + br.h;
    }
}

float wdStringWidth(WD_HCANVAS hCanvas, WD_HFONT hFont, const WCHAR* pszText) {
    const WD_RECT rcClip = {0.0f, 0.0f, 10000.0f, 10000.0f};
    WD_RECT rcResult;

    wdMeasureString(hCanvas, hFont, &rcClip, pszText, wcslen(pszText), &rcResult, WD_STR_LEFTALIGN | WD_STR_NOWRAP);
    return WD_ABS(rcResult.x1 - rcResult.x0);
}

float wdStringHeight(WD_HFONT hFont, const WCHAR* pszText) {
    WD_FONTMETRICS metrics;

    wdFontMetrics(hFont, &metrics);
    return metrics.fLeading;
}

/* ------------------------------------- misc.h ---------------------------------- */

#ifdef DEBUG

void wd_log(const char* fmt, ...) {
    DWORD last_error;
    va_list args;
    char buffer[512];
    int offset = 0;
    int n;

    last_error = GetLastError();

    offset += sprintf(buffer, "[%08x] ", (unsigned)GetCurrentThreadId());

    va_start(args, fmt);
    n = _vsnprintf(buffer + offset, sizeof(buffer) - offset - 2, fmt, args);
    va_end(args);
    if (n < 0 || n > (int)sizeof(buffer) - offset - 2)
        n = sizeof(buffer) - offset - 2;
    offset += n;
    buffer[offset++] = '\n';
    buffer[offset++] = '\0';
    OutputDebugStringA(buffer);
    SetLastError(last_error);
}

#endif /* DEBUG */

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#endif

HMODULE
wd_load_system_dll(const TCHAR* dll_name) {
    HMODULE dll_kernel32;
    HMODULE dll;

    /* Check whether flag LOAD_LIBRARY_SEARCH_SYSTEM32 is supported on this
     * system. It has been added in Win Vista/7 with the security update
     * KB2533623. The update also added new symbol AddDllDirectory() so we
     * use that as a canary. */
    dll_kernel32 = GetModuleHandle(_T("KERNEL32.DLL"));
    if (dll_kernel32 != NULL && GetProcAddress(dll_kernel32, "AddDllDirectory") != NULL) {
        dll = LoadLibraryEx(dll_name, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (dll == NULL) {
            WD_TRACE(
                "wd_load_system_library: "
                "LoadLibraryEx(%s, LOAD_LIBRARY_SEARCH_SYSTEM32) [%lu]",
                dll_name, GetLastError());
        }
    } else {
        TCHAR path[MAX_PATH];
        UINT dllname_len;
        UINT sysdir_len;

        dllname_len = _tcslen(dll_name);
        sysdir_len = GetSystemDirectory(path, MAX_PATH);
        if (sysdir_len + 1 + dllname_len >= MAX_PATH) {
            WD_TRACE("wd_load_system_library: Buffer too small.");
            SetLastError(ERROR_BUFFER_OVERFLOW);
            return NULL;
        }

        path[sysdir_len] = _T('\\');
        memcpy(path + sysdir_len + 1, dll_name, (dllname_len + 1) * sizeof(TCHAR));
        dll = LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (dll == NULL) {
            WD_TRACE(
                "wd_load_system_library: "
                "LoadLibraryEx(%s, LOAD_WITH_ALTERED_SEARCH_PATH) [%lu]",
                dll_name, GetLastError());
        }
    }

    return dll;
}

/* ---------------------------------- memstream.h ----------------------------- */

/* Trivial read-only IStream implementation.
 *
 * This is more lightweight alternative to SHCreateMemStream() from SHLWAPI.DLL.
 *
 * This implementation provides these main benefits:
 *   (1) We do not copy the data.
 *   (2) Application does not need SHLWAPI.DLL.
 *
 * (Note that caller is responsible the data in the provided buffer remain
 * valid and immutable for the life time of the IStream.)
 *
 * When not needed anymore, the caller should release the stream as a standard
 * COM object, i.e. via method IStream::Release().
 */

HRESULT memstream_create(const BYTE* buffer, ULONG size, IStream** p_stream);

HRESULT memstream_create_from_resource(HINSTANCE instance, const WCHAR* res_type, const WCHAR* res_name,
                                       IStream** p_stream);

/* ------------------------------------ memstream.c ------------------------------------- */

typedef struct MEMSTREAM_TAG MEMSTREAM;
struct MEMSTREAM_TAG {
    IStream stream; /* COM interface */
    LONG refs;

    const BYTE* buffer;
    ULONG pos;
    ULONG size;
};

#define OFFSETOF(type, member) ((size_t) & ((type*)0)->member)
#define CONTAINEROF(ptr, type, member) ((type*)((BYTE*)(ptr)-OFFSETOF(type, member)))

#define MEMSTREAM_FROM_IFACE(stream_iface) CONTAINEROF(stream_iface, MEMSTREAM, stream)

static HRESULT STDMETHODCALLTYPE memstream_QueryInterface(IStream* self, REFIID riid, void** obj) {
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch) ||
        IsEqualGUID(riid, &IID_ISequentialStream) || IsEqualGUID(riid, &IID_IStream)) {
        MEMSTREAM* s = MEMSTREAM_FROM_IFACE(self);
        InterlockedIncrement(&s->refs);
        *obj = s;
        return S_OK;
    } else {
        *obj = NULL;
        return E_NOINTERFACE;
    }
}

static ULONG STDMETHODCALLTYPE memstream_AddRef(IStream* self) {
    MEMSTREAM* s = MEMSTREAM_FROM_IFACE(self);
    return InterlockedIncrement(&s->refs);
}

static ULONG STDMETHODCALLTYPE memstream_Release(IStream* self) {
    MEMSTREAM* s = MEMSTREAM_FROM_IFACE(self);
    ULONG refs;

    refs = InterlockedDecrement(&s->refs);
    if (refs == 0)
        free(s);
    return refs;
}

static HRESULT STDMETHODCALLTYPE memstream_Read(IStream* self, void* buf, ULONG n, ULONG* n_read) {
    MEMSTREAM* s = MEMSTREAM_FROM_IFACE(self);

    /* Return S_FALSE, if we are already in the end-of-file situation. */
    if (s->pos >= s->size) {
        n = 0;
        if (n_read != NULL)
            *n_read = 0;
        return S_FALSE;
    }

    if (n > s->size - s->pos)
        n = s->size - s->pos;
    memcpy(buf, s->buffer + s->pos, n);
    s->pos += n;

    if (n_read != NULL)
        *n_read = n;

    /* From https://msdn.microsoft.com/en-us/library/windows/desktop/aa380037(v=vs.85).aspx:
     *
     * Reads a specified number of bytes from the stream object into memory
     * starting at the current seek pointer. This implementation returns S_OK
     * if the end of the stream was reached during the read. (This is the same
     * as the "end of file" behavior found in the MS-DOS FAT file system.)
     */
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE memstream_Write(IStream* self, const void* buf, ULONG n, ULONG* n_written) {
    /* We are read-only stream. */
    if (n_written != NULL)
        *n_written = 0;
    return STG_E_ACCESSDENIED;
}

static HRESULT STDMETHODCALLTYPE memstream_Seek(IStream* self, LARGE_INTEGER delta, DWORD origin,
                                                ULARGE_INTEGER* p_new_pos) {
    MEMSTREAM* s = MEMSTREAM_FROM_IFACE(self);
    LARGE_INTEGER pos;
    HRESULT hr;

    switch (origin) {
        case STREAM_SEEK_SET:
            pos.QuadPart = delta.QuadPart;
            break;
        case STREAM_SEEK_CUR:
            pos.QuadPart = s->pos + delta.QuadPart;
            break;
        case STREAM_SEEK_END:
            pos.QuadPart = s->size + delta.QuadPart;
            break;
        default:
            hr = STG_E_INVALIDPARAMETER;
            goto end;
    }

    /* Note MSDN states it is an error, if the result is negative, but it is
     * in principle OK, if we are beyond end-of-file. (In such case subsequent
     * Read() just shall return S_FALSE to indicate end-of-file situation.)
     */
    if (pos.QuadPart < 0) {
        hr = STG_E_INVALIDFUNCTION;
        goto end;
    }

    /* In 32-bit, there is a danger of overflow. */
    if (pos.QuadPart != (ULONG)pos.QuadPart) {
        hr = STG_E_INVALIDFUNCTION;
        goto end;
    }

    s->pos = (ULONG)pos.QuadPart;
    hr = S_OK;

end:
    if (p_new_pos != NULL)
        p_new_pos->QuadPart = s->pos;
    return hr;
}

static HRESULT STDMETHODCALLTYPE memstream_SetSize(IStream* self, ULARGE_INTEGER new_size) {
    /* We are read-only stream. */
    return STG_E_INVALIDFUNCTION;
}

static HRESULT STDMETHODCALLTYPE memstream_CopyTo(IStream* self, IStream* other, ULARGE_INTEGER n,
                                                  ULARGE_INTEGER* n_read, ULARGE_INTEGER* n_written) {
    MEMSTREAM* s = MEMSTREAM_FROM_IFACE(self);
    ULONG written;
    HRESULT hr;

    if (s->pos + n.QuadPart >= s->size)
        n.QuadPart = (s->pos < s->size ? s->size - s->pos : 0);

    hr = IStream_Write(other, s->buffer + s->pos, (ULONG)n.QuadPart, &written);

    /* In case of failure, MSDN states that the seek pointers are invalid
     * in source as well as destinations streams. So lets just abort. */
    if (FAILED(hr))
        return hr;

    s->pos += written;

    /* And in the case of success, MSDN specifies that *n_read and *n_written
     * are set to the same value. */
    if (n_read != NULL)
        n_read->QuadPart = written;
    if (n_written != NULL)
        n_written->QuadPart = written;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE memstream_Commit(IStream* self, DWORD flags) {
    /* Noop, since we never change contents of the stream. */
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE memstream_Revert(IStream* self) {
    /* Noop, since we never change contents of the stream. */
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE memstream_LockRegion(IStream* self, ULARGE_INTEGER offset, ULARGE_INTEGER n,
                                                      DWORD type) {
    /* We do not support locking. */
    return STG_E_INVALIDFUNCTION;
}

static HRESULT STDMETHODCALLTYPE memstream_UnlockRegion(IStream* self, ULARGE_INTEGER offset, ULARGE_INTEGER n,
                                                        DWORD type) {
    /* We do not support locking. */
    return STG_E_INVALIDFUNCTION;
}

static HRESULT STDMETHODCALLTYPE memstream_Stat(IStream* self, STATSTG* stat, DWORD flag) {
    MEMSTREAM* s = MEMSTREAM_FROM_IFACE(self);

    memset(stat, 0, sizeof(STATSTG));
    stat->type = STGTY_STREAM;
    stat->cbSize.QuadPart = s->size;
    stat->grfMode = STGM_READ; /* We are read-only. */
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE memstream_Clone(IStream* self, IStream** p_other) {
    MEMSTREAM* s = MEMSTREAM_FROM_IFACE(self);
    IStream* o;
    HRESULT hr;

    hr = memstream_create(s->buffer, s->size, &o);
    if (o != NULL) {
        MEMSTREAM* so = MEMSTREAM_FROM_IFACE(o);
        so->pos = s->pos;
    }

    *p_other = o;
    return hr;
}

static IStreamVtbl memstream_vtable = {
    memstream_QueryInterface, memstream_AddRef,       memstream_Release, memstream_Read,   memstream_Write,
    memstream_Seek,           memstream_SetSize,      memstream_CopyTo,  memstream_Commit, memstream_Revert,
    memstream_LockRegion,     memstream_UnlockRegion, memstream_Stat,    memstream_Clone};

HRESULT
memstream_create(const BYTE* buffer, ULONG size, IStream** p_stream) {
    MEMSTREAM* s;

    s = (MEMSTREAM*)malloc(sizeof(MEMSTREAM));
    if (s == NULL) {
        *p_stream = NULL;
        return ERROR_OUTOFMEMORY;
    }

    s->buffer = buffer;
    s->pos = 0;
    s->size = size;
    s->refs = 1;
    s->stream.lpVtbl = &memstream_vtable;

    *p_stream = &s->stream;
    return S_OK;
}

HRESULT
memstream_create_from_resource(HINSTANCE instance, const WCHAR* res_type, const WCHAR* res_name, IStream** p_stream) {
    HRSRC res;
    DWORD res_size;
    HGLOBAL res_global;
    void* res_data;

    /* We rely on the fact that UnlockResource() and FreeResource() do nothing:
     *  -- MSDN docs for LockResource() says no unlocking is needed.
     *  -- MSDN docs for FreeResource() says it just returns FALSE on 32/64-bit
     *     Windows.
     *
     * See also http://blogs.msdn.com/b/oldnewthing/archive/2011/03/07/10137456.aspx
     *
     * It may look a bit ugly, but it simplifies things a lot as the stream
     * does not need to do any bookkeeping for the resource.
     */

    if ((res = FindResourceW(instance, res_name, res_type)) == NULL ||
        (res_size = SizeofResource(instance, res)) == 0 || (res_global = LoadResource(instance, res)) == NULL ||
        (res_data = LockResource(res_global)) == NULL) {
        *p_stream = NULL;
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return memstream_create(res_data, res_size, p_stream);
}

/* -------------------------------------- bitblt.c -------------------------------------- */


void wdBitBltImage(WD_HCANVAS hCanvas, const WD_HIMAGE hImage, const WD_RECT* pDestRect, const WD_RECT* pSourceRect) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        IWICBitmapSource* bitmap = (IWICBitmapSource*)hImage;
        c_ID2D1Bitmap* b;
        HRESULT hr;

        /* Compensation for the translation in the base transformation matrix.
         * This is to fit the image precisely into the pixel grid the canvas
         * when there is no custom transformation applied.
         */
        c_D2D1_RECT_F dest = {pDestRect->x0 - D2D_BASEDELTA_X, pDestRect->y0 - D2D_BASEDELTA_Y,
                              pDestRect->x1 - D2D_BASEDELTA_X, pDestRect->y1 - D2D_BASEDELTA_Y};

        hr = c_ID2D1RenderTarget_CreateBitmapFromWicBitmap(c->target, bitmap, NULL, &b);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdBitBltImage: "
                "ID2D1RenderTarget::CreateBitmapFromWicBitmap() failed.");
            return;
        }

        c_ID2D1RenderTarget_DrawBitmap(c->target, b, &dest, 1.0f, c_D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                                       (c_D2D1_RECT_F*)pSourceRect);
        c_ID2D1Bitmap_Release(b);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        c_GpImage* b = (c_GpImage*)hImage;
        float dx, dy, dw, dh;
        float sx, sy, sw, sh;

        dx = pDestRect->x0;
        dy = pDestRect->y0;
        dw = pDestRect->x1 - pDestRect->x0;
        dh = pDestRect->y1 - pDestRect->y0;

        if (pSourceRect != NULL) {
            sx = pSourceRect->x0;
            sy = pSourceRect->y0;
            sw = pSourceRect->x1 - pSourceRect->x0;
            sh = pSourceRect->y1 - pSourceRect->y0;
        } else {
            UINT w, h;

            gdix_vtable->fn_GetImageWidth(b, &w);
            gdix_vtable->fn_GetImageHeight(b, &h);

            sx = 0.0f;
            sy = 0.0f;
            sw = (float)w;
            sh = (float)h;
        }

        gdix_vtable->fn_DrawImageRectRect(c->graphics, b, dx, dy, dw, dh, sx, sy, sw, sh, c_UnitPixel, NULL, NULL,
                                          NULL);
    }
}

void wdBitBltCachedImage(WD_HCANVAS hCanvas, const WD_HCACHEDIMAGE hCachedImage, float x, float y) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Bitmap* b = (c_ID2D1Bitmap*)hCachedImage;
        c_D2D1_SIZE_U sz;
        c_D2D1_RECT_F dest;

        c_ID2D1Bitmap_GetPixelSize(b, &sz);

        dest.left = x - D2D_BASEDELTA_X;
        dest.top = y - D2D_BASEDELTA_X;
        dest.right = (x + sz.width) - D2D_BASEDELTA_X;
        dest.bottom = (y + sz.height) - D2D_BASEDELTA_X;

        c_ID2D1RenderTarget_DrawBitmap(c->target, b, &dest, 1.0f, c_D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        c_GpCachedBitmap* cb = (c_GpCachedBitmap*)hCachedImage;

        gdix_vtable->fn_DrawCachedBitmap(c->graphics, cb, (INT)x, (INT)y);
    }
}

void wdBitBltHICON(WD_HCANVAS hCanvas, HICON hIcon, const WD_RECT* pDestRect, const WD_RECT* pSourceRect) {
    if (d2d_enabled()) {
        IWICBitmap* bitmap;
        IWICFormatConverter* converter;
        HRESULT hr;

        hr = IWICImagingFactory_CreateBitmapFromHICON(wic_factory, hIcon, &bitmap);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdBitBltHICON: "
                "IWICImagingFactory::CreateBitmapFromHICON() failed.");
            goto err_CreateBitmapFromHICON;
        }

        hr = IWICImagingFactory_CreateFormatConverter(wic_factory, &converter);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdBitBltHICON: "
                "IWICImagingFactory::CreateFormatConverter() failed.");
            goto err_CreateFormatConverter;
        }

        hr = IWICFormatConverter_Initialize(converter, (IWICBitmapSource*)bitmap, &wic_pixel_format,
                                            WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdBitBltHICON: "
                "IWICFormatConverter::Initialize() failed.");
            goto err_Initialize;
        }

        wdBitBltImage(hCanvas, (WD_HIMAGE)converter, pDestRect, pSourceRect);

    err_Initialize:
        IWICFormatConverter_Release(converter);
    err_CreateFormatConverter:
        IWICBitmap_Release(bitmap);
    err_CreateBitmapFromHICON:; /* noop */
    } else {
        c_GpBitmap* b;
        int status;

        status = gdix_vtable->fn_CreateBitmapFromHICON(hIcon, &b);
        if (status != 0) {
            WD_TRACE(
                "wdBitBltHICON: GdipCreateBitmapFromHICON() failed. "
                "[%d]",
                status);
            return;
        }
        wdBitBltImage(hCanvas, (WD_HIMAGE)b, pDestRect, pSourceRect);
        gdix_vtable->fn_DisposeImage(b);
    }
}

/* ------------------------------------ brush.c ----------------------------------------- */


WD_HBRUSH
wdCreateSolidBrush(WD_HCANVAS hCanvas, WD_COLOR color) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1SolidColorBrush* b;
        c_D2D1_COLOR_F clr;
        HRESULT hr;

        d2d_init_color(&clr, color);
        hr = c_ID2D1RenderTarget_CreateSolidColorBrush(c->target, &clr, NULL, &b);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateSolidBrush: "
                "ID2D1RenderTarget::CreateSolidColorBrush() failed.");
            return NULL;
        }
        return (WD_HBRUSH)b;
    } else {
        c_GpSolidFill* b;
        int status;

        status = gdix_vtable->fn_CreateSolidFill(color, &b);
        if (status != 0) {
            WD_TRACE(
                "wdCreateSolidBrush: "
                "GdipCreateSolidFill() failed. [%d]",
                status);
            return NULL;
        }
        return (WD_HBRUSH)b;
    }
}

void wdDestroyBrush(WD_HBRUSH hBrush) {
    if (d2d_enabled()) {
        c_ID2D1Brush_Release((c_ID2D1Brush*)hBrush);
    } else {
        gdix_vtable->fn_DeleteBrush((void*)hBrush);
    }
}

void wdSetSolidBrushColor(WD_HBRUSH hBrush, WD_COLOR color) {
    if (d2d_enabled()) {
        c_D2D1_COLOR_F clr;

        d2d_init_color(&clr, color);
        c_ID2D1SolidColorBrush_SetColor((c_ID2D1SolidColorBrush*)hBrush, &clr);
    } else {
        c_GpSolidFill* b = (c_GpSolidFill*)hBrush;

        gdix_vtable->fn_SetSolidFillColor(b, (c_ARGB)color);
    }
}

WD_HBRUSH
wdCreateLinearGradientBrushEx(WD_HCANVAS hCanvas, float x0, float y0, float x1, float y1, const WD_COLOR* colors,
                              const float* offsets, UINT numStops) {
    if (numStops < 2)
        return NULL;
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;

        HRESULT hr;
        c_ID2D1GradientStopCollection* collection;
        c_ID2D1LinearGradientBrush* b;
        c_D2D1_GRADIENT_STOP* stops = (c_D2D1_GRADIENT_STOP*)malloc(numStops * sizeof(c_D2D1_GRADIENT_STOP));
        c_D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gradientProperties;

        for (UINT i = 0; i < numStops; i++) {
            d2d_init_color(&stops[i].color, colors[i]);
            stops[i].position = offsets[i];
        }
        hr = c_ID2D1RenderTarget_CreateGradientStopCollection(c->target, stops, numStops, c_D2D1_GAMMA_2_2,
                                                              c_D2D1_EXTEND_MODE_CLAMP, &collection);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateLinearGradientBrushEx: "
                "ID2D1RenderTarget::CreateGradientStopCollection() failed.");
            free(stops);
            return NULL;
        }
        gradientProperties.startPoint.x = x0;
        gradientProperties.startPoint.y = y0;
        gradientProperties.endPoint.x = x1;
        gradientProperties.endPoint.y = y1;
        hr = c_ID2D1RenderTarget_CreateLinearGradientBrush(c->target, &gradientProperties, NULL, collection, &b);
        c_ID2D1GradientStopCollection_Release(collection);
        free(stops);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateLinearGradientBrushEx: "
                "ID2D1RenderTarget::CreateLinearGradientBrush() failed.");
            return NULL;
        }
        return (WD_HBRUSH)b;
    } else {
        int status;
        WD_COLOR color0 = colors[0];
        WD_COLOR color1 = colors[numStops - 1];
        c_GpLineGradient* grad;
        c_GpPointF p0;
        c_GpPointF p1;
        p0.x = x0;
        p0.y = y0;
        p1.x = x1;
        p1.y = y1;
        status = gdix_vtable->fn_CreateLineBrush(&p0, &p1, color0, color1, c_WrapModeTile, &grad);
        if (status != 0) {
            WD_TRACE(
                "wdCreateLinearGradientBrushEx: "
                "GdipCreateLineBrush() failed. [%d]",
                status);
            return NULL;
        }
        status = gdix_vtable->fn_SetLinePresetBlend(grad, colors, offsets, numStops);
        if (status != 0) {
            WD_TRACE(
                "wdCreateLinearGradientBrushEx: "
                "GdipSetLinePresetBlend() failed. [%d]",
                status);
            return NULL;
        }
        return (WD_HBRUSH)grad;
    }
    return NULL;
}

WD_HBRUSH
wdCreateLinearGradientBrush(WD_HCANVAS hCanvas, float x0, float y0, WD_COLOR color0, float x1, float y1,
                            WD_COLOR color1) {
    WD_COLOR colors[] = {color0, color1};
    float offsets[] = {0.0f, 1.0f};
    return wdCreateLinearGradientBrushEx(hCanvas, x0, y0, x1, y1, colors, offsets, 2);
}

WD_HBRUSH
wdCreateRadialGradientBrushEx(WD_HCANVAS hCanvas, float cx, float cy, float r, float fx, float fy,
                              const WD_COLOR* colors, const float* offsets, UINT numStops) {
    if (numStops < 2)
        return NULL;
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;

        HRESULT hr;
        c_ID2D1GradientStopCollection* collection;
        c_ID2D1RadialGradientBrush* b;
        c_D2D1_GRADIENT_STOP* stops = (c_D2D1_GRADIENT_STOP*)malloc(numStops * sizeof(c_D2D1_GRADIENT_STOP));
        c_D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES gradientProperties;

        for (UINT i = 0; i < numStops; i++) {
            d2d_init_color(&stops[i].color, colors[i]);
            stops[i].position = offsets[i];
        }
        hr = c_ID2D1RenderTarget_CreateGradientStopCollection(c->target, stops, numStops, c_D2D1_GAMMA_2_2,
                                                              c_D2D1_EXTEND_MODE_CLAMP, &collection);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateRadialGradientBrushEx: "
                "ID2D1RenderTarget::CreateGradientStopCollection() failed.");
            free(stops);
            return NULL;
        }
        gradientProperties.center.x = cx;
        gradientProperties.center.y = cy;
        gradientProperties.gradientOriginOffset.x = fx - cx;
        gradientProperties.gradientOriginOffset.y = fy - cy;
        gradientProperties.radiusX = r;
        gradientProperties.radiusY = r;
        hr = c_ID2D1RenderTarget_CreateRadialGradientBrush(c->target, &gradientProperties, NULL, collection, &b);
        c_ID2D1GradientStopCollection_Release(collection);
        free(stops);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateRadialGradientBrushEx: "
                "ID2D1RenderTarget::CreateRadialGradientBrush() failed.");
            return NULL;
        }
        return (WD_HBRUSH)b;
    } else {
        // TODO: Colors outside of the ellipse can only get faked
        // with a second brush.
        WD_HPATH p;
        WD_RECT rect;
        rect.x0 = cx - r;
        rect.y0 = cy - r;
        rect.x1 = cx + r;
        rect.y1 = cy + r;
        p = wdCreateRoundedRectPath(hCanvas, &rect, r);

        int status;
        c_GpPathGradient* grad;
        status = gdix_vtable->fn_CreatePathGradientFromPath((void*)p, &grad);
        wdDestroyPath(p);
        if (status != 0) {
            WD_TRACE(
                "wdCreateRadialGradientBrushEx: "
                "GdipCreatePathGradientFromPath() failed. [%d]",
                status);
            return NULL;
        }
        WD_POINT focalPoint[1];
        focalPoint[0].x = fx;
        focalPoint[0].y = fy;
        gdix_vtable->fn_SetPathGradientCenterPoint(grad, (c_GpPointF*)focalPoint);

        float* reverseStops = (float*)malloc(numStops * sizeof(float));
        WD_COLOR* reverseColors = (WD_COLOR*)malloc(numStops * sizeof(WD_COLOR));
        for (UINT i = 0; i < numStops; i++) {
            reverseStops[i] = 1 - offsets[numStops - i - 1];
            reverseColors[i] = colors[numStops - i - 1];
        }

        status = gdix_vtable->fn_SetPathGradientPresetBlend(grad, reverseColors, reverseStops, numStops);
        free(reverseStops);
        free(reverseColors);
        if (status != 0) {
            WD_TRACE(
                "wdCreateRadialGradientBrushEx: "
                "GdipSetPathGradientPresetBlend() failed. [%d]",
                status);
            return NULL;
        }
        return (WD_HBRUSH)grad;
    }
    return NULL;
}

WD_HBRUSH
wdCreateRadialGradientBrush(WD_HCANVAS hCanvas, float cx, float cy, float r, WD_COLOR color0, WD_COLOR color1) {
    WD_COLOR colors[] = {color0, color1};
    float offsets[] = {0.0f, 1.0f};
    return wdCreateRadialGradientBrushEx(hCanvas, cx, cy, r, cx, cy, colors, offsets, 2);
}

/* ------------------------------------------ canvas.c ----------------------------------- */


WD_HCANVAS
wdCreateCanvasWithPaintStruct(HWND hWnd, PAINTSTRUCT* pPS, DWORD dwFlags) {
    RECT rect;

    GetClientRect(hWnd, &rect);

    if (d2d_enabled()) {
        c_D2D1_RENDER_TARGET_PROPERTIES props = {
            c_D2D1_RENDER_TARGET_TYPE_DEFAULT,
            {c_DXGI_FORMAT_B8G8R8A8_UNORM, c_D2D1_ALPHA_MODE_PREMULTIPLIED},
            0.0f,
            0.0f,
            ((dwFlags & WD_CANVAS_NOGDICOMPAT) ? 0 : c_D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE),
            c_D2D1_FEATURE_LEVEL_DEFAULT};
        c_D2D1_HWND_RENDER_TARGET_PROPERTIES props2;
        d2d_canvas_t* c;
        c_ID2D1HwndRenderTarget* target;
        HRESULT hr;

        props2.hwnd = hWnd;
        props2.pixelSize.width = rect.right - rect.left;
        props2.pixelSize.height = rect.bottom - rect.top;
        props2.presentOptions = c_D2D1_PRESENT_OPTIONS_NONE;

        wd_lock();
        /* Note ID2D1HwndRenderTarget is implicitly double-buffered. */
        hr = c_ID2D1Factory_CreateHwndRenderTarget(d2d_factory, &props, &props2, &target);
        wd_unlock();
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateCanvasWithPaintStruct: "
                "ID2D1Factory::CreateHwndRenderTarget() failed.");
            return NULL;
        }

        c = d2d_canvas_alloc((c_ID2D1RenderTarget*)target, D2D_CANVASTYPE_HWND, rect.right,
                             (dwFlags & WD_CANVAS_LAYOUTRTL));
        if (c == NULL) {
            WD_TRACE("wdCreateCanvasWithPaintStruct: d2d_canvas_alloc() failed.");
            c_ID2D1RenderTarget_Release((c_ID2D1RenderTarget*)target);
            return NULL;
        }

        /* make sure text anti-aliasing is clear type */
        c_ID2D1RenderTarget_SetTextAntialiasMode(c->target, c_D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

        return (WD_HCANVAS)c;
    } else {
        BOOL use_doublebuffer = (dwFlags & WD_CANVAS_DOUBLEBUFFER);
        gdix_canvas_t* c;

        c = gdix_canvas_alloc(pPS->hdc, (use_doublebuffer ? &pPS->rcPaint : NULL), rect.right,
                              (dwFlags & WD_CANVAS_LAYOUTRTL));
        if (c == NULL) {
            WD_TRACE("wdCreateCanvasWithPaintStruct: gdix_canvas_alloc() failed.");
            return NULL;
        }
        return (WD_HCANVAS)c;
    }
}

WD_HCANVAS
wdCreateCanvasWithHDC(HDC hDC, const RECT* pRect, DWORD dwFlags) {
    if (d2d_enabled()) {
        c_D2D1_RENDER_TARGET_PROPERTIES props = {
            c_D2D1_RENDER_TARGET_TYPE_DEFAULT,
            {c_DXGI_FORMAT_B8G8R8A8_UNORM, c_D2D1_ALPHA_MODE_PREMULTIPLIED},
            0.0f,
            0.0f,
            ((dwFlags & WD_CANVAS_NOGDICOMPAT) ? 0 : c_D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE),
            c_D2D1_FEATURE_LEVEL_DEFAULT};
        d2d_canvas_t* c;
        c_ID2D1DCRenderTarget* target;
        HRESULT hr;

        wd_lock();
        hr = c_ID2D1Factory_CreateDCRenderTarget(d2d_factory, &props, &target);
        wd_unlock();
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateCanvasWithHDC: "
                "ID2D1Factory::CreateDCRenderTarget() failed.");
            goto err_CreateDCRenderTarget;
        }

        hr = c_ID2D1DCRenderTarget_BindDC(target, hDC, pRect);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateCanvasWithHDC: "
                "ID2D1Factory::BindDC() failed.");
            goto err_BindDC;
        }

        c = d2d_canvas_alloc((c_ID2D1RenderTarget*)target, D2D_CANVASTYPE_DC, pRect->right - pRect->left,
                             (dwFlags & WD_CANVAS_LAYOUTRTL));
        if (c == NULL) {
            WD_TRACE("wdCreateCanvasWithHDC: d2d_canvas_alloc() failed.");
            goto err_d2d_canvas_alloc;
        }

        /* make sure text anti-aliasing is clear type */
        c_ID2D1RenderTarget_SetTextAntialiasMode(c->target, c_D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

        return (WD_HCANVAS)c;

    err_d2d_canvas_alloc:
    err_BindDC:
        c_ID2D1RenderTarget_Release((c_ID2D1RenderTarget*)target);
    err_CreateDCRenderTarget:
        return NULL;
    } else {
        BOOL use_doublebuffer = (dwFlags & WD_CANVAS_DOUBLEBUFFER);
        gdix_canvas_t* c;

        c = gdix_canvas_alloc(hDC, (use_doublebuffer ? pRect : NULL), pRect->right - pRect->left,
                              (dwFlags & WD_CANVAS_LAYOUTRTL));
        if (c == NULL) {
            WD_TRACE("wdCreateCanvasWithHDC: gdix_canvas_alloc() failed.");
            return NULL;
        }
        return (WD_HCANVAS)c;
    }
}

void wdDestroyCanvas(WD_HCANVAS hCanvas) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;

        /* Check for common logical errors. */
        if (c->clip_layer != NULL || (c->flags & D2D_CANVASFLAG_RECTCLIP))
            WD_TRACE("wdDestroyCanvas: Logical error: Canvas has dangling clip.");
        if (c->gdi_interop != NULL)
            WD_TRACE("wdDestroyCanvas: Logical error: Unpaired wdStartGdi()/wdEndGdi().");

        c_ID2D1RenderTarget_Release(c->target);
        free(c);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;

        gdix_vtable->fn_DeleteStringFormat(c->string_format);
        gdix_vtable->fn_DeletePen(c->pen);
        gdix_vtable->fn_DeleteGraphics(c->graphics);

        if (c->real_dc != NULL) {
            HBITMAP mem_bmp;

            mem_bmp = SelectObject(c->dc, c->orig_bmp);
            DeleteObject(mem_bmp);
            DeleteObject(c->dc);
        }

        free(c);
    }
}

void wdBeginPaint(WD_HCANVAS hCanvas) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1RenderTarget_BeginDraw(c->target);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        SetLayout(c->dc, 0);
    }
}

BOOL wdEndPaint(WD_HCANVAS hCanvas) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        HRESULT hr;

        d2d_reset_clip(c);

        hr = c_ID2D1RenderTarget_EndDraw(c->target, NULL, NULL);
        if (FAILED(hr)) {
            if (hr != D2DERR_RECREATE_TARGET)
                WD_TRACE_HR("wdEndPaint: ID2D1RenderTarget::EndDraw() failed.");
            return FALSE;
        }
        return TRUE;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;

        /* If double-buffering, blit the memory DC to the display DC. */
        if (c->real_dc != NULL)
            BitBlt(c->real_dc, c->x, c->y, c->cx, c->cy, c->dc, 0, 0, SRCCOPY);

        SetLayout(c->real_dc, c->dc_layout);

        /* For GDI+, disable caching. */
        return FALSE;
    }
}

BOOL wdResizeCanvas(WD_HCANVAS hCanvas, UINT uWidth, UINT uHeight) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        if (c->type == D2D_CANVASTYPE_HWND) {
            c_D2D1_SIZE_U size = {uWidth, uHeight};
            HRESULT hr;

            hr = c_ID2D1HwndRenderTarget_Resize(c->hwnd_target, &size);
            if (FAILED(hr)) {
                WD_TRACE_HR(
                    "wdResizeCanvas: "
                    "ID2D1HwndRenderTarget_Resize() failed.");
                return FALSE;
            }

            /* In RTL mode, we have to update the transformation matrix
             * accordingly. */
            if (c->flags & D2D_CANVASFLAG_RTL) {
                c_D2D1_MATRIX_3X2_F m;

                c_ID2D1RenderTarget_GetTransform(c->target, &m);
                m._31 = m._11 * (float)(uWidth - c->width);
                m._32 = m._12 * (float)(uWidth - c->width);
                c_ID2D1RenderTarget_SetTransform(c->target, &m);

                c->width = uWidth;
            }
            return TRUE;
        } else {
            /* Operation not supported. */
            WD_TRACE("wdResizeCanvas: Not supported (not ID2D1HwndRenderTarget).");
            return FALSE;
        }
    } else {
        /* Actually we should never be here as GDI+ back-end never allows
         * caching of the canvas so there is no need to ever resize it. */
        WD_TRACE("wdResizeCanvas: Not supported (GDI+ back-end).");
        return FALSE;
    }
}

HDC wdStartGdi(WD_HCANVAS hCanvas, BOOL bKeepContents) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1GdiInteropRenderTarget* gdi_interop;
        c_D2D1_DC_INITIALIZE_MODE init_mode;
        HRESULT hr;
        HDC dc;

        hr = c_ID2D1RenderTarget_QueryInterface(c->target, &c_IID_ID2D1GdiInteropRenderTarget, (void**)&gdi_interop);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdStartGdi: ID2D1RenderTarget::"
                "QueryInterface(IID_ID2D1GdiInteropRenderTarget) failed.");
            return NULL;
        }

        init_mode = (bKeepContents ? c_D2D1_DC_INITIALIZE_MODE_COPY : c_D2D1_DC_INITIALIZE_MODE_CLEAR);
        hr = c_ID2D1GdiInteropRenderTarget_GetDC(gdi_interop, init_mode, &dc);
        if (FAILED(hr)) {
            WD_TRACE_HR("wdStartGdi: ID2D1GdiInteropRenderTarget::GetDC() failed.");
            c_ID2D1GdiInteropRenderTarget_Release(gdi_interop);
            return NULL;
        }

        c->gdi_interop = gdi_interop;

        if (c->flags & D2D_CANVASFLAG_RTL)
            SetLayout(dc, LAYOUT_RTL);

        return dc;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        int status;
        HDC dc;

        status = gdix_vtable->fn_GetDC(c->graphics, &dc);
        if (status != 0) {
            WD_TRACE_ERR_("wdStartGdi: GdipGetDC() failed.", status);
            return NULL;
        }

        SetLayout(dc, c->dc_layout);

        if (c->dc_layout & LAYOUT_RTL)
            SetViewportOrgEx(dc, c->x + c->cx - (c->width - 1), -c->y, NULL);

        return dc;
    }
}

void wdEndGdi(WD_HCANVAS hCanvas, HDC hDC) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;

        c_ID2D1GdiInteropRenderTarget_ReleaseDC(c->gdi_interop, NULL);
        c_ID2D1GdiInteropRenderTarget_Release(c->gdi_interop);
        c->gdi_interop = NULL;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;

        if (c->rtl)
            SetLayout(hDC, 0);
        gdix_vtable->fn_ReleaseDC(c->graphics, hDC);
    }
}

void wdClear(WD_HCANVAS hCanvas, WD_COLOR color) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_D2D1_COLOR_F clr;

        d2d_init_color(&clr, color);
        c_ID2D1RenderTarget_Clear(c->target, &clr);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_vtable->fn_GraphicsClear(c->graphics, color);
    }
}

void wdSetClip(WD_HCANVAS hCanvas, const WD_RECT* pRect, const WD_HPATH hPath) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;

        d2d_reset_clip(c);

        if (hPath != NULL) {
            c_ID2D1PathGeometry* g = (c_ID2D1PathGeometry*)hPath;
            c_D2D1_LAYER_PARAMETERS layer_params;
            HRESULT hr;

            hr = c_ID2D1RenderTarget_CreateLayer(c->target, NULL, &c->clip_layer);
            if (FAILED(hr)) {
                WD_TRACE_HR("wdSetClip: ID2D1RenderTarget::CreateLayer() failed.");
                return;
            }

            if (pRect != NULL) {
                layer_params.contentBounds.left = pRect->x0;
                layer_params.contentBounds.top = pRect->y0;
                layer_params.contentBounds.right = pRect->x1;
                layer_params.contentBounds.bottom = pRect->y1;
            } else {
                layer_params.contentBounds.left = FLT_MIN;
                layer_params.contentBounds.top = FLT_MIN;
                layer_params.contentBounds.right = FLT_MAX;
                layer_params.contentBounds.bottom = FLT_MAX;
            }
            layer_params.geometricMask = (c_ID2D1Geometry*)g;
            layer_params.maskAntialiasMode = c_D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
            layer_params.maskTransform._11 = 1.0f;
            layer_params.maskTransform._12 = 0.0f;
            layer_params.maskTransform._21 = 0.0f;
            layer_params.maskTransform._22 = 1.0f;
            layer_params.maskTransform._31 = 0.0f;
            layer_params.maskTransform._32 = 0.0f;
            layer_params.opacity = 1.0f;
            layer_params.opacityBrush = NULL;
            layer_params.layerOptions = c_D2D1_LAYER_OPTIONS_NONE;

            c_ID2D1RenderTarget_PushLayer(c->target, &layer_params, c->clip_layer);
        } else if (pRect != NULL) {
            c_ID2D1RenderTarget_PushAxisAlignedClip(c->target, (const c_D2D1_RECT_F*)pRect,
                                                    c_D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            c->flags |= D2D_CANVASFLAG_RECTCLIP;
        }
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        int mode;

        if (pRect == NULL && hPath == NULL) {
            gdix_vtable->fn_ResetClip(c->graphics);
            return;
        }

        mode = c_CombineModeReplace;

        if (pRect != NULL) {
            gdix_vtable->fn_SetClipRect(c->graphics, pRect->x0, pRect->y0, pRect->x1, pRect->y1, mode);
            mode = c_CombineModeIntersect;
        }

        if (hPath != NULL)
            gdix_vtable->fn_SetClipPath(c->graphics, (void*)hPath, mode);
    }
}

void wdRotateWorld(WD_HCANVAS hCanvas, float cx, float cy, float fAngle) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_D2D1_MATRIX_3X2_F m;
        float a_rads = fAngle * (WD_PI / 180.0f);
        float a_sin = sinf(a_rads);
        float a_cos = cosf(a_rads);

        m._11 = a_cos;
        m._12 = a_sin;
        m._21 = -a_sin;
        m._22 = a_cos;
        m._31 = cx - cx * a_cos + cy * a_sin;
        m._32 = cy - cx * a_sin - cy * a_cos;
        d2d_apply_transform(c, &m);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;

        gdix_vtable->fn_TranslateWorldTransform(c->graphics, cx, cy, c_MatrixOrderPrepend);
        gdix_vtable->fn_RotateWorldTransform(c->graphics, fAngle, c_MatrixOrderPrepend);
        gdix_vtable->fn_TranslateWorldTransform(c->graphics, -cx, -cy, c_MatrixOrderPrepend);
    }
}

void wdTranslateWorld(WD_HCANVAS hCanvas, float dx, float dy) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_D2D1_MATRIX_3X2_F m;

        c_ID2D1RenderTarget_GetTransform(c->target, &m);
        m._31 += dx;
        m._32 += dy;
        c_ID2D1RenderTarget_SetTransform(c->target, &m);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_vtable->fn_TranslateWorldTransform(c->graphics, dx, dy, c_MatrixOrderAppend);
    }
}

void wdTransformWorld(WD_HCANVAS hCanvas, const WD_MATRIX* pMatrix) {
    if (pMatrix == NULL) {
        WD_TRACE("wdSetWorldTransform: Invalid pMatrix");
        return;
    }
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;

        c_D2D1_MATRIX_3X2_F m;

        m._11 = pMatrix->m11;
        m._12 = pMatrix->m12;
        m._21 = pMatrix->m21;
        m._22 = pMatrix->m22;
        m._31 = pMatrix->dx;
        m._32 = pMatrix->dy;
        d2d_apply_transform(c, &m);
    } else {
        int status;
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;

        c_GpMatrix* matrix;
        status = gdix_vtable->fn_CreateMatrix2(pMatrix->m11, pMatrix->m12, pMatrix->m21, pMatrix->m22, pMatrix->dx,
                                               pMatrix->dy, &matrix);
        if (status != 0) {
            WD_TRACE_ERR_("wdSetWorldTransform: GdpiCreateMatrix2() failed", status);
            return;
        }
        status = gdix_vtable->fn_MultiplyWorldTransform(c->graphics, matrix, c_MatrixOrderPrepend);
        if (status != 0) {
            WD_TRACE_ERR_("wdSetWorldTransform: MultiplyWorldTransform() failed", status);
            gdix_delete_matrix(matrix);
            return;
        }
        gdix_delete_matrix(matrix);
    }
}

void wdResetWorld(WD_HCANVAS hCanvas) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        d2d_reset_transform(c);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_reset_transform(c);
    }
}

/* ---------------------------------------------- draw.c ---------------------------------- */


void wdDrawEllipseArcStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                            float fBaseAngle, float fSweepAngle, float fStrokeWidth, WD_HSTROKESTYLE hStrokeStyle) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_ID2D1Geometry* g;
        c_ID2D1StrokeStyle* s = (c_ID2D1StrokeStyle*)hStrokeStyle;

        g = d2d_create_arc_geometry(cx, cy, rx, ry, fBaseAngle, fSweepAngle, FALSE);
        if (g == NULL) {
            WD_TRACE("wdDrawArc: d2d_create_arc_geometry() failed.");
            return;
        }

        c_ID2D1RenderTarget_DrawGeometry(c->target, g, b, fStrokeWidth, s);
        c_ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_strokestyle_t* s = (gdix_strokestyle_t*)hStrokeStyle;
        c_GpBrush* b = (c_GpBrush*)hBrush;
        float dx = 2.0f * rx;
        float dy = 2.0f * ry;

        gdix_setpen(c->pen, b, fStrokeWidth, s);

        gdix_vtable->fn_DrawArc(c->graphics, c->pen, cx - rx, cy - ry, dx, dy, fBaseAngle, fSweepAngle);
    }
}

void wdDrawEllipseStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                         float fStrokeWidth, WD_HSTROKESTYLE hStrokeStyle) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_D2D1_ELLIPSE e = {{cx, cy}, rx, ry};
        c_ID2D1StrokeStyle* s = (c_ID2D1StrokeStyle*)hStrokeStyle;

        c_ID2D1RenderTarget_DrawEllipse(c->target, &e, b, fStrokeWidth, s);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_strokestyle_t* s = (gdix_strokestyle_t*)hStrokeStyle;
        c_GpBrush* b = (c_GpBrush*)hBrush;
        float dx = 2.0f * rx;
        float dy = 2.0f * ry;

        gdix_setpen(c->pen, b, fStrokeWidth, s);

        gdix_vtable->fn_DrawEllipse(c->graphics, (void*)c->pen, cx - rx, cy - ry, dx, dy);
    }
}

void wdDrawLineStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float x0, float y0, float x1, float y1, float fStrokeWidth,
                      WD_HSTROKESTYLE hStrokeStyle) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_D2D1_POINT_2F pt0 = {x0, y0};
        c_D2D1_POINT_2F pt1 = {x1, y1};
        c_ID2D1StrokeStyle* s = (c_ID2D1StrokeStyle*)hStrokeStyle;

        c_ID2D1RenderTarget_DrawLine(c->target, pt0, pt1, b, fStrokeWidth, s);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_strokestyle_t* s = (gdix_strokestyle_t*)hStrokeStyle;
        c_GpBrush* b = (c_GpBrush*)hBrush;

        gdix_setpen(c->pen, b, fStrokeWidth, s);

        gdix_vtable->fn_DrawLine(c->graphics, c->pen, x0, y0, x1, y1);
    }
}

void wdDrawPathStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_HPATH hPath, float fStrokeWidth,
                      WD_HSTROKESTYLE hStrokeStyle) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Geometry* g = (c_ID2D1Geometry*)hPath;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_ID2D1StrokeStyle* s = (c_ID2D1StrokeStyle*)hStrokeStyle;

        c_ID2D1RenderTarget_DrawGeometry(c->target, g, b, fStrokeWidth, s);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_strokestyle_t* s = (gdix_strokestyle_t*)hStrokeStyle;
        c_GpBrush* b = (c_GpBrush*)hBrush;

        gdix_setpen(c->pen, b, fStrokeWidth, s);

        gdix_vtable->fn_DrawPath(c->graphics, (void*)c->pen, (void*)hPath);
    }
}

void wdDrawEllipsePieStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry,
                            float fBaseAngle, float fSweepAngle, float fStrokeWidth, WD_HSTROKESTYLE hStrokeStyle) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_ID2D1Geometry* g;
        c_ID2D1StrokeStyle* s = (c_ID2D1StrokeStyle*)hStrokeStyle;

        g = d2d_create_arc_geometry(cx, cy, rx, ry, fBaseAngle, fSweepAngle, TRUE);
        if (g == NULL) {
            WD_TRACE("wdDrawPie: d2d_create_arc_geometry() failed.");
            return;
        }

        c_ID2D1RenderTarget_DrawGeometry(c->target, g, b, fStrokeWidth, s);
        c_ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_strokestyle_t* s = (gdix_strokestyle_t*)hStrokeStyle;
        c_GpBrush* b = (c_GpBrush*)hBrush;
        float dx = 2.0f * rx;
        float dy = 2.0f * ry;

        gdix_setpen(c->pen, b, fStrokeWidth, s);

        gdix_vtable->fn_DrawPie(c->graphics, c->pen, cx - rx, cy - ry, dx, dy, fBaseAngle, fSweepAngle);
    }
}

void wdDrawRectStyled(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float x0, float y0, float x1, float y1, float fStrokeWidth,
                      WD_HSTROKESTYLE hStrokeStyle) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_D2D1_RECT_F r = {x0, y0, x1, y1};
        c_ID2D1StrokeStyle* s = (c_ID2D1StrokeStyle*)hStrokeStyle;

        c_ID2D1RenderTarget_DrawRectangle(c->target, &r, b, fStrokeWidth, s);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        gdix_strokestyle_t* s = (gdix_strokestyle_t*)hStrokeStyle;
        c_GpBrush* b = (c_GpBrush*)hBrush;
        float tmp;

        /* Make sure x0 <= x1 and y0 <= y1. */
        if (x0 > x1) {
            tmp = x0;
            x0 = x1;
            x1 = tmp;
        }
        if (y0 > y1) {
            tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        gdix_setpen(c->pen, b, fStrokeWidth, s);

        gdix_vtable->fn_DrawRectangle(c->graphics, c->pen, x0, y0, x1 - x0, y1 - y0);
    }
}

/* ------------------------------------------ fill.c ---------------------------------- */


void wdFillEllipse(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_D2D1_ELLIPSE e = {{cx, cy}, rx, ry};

        c_ID2D1RenderTarget_FillEllipse(c->target, &e, b);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        float dx = 2.0f * rx;
        float dy = 2.0f * ry;

        gdix_vtable->fn_FillEllipse(c->graphics, (void*)hBrush, cx - rx, cy - ry, dx, dy);
    }
}

void wdFillPath(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, const WD_HPATH hPath) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Geometry* g = (c_ID2D1Geometry*)hPath;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;

        c_ID2D1RenderTarget_FillGeometry(c->target, g, b, NULL);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;

        gdix_vtable->fn_FillPath(c->graphics, (void*)hBrush, (void*)hPath);
    }
}

void wdFillEllipsePie(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float cx, float cy, float rx, float ry, float fBaseAngle,
                      float fSweepAngle) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_ID2D1Geometry* g;

        g = d2d_create_arc_geometry(cx, cy, rx, ry, fBaseAngle, fSweepAngle, TRUE);
        if (g == NULL) {
            WD_TRACE("wdFillPie: d2d_create_arc_geometry() failed.");
            return;
        }

        c_ID2D1RenderTarget_FillGeometry(c->target, g, b, NULL);
        c_ID2D1Geometry_Release(g);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        float dx = 2.0f * rx;
        float dy = 2.0f * ry;

        gdix_vtable->fn_FillPie(c->graphics, (void*)hBrush, cx - rx, cy - ry, dx, dy, fBaseAngle, fSweepAngle);
    }
}

void wdFillRect(WD_HCANVAS hCanvas, WD_HBRUSH hBrush, float x0, float y0, float x1, float y1) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Brush* b = (c_ID2D1Brush*)hBrush;
        c_D2D1_RECT_F r = {x0, y0, x1, y1};

        c_ID2D1RenderTarget_FillRectangle(c->target, &r, b);
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        float tmp;

        /* Make sure x0 <= x1 and y0 <= y1. */
        if (x0 > x1) {
            tmp = x0;
            x0 = x1;
            x1 = tmp;
        }
        if (y0 > y1) {
            tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        gdix_vtable->fn_FillRectangle(c->graphics, (void*)hBrush, x0, y0, x1 - x0, y1 - y0);
    }
}

/* --------------------------------------------- font.c ------------------------------- */


static void wd_get_default_gui_fontface(WCHAR buffer[LF_FACESIZE]) {
    NONCLIENTMETRICSW metrics;

#if WINVER < 0x0600
    metrics.cbSize = sizeof(NONCLIENTMETRICSW);
#else
    metrics.cbSize = WD_OFFSETOF(NONCLIENTMETRICSW, iPaddedBorderWidth);
#endif
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 0, (void*)&metrics, 0);
    wcsncpy(buffer, metrics.lfMessageFont.lfFaceName, LF_FACESIZE);
}

WD_HFONT
wdCreateFont(const LOGFONTW* pLogFont) {
    if (d2d_enabled()) {
        static WCHAR no_locale[] = L"";
        static WCHAR enus_locale[] = L"en-us";

        WCHAR user_locale[LOCALE_NAME_MAX_LENGTH];
        WCHAR* locales[3];
        WCHAR default_fontface[LF_FACESIZE];
        dwrite_font_t* font;
        int i;

        font = (dwrite_font_t*)malloc(sizeof(dwrite_font_t));
        if (font == NULL) {
            WD_TRACE("wdCreateFont: malloc() failed.");
            return NULL;
        }

        dwrite_default_user_locale(user_locale);
        locales[0] = user_locale;
        locales[1] = no_locale;
        locales[2] = enus_locale;

        /* Direct 2D seems to not understand "MS Shell Dlg" and "MS Shell Dlg 2"
         * so we skip the attempts to use it. */
        if (wcscmp(pLogFont->lfFaceName, L"MS Shell Dlg") != 0 &&
            wcscmp(pLogFont->lfFaceName, L"MS Shell Dlg 2") != 0) {
            for (i = 0; i < WD_SIZEOF_ARRAY(locales); i++) {
                font->tf = dwrite_create_text_format(locales[i], pLogFont, &font->metrics);
                if (font->tf != NULL)
                    return (WD_HFONT)font;
            }
        }

        /* In case of a failure, we retry with a default GUI font face. */
        wd_get_default_gui_fontface(default_fontface);
        if (wcscmp(default_fontface, pLogFont->lfFaceName) != 0) {
            /* Make a temporary copy of pLogFont to not overwrite caller's
             * data. */
            LOGFONTW tmp;

            memcpy(&tmp, pLogFont, sizeof(LOGFONTW));
            wcsncpy(tmp.lfFaceName, default_fontface, LF_FACESIZE);

            for (i = 0; i < WD_SIZEOF_ARRAY(locales); i++) {
                font->tf = dwrite_create_text_format(locales[i], &tmp, &font->metrics);
                if (font->tf != NULL)
                    return (WD_HFONT)font;
            }
        }

        WD_TRACE("wdCreateFont: dwrite_create_text_format(%S, %S) failed.", pLogFont->lfFaceName, user_locale);
        free(font);
        return NULL;
    } else {
        HDC dc;
        c_GpFont* f;
        int status;

        dc = GetDC(NULL);
        status = gdix_vtable->fn_CreateFontFromLogfontW(dc, pLogFont, &f);
        if (status != 0) {
            LOGFONTW fallback_logfont;

            /* Failure: This may happen because GDI+ does not support
             * non-TrueType fonts. Fallback to default GUI font, typically
             * Tahoma or Segoe UI on newer Windows. */
            memcpy(&fallback_logfont, pLogFont, sizeof(LOGFONTW));
            wd_get_default_gui_fontface(fallback_logfont.lfFaceName);
            status = gdix_vtable->fn_CreateFontFromLogfontW(dc, &fallback_logfont, &f);
        }
        ReleaseDC(NULL, dc);

        if (status != 0) {
            WD_TRACE("wdCreateFont: GdipCreateFontFromLogfontW(%S) failed. [%d]", pLogFont->lfFaceName, status);
            return NULL;
        }

        return (WD_HFONT)f;
    }
}

WD_HFONT
wdCreateFontWithGdiHandle(HFONT hGdiFont) {
    LOGFONTW lf;

    if (hGdiFont == NULL)
        hGdiFont = GetStockObject(SYSTEM_FONT);

    GetObjectW(hGdiFont, sizeof(LOGFONTW), &lf);
    return wdCreateFont(&lf);
}

void wdDestroyFont(WD_HFONT hFont) {
    if (d2d_enabled()) {
        dwrite_font_t* font = (dwrite_font_t*)hFont;

        c_IDWriteTextFormat_Release(font->tf);
        free(font);
    } else {
        gdix_vtable->fn_DeleteFont((c_GpFont*)hFont);
    }
}

void wdFontMetrics(WD_HFONT hFont, WD_FONTMETRICS* pMetrics) {
    if (hFont == NULL) {
        /* Treat NULL as "no font". This simplifies paint code when font
         * creation fails. */
        WD_TRACE("wdFontMetrics: font == NULL");
        goto err;
    }

    if (d2d_enabled()) {
        dwrite_font_t* font = (dwrite_font_t*)hFont;
        float factor;

        pMetrics->fEmHeight = c_IDWriteTextFormat_GetFontSize(font->tf);

        factor = (pMetrics->fEmHeight / (float)font->metrics.designUnitsPerEm);

        pMetrics->fAscent = factor * (float)font->metrics.ascent;
        pMetrics->fDescent = factor * (float)WD_ABS(font->metrics.descent);
        pMetrics->fLeading =
            factor * (float)(font->metrics.ascent + WD_ABS(font->metrics.descent) + font->metrics.lineGap);
    } else {
        int font_style;
        float font_size;
        void* font_family;
        UINT16 cell_ascent;
        UINT16 cell_descent;
        UINT16 em_height;
        UINT16 line_spacing;
        int status;

        gdix_vtable->fn_GetFontSize((void*)hFont, &font_size);
        gdix_vtable->fn_GetFontStyle((void*)hFont, &font_style);

        status = gdix_vtable->fn_GetFamily((void*)hFont, &font_family);
        if (status != 0) {
            WD_TRACE("wdFontMetrics: GdipGetFamily() failed. [%d]", status);
            goto err;
        }
        gdix_vtable->fn_GetCellAscent(font_family, font_style, &cell_ascent);
        gdix_vtable->fn_GetCellDescent(font_family, font_style, &cell_descent);
        gdix_vtable->fn_GetEmHeight(font_family, font_style, &em_height);
        gdix_vtable->fn_GetLineSpacing(font_family, font_style, &line_spacing);
        gdix_vtable->fn_DeleteFontFamily(font_family);

        pMetrics->fEmHeight = font_size;
        pMetrics->fAscent = font_size * (float)cell_ascent / (float)em_height;
        pMetrics->fDescent = WD_ABS(font_size * (float)cell_descent / (float)em_height);
        pMetrics->fLeading = font_size * (float)line_spacing / (float)em_height;
    }

    return;

err:
    pMetrics->fEmHeight = 0.0f;
    pMetrics->fAscent = 0.0f;
    pMetrics->fDescent = 0.0f;
    pMetrics->fLeading = 0.0f;
}

/* -------------------------------------------- path.c --------------------------------- */


WD_HPATH
wdCreatePath(WD_HCANVAS hCanvas) {
    if (d2d_enabled()) {
        c_ID2D1PathGeometry* g;
        HRESULT hr;

        wd_lock();
        hr = c_ID2D1Factory_CreatePathGeometry(d2d_factory, &g);
        wd_unlock();
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreatePath: "
                "ID2D1Factory::CreatePathGeometry() failed.");
            return NULL;
        }

        return (WD_HPATH)g;
    } else {
        c_GpPath* p;
        int status;

        status = gdix_vtable->fn_CreatePath(c_FillModeAlternate, &p);
        if (status != 0) {
            WD_TRACE("wdCreatePath: GdipCreatePath() failed. [%d]", status);
            return NULL;
        }

        return (WD_HPATH)p;
    }
}

WD_HPATH
wdCreatePolygonPath(WD_HCANVAS hCanvas, const WD_POINT* pPoints, UINT uCount) {
    WD_HPATH p;

    p = wdCreatePath(hCanvas);
    if (p == NULL) {
        WD_TRACE("wdCreatePolygonPath: wdCreatePath() failed.");
        return NULL;
    }

    if (uCount > 0) {
        WD_PATHSINK sink;
        UINT i;

        if (!wdOpenPathSink(&sink, p)) {
            WD_TRACE("wdCreatePolygonPath: wdOpenPathSink() failed.");
            wdDestroyPath(p);
            return NULL;
        }

        wdBeginFigure(&sink, pPoints[0].x, pPoints[0].y);
        for (i = 1; i < uCount; i++)
            wdAddLine(&sink, pPoints[i].x, pPoints[i].y);
        wdEndFigure(&sink, TRUE);

        wdClosePathSink(&sink);
    }

    return p;
}

WD_HPATH
wdCreateRoundedRectPath(WD_HCANVAS hCanvas, const WD_RECT* prc, float r) {
    WD_HPATH p;
    WD_PATHSINK sink;
    float w_2, h_2;

    /* Adjust the radius according to the maximum size allowed */
    w_2 = (prc->x1 - prc->x0) / 2.f + 0.5f;
    h_2 = (prc->y1 - prc->y0) / 2.f + 0.5f;

    if (r > w_2)
        r = w_2;
    if (r > h_2)
        r = h_2;

    /* Create the path */
    p = wdCreatePath(hCanvas);
    if (p == NULL) {
        WD_TRACE("wdCreateRoundRectPath: wdCreatePath() failed.");
        return NULL;
    }

    if (!wdOpenPathSink(&sink, p)) {
        WD_TRACE("wdCreateRoundRectPath: wdOpenPathSink() failed.");
        wdDestroyPath(p);
        return NULL;
    }

    wdBeginFigure(&sink, prc->x0 + r, prc->y0);

    wdAddLine(&sink, prc->x1 - r, prc->y0);
    wdAddArc(&sink, prc->x1 - r, prc->y0 + r, 90.0f);
    wdAddLine(&sink, prc->x1, prc->y1 - r);
    wdAddArc(&sink, prc->x1 - r, prc->y1 - r, 90.0f);
    wdAddLine(&sink, prc->x0 + r, prc->y1);
    wdAddArc(&sink, prc->x0 + r, prc->y1 - r, 90.0f);
    wdAddLine(&sink, prc->x0, prc->y0 + r);
    wdAddArc(&sink, prc->x0 + r, prc->y0 + r, 90.0f);

    wdEndFigure(&sink, TRUE);
    wdClosePathSink(&sink);

    return p;
}

void wdDestroyPath(WD_HPATH hPath) {
    if (d2d_enabled()) {
        c_ID2D1PathGeometry_Release((c_ID2D1PathGeometry*)hPath);
    } else {
        gdix_vtable->fn_DeletePath((c_GpPath*)hPath);
    }
}

BOOL wdOpenPathSink(WD_PATHSINK* pSink, WD_HPATH hPath) {
    if (d2d_enabled()) {
        c_ID2D1PathGeometry* g = (c_ID2D1PathGeometry*)hPath;
        c_ID2D1GeometrySink* s;
        HRESULT hr;

        hr = c_ID2D1PathGeometry_Open(g, &s);
        if (FAILED(hr)) {
            WD_TRACE_HR("wdOpenPathSink: ID2D1PathGeometry::Open() failed.");
            return FALSE;
        }

        pSink->pData = (void*)s;
        return TRUE;
    } else {
        /* GDI+ doesn't have any concept of path sink as Direct2D does, it
         * operates directly with the path object. */
        pSink->pData = (void*)hPath;
        return TRUE;
    }
}

void wdClosePathSink(WD_PATHSINK* pSink) {
    if (d2d_enabled()) {
        c_ID2D1GeometrySink* s = (c_ID2D1GeometrySink*)pSink->pData;
        c_ID2D1GeometrySink_Close(s);
        c_ID2D1GeometrySink_Release(s);
    } else {
        /* noop */
    }
}

void wdBeginFigure(WD_PATHSINK* pSink, float x, float y) {
    if (d2d_enabled()) {
        c_ID2D1GeometrySink* s = (c_ID2D1GeometrySink*)pSink->pData;
        c_D2D1_POINT_2F pt = {x, y};

        c_ID2D1GeometrySink_BeginFigure(s, pt, c_D2D1_FIGURE_BEGIN_FILLED);
    } else {
        gdix_vtable->fn_StartPathFigure(pSink->pData);
    }

    pSink->ptEnd.x = x;
    pSink->ptEnd.y = y;
}

void wdEndFigure(WD_PATHSINK* pSink, BOOL bCloseFigure) {
    if (d2d_enabled()) {
        c_ID2D1GeometrySink_EndFigure((c_ID2D1GeometrySink*)pSink->pData,
                                      (bCloseFigure ? c_D2D1_FIGURE_END_CLOSED : c_D2D1_FIGURE_END_OPEN));
    } else {
        if (bCloseFigure)
            gdix_vtable->fn_ClosePathFigure(pSink->pData);
    }
}

void wdAddLine(WD_PATHSINK* pSink, float x, float y) {
    if (d2d_enabled()) {
        c_ID2D1GeometrySink* s = (c_ID2D1GeometrySink*)pSink->pData;
        c_D2D1_POINT_2F pt = {x, y};

        c_ID2D1GeometrySink_AddLine(s, pt);
    } else {
        gdix_vtable->fn_AddPathLine(pSink->pData, pSink->ptEnd.x, pSink->ptEnd.y, x, y);
    }

    pSink->ptEnd.x = x;
    pSink->ptEnd.y = y;
}

void wdAddArc(WD_PATHSINK* pSink, float cx, float cy, float fSweepAngle) {
    float ax = pSink->ptEnd.x;
    float ay = pSink->ptEnd.y;
    float xdiff = ax - cx;
    float ydiff = ay - cy;
    float r;
    float base_angle;

    r = sqrtf(xdiff * xdiff + ydiff * ydiff);

    /* Avoid undefined case for atan2f(). */
    if (r < 0.001f)
        return;

    base_angle = atan2f(ydiff, xdiff) * (180.0f / WD_PI);

    if (d2d_enabled()) {
        c_ID2D1GeometrySink* s = (c_ID2D1GeometrySink*)pSink->pData;
        c_D2D1_ARC_SEGMENT arc_seg;

        d2d_setup_arc_segment(&arc_seg, cx, cy, r, r, base_angle, fSweepAngle);
        c_ID2D1GeometrySink_AddArc(s, &arc_seg);
        pSink->ptEnd.x = arc_seg.point.x;
        pSink->ptEnd.y = arc_seg.point.y;
    } else {
        float d = 2.0f * r;
        float sweep_rads = (base_angle + fSweepAngle) * (WD_PI / 180.0f);

        gdix_vtable->fn_AddPathArc(pSink->pData, cx - r, cy - r, d, d, base_angle, fSweepAngle);
        pSink->ptEnd.x = cx + r * cosf(sweep_rads);
        pSink->ptEnd.y = cy + r * sinf(sweep_rads);
    }
}

void wdAddBezier(WD_PATHSINK* pSink, float x0, float y0, float x1, float y1, float x2, float y2) {
    if (d2d_enabled()) {
        c_ID2D1GeometrySink* s = (c_ID2D1GeometrySink*)pSink->pData;
        c_D2D1_BEZIER_SEGMENT bezier_seg;

        d2d_setup_bezier_segment(&bezier_seg, x0, y0, x1, y1, x2, y2);
        c_ID2D1GeometrySink_AddBezier(s, &bezier_seg);
    } else {
        gdix_vtable->fn_AddPathBezier(pSink->pData, pSink->ptEnd.x, pSink->ptEnd.y, x0, y0, x1, y1, x2, y2);
    }
    pSink->ptEnd.x = x2;
    pSink->ptEnd.y = y2;
}

/* ------------------------------------------ image.c ------------------------------- */


WD_HIMAGE
wdCreateImageFromHBITMAP(HBITMAP hBmp) {
    return wdCreateImageFromHBITMAPWithAlpha(hBmp, 0);
}

WD_HIMAGE
wdCreateImageFromHBITMAPWithAlpha(HBITMAP hBmp, int alphaMode) {
    if (d2d_enabled()) {
        IWICBitmap* bitmap;
        IWICBitmapSource* converted_bitmap;
        WICBitmapAlphaChannelOption alpha_option;
        HRESULT hr;

        if (wic_factory == NULL) {
            WD_TRACE("wdCreateImageFromHBITMAP: Image API disabled.");
            return NULL;
        }

        switch (alphaMode) {
            case WD_ALPHA_USE:
                alpha_option = WICBitmapUseAlpha;
                break;
            case WD_ALPHA_USE_PREMULTIPLIED:
                alpha_option = WICBitmapUsePremultipliedAlpha;
                break;
            case WD_ALPHA_IGNORE: /* Pass through. */
            default:
                alpha_option = WICBitmapIgnoreAlpha;
                break;
        }

        hr = IWICImagingFactory_CreateBitmapFromHBITMAP(wic_factory, hBmp, NULL, alpha_option, &bitmap);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateImageFromHBITMAP: "
                "IWICImagingFactory::CreateBitmapFromHBITMAP() failed.");
            return NULL;
        }

        converted_bitmap = wic_convert_bitmap((IWICBitmapSource*)bitmap);
        if (converted_bitmap == NULL)
            WD_TRACE("wdCreateImageFromHBITMAP: wic_convert_bitmap() failed.");

        IWICBitmap_Release(bitmap);

        return (WD_HIMAGE)converted_bitmap;
    } else {
        c_GpBitmap* b;

        if (alphaMode == WD_ALPHA_IGNORE) {
            int status;

            status = gdix_vtable->fn_CreateBitmapFromHBITMAP(hBmp, NULL, &b);
            if (status != 0) {
                WD_TRACE(
                    "wdCreateImageFromHBITMAP: "
                    "GdipCreateBitmapFromHBITMAP() failed. [%d]",
                    status);
                return NULL;
            }
        } else {
            /* GdipCreateBitmapFromHBITMAP() ignores alpha channel. We have to
             * do it manually. */
            b = gdix_bitmap_from_HBITMAP_with_alpha(hBmp, (alphaMode == WD_ALPHA_USE_PREMULTIPLIED));
        }

        return (WD_HIMAGE)b;
    }
}

WD_HIMAGE
wdLoadImageFromFile(const WCHAR* pszPath) {
    if (d2d_enabled()) {
        IWICBitmapDecoder* decoder;
        IWICBitmapFrameDecode* bitmap;
        IWICBitmapSource* converted_bitmap = NULL;
        HRESULT hr;

        if (wic_factory == NULL) {
            WD_TRACE("wdLoadImageFromFile: Image API disabled.");
            return NULL;
        }

        hr = IWICImagingFactory_CreateDecoderFromFilename(wic_factory, pszPath, NULL, GENERIC_READ,
                                                          WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdLoadImageFromFile: "
                "IWICImagingFactory::CreateDecoderFromFilename() failed.");
            goto err_CreateDecoderFromFilename;
        }

        hr = IWICBitmapDecoder_GetFrame(decoder, 0, &bitmap);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdLoadImageFromFile: "
                "IWICBitmapDecoder::GetFrame() failed.");
            goto err_GetFrame;
        }

        converted_bitmap = wic_convert_bitmap((IWICBitmapSource*)bitmap);
        if (converted_bitmap == NULL)
            WD_TRACE("wdLoadImageFromFile: wic_convert_bitmap() failed.");

        IWICBitmapFrameDecode_Release(bitmap);
    err_GetFrame:
        IWICBitmapDecoder_Release(decoder);
    err_CreateDecoderFromFilename:
        return (WD_HIMAGE)converted_bitmap;
    } else {
        c_GpImage* img;
        int status;

        status = gdix_vtable->fn_LoadImageFromFile(pszPath, &img);
        if (status != 0) {
            WD_TRACE(
                "wdLoadImageFromFile: "
                "GdipLoadImageFromFile() failed. [%d]",
                status);
            return NULL;
        }

        return (WD_HIMAGE)img;
    }
}

WD_HIMAGE
wdLoadImageFromIStream(IStream* pStream) {
    if (d2d_enabled()) {
        IWICBitmapDecoder* decoder;
        IWICBitmapFrameDecode* bitmap;
        IWICBitmapSource* converted_bitmap = NULL;
        HRESULT hr;

        if (wic_factory == NULL) {
            WD_TRACE("wdLoadImageFromIStream: Image API disabled.");
            return NULL;
        }

        hr = IWICImagingFactory_CreateDecoderFromStream(wic_factory, pStream, NULL, WICDecodeMetadataCacheOnLoad,
                                                        &decoder);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdLoadImageFromIStream: "
                "IWICImagingFactory::CreateDecoderFromFilename() failed.");
            goto err_CreateDecoderFromFilename;
        }

        hr = IWICBitmapDecoder_GetFrame(decoder, 0, &bitmap);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdLoadImageFromIStream: "
                "IWICBitmapDecoder::GetFrame() failed.");
            goto err_GetFrame;
        }

        converted_bitmap = wic_convert_bitmap((IWICBitmapSource*)bitmap);
        if (converted_bitmap == NULL)
            WD_TRACE("wdLoadImageFromIStream: wic_convert_bitmap() failed.");

        IWICBitmapFrameDecode_Release(bitmap);
    err_GetFrame:
        IWICBitmapDecoder_Release(decoder);
    err_CreateDecoderFromFilename:
        return (WD_HIMAGE)converted_bitmap;
    } else {
        c_GpImage* img;
        int status;

        status = gdix_vtable->fn_LoadImageFromStream(pStream, &img);
        if (status != 0) {
            WD_TRACE(
                "wdLoadImageFromIStream: "
                "GdipLoadImageFromFile() failed. [%d]",
                status);
            return NULL;
        }

        return (WD_HIMAGE)img;
    }
}

WD_HIMAGE
wdLoadImageFromResource(HINSTANCE hInstance, const WCHAR* pszResType, const WCHAR* pszResName) {
    IStream* stream;
    WD_HIMAGE img;
    HRESULT hr;

    hr = memstream_create_from_resource(hInstance, pszResType, pszResName, &stream);
    if (FAILED(hr)) {
        WD_TRACE_HR(
            "wdLoadImageFromResource: "
            "memstream_create_from_resource() failed.");
        return NULL;
    }

    img = wdLoadImageFromIStream(stream);
    if (img == NULL)
        WD_TRACE("wdLoadImageFromResource: wdLoadImageFromIStream() failed.");

    IStream_Release(stream);
    return img;
}

void wdDestroyImage(WD_HIMAGE hImage) {
    if (d2d_enabled()) {
        IWICBitmapSource_Release((IWICBitmapSource*)hImage);
    } else {
        gdix_vtable->fn_DisposeImage((c_GpImage*)hImage);
    }
}

void wdGetImageSize(WD_HIMAGE hImage, UINT* puWidth, UINT* puHeight) {
    if (d2d_enabled()) {
        UINT w, h;

        IWICBitmapSource_GetSize((IWICBitmapSource*)hImage, &w, &h);
        if (puWidth != NULL)
            *puWidth = w;
        if (puHeight != NULL)
            *puHeight = h;
    } else {
        if (puWidth != NULL)
            gdix_vtable->fn_GetImageWidth((c_GpImage*)hImage, puWidth);
        if (puHeight != NULL)
            gdix_vtable->fn_GetImageHeight((c_GpImage*)hImage, puHeight);
    }
}

#define RAW_BUFFER_FLAG_BOTTOMUP 0x0001
#define RAW_BUFFER_FLAG_HASALPHA 0x0002
#define RAW_BUFFER_FLAG_PREMULTIPLYALPHA 0x0004

static void raw_buffer_to_bitmap_data(UINT width, UINT height, BYTE* dst_buffer, int dst_stride,
                                      int dst_bytes_per_pixel, const BYTE* src_buffer, int src_stride,
                                      int src_bytes_per_pixel, int red_offset, int green_offset, int blue_offset,
                                      int alpha_offset, DWORD flags) {
    UINT x, y;
    BYTE* dst_line = dst_buffer;
    BYTE* dst;
    const BYTE* src_line = src_buffer;
    const BYTE* src;

    if (src_stride == 0)
        src_stride = width * src_bytes_per_pixel;

    if (flags & RAW_BUFFER_FLAG_BOTTOMUP) {
        src_line = src_buffer + (height - 1) * src_stride;
        src_stride = -src_stride;
    }

    for (y = 0; y < height; y++) {
        dst = dst_line;
        src = src_line;

        for (x = 0; x < width; x++) {
            dst[0] = src[blue_offset];
            dst[1] = src[green_offset];
            dst[2] = src[red_offset];

            if (dst_bytes_per_pixel >= 4) {
                dst[3] = (flags & RAW_BUFFER_FLAG_HASALPHA) ? src[alpha_offset] : 255;

                if (flags & RAW_BUFFER_FLAG_PREMULTIPLYALPHA) {
                    dst[0] = (dst[0] * dst[3]) / 255;
                    dst[1] = (dst[1] * dst[3]) / 255;
                    dst[2] = (dst[2] * dst[3]) / 255;
                }
            }

            dst += dst_bytes_per_pixel;
            src += src_bytes_per_pixel;
        }

        dst_line += dst_stride;
        src_line += src_stride;
    }
}

static void colormap_buffer_to_bitmap_data(UINT width, UINT height, BYTE* dst_buffer, int dst_stride,
                                           int dst_bytes_per_pixel, const BYTE* src_buffer, int src_stride,
                                           const COLORREF* palette, UINT palette_size) {
    UINT x, y;
    BYTE* dst_line = dst_buffer;
    BYTE* dst;
    const BYTE* src_line = src_buffer;
    const BYTE* src;

    if (src_stride == 0)
        src_stride = width;

    for (y = 0; y < height; y++) {
        dst = dst_line;
        src = src_line;

        for (x = 0; x < width; x++) {
            dst[0] = GetBValue(palette[*src]);
            dst[1] = GetGValue(palette[*src]);
            dst[2] = GetRValue(palette[*src]);

            if (dst_bytes_per_pixel >= 4)
                dst[3] = 0xff;

            dst += dst_bytes_per_pixel;
            src++;
        }

        dst_line += dst_stride;
        src_line += src_stride;
    }
}

WD_HIMAGE
wdCreateImageFromBuffer(UINT uWidth, UINT uHeight, UINT srcStride, const BYTE* pBuffer, int pixelFormat,
                        const COLORREF* cPalette, UINT uPaletteSize) {
    WD_HIMAGE b = NULL;
    BYTE* scan0 = NULL;
    UINT dstStride = 0;
    IWICBitmapLock* bitmap_lock = NULL;
    c_GpBitmapData bitmapData;

    if (d2d_enabled()) {
        IWICBitmap* bitmap = NULL;
        HRESULT hr;
        WICRect rect = {0, 0, uWidth, uHeight};
        UINT cbBufferSize = 0;

        if (wic_factory == NULL) {
            WD_TRACE("wdCreateImageFromBuffer: Image API disabled.");
            return NULL;
        }

        /* wic_pixel_format is GUID_WICPixelFormat32bppPBGRA;
         * i.e. pre-multiplied alpha, BGRA order */
        hr = IWICImagingFactory_CreateBitmap(wic_factory, uWidth, uHeight, &wic_pixel_format, WICBitmapCacheOnDemand,
                                             &bitmap);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateImageFromBuffer: "
                "IWICImagingFactory::CreateBitmap() failed.");
            return NULL;
        }

        hr = IWICBitmap_Lock(bitmap, &rect, WICBitmapLockWrite, &bitmap_lock);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateImageFromBuffer: "
                "IWICBitmap::Lock() failed.");
            IWICBitmap_Release(bitmap);
            return NULL;
        }

        IWICBitmapLock_GetStride(bitmap_lock, &dstStride);
        IWICBitmapLock_GetDataPointer(bitmap_lock, &cbBufferSize, &scan0);
        b = (WD_HIMAGE)bitmap;
    } else {
        c_GpPixelFormat format;
        int status;
        c_GpBitmap* bitmap = NULL;
        c_GpRectI rect = {0, 0, uWidth, uHeight};

        if (pixelFormat == WD_PIXELFORMAT_R8G8B8 || pixelFormat == WD_PIXELFORMAT_PALETTE)
            format = c_PixelFormat24bppRGB;
        else if (pixelFormat == WD_PIXELFORMAT_R8G8B8A8)
            format = c_PixelFormat32bppARGB;
        else
            format = c_PixelFormat32bppPARGB;

        status = gdix_vtable->fn_CreateBitmapFromScan0(uWidth, uHeight, 0, format, NULL, &bitmap);
        if (status != 0) {
            WD_TRACE(
                "wdCreateImageFromBuffer: "
                "GdipCreateBitmapFromScan0() failed. [%d]",
                status);
            return NULL;
        }

        gdix_vtable->fn_BitmapLockBits(bitmap, &rect, c_ImageLockModeWrite, format, &bitmapData);

        scan0 = (BYTE*)bitmapData.Scan0;
        dstStride = bitmapData.Stride;
        b = (WD_HIMAGE)bitmap;
    }

    switch (pixelFormat) {
        case WD_PIXELFORMAT_PALETTE:
            colormap_buffer_to_bitmap_data(uWidth, uHeight, scan0, dstStride, 4, pBuffer, srcStride, cPalette,
                                           uPaletteSize);
            break;

        case WD_PIXELFORMAT_R8G8B8:
            raw_buffer_to_bitmap_data(uWidth, uHeight, scan0, dstStride, 4, pBuffer, srcStride, 3, 0, 1, 2, 0, 0);
            break;

        case WD_PIXELFORMAT_R8G8B8A8:
            raw_buffer_to_bitmap_data(uWidth, uHeight, scan0, dstStride, 4, pBuffer, srcStride, 4, 0, 1, 2, 3,
                                      RAW_BUFFER_FLAG_HASALPHA | RAW_BUFFER_FLAG_PREMULTIPLYALPHA);
            break;

        case WD_PIXELFORMAT_B8G8R8A8:
            raw_buffer_to_bitmap_data(
                uWidth, uHeight, scan0, dstStride, 4, pBuffer, srcStride, 4, 2, 1, 0, 3,
                RAW_BUFFER_FLAG_HASALPHA | RAW_BUFFER_FLAG_PREMULTIPLYALPHA | RAW_BUFFER_FLAG_BOTTOMUP);
            break;

        case WD_PIXELFORMAT_B8G8R8A8_PREMULTIPLIED:
            raw_buffer_to_bitmap_data(uWidth, uHeight, scan0, dstStride, 4, pBuffer, srcStride, 4, 2, 1, 0, 3,
                                      RAW_BUFFER_FLAG_HASALPHA | RAW_BUFFER_FLAG_BOTTOMUP);
            break;
    }

    if (d2d_enabled()) {
        IWICBitmapLock_Release(bitmap_lock);
    } else {
        gdix_vtable->fn_BitmapUnlockBits((c_GpBitmap*)b, &bitmapData);
    }

    return b;
}

/* ----------------------------------- cachedimage.c -------------------------- */

WD_HCACHEDIMAGE
wdCreateCachedImage(WD_HCANVAS hCanvas, WD_HIMAGE hImage) {
    if (d2d_enabled()) {
        d2d_canvas_t* c = (d2d_canvas_t*)hCanvas;
        c_ID2D1Bitmap* b;
        HRESULT hr;

        hr = c_ID2D1RenderTarget_CreateBitmapFromWicBitmap(c->target, (IWICBitmapSource*)hImage, NULL, &b);
        if (FAILED(hr)) {
            WD_TRACE_HR(
                "wdCreateCachedImage: "
                "ID2D1RenderTarget::CreateBitmapFromWicBitmap() failed.");
            return NULL;
        }

        return (WD_HCACHEDIMAGE)b;
    } else {
        gdix_canvas_t* c = (gdix_canvas_t*)hCanvas;
        c_GpCachedBitmap* cb;
        int status;

        status = gdix_vtable->fn_CreateCachedBitmap((c_GpImage*)hImage, c->graphics, &cb);
        if (status != 0) {
            WD_TRACE(
                "wdCreateCachedImage: "
                "GdipCreateCachedBitmap() failed. [%d]",
                status);
            return NULL;
        }

        return (WD_HCACHEDIMAGE)cb;
    }
}

void wdDestroyCachedImage(WD_HCACHEDIMAGE hCachedImage) {
    if (d2d_enabled()) {
        c_ID2D1Bitmap_Release((c_ID2D1Bitmap*)hCachedImage);
    } else {
        gdix_vtable->fn_DeleteCachedBitmap((c_GpCachedBitmap*)hCachedImage);
    }
}
