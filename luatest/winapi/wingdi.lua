--proc/wingdi: windows GDI API.
setfenv(1, require'winapi')
require'winapi.winuser'

--glue

CLR_INVALID = 0xFFFFFFFF
local function validclr(ret) return ret ~= CLR_INVALID, 'valid color expected, got CLR_INVALID' end
checkclr = checkwith(validclr)

--macros

function RGB(r, g, b)
	return b * 65536 + g * 256 + r
end

--stock objects

ffi.cdef[[
HGDIOBJ  GetStockObject(int i);
BOOL  DeleteObject(HGDIOBJ ho);
]]

WHITE_BRUSH          = 0
LTGRAY_BRUSH         = 1
GRAY_BRUSH           = 2
DKGRAY_BRUSH         = 3
BLACK_BRUSH          = 4
NULL_BRUSH           = 5
HOLLOW_BRUSH         = NULL_BRUSH

WHITE_PEN            = 6
BLACK_PEN            = 7
NULL_PEN             = 8

OEM_FIXED_FONT       = 10
ANSI_FIXED_FONT      = 11
ANSI_VAR_FONT        = 12
SYSTEM_FONT          = 13
DEVICE_DEFAULT_FONT  = 14
DEFAULT_PALETTE      = 15
SYSTEM_FIXED_FONT    = 16
DEFAULT_GUI_FONT     = 17

DC_BRUSH             = 18
DC_PEN               = 19


function GetStockObject(i) return checkh(C.GetStockObject(i)) end
function DeleteObject(ho) checknz(C.DeleteObject(ho)) end

--device contexts

ffi.cdef[[
HDC GetDC(HWND hWnd);
int ReleaseDC(HWND hWnd, HDC hDC);
typedef struct tagPAINTSTRUCT {
    HDC         hdc;
    BOOL        fErase;
    RECT        rcPaint;
    BOOL        fRestore;
    BOOL        fIncUpdate;
    BYTE        rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *NPPAINTSTRUCT, *LPPAINTSTRUCT;
HDC BeginPaint(HWND hwnd, LPPAINTSTRUCT lpPaint);
BOOL EndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint);
BOOL InvalidateRect(HWND hWnd, const RECT *lpRect, BOOL bErase);
BOOL RedrawWindow(HWND hWnd, const RECT *lprcUpdate, HRGN hrgnUpdate, UINT flags);
HGDIOBJ  SelectObject(HDC hdc, HGDIOBJ h);
COLORREF SetDCBrushColor(HDC hdc, COLORREF color);
COLORREF SetDCPenColor(HDC hdc, COLORREF color);
int      SetBkMode(HDC hdc, int mode);
HDC      CreateCompatibleDC(HDC hdc);
BOOL     DeleteDC(HDC hdc);
BOOL     SwapBuffers(HDC);
]]

function GetDC(hwnd)
	return checkh(C.GetDC(hwnd))
end

function ReleaseDC(hwnd, hdc)
	checktrue(C.ReleaseDC(hwnd, hdc))
	disown(hdc)
end

SelectObject = C.SelectObject --TODO: checkh for non-regions, HGDI_ERROR (-1U) for regions

function BeginPaint(hwnd, paintstruct)
	return checkh(C.BeginPaint(hwnd, paintstruct))
end

function EndPaint(hwnd, paintstruct)
	return checktrue(C.EndPaint(hwnd, paintstruct))
end

RDW_INVALIDATE          = 0x0001
RDW_INTERNALPAINT       = 0x0002
RDW_ERASE               = 0x0004
RDW_VALIDATE            = 0x0008
RDW_NOINTERNALPAINT     = 0x0010
RDW_NOERASE             = 0x0020
RDW_NOCHILDREN          = 0x0040
RDW_ALLCHILDREN         = 0x0080
RDW_UPDATENOW           = 0x0100
RDW_ERASENOW            = 0x0200
RDW_FRAME               = 0x0400
RDW_NOFRAME             = 0x0800

function RedrawWindow(hwnd, rect_or_region, RDW)
	local rect, region
	if ffi.istype('RECT', rect_or_region) then
		rect = rect_or_region
	else
		region = rect_or_region
	end
	checknz(C.RedrawWindow(hwnd, rect, region, flags(RDW)))
end

function InvalidateRect(hwnd, rect, erase_bk)
	return checktrue(C.InvalidateRect(hwnd, rect, erase_bk or false))
end

function SetDCBrushColor(hdc, color)
	return checkclr(C.SetDCBrushColor(hdc, color))
end

function SetDCPenColor(hdc, color)
	return checkclr(C.SetDCPenColor(hdc, color))
end

TRANSPARENT         = 1
OPAQUE              = 2

function SetBkMode(hdc, mode)
	return checknz(SetBkMode, hdc, flags(mode))
end

function CreateCompatibleDC(hdc)
	return checkh(C.CreateCompatibleDC(hdc))
end

function DeleteDC(hdc)
	return checknz(C.DeleteDC(hdc))
end

function SwapBuffers(hdc)
	return checknz(C.SwapBuffers(hdc))
end

--dc pixel format

ffi.cdef[[
typedef struct tagPIXELFORMATDESCRIPTOR
{
    WORD  nSize;
    WORD  nVersion;
    DWORD dwFlags;
    BYTE  iPixelType;
    BYTE  cColorBits;
    BYTE  cRedBits;
    BYTE  cRedShift;
    BYTE  cGreenBits;
    BYTE  cGreenShift;
    BYTE  cBlueBits;
    BYTE  cBlueShift;
    BYTE  cAlphaBits;
    BYTE  cAlphaShift;
    BYTE  cAccumBits;
    BYTE  cAccumRedBits;
    BYTE  cAccumGreenBits;
    BYTE  cAccumBlueBits;
    BYTE  cAccumAlphaBits;
    BYTE  cDepthBits;
    BYTE  cStencilBits;
    BYTE  cAuxBuffers;
    BYTE  iLayerType;
    BYTE  bReserved;
    DWORD dwLayerMask;
    DWORD dwVisibleMask;
    DWORD dwDamageMask;
} PIXELFORMATDESCRIPTOR, *PPIXELFORMATDESCRIPTOR,  *LPPIXELFORMATDESCRIPTOR;
int  ChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR *ppfd);
BOOL SetPixelFormat(HDC hdc, int format, const PIXELFORMATDESCRIPTOR* ppfd);
]]

PIXELFORMATDESCRIPTOR = struct{
	ctype = 'PIXELFORMATDESCRIPTOR', size = 'nSize', defaults = {nVersion = 1},
	fields = sfields{
		'flags', 'dwFlags', flags, pass, --PFD_*
		'pixel_type', 'iPixelType', flags, pass, --PFD_TYPE_*
		'layer_type', 'iLayerType', flags, pass, --PFD_*_PLANE
	},
}

-- pixel types
PFD_TYPE_RGBA         = 0
PFD_TYPE_COLORINDEX   = 1

-- layer types
PFD_MAIN_PLANE        = 0
PFD_OVERLAY_PLANE     = 1
PFD_UNDERLAY_PLANE    = (-1)

-- PIXELFORMATDESCRIPTOR flags
PFD_DOUBLEBUFFER             = 0x00000001
PFD_STEREO                   = 0x00000002
PFD_DRAW_TO_WINDOW           = 0x00000004
PFD_DRAW_TO_BITMAP           = 0x00000008
PFD_SUPPORT_GDI              = 0x00000010
PFD_SUPPORT_OPENGL           = 0x00000020
PFD_GENERIC_FORMAT           = 0x00000040
PFD_NEED_PALETTE             = 0x00000080
PFD_NEED_SYSTEM_PALETTE      = 0x00000100
PFD_SWAP_EXCHANGE            = 0x00000200
PFD_SWAP_COPY                = 0x00000400
PFD_SWAP_LAYER_BUFFERS       = 0x00000800
PFD_GENERIC_ACCELERATED      = 0x00001000
PFD_SUPPORT_DIRECTDRAW       = 0x00002000
PFD_DIRECT3D_ACCELERATED     = 0x00004000
PFD_SUPPORT_COMPOSITION      = 0x00008000

-- PIXELFORMATDESCRIPTOR flags for use in ChoosePixelFormat only
PFD_DEPTH_DONTCARE           = 0x20000000
PFD_DOUBLEBUFFER_DONTCARE    = 0x40000000
PFD_STEREO_DONTCARE          = 0x80000000

function ChoosePixelFormat(hdc, pfd)
	pfd = PIXELFORMATDESCRIPTOR(pfd)
	return checkpoz(C.ChoosePixelFormat(hdc, pfd))
end

function SetPixelFormat(hdc, format, pfd)
	pfd = PIXELFORMATDESCRIPTOR(pfd)
	return checktrue(C.SetPixelFormat(hdc, format, pfd))
end

--brushes

ffi.cdef[[
HBRUSH CreateSolidBrush(COLORREF color);
]]

function CreateSolidBrush(color)
	return checkh(C.CreateSolidBrush(color))
end

--pens

ffi.cdef[[
HPEN CreatePen(int iStyle, int cWidth, COLORREF color);
]]

PS_SOLID            = 0
PS_DASH             = 1       -- -------
PS_DOT              = 2       -- .......
PS_DASHDOT          = 3       -- _._._._
PS_DASHDOTDOT       = 4       -- _.._.._
PS_NULL             = 5
PS_INSIDEFRAME      = 6
PS_USERSTYLE        = 7
PS_ALTERNATE        = 8
PS_STYLE_MASK       = 0x0000000F

PS_ENDCAP_ROUND     = 0x00000000
PS_ENDCAP_SQUARE    = 0x00000100
PS_ENDCAP_FLAT      = 0x00000200
PS_ENDCAP_MASK      = 0x00000F00

PS_JOIN_ROUND       = 0x00000000
PS_JOIN_BEVEL       = 0x00001000
PS_JOIN_MITER       = 0x00002000
PS_JOIN_MASK        = 0x0000F000

PS_COSMETIC         = 0x00000000
PS_GEOMETRIC        = 0x00010000
PS_TYPE_MASK        = 0x000F0000

function CreatePen(style, width, color)
	return checkh(C.CreatePen(style, width, color))
end

--text

ffi.cdef[[
COLORREF SetTextColor(HDC hdc, COLORREF color);
]]

function SetTextColor(hdc, color)
	return checkclr(C.SetTextColor(hdc, color))
end

--bitmaps

--constants for the biCompression field
BI_RGB        = 0
BI_RLE8       = 1
BI_RLE4       = 2
BI_BITFIELDS  = 3
BI_JPEG       = 4
BI_PNG        = 5

DIB_RGB_COLORS = 0
DIB_PAL_COLORS = 1

ffi.cdef[[
typedef struct tagRGBQUAD {
        BYTE    rgbBlue;
        BYTE    rgbGreen;
        BYTE    rgbRed;
        BYTE    rgbReserved;
} RGBQUAD;

typedef long FXPT2DOT30, *LPFXPT2DOT30;

typedef struct tagCIEXYZ
{
        FXPT2DOT30 ciexyzX;
        FXPT2DOT30 ciexyzY;
        FXPT2DOT30 ciexyzZ;
} CIEXYZ;
typedef CIEXYZ   *LPCIEXYZ;

typedef struct tagICEXYZTRIPLE
{
        CIEXYZ  ciexyzRed;
        CIEXYZ  ciexyzGreen;
        CIEXYZ  ciexyzBlue;
} CIEXYZTRIPLE;
typedef CIEXYZTRIPLE     *LPCIEXYZTRIPLE;

typedef struct tagBITMAPINFOHEADER{
        DWORD      biSize;
        LONG       biWidth;
        LONG       biHeight;
        WORD       biPlanes;
        WORD       biBitCount;
        DWORD      biCompression;
        DWORD      biSizeImage;
        LONG       biXPelsPerMeter;
        LONG       biYPelsPerMeter;
        DWORD      biClrUsed;
        DWORD      biClrImportant;
} BITMAPINFOHEADER,  *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct {
        DWORD        bV4Size;
        LONG         bV4Width;
        LONG         bV4Height;
        WORD         bV4Planes;
        WORD         bV4BitCount;
        DWORD        bV4V4Compression;
        DWORD        bV4SizeImage;
        LONG         bV4XPelsPerMeter;
        LONG         bV4YPelsPerMeter;
        DWORD        bV4ClrUsed;
        DWORD        bV4ClrImportant;
        DWORD        bV4RedMask;
        DWORD        bV4GreenMask;
        DWORD        bV4BlueMask;
        DWORD        bV4AlphaMask;
        DWORD        bV4CSType;
        CIEXYZTRIPLE bV4Endpoints;
        DWORD        bV4GammaRed;
        DWORD        bV4GammaGreen;
        DWORD        bV4GammaBlue;
} BITMAPV4HEADER,  *LPBITMAPV4HEADER, *PBITMAPV4HEADER;

typedef struct {
        DWORD        bV5Size;
        LONG         bV5Width;
        LONG         bV5Height;
        WORD         bV5Planes;
        WORD         bV5BitCount;
        DWORD        bV5Compression;
        DWORD        bV5SizeImage;
        LONG         bV5XPelsPerMeter;
        LONG         bV5YPelsPerMeter;
        DWORD        bV5ClrUsed;
        DWORD        bV5ClrImportant;
        DWORD        bV5RedMask;
        DWORD        bV5GreenMask;
        DWORD        bV5BlueMask;
        DWORD        bV5AlphaMask;
        DWORD        bV5CSType;
        CIEXYZTRIPLE bV5Endpoints;
        DWORD        bV5GammaRed;
        DWORD        bV5GammaGreen;
        DWORD        bV5GammaBlue;
        DWORD        bV5Intent;
        DWORD        bV5ProfileData;
        DWORD        bV5ProfileSize;
        DWORD        bV5Reserved;
} BITMAPV5HEADER,  *LPBITMAPV5HEADER, *PBITMAPV5HEADER;
typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER    bmiHeader;
    RGBQUAD             bmiColors[1];
} BITMAPINFO,  *LPBITMAPINFO, *PBITMAPINFO;

HBITMAP  CreateCompatibleBitmap(HDC hdc, int cx, int cy);
COLORREF SetPixel(HDC hdc, int x, int y, COLORREF color);
HBITMAP  CreateDIBSection(HDC hdc, const BITMAPINFO *lpbmi, UINT usage, void **ppvBits, HANDLE hSection, DWORD offset);
BOOL     BitBlt(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop);
]]

function CreateCompatibleBitmap(hdc, w, h)
	return checkh(C.CreateCompatibleBitmap(hdc, w, h))
end

function CreateDIBSection(hdc, bmi, usage, hSection, offset, bits)
	local bits = bits or 'void*[1]'
	local hbitmap = checkh(C.CreateDIBSection(hdc, bmi, usage, bits, hSection, offset or 0))
	return hbitmap, bits[0]
end

function SetPixel(hdc, x, y, color)
	return C.SetPixel(hdc, x, y, color) --TODO: checkclr
end

R2_BLACK            = 1   -- 0
R2_NOTMERGEPEN      = 2   -- DPon
R2_MASKNOTPEN       = 3   -- DPna
R2_NOTCOPYPEN       = 4   -- PN
R2_MASKPENNOT       = 5   -- PDna
R2_NOT              = 6   -- Dn
R2_XORPEN           = 7   -- DPx
R2_NOTMASKPEN       = 8   -- DPan
R2_MASKPEN          = 9   -- DPa
R2_NOTXORPEN        = 10  -- DPxn
R2_NOP              = 11  -- D
R2_MERGENOTPEN      = 12  -- DPno
R2_COPYPEN          = 13  -- P
R2_MERGEPENNOT      = 14  -- PDno
R2_MERGEPEN         = 15  -- DPo
R2_WHITE            = 16  -- 1
R2_LAST             = 16
SRCCOPY             = 0x00CC0020 -- dest = source
SRCPAINT            = 0x00EE0086 -- dest = source OR dest
SRCAND              = 0x008800C6 -- dest = source AND dest
SRCINVERT           = 0x00660046 -- dest = source XOR dest
SRCERASE            = 0x00440328 -- dest = source AND (NOT dest )
NOTSRCCOPY          = 0x00330008 -- dest = (NOT source)
NOTSRCERASE         = 0x001100A6 -- dest = (NOT src) AND (NOT dest)
MERGECOPY           = 0x00C000CA -- dest = (source AND pattern)
MERGEPAINT          = 0x00BB0226 -- dest = (NOT source) OR dest
PATCOPY             = 0x00F00021 -- dest = pattern
PATPAINT            = 0x00FB0A09 -- dest = DPSnoo
PATINVERT           = 0x005A0049 -- dest = pattern XOR dest
DSTINVERT           = 0x00550009 -- dest = (NOT dest)
BLACKNESS           = 0x00000042 -- dest = BLACK
WHITENESS           = 0x00FF0062 -- dest = WHITE
NOMIRRORBITMAP      = 0x80000000 -- Do not Mirror the bitmap in this call
CAPTUREBLT          = 0x40000000 -- Include layered windows

function MAKEROP4(fore,back)
	return bit.bor(bit.band(bit.lshift(back, 8), 0xFF000000), fore)
end

function BitBlt(dst, x, y, w, h, src, x1, y1, rop)
	return checknz(C.BitBlt(dst, x, y, w, h, src, x1, y1, flags(rop)))
end

--filled shapes

ffi.cdef[[
BOOL Chord(HDC hdc, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
BOOL Ellipse(HDC hdc, int left, int top, int right, int bottom);
int FillRect(HDC hDC, const RECT *lprc, HBRUSH hbr);
int FrameRect(HDC hDC, const RECT *lprc, HBRUSH hbr);
BOOL InvertRect(HDC hDC, const RECT *lprc);
BOOL Pie(HDC hdc, int left, int top, int right, int bottom, int xr1, int yr1, int xr2, int yr2);
BOOL PolyPolygon(HDC hdc, const POINT *apt, const INT *asz, int csz);
BOOL Polygon(HDC hdc, const POINT *apt, int cpt);
BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom);
BOOL RoundRect(HDC hdc, int left, int top, int right, int bottom, int width, int height);
]]

function Chord(...) return checknz(C.Chord(...)) end
function Ellipse(...) return checknz(C.Ellipse(...)) end
function FillRect(...) return checknz(C.FillRect(...)) end
function FrameRect(...) return checknz(C.FrameRect(...)) end
function InvertRect(...) return checknz(C.InvertRect(...)) end
function Pie(...) return checknz(C.Pie(...)) end
function PolyPolygon(...) return checknz(C.PolyPolygon(...)) end
function Polygon(...) return checknz(C.Polygon(...)) end
function Rectangle(...) return checknz(C.Rectangle(...)) end
function RoundRect(...) return checknz(C.RoundRect(...)) end

--batching

ffi.cdef[[
BOOL GdiFlush(void);
]]

GdiFlush = C.GdiFlush

--showcase

if not ... then
print(GetStockObject(WHITE_BRUSH))
print(GetStockObject(DEFAULT_GUI_FONT))
end

