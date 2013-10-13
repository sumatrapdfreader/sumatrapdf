--proc/wingdi: windows GDI API.
setfenv(1, require'winapi')
require'winapi.winuser'

-- from gdiplusgraphics.h

FlatnessDefault = 1.0/4.0

ffi.cdef[[
typedef UINT     GraphicsState;
typedef UINT     GraphicsContainer;

enum FillMode
{
    FillModeAlternate,        // 0
    FillModeWinding           // 1
};

enum QualityMode
{
    QualityModeInvalid   = -1,
    QualityModeDefault   = 0,
    QualityModeLow       = 1, // Best performance
    QualityModeHigh      = 2  // Best rendering quality
};

enum CompositingMode
{
    CompositingModeSourceOver,    // 0
    CompositingModeSourceCopy     // 1
};

enum CompositingQuality
{
    CompositingQualityInvalid          = QualityModeInvalid,
    CompositingQualityDefault          = QualityModeDefault,
    CompositingQualityHighSpeed        = QualityModeLow,
    CompositingQualityHighQuality      = QualityModeHigh,
    CompositingQualityGammaCorrected,
    CompositingQualityAssumeLinear
};

enum Unit
{
    UnitWorld,      // 0 -- World coordinate (non-physical unit)
    UnitDisplay,    // 1 -- Variable -- for PageTransform only
    UnitPixel,      // 2 -- Each unit is one device pixel.
    UnitPoint,      // 3 -- Each unit is a printer's point, or 1/72 inch.
    UnitInch,       // 4 -- Each unit is 1 inch.
    UnitDocument,   // 5 -- Each unit is 1/300 inch.
    UnitMillimeter  // 6 -- Each unit is 1 millimeter.
};

enum MetafileFrameUnit
{
    MetafileFrameUnitPixel      = UnitPixel,
    MetafileFrameUnitPoint      = UnitPoint,
    MetafileFrameUnitInch       = UnitInch,
    MetafileFrameUnitDocument   = UnitDocument,
    MetafileFrameUnitMillimeter = UnitMillimeter,
    MetafileFrameUnitGdi                        // GDI compatible .01 MM units
};

enum CoordinateSpace
{
    CoordinateSpaceWorld,     // 0
    CoordinateSpacePage,      // 1
    CoordinateSpaceDevice     // 2
};

enum WrapMode
{
    WrapModeTile,        // 0
    WrapModeTileFlipX,   // 1
    WrapModeTileFlipY,   // 2
    WrapModeTileFlipXY,  // 3
    WrapModeClamp        // 4
};

enum HatchStyle
{
    HatchStyleHorizontal,                   // 0
    HatchStyleVertical,                     // 1
    HatchStyleForwardDiagonal,              // 2
    HatchStyleBackwardDiagonal,             // 3
    HatchStyleCross,                        // 4
    HatchStyleDiagonalCross,                // 5
    HatchStyle05Percent,                    // 6
    HatchStyle10Percent,                    // 7
    HatchStyle20Percent,                    // 8
    HatchStyle25Percent,                    // 9
    HatchStyle30Percent,                    // 10
    HatchStyle40Percent,                    // 11
    HatchStyle50Percent,                    // 12
    HatchStyle60Percent,                    // 13
    HatchStyle70Percent,                    // 14
    HatchStyle75Percent,                    // 15
    HatchStyle80Percent,                    // 16
    HatchStyle90Percent,                    // 17
    HatchStyleLightDownwardDiagonal,        // 18
    HatchStyleLightUpwardDiagonal,          // 19
    HatchStyleDarkDownwardDiagonal,         // 20
    HatchStyleDarkUpwardDiagonal,           // 21
    HatchStyleWideDownwardDiagonal,         // 22
    HatchStyleWideUpwardDiagonal,           // 23
    HatchStyleLightVertical,                // 24
    HatchStyleLightHorizontal,              // 25
    HatchStyleNarrowVertical,               // 26
    HatchStyleNarrowHorizontal,             // 27
    HatchStyleDarkVertical,                 // 28
    HatchStyleDarkHorizontal,               // 29
    HatchStyleDashedDownwardDiagonal,       // 30
    HatchStyleDashedUpwardDiagonal,         // 31
    HatchStyleDashedHorizontal,             // 32
    HatchStyleDashedVertical,               // 33
    HatchStyleSmallConfetti,                // 34
    HatchStyleLargeConfetti,                // 35
    HatchStyleZigZag,                       // 36
    HatchStyleWave,                         // 37
    HatchStyleDiagonalBrick,                // 38
    HatchStyleHorizontalBrick,              // 39
    HatchStyleWeave,                        // 40
    HatchStylePlaid,                        // 41
    HatchStyleDivot,                        // 42
    HatchStyleDottedGrid,                   // 43
    HatchStyleDottedDiamond,                // 44
    HatchStyleShingle,                      // 45
    HatchStyleTrellis,                      // 46
    HatchStyleSphere,                       // 47
    HatchStyleSmallGrid,                    // 48
    HatchStyleSmallCheckerBoard,            // 49
    HatchStyleLargeCheckerBoard,            // 50
    HatchStyleOutlinedDiamond,              // 51
    HatchStyleSolidDiamond,                 // 52

    HatchStyleTotal,
    HatchStyleLargeGrid = HatchStyleCross,  // 4

    HatchStyleMin       = HatchStyleHorizontal,
    HatchStyleMax       = HatchStyleTotal - 1,
};

enum DashStyle
{
    DashStyleSolid,          // 0
    DashStyleDash,           // 1
    DashStyleDot,            // 2
    DashStyleDashDot,        // 3
    DashStyleDashDotDot,     // 4
    DashStyleCustom          // 5
};

enum DashCap
{
    DashCapFlat             = 0,
    DashCapRound            = 2,
    DashCapTriangle         = 3
};

enum LineCap
{
    LineCapFlat             = 0,
    LineCapSquare           = 1,
    LineCapRound            = 2,
    LineCapTriangle         = 3,

    LineCapNoAnchor         = 0x10, // corresponds to flat cap
    LineCapSquareAnchor     = 0x11, // corresponds to square cap
    LineCapRoundAnchor      = 0x12, // corresponds to round cap
    LineCapDiamondAnchor    = 0x13, // corresponds to triangle cap
    LineCapArrowAnchor      = 0x14, // no correspondence

    LineCapCustom           = 0xff, // custom cap

    LineCapAnchorMask       = 0xf0  // mask to check for anchor or not.
};

enum CustomLineCapType
{
    CustomLineCapTypeDefault         = 0,
    CustomLineCapTypeAdjustableArrow = 1
};

enum LineJoin
{
    LineJoinMiter        = 0,
    LineJoinBevel        = 1,
    LineJoinRound        = 2,
    LineJoinMiterClipped = 3
};

enum PathPointType
{
    PathPointTypeStart           = 0,    // move
    PathPointTypeLine            = 1,    // line
    PathPointTypeBezier          = 3,    // default Bezier (= cubic Bezier)
    PathPointTypePathTypeMask    = 0x07, // type mask (lowest 3 bits).
    PathPointTypeDashMode        = 0x10, // currently in dash mode.
    PathPointTypePathMarker      = 0x20, // a marker for the path.
    PathPointTypeCloseSubpath    = 0x80, // closed flag

    // Path types used for advanced path.

    PathPointTypeBezier3    = 3,         // cubic Bezier
};

enum WarpMode
{
    WarpModePerspective,    // 0
    WarpModeBilinear        // 1
};

enum LinearGradientMode
{
    LinearGradientModeHorizontal,         // 0
    LinearGradientModeVertical,           // 1
    LinearGradientModeForwardDiagonal,    // 2
    LinearGradientModeBackwardDiagonal    // 3
};

enum CombineMode
{
    CombineModeReplace,     // 0
    CombineModeIntersect,   // 1
    CombineModeUnion,       // 2
    CombineModeXor,         // 3
    CombineModeExclude,     // 4
    CombineModeComplement   // 5 (Exclude From)
};

enum ImageType
{
    ImageTypeUnknown,   // 0
    ImageTypeBitmap,    // 1
    ImageTypeMetafile   // 2
};

enum InterpolationMode
{
    InterpolationModeInvalid          = QualityModeInvalid,
    InterpolationModeDefault          = QualityModeDefault,
    InterpolationModeLowQuality       = QualityModeLow,
    InterpolationModeHighQuality      = QualityModeHigh,
    InterpolationModeBilinear,
    InterpolationModeBicubic,
    InterpolationModeNearestNeighbor,
    InterpolationModeHighQualityBilinear,
    InterpolationModeHighQualityBicubic
};

enum PenAlignment
{
    PenAlignmentCenter       = 0,
    PenAlignmentInset        = 1
};

enum BrushType
{
   BrushTypeSolidColor       = 0,
   BrushTypeHatchFill        = 1,
   BrushTypeTextureFill      = 2,
   BrushTypePathGradient     = 3,
   BrushTypeLinearGradient   = 4
};

enum PenType
{
   PenTypeSolidColor       = BrushTypeSolidColor,
   PenTypeHatchFill        = BrushTypeHatchFill,
   PenTypeTextureFill      = BrushTypeTextureFill,
   PenTypePathGradient     = BrushTypePathGradient,
   PenTypeLinearGradient   = BrushTypeLinearGradient,
   PenTypeUnknown          = -1
};

enum MatrixOrder
{
    MatrixOrderPrepend    = 0,
    MatrixOrderAppend     = 1
};

enum GenericFontFamily
{
    GenericFontFamilySerif,
    GenericFontFamilySansSerif,
    GenericFontFamilyMonospace
};

enum FontStyle
{
    FontStyleRegular    = 0,
    FontStyleBold       = 1,
    FontStyleItalic     = 2,
    FontStyleBoldItalic = 3,
    FontStyleUnderline  = 4,
    FontStyleStrikeout  = 8
};

enum SmoothingMode
{
    SmoothingModeInvalid     = QualityModeInvalid,
    SmoothingModeDefault     = QualityModeDefault,
    SmoothingModeHighSpeed   = QualityModeLow,
    SmoothingModeHighQuality = QualityModeHigh,
    SmoothingModeNone,
    SmoothingModeAntiAlias,
/* TODO: make this work
#if (GDIPVER >= 0x0110)
    SmoothingModeAntiAlias8x4 = SmoothingModeAntiAlias,
    SmoothingModeAntiAlias8x8
#endif //(GDIPVER >= 0x0110)
*/
};

enum PixelOffsetMode
{
    PixelOffsetModeInvalid     = QualityModeInvalid,
    PixelOffsetModeDefault     = QualityModeDefault,
    PixelOffsetModeHighSpeed   = QualityModeLow,
    PixelOffsetModeHighQuality = QualityModeHigh,
    PixelOffsetModeNone,    // No pixel offset
    PixelOffsetModeHalf     // Offset by -0.5, -0.5 for fast anti-alias perf
};

enum TextRenderingHint
{
    TextRenderingHintSystemDefault = 0,            // Glyph with system default rendering hint
    TextRenderingHintSingleBitPerPixelGridFit,     // Glyph bitmap with hinting
    TextRenderingHintSingleBitPerPixel,            // Glyph bitmap without hinting
    TextRenderingHintAntiAliasGridFit,             // Glyph anti-alias bitmap with hinting
    TextRenderingHintAntiAlias,                    // Glyph anti-alias bitmap without hinting
    TextRenderingHintClearTypeGridFit              // Glyph CT bitmap with hinting
};

enum MetafileType
{
    MetafileTypeInvalid,            // Invalid metafile
    MetafileTypeWmf,                // Standard WMF
    MetafileTypeWmfPlaceable,       // Placeable WMF
    MetafileTypeEmf,                // EMF (not EMF+)
    MetafileTypeEmfPlusOnly,        // EMF+ without dual, down-level records
    MetafileTypeEmfPlusDual         // EMF+ with dual, down-level records
};

enum EmfType
{
    EmfTypeEmfOnly     = MetafileTypeEmf,          // no EMF+, only EMF
    EmfTypeEmfPlusOnly = MetafileTypeEmfPlusOnly,  // no EMF, only EMF+
    EmfTypeEmfPlusDual = MetafileTypeEmfPlusDual   // both EMF+ and EMF
};

enum ObjectType
{
    ObjectTypeInvalid,
    ObjectTypeBrush,
    ObjectTypePen,
    ObjectTypePath,
    ObjectTypeRegion,
    ObjectTypeImage,
    ObjectTypeFont,
    ObjectTypeStringFormat,
    ObjectTypeImageAttributes,
    ObjectTypeCustomLineCap,
/* TODO: make this work
#if (GDIPVER >= 0x0110)
    ObjectTypeGraphics,

    ObjectTypeMax = ObjectTypeGraphics,
#else
    ObjectTypeMax = ObjectTypeCustomLineCap,
#endif //(GDIPVER >= 0x0110)
*/
    ObjectTypeMin = ObjectTypeBrush
};

/* TODO: fix this
#define GDIP_EMFPLUS_RECORD_BASE        0x00004000
#define GDIP_WMF_RECORD_BASE            0x00010000
#define GDIP_WMF_RECORD_TO_EMFPLUS(n)   ((EmfPlusRecordType)((n) | GDIP_WMF_RECORD_BASE))
#define GDIP_EMFPLUS_RECORD_TO_WMF(n)   ((n) & (~GDIP_WMF_RECORD_BASE))
#define GDIP_IS_WMF_RECORDTYPE(n)       (((n) & GDIP_WMF_RECORD_BASE) != 0)
*/

/* TODO: fix this
enum EmfPlusRecordType
{
   // Since we have to enumerate GDI records right along with GDI+ records,
   // We list all the GDI records here so that they can be part of the
   // same enumeration type which is used in the enumeration callback.

    WmfRecordTypeSetBkColor              = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETBKCOLOR),
    WmfRecordTypeSetBkMode               = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETBKMODE),
    WmfRecordTypeSetMapMode              = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETMAPMODE),
    WmfRecordTypeSetROP2                 = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETROP2),
    WmfRecordTypeSetRelAbs               = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETRELABS),
    WmfRecordTypeSetPolyFillMode         = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETPOLYFILLMODE),
    WmfRecordTypeSetStretchBltMode       = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETSTRETCHBLTMODE),
    WmfRecordTypeSetTextCharExtra        = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETTEXTCHAREXTRA),
    WmfRecordTypeSetTextColor            = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETTEXTCOLOR),
    WmfRecordTypeSetTextJustification    = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETTEXTJUSTIFICATION),
    WmfRecordTypeSetWindowOrg            = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETWINDOWORG),
    WmfRecordTypeSetWindowExt            = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETWINDOWEXT),
    WmfRecordTypeSetViewportOrg          = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETVIEWPORTORG),
    WmfRecordTypeSetViewportExt          = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETVIEWPORTEXT),
    WmfRecordTypeOffsetWindowOrg         = GDIP_WMF_RECORD_TO_EMFPLUS(META_OFFSETWINDOWORG),
    WmfRecordTypeScaleWindowExt          = GDIP_WMF_RECORD_TO_EMFPLUS(META_SCALEWINDOWEXT),
    WmfRecordTypeOffsetViewportOrg       = GDIP_WMF_RECORD_TO_EMFPLUS(META_OFFSETVIEWPORTORG),
    WmfRecordTypeScaleViewportExt        = GDIP_WMF_RECORD_TO_EMFPLUS(META_SCALEVIEWPORTEXT),
    WmfRecordTypeLineTo                  = GDIP_WMF_RECORD_TO_EMFPLUS(META_LINETO),
    WmfRecordTypeMoveTo                  = GDIP_WMF_RECORD_TO_EMFPLUS(META_MOVETO),
    WmfRecordTypeExcludeClipRect         = GDIP_WMF_RECORD_TO_EMFPLUS(META_EXCLUDECLIPRECT),
    WmfRecordTypeIntersectClipRect       = GDIP_WMF_RECORD_TO_EMFPLUS(META_INTERSECTCLIPRECT),
    WmfRecordTypeArc                     = GDIP_WMF_RECORD_TO_EMFPLUS(META_ARC),
    WmfRecordTypeEllipse                 = GDIP_WMF_RECORD_TO_EMFPLUS(META_ELLIPSE),
    WmfRecordTypeFloodFill               = GDIP_WMF_RECORD_TO_EMFPLUS(META_FLOODFILL),
    WmfRecordTypePie                     = GDIP_WMF_RECORD_TO_EMFPLUS(META_PIE),
    WmfRecordTypeRectangle               = GDIP_WMF_RECORD_TO_EMFPLUS(META_RECTANGLE),
    WmfRecordTypeRoundRect               = GDIP_WMF_RECORD_TO_EMFPLUS(META_ROUNDRECT),
    WmfRecordTypePatBlt                  = GDIP_WMF_RECORD_TO_EMFPLUS(META_PATBLT),
    WmfRecordTypeSaveDC                  = GDIP_WMF_RECORD_TO_EMFPLUS(META_SAVEDC),
    WmfRecordTypeSetPixel                = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETPIXEL),
    WmfRecordTypeOffsetClipRgn           = GDIP_WMF_RECORD_TO_EMFPLUS(META_OFFSETCLIPRGN),
    WmfRecordTypeTextOut                 = GDIP_WMF_RECORD_TO_EMFPLUS(META_TEXTOUT),
    WmfRecordTypeBitBlt                  = GDIP_WMF_RECORD_TO_EMFPLUS(META_BITBLT),
    WmfRecordTypeStretchBlt              = GDIP_WMF_RECORD_TO_EMFPLUS(META_STRETCHBLT),
    WmfRecordTypePolygon                 = GDIP_WMF_RECORD_TO_EMFPLUS(META_POLYGON),
    WmfRecordTypePolyline                = GDIP_WMF_RECORD_TO_EMFPLUS(META_POLYLINE),
    WmfRecordTypeEscape                  = GDIP_WMF_RECORD_TO_EMFPLUS(META_ESCAPE),
    WmfRecordTypeRestoreDC               = GDIP_WMF_RECORD_TO_EMFPLUS(META_RESTOREDC),
    WmfRecordTypeFillRegion              = GDIP_WMF_RECORD_TO_EMFPLUS(META_FILLREGION),
    WmfRecordTypeFrameRegion             = GDIP_WMF_RECORD_TO_EMFPLUS(META_FRAMEREGION),
    WmfRecordTypeInvertRegion            = GDIP_WMF_RECORD_TO_EMFPLUS(META_INVERTREGION),
    WmfRecordTypePaintRegion             = GDIP_WMF_RECORD_TO_EMFPLUS(META_PAINTREGION),
    WmfRecordTypeSelectClipRegion        = GDIP_WMF_RECORD_TO_EMFPLUS(META_SELECTCLIPREGION),
    WmfRecordTypeSelectObject            = GDIP_WMF_RECORD_TO_EMFPLUS(META_SELECTOBJECT),
    WmfRecordTypeSetTextAlign            = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETTEXTALIGN),
    WmfRecordTypeDrawText                = GDIP_WMF_RECORD_TO_EMFPLUS(0x062F),  // META_DRAWTEXT
    WmfRecordTypeChord                   = GDIP_WMF_RECORD_TO_EMFPLUS(META_CHORD),
    WmfRecordTypeSetMapperFlags          = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETMAPPERFLAGS),
    WmfRecordTypeExtTextOut              = GDIP_WMF_RECORD_TO_EMFPLUS(META_EXTTEXTOUT),
    WmfRecordTypeSetDIBToDev             = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETDIBTODEV),
    WmfRecordTypeSelectPalette           = GDIP_WMF_RECORD_TO_EMFPLUS(META_SELECTPALETTE),
    WmfRecordTypeRealizePalette          = GDIP_WMF_RECORD_TO_EMFPLUS(META_REALIZEPALETTE),
    WmfRecordTypeAnimatePalette          = GDIP_WMF_RECORD_TO_EMFPLUS(META_ANIMATEPALETTE),
    WmfRecordTypeSetPalEntries           = GDIP_WMF_RECORD_TO_EMFPLUS(META_SETPALENTRIES),
    WmfRecordTypePolyPolygon             = GDIP_WMF_RECORD_TO_EMFPLUS(META_POLYPOLYGON),
    WmfRecordTypeResizePalette           = GDIP_WMF_RECORD_TO_EMFPLUS(META_RESIZEPALETTE),
    WmfRecordTypeDIBBitBlt               = GDIP_WMF_RECORD_TO_EMFPLUS(META_DIBBITBLT),
    WmfRecordTypeDIBStretchBlt           = GDIP_WMF_RECORD_TO_EMFPLUS(META_DIBSTRETCHBLT),
    WmfRecordTypeDIBCreatePatternBrush   = GDIP_WMF_RECORD_TO_EMFPLUS(META_DIBCREATEPATTERNBRUSH),
    WmfRecordTypeStretchDIB              = GDIP_WMF_RECORD_TO_EMFPLUS(META_STRETCHDIB),
    WmfRecordTypeExtFloodFill            = GDIP_WMF_RECORD_TO_EMFPLUS(META_EXTFLOODFILL),
    WmfRecordTypeSetLayout               = GDIP_WMF_RECORD_TO_EMFPLUS(0x0149),  // META_SETLAYOUT
    WmfRecordTypeResetDC                 = GDIP_WMF_RECORD_TO_EMFPLUS(0x014C),  // META_RESETDC
    WmfRecordTypeStartDoc                = GDIP_WMF_RECORD_TO_EMFPLUS(0x014D),  // META_STARTDOC
    WmfRecordTypeStartPage               = GDIP_WMF_RECORD_TO_EMFPLUS(0x004F),  // META_STARTPAGE
    WmfRecordTypeEndPage                 = GDIP_WMF_RECORD_TO_EMFPLUS(0x0050),  // META_ENDPAGE
    WmfRecordTypeAbortDoc                = GDIP_WMF_RECORD_TO_EMFPLUS(0x0052),  // META_ABORTDOC
    WmfRecordTypeEndDoc                  = GDIP_WMF_RECORD_TO_EMFPLUS(0x005E),  // META_ENDDOC
    WmfRecordTypeDeleteObject            = GDIP_WMF_RECORD_TO_EMFPLUS(META_DELETEOBJECT),
    WmfRecordTypeCreatePalette           = GDIP_WMF_RECORD_TO_EMFPLUS(META_CREATEPALETTE),
    WmfRecordTypeCreateBrush             = GDIP_WMF_RECORD_TO_EMFPLUS(0x00F8),  // META_CREATEBRUSH
    WmfRecordTypeCreatePatternBrush      = GDIP_WMF_RECORD_TO_EMFPLUS(META_CREATEPATTERNBRUSH),
    WmfRecordTypeCreatePenIndirect       = GDIP_WMF_RECORD_TO_EMFPLUS(META_CREATEPENINDIRECT),
    WmfRecordTypeCreateFontIndirect      = GDIP_WMF_RECORD_TO_EMFPLUS(META_CREATEFONTINDIRECT),
    WmfRecordTypeCreateBrushIndirect     = GDIP_WMF_RECORD_TO_EMFPLUS(META_CREATEBRUSHINDIRECT),
    WmfRecordTypeCreateBitmapIndirect    = GDIP_WMF_RECORD_TO_EMFPLUS(0x02FD),  // META_CREATEBITMAPINDIRECT
    WmfRecordTypeCreateBitmap            = GDIP_WMF_RECORD_TO_EMFPLUS(0x06FE),  // META_CREATEBITMAP
    WmfRecordTypeCreateRegion            = GDIP_WMF_RECORD_TO_EMFPLUS(META_CREATEREGION),

    EmfRecordTypeHeader                  = EMR_HEADER,
    EmfRecordTypePolyBezier              = EMR_POLYBEZIER,
    EmfRecordTypePolygon                 = EMR_POLYGON,
    EmfRecordTypePolyline                = EMR_POLYLINE,
    EmfRecordTypePolyBezierTo            = EMR_POLYBEZIERTO,
    EmfRecordTypePolyLineTo              = EMR_POLYLINETO,
    EmfRecordTypePolyPolyline            = EMR_POLYPOLYLINE,
    EmfRecordTypePolyPolygon             = EMR_POLYPOLYGON,
    EmfRecordTypeSetWindowExtEx          = EMR_SETWINDOWEXTEX,
    EmfRecordTypeSetWindowOrgEx          = EMR_SETWINDOWORGEX,
    EmfRecordTypeSetViewportExtEx        = EMR_SETVIEWPORTEXTEX,
    EmfRecordTypeSetViewportOrgEx        = EMR_SETVIEWPORTORGEX,
    EmfRecordTypeSetBrushOrgEx           = EMR_SETBRUSHORGEX,
    EmfRecordTypeEOF                     = EMR_EOF,
    EmfRecordTypeSetPixelV               = EMR_SETPIXELV,
    EmfRecordTypeSetMapperFlags          = EMR_SETMAPPERFLAGS,
    EmfRecordTypeSetMapMode              = EMR_SETMAPMODE,
    EmfRecordTypeSetBkMode               = EMR_SETBKMODE,
    EmfRecordTypeSetPolyFillMode         = EMR_SETPOLYFILLMODE,
    EmfRecordTypeSetROP2                 = EMR_SETROP2,
    EmfRecordTypeSetStretchBltMode       = EMR_SETSTRETCHBLTMODE,
    EmfRecordTypeSetTextAlign            = EMR_SETTEXTALIGN,
    EmfRecordTypeSetColorAdjustment      = EMR_SETCOLORADJUSTMENT,
    EmfRecordTypeSetTextColor            = EMR_SETTEXTCOLOR,
    EmfRecordTypeSetBkColor              = EMR_SETBKCOLOR,
    EmfRecordTypeOffsetClipRgn           = EMR_OFFSETCLIPRGN,
    EmfRecordTypeMoveToEx                = EMR_MOVETOEX,
    EmfRecordTypeSetMetaRgn              = EMR_SETMETARGN,
    EmfRecordTypeExcludeClipRect         = EMR_EXCLUDECLIPRECT,
    EmfRecordTypeIntersectClipRect       = EMR_INTERSECTCLIPRECT,
    EmfRecordTypeScaleViewportExtEx      = EMR_SCALEVIEWPORTEXTEX,
    EmfRecordTypeScaleWindowExtEx        = EMR_SCALEWINDOWEXTEX,
    EmfRecordTypeSaveDC                  = EMR_SAVEDC,
    EmfRecordTypeRestoreDC               = EMR_RESTOREDC,
    EmfRecordTypeSetWorldTransform       = EMR_SETWORLDTRANSFORM,
    EmfRecordTypeModifyWorldTransform    = EMR_MODIFYWORLDTRANSFORM,
    EmfRecordTypeSelectObject            = EMR_SELECTOBJECT,
    EmfRecordTypeCreatePen               = EMR_CREATEPEN,
    EmfRecordTypeCreateBrushIndirect     = EMR_CREATEBRUSHINDIRECT,
    EmfRecordTypeDeleteObject            = EMR_DELETEOBJECT,
    EmfRecordTypeAngleArc                = EMR_ANGLEARC,
    EmfRecordTypeEllipse                 = EMR_ELLIPSE,
    EmfRecordTypeRectangle               = EMR_RECTANGLE,
    EmfRecordTypeRoundRect               = EMR_ROUNDRECT,
    EmfRecordTypeArc                     = EMR_ARC,
    EmfRecordTypeChord                   = EMR_CHORD,
    EmfRecordTypePie                     = EMR_PIE,
    EmfRecordTypeSelectPalette           = EMR_SELECTPALETTE,
    EmfRecordTypeCreatePalette           = EMR_CREATEPALETTE,
    EmfRecordTypeSetPaletteEntries       = EMR_SETPALETTEENTRIES,
    EmfRecordTypeResizePalette           = EMR_RESIZEPALETTE,
    EmfRecordTypeRealizePalette          = EMR_REALIZEPALETTE,
    EmfRecordTypeExtFloodFill            = EMR_EXTFLOODFILL,
    EmfRecordTypeLineTo                  = EMR_LINETO,
    EmfRecordTypeArcTo                   = EMR_ARCTO,
    EmfRecordTypePolyDraw                = EMR_POLYDRAW,
    EmfRecordTypeSetArcDirection         = EMR_SETARCDIRECTION,
    EmfRecordTypeSetMiterLimit           = EMR_SETMITERLIMIT,
    EmfRecordTypeBeginPath               = EMR_BEGINPATH,
    EmfRecordTypeEndPath                 = EMR_ENDPATH,
    EmfRecordTypeCloseFigure             = EMR_CLOSEFIGURE,
    EmfRecordTypeFillPath                = EMR_FILLPATH,
    EmfRecordTypeStrokeAndFillPath       = EMR_STROKEANDFILLPATH,
    EmfRecordTypeStrokePath              = EMR_STROKEPATH,
    EmfRecordTypeFlattenPath             = EMR_FLATTENPATH,
    EmfRecordTypeWidenPath               = EMR_WIDENPATH,
    EmfRecordTypeSelectClipPath          = EMR_SELECTCLIPPATH,
    EmfRecordTypeAbortPath               = EMR_ABORTPATH,
    EmfRecordTypeReserved_069            = 69,  // Not Used
    EmfRecordTypeGdiComment              = EMR_GDICOMMENT,
    EmfRecordTypeFillRgn                 = EMR_FILLRGN,
    EmfRecordTypeFrameRgn                = EMR_FRAMERGN,
    EmfRecordTypeInvertRgn               = EMR_INVERTRGN,
    EmfRecordTypePaintRgn                = EMR_PAINTRGN,
    EmfRecordTypeExtSelectClipRgn        = EMR_EXTSELECTCLIPRGN,
    EmfRecordTypeBitBlt                  = EMR_BITBLT,
    EmfRecordTypeStretchBlt              = EMR_STRETCHBLT,
    EmfRecordTypeMaskBlt                 = EMR_MASKBLT,
    EmfRecordTypePlgBlt                  = EMR_PLGBLT,
    EmfRecordTypeSetDIBitsToDevice       = EMR_SETDIBITSTODEVICE,
    EmfRecordTypeStretchDIBits           = EMR_STRETCHDIBITS,
    EmfRecordTypeExtCreateFontIndirect   = EMR_EXTCREATEFONTINDIRECTW,
    EmfRecordTypeExtTextOutA             = EMR_EXTTEXTOUTA,
    EmfRecordTypeExtTextOutW             = EMR_EXTTEXTOUTW,
    EmfRecordTypePolyBezier16            = EMR_POLYBEZIER16,
    EmfRecordTypePolygon16               = EMR_POLYGON16,
    EmfRecordTypePolyline16              = EMR_POLYLINE16,
    EmfRecordTypePolyBezierTo16          = EMR_POLYBEZIERTO16,
    EmfRecordTypePolylineTo16            = EMR_POLYLINETO16,
    EmfRecordTypePolyPolyline16          = EMR_POLYPOLYLINE16,
    EmfRecordTypePolyPolygon16           = EMR_POLYPOLYGON16,
    EmfRecordTypePolyDraw16              = EMR_POLYDRAW16,
    EmfRecordTypeCreateMonoBrush         = EMR_CREATEMONOBRUSH,
    EmfRecordTypeCreateDIBPatternBrushPt = EMR_CREATEDIBPATTERNBRUSHPT,
    EmfRecordTypeExtCreatePen            = EMR_EXTCREATEPEN,
    EmfRecordTypePolyTextOutA            = EMR_POLYTEXTOUTA,
    EmfRecordTypePolyTextOutW            = EMR_POLYTEXTOUTW,
    EmfRecordTypeSetICMMode              = 98,  // EMR_SETICMMODE,
    EmfRecordTypeCreateColorSpace        = 99,  // EMR_CREATECOLORSPACE,
    EmfRecordTypeSetColorSpace           = 100, // EMR_SETCOLORSPACE,
    EmfRecordTypeDeleteColorSpace        = 101, // EMR_DELETECOLORSPACE,
    EmfRecordTypeGLSRecord               = 102, // EMR_GLSRECORD,
    EmfRecordTypeGLSBoundedRecord        = 103, // EMR_GLSBOUNDEDRECORD,
    EmfRecordTypePixelFormat             = 104, // EMR_PIXELFORMAT,
    EmfRecordTypeDrawEscape              = 105, // EMR_RESERVED_105,
    EmfRecordTypeExtEscape               = 106, // EMR_RESERVED_106,
    EmfRecordTypeStartDoc                = 107, // EMR_RESERVED_107,
    EmfRecordTypeSmallTextOut            = 108, // EMR_RESERVED_108,
    EmfRecordTypeForceUFIMapping         = 109, // EMR_RESERVED_109,
    EmfRecordTypeNamedEscape             = 110, // EMR_RESERVED_110,
    EmfRecordTypeColorCorrectPalette     = 111, // EMR_COLORCORRECTPALETTE,
    EmfRecordTypeSetICMProfileA          = 112, // EMR_SETICMPROFILEA,
    EmfRecordTypeSetICMProfileW          = 113, // EMR_SETICMPROFILEW,
    EmfRecordTypeAlphaBlend              = 114, // EMR_ALPHABLEND,
    EmfRecordTypeSetLayout               = 115, // EMR_SETLAYOUT,
    EmfRecordTypeTransparentBlt          = 116, // EMR_TRANSPARENTBLT,
    EmfRecordTypeReserved_117            = 117, // Not Used
    EmfRecordTypeGradientFill            = 118, // EMR_GRADIENTFILL,
    EmfRecordTypeSetLinkedUFIs           = 119, // EMR_RESERVED_119,
    EmfRecordTypeSetTextJustification    = 120, // EMR_RESERVED_120,
    EmfRecordTypeColorMatchToTargetW     = 121, // EMR_COLORMATCHTOTARGETW,
    EmfRecordTypeCreateColorSpaceW       = 122, // EMR_CREATECOLORSPACEW,
    EmfRecordTypeMax                     = 122,
    EmfRecordTypeMin                     = 1,

    // That is the END of the GDI EMF records.

    // Now we start the list of EMF+ records.  We leave quite
    // a bit of room here for the addition of any new GDI
    // records that may be added later.

    EmfPlusRecordTypeInvalid = GDIP_EMFPLUS_RECORD_BASE,
    EmfPlusRecordTypeHeader,
    EmfPlusRecordTypeEndOfFile,

    EmfPlusRecordTypeComment,

    EmfPlusRecordTypeGetDC,

    EmfPlusRecordTypeMultiFormatStart,
    EmfPlusRecordTypeMultiFormatSection,
    EmfPlusRecordTypeMultiFormatEnd,

    // For all persistent objects

    EmfPlusRecordTypeObject,

    // Drawing Records

    EmfPlusRecordTypeClear,
    EmfPlusRecordTypeFillRects,
    EmfPlusRecordTypeDrawRects,
    EmfPlusRecordTypeFillPolygon,
    EmfPlusRecordTypeDrawLines,
    EmfPlusRecordTypeFillEllipse,
    EmfPlusRecordTypeDrawEllipse,
    EmfPlusRecordTypeFillPie,
    EmfPlusRecordTypeDrawPie,
    EmfPlusRecordTypeDrawArc,
    EmfPlusRecordTypeFillRegion,
    EmfPlusRecordTypeFillPath,
    EmfPlusRecordTypeDrawPath,
    EmfPlusRecordTypeFillClosedCurve,
    EmfPlusRecordTypeDrawClosedCurve,
    EmfPlusRecordTypeDrawCurve,
    EmfPlusRecordTypeDrawBeziers,
    EmfPlusRecordTypeDrawImage,
    EmfPlusRecordTypeDrawImagePoints,
    EmfPlusRecordTypeDrawString,

    // Graphics State Records

    EmfPlusRecordTypeSetRenderingOrigin,
    EmfPlusRecordTypeSetAntiAliasMode,
    EmfPlusRecordTypeSetTextRenderingHint,
    EmfPlusRecordTypeSetTextContrast,
    EmfPlusRecordTypeSetInterpolationMode,
    EmfPlusRecordTypeSetPixelOffsetMode,
    EmfPlusRecordTypeSetCompositingMode,
    EmfPlusRecordTypeSetCompositingQuality,
    EmfPlusRecordTypeSave,
    EmfPlusRecordTypeRestore,
    EmfPlusRecordTypeBeginContainer,
    EmfPlusRecordTypeBeginContainerNoParams,
    EmfPlusRecordTypeEndContainer,
    EmfPlusRecordTypeSetWorldTransform,
    EmfPlusRecordTypeResetWorldTransform,
    EmfPlusRecordTypeMultiplyWorldTransform,
    EmfPlusRecordTypeTranslateWorldTransform,
    EmfPlusRecordTypeScaleWorldTransform,
    EmfPlusRecordTypeRotateWorldTransform,
    EmfPlusRecordTypeSetPageTransform,
    EmfPlusRecordTypeResetClip,
    EmfPlusRecordTypeSetClipRect,
    EmfPlusRecordTypeSetClipPath,
    EmfPlusRecordTypeSetClipRegion,
    EmfPlusRecordTypeOffsetClip,

    EmfPlusRecordTypeDrawDriverString,
#if (GDIPVER >= 0x0110)
    EmfPlusRecordTypeStrokeFillPath,
    EmfPlusRecordTypeSerializableObject,

    EmfPlusRecordTypeSetTSGraphics,
    EmfPlusRecordTypeSetTSClip,
#endif
    // NOTE: New records *must* be added immediately before this line.

    EmfPlusRecordTotal,

    EmfPlusRecordTypeMax = EmfPlusRecordTotal-1,
    EmfPlusRecordTypeMin = EmfPlusRecordTypeHeader,
};
*/

enum StringFormatFlags
{
    StringFormatFlagsDirectionRightToLeft        = 0x00000001,
    StringFormatFlagsDirectionVertical           = 0x00000002,
    StringFormatFlagsNoFitBlackBox               = 0x00000004,
    StringFormatFlagsDisplayFormatControl        = 0x00000020,
    StringFormatFlagsNoFontFallback              = 0x00000400,
    StringFormatFlagsMeasureTrailingSpaces       = 0x00000800,
    StringFormatFlagsNoWrap                      = 0x00001000,
    StringFormatFlagsLineLimit                   = 0x00002000,

    StringFormatFlagsNoClip                      = 0x00004000,
    StringFormatFlagsBypassGDI                   = 0x80000000
};

enum StringTrimming {
    StringTrimmingNone              = 0,
    StringTrimmingCharacter         = 1,
    StringTrimmingWord              = 2,
    StringTrimmingEllipsisCharacter = 3,
    StringTrimmingEllipsisWord      = 4,
    StringTrimmingEllipsisPath      = 5
};

enum StringDigitSubstitute
{
    StringDigitSubstituteUser        = 0,  // As NLS setting
    StringDigitSubstituteNone        = 1,
    StringDigitSubstituteNational    = 2,
    StringDigitSubstituteTraditional = 3
};

enum HotkeyPrefix
{
    HotkeyPrefixNone        = 0,
    HotkeyPrefixShow        = 1,
    HotkeyPrefixHide        = 2
};

enum StringAlignment
{
    StringAlignmentNear   = 0,
    StringAlignmentCenter = 1,
    StringAlignmentFar    = 2
};

enum DriverStringOptions
{
    DriverStringOptionsCmapLookup             = 1,
    DriverStringOptionsVertical               = 2,
    DriverStringOptionsRealizedAdvance        = 4,
    DriverStringOptionsLimitSubpixel          = 8
};

enum FlushIntention
{
    FlushIntentionFlush = 0,        // Flush all batched rendering operations
    FlushIntentionSync = 1          // Flush all batched rendering operations
                                    // and wait for them to complete
};

enum EncoderParameterValueType
{
    EncoderParameterValueTypeByte           = 1,    // 8-bit unsigned int
    EncoderParameterValueTypeASCII          = 2,    // 8-bit byte containing one 7-bit ASCII
                                                    // code. NULL terminated.
    EncoderParameterValueTypeShort          = 3,    // 16-bit unsigned int
    EncoderParameterValueTypeLong           = 4,    // 32-bit unsigned int
    EncoderParameterValueTypeRational       = 5,    // Two Longs. The first Long is the
                                                    // numerator, the second Long expresses the
                                                    // denomintor.
    EncoderParameterValueTypeLongRange      = 6,    // Two longs which specify a range of
                                                    // integer values. The first Long specifies
                                                    // the lower end and the second one
                                                    // specifies the higher end. All values
                                                    // are inclusive at both ends
    EncoderParameterValueTypeUndefined      = 7,    // 8-bit byte that can take any value
                                                    // depending on field definition
    EncoderParameterValueTypeRationalRange  = 8,    // Two Rationals. The first Rational
                                                    // specifies the lower end and the second
                                                    // specifies the higher end. All values
                                                    // are inclusive at both ends
/* TODO: fix this
#if (GDIPVER >= 0x0110)
    EncoderParameterValueTypePointer        = 9     // a pointer to a parameter defined data.
#endif //(GDIPVER >= 0x0110)
*/
};

enum EncoderValue
{
    EncoderValueColorTypeCMYK,
    EncoderValueColorTypeYCCK,
    EncoderValueCompressionLZW,
    EncoderValueCompressionCCITT3,
    EncoderValueCompressionCCITT4,
    EncoderValueCompressionRle,
    EncoderValueCompressionNone,
    EncoderValueScanMethodInterlaced,
    EncoderValueScanMethodNonInterlaced,
    EncoderValueVersionGif87,
    EncoderValueVersionGif89,
    EncoderValueRenderProgressive,
    EncoderValueRenderNonProgressive,
    EncoderValueTransformRotate90,
    EncoderValueTransformRotate180,
    EncoderValueTransformRotate270,
    EncoderValueTransformFlipHorizontal,
    EncoderValueTransformFlipVertical,
    EncoderValueMultiFrame,
    EncoderValueLastFrame,
    EncoderValueFlush,
    EncoderValueFrameDimensionTime,
    EncoderValueFrameDimensionResolution,
    EncoderValueFrameDimensionPage,
/* TODO: fix this
#if (GDIPVER >= 0x0110)
    EncoderValueColorTypeGray,
    EncoderValueColorTypeRGB,
#endif
*/
};

enum EmfToWmfBitsFlags
{
    EmfToWmfBitsFlagsDefault          = 0x00000000,
    EmfToWmfBitsFlagsEmbedEmf         = 0x00000001,
    EmfToWmfBitsFlagsIncludePlaceable = 0x00000002,
    EmfToWmfBitsFlagsNoXORClip        = 0x00000004
};

/* TODO: should include?
#if (GDIPVER >= 0x0110)
enum ConvertToEmfPlusFlags
{
    ConvertToEmfPlusFlagsDefault       = 0x00000000,
    ConvertToEmfPlusFlagsRopUsed       = 0x00000001,
    ConvertToEmfPlusFlagsText          = 0x00000002,
    ConvertToEmfPlusFlagsInvalidRecord = 0x00000004
};
#endif
*/

enum GpTestControlEnum
{
    TestControlForceBilinear = 0,
    TestControlNoICM = 1,
    TestControlGetBuildNumber = 2
};
]]

