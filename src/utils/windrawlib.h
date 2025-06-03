
#define WD_PI 3.14159265358979323846f

#define WD_MIN(a, b) ((a) < (b) ? (a) : (b))
#define WD_MAX(a, b) ((a) > (b) ? (a) : (b))

#define WD_ABS(a) ((a) > 0 ? (a) : -(a))

#define WD_SIZEOF_ARRAY(a) (sizeof((a)) / sizeof((a)[0]))

#define WD_OFFSETOF(type, member) ((size_t)&((type*)0)->member)
#define WD_CONTAINEROF(ptr, type, member) ((type*)((BYTE*)(ptr) - WD_OFFSETOF(type, member)))

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

#define WD_ARGB(a, r, g, b)                                                                            \
    ((((WD_COLOR)(a) & 0xff) << 24) | (((WD_COLOR)(r) & 0xff) << 16) | (((WD_COLOR)(g) & 0xff) << 8) | \
     (((WD_COLOR)(b) & 0xff) << 0))
#define WD_RGB(r, g, b) WD_ARGB(255, (r), (g), (b))

#define WD_AVALUE(color) (((WD_COLOR)(color) & 0xff000000U) >> 24)
#define WD_RVALUE(color) (((WD_COLOR)(color) & 0x00ff0000U) >> 16)
#define WD_GVALUE(color) (((WD_COLOR)(color) & 0x0000ff00U) >> 8)
#define WD_BVALUE(color) (((WD_COLOR)(color) & 0x000000ffU) >> 0)

/* Create WD_COLOR from GDI's COLORREF. */
#define WD_COLOR_FROM_GDI_EX(a, cref) WD_ARGB((a), GetRValue(cref), GetGValue(cref), GetBValue(cref))
#define WD_COLOR_FROM_GDI(cref) WD_COLOR_FROM_GDI_EX(255, (cref))

/* Get GDI's COLORREF from WD_COLOR. */
#define WD_COLOR_TO_GDI(color) RGB(WD_RVALUE(color), WD_GVALUE(color), WD_BVALUE(color))

/*****************************
 ***  2D Geometry Objects  ***
 *****************************/

struct WD_POINT {
    float x;
    float y;
};

struct WD_RECT {
    float x0;
    float y0;
    float x1;
    float y1;
};

struct WD_MATRIX {
    float m11;
    float m12;
    float m21;
    float m22;
    float dx;
    float dy;
};

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

#define D2D_CANVASTYPE_BITMAP 0
#define D2D_CANVASTYPE_DC 1
#define D2D_CANVASTYPE_HWND 2

#define D2D_CANVASFLAG_RECTCLIP 0x1
#define D2D_CANVASFLAG_RTL 0x2

#define D2D_BASEDELTA_X 0.5f
#define D2D_BASEDELTA_Y 0.5f

struct d2d_canvas_t {
    WORD type;
    WORD flags;
    UINT width;
    union {
        ID2D1RenderTarget* target;
        ID2D1BitmapRenderTarget* bmp_target;
        ID2D1HwndRenderTarget* hwnd_target;
    };
    ID2D1GdiInteropRenderTarget* gdi_interop;
    ID2D1Layer* clip_layer;
};

int d2d_init(void);
void d2d_fini(void);
void d2d_init_color(D2D1_COLOR_F* c, WD_COLOR color);
void d2d_matrix_mult(D2D1_MATRIX_3X2_F* res, const D2D1_MATRIX_3X2_F* a, const D2D1_MATRIX_3X2_F* b);

d2d_canvas_t* d2d_canvas_alloc(ID2D1RenderTarget* target, WORD type, UINT width, BOOL rtl);
void d2d_reset_transform(d2d_canvas_t* c);
void d2d_reset_clip(d2d_canvas_t* c);
void d2d_apply_transform(d2d_canvas_t* c, const D2D1_MATRIX_3X2_F* matrix);

IWICBitmapSource* wic_convert_bitmap(IWICBitmapSource* bitmap);

void dwrite_default_user_locale(WCHAR buffer[LOCALE_NAME_MAX_LENGTH]);

bool wdInitialize();
void wdTerminate();
