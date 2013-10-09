--proc/imagelist: image list resources.
setfenv(1, require'winapi')
require'winapi.comctl'
require'winapi.rpc_types'

ffi.cdef[[
HIMAGELIST  ImageList_Create(int cx, int cy, UINT flags, int cInitial, int cGrow);
BOOL        ImageList_Destroy(HIMAGELIST himl);
]]

ILC_MASK                 = 0x00000001
ILC_COLOR                = 0x00000000
ILC_COLORDDB             = 0x000000FE
ILC_COLOR4               = 0x00000004
ILC_COLOR8               = 0x00000008
ILC_COLOR16              = 0x00000010
ILC_COLOR24              = 0x00000018
ILC_COLOR32              = 0x00000020
ILC_PALETTE              = 0x00000800  -- (not implemented)
ILC_MIRROR               = 0x00002000  -- mirror the icons contained, if the process is mirrored
ILC_PERITEMMIRROR        = 0x00008000  -- mirror each item when inserting a set of images, versus the whole strip
ILC_ORIGINALSIZE         = 0x00010000  -- accept smaller than set images (vista+)
ILC_HIGHQUALITYSCALE     = 0x00020000  -- enable use of the high quality scaler (vista+)

function ImageList_Create(t)
	return own(checkh(comctl.ImageList_Create(t.w, t.h, flags(t.flags),
								t.initial_size, t.grow_size)), ImageList_Destroy)
end

function ImageList_Destroy(himl)
	checknz(comctl.ImageList_Destroy(himl))
	disown(himl)
end

--

ffi.cdef[[
int         ImageList_Add(HIMAGELIST himl, HBITMAP hbmImage, HBITMAP hbmMask);
BOOL        ImageList_Replace(HIMAGELIST himl, int i, HBITMAP hbmImage, HBITMAP hbmMask);
BOOL        ImageList_Remove(HIMAGELIST himl, int i);

int         ImageList_GetImageCount(HIMAGELIST himl);
BOOL        ImageList_SetImageCount(HIMAGELIST himl, UINT uNewCount);

COLORREF    ImageList_GetBkColor(HIMAGELIST himl);
COLORREF    ImageList_SetBkColor(HIMAGELIST himl, COLORREF clrBk);

int         ImageList_ReplaceIcon(HIMAGELIST himl, int i, HICON hicon);
HICON       ImageList_GetIcon(HIMAGELIST himl, int i, UINT flags);
BOOL        ImageList_GetIconSize(HIMAGELIST himl, int *cx, int *cy);
BOOL        ImageList_SetIconSize(HIMAGELIST himl, int cx, int cy);

int         ImageList_AddMasked(HIMAGELIST himl, HBITMAP hbmImage, COLORREF crMask);

BOOL        ImageList_Copy(HIMAGELIST himlDst, int iDst, HIMAGELIST himlSrc, int iSrc, UINT uFlags);
HIMAGELIST  ImageList_Merge(HIMAGELIST himl1, int i1, HIMAGELIST himl2, int i2, int dx, int dy);
HIMAGELIST  ImageList_Duplicate(HIMAGELIST himl);

HIMAGELIST  ImageList_LoadImageW(HINSTANCE hi, LPCWSTR lpbmp, int cx, int cGrow, COLORREF crMask, UINT uType, UINT uFlags);

BOOL        ImageList_SetOverlayImage(HIMAGELIST himl, int iImage, int iOverlay);

BOOL        ImageList_BeginDrag(HIMAGELIST himlTrack, int iTrack, int dxHotspot, int dyHotspot);
void        ImageList_EndDrag(void);
BOOL        ImageList_DragEnter(HWND hwndLock, int x, int y);
BOOL        ImageList_DragLeave(HWND hwndLock);
BOOL        ImageList_DragMove(int x, int y);
BOOL        ImageList_SetDragCursorImage(HIMAGELIST himlDrag, int iDrag, int dxHotspot, int dyHotspot);
BOOL        ImageList_DragShowNolock(BOOL fShow);
HIMAGELIST  ImageList_GetDragImage(POINT *ppt, POINT *pptHotspot);

HIMAGELIST  ImageList_Read(struct IStream *pstm);
BOOL        ImageList_Write(HIMAGELIST himl, struct IStream *pstm);
HRESULT     ImageList_ReadEx(DWORD dwFlags, struct IStream *pstm, REFIID riid, PVOID* ppv);
HRESULT     ImageList_WriteEx(HIMAGELIST himl, DWORD dwFlags, struct IStream *pstm);

HRESULT     HIMAGELIST_QueryInterface(HIMAGELIST himl, REFIID riid, void** ppv);

]]

CLR_NONE                = 0xFFFFFFFF
CLR_DEFAULT             = 0xFF000000
CLR_HILIGHT             = CLR_DEFAULT

ILD_NORMAL               = 0x00000000
ILD_TRANSPARENT          = 0x00000001
ILD_MASK                 = 0x00000010
ILD_IMAGE                = 0x00000020
ILD_ROP                  = 0x00000040
ILD_BLEND25              = 0x00000002
ILD_BLEND50              = 0x00000004
ILD_OVERLAYMASK          = 0x00000F00
function INDEXTOOVERLAYMASK(i) return bit.lshift(i, 8) end
ILD_PRESERVEALPHA        = 0x00001000  -- This preserves the alpha channel in dest
ILD_SCALE                = 0x00002000  -- Causes the image to be scaled to cx, cy instead of clipped
ILD_DPISCALE             = 0x00004000
ILD_ASYNC                = 0x00008000
ILD_SELECTED             = ILD_BLEND50
ILD_FOCUS                = ILD_BLEND25
ILD_BLEND                = ILD_BLEND50

ILS_NORMAL               = 0x00000000
ILS_GLOW                 = 0x00000001
ILS_SHADOW               = 0x00000002
ILS_SATURATE             = 0x00000004
ILS_ALPHA                = 0x00000008

ILGT_NORMAL              = 0x00000000
ILGT_ASYNC               = 0x00000001

HBITMAP_CALLBACK                = -1 --only for SparseImageList

ILCF_MOVE    = (0x00000000)
ILCF_SWAP    = (0x00000001)

ILP_NORMAL           = 0 -- writes or reads the stream using new semantics for this version of comctl32
ILP_DOWNLEVEL        = 1 -- writes or reads the stream using downlevel semantics.

function ImageList_Add(himl, image, mask)
	return checkpoz(comctl.ImageList_Add(himl, image, mask))
end

function ImageList_Replace(himl, i, image, mask)
	return checknz(comctl.ImageList_Replace(himl, countfrom0(i), image, mask))
end

function ImageList_Remove(himl, i)
	return checknz(comctl.ImageList_Remove(himl, countfrom0(i)))
end
ImageList_RemoveAll = ImageList_Remove

function ImageList_GetImageCount(himl)
	return checkpoz(comctl.ImageList_GetImageCount(himl))
end
function ImageList_SetImageCount(himl, count)
	return checknz(comctl.ImageList_SetImageCount(himl, count))
end

function ImageList_GetBkColor(himl)
	return checkh(comctl.ImageList_GetBkColor(himl))
end

function ImageList_SetBkColor(himl, color)
	return checkh(comctl.ImageList_SetBkColor(himl, color))
end

function ImageList_ReplaceIcon(himl, i, hicon)
	return checkpoz(comctl.ImageList_ReplaceIcon(himl, countfrom0(i), hicon))
end

function ImageList_AddIcon(himl, hicon)
	return ImageList_ReplaceIcon(himl, nil, hicon)
end

function ImageList_GetIcon(himl, i, ILD)
	return checkh(comctl.ImageList_GetIcon(himl, countfrom0(i), flags(ILD)))
end

function ImageList_GetIconSize(himl)
	local w,h = ffi.new'int[1]', ffi.new'int[1]'
	checknz(comctl.ImageList_GetIconSize(himl, w, h))
	return w[0],h[0]
end
function ImageList_SetIconSize(himl,w,h)
	checknz(comctl.ImageList_SetIconSize(himl,w,h))
end

function ImageList_AddMasked(himl, bitmap, mask)
	return checknz(comctl.ImageList_AddMasked(himl, bitmap, mask))
end

function ImageList_Copy(himl, dst, src, ILCF)
	return checknz(comctl.ImageList_Copy(himl, countfrom0(dst), himl, countfrom0(src), flags(ILCF)))
end

function ImageList_Merge(h1, i1, h2, i2, dx, dy)
	return checkh(comctl.ImageList_Merge(h1, countfrom0(i1), h2, countfrom0(i2), dx, dy))
end

function ImageList_Duplicate(himl)
	return checkh(comctl.ImageList_Duplicate(himl))
end

function ImageList_LoadImage(hinstance, name, cx, cGrow, colormask, type, flags)
	return checkh(comctl.ImageList_LoadImageW(hinstance, name, cx, cGrow, colormask, type, flags))
end

function ImageList_LoadBitmap(hinstance, lpbmp, cx, cGrow, crMask)
	return ImageList_LoadImage(hinstance, lpbmp, cx, cGrow, crMask, IMAGE_BITMAP, 0)
end

function ImageList_SetOverlayImage(himl, i, ioverlay)
	return checknz(comctl.ImageList_SetOverlayImage(himl, countfrom0(i), ioverlay))
end

--TODO: these
function ImageList_BeginDrag(...) return checknz(comctl.ImageList_BeginDrag(...)) end
function ImageList_EndDrag(...) return comctl.ImageList_EndDrag(...) end
function ImageList_DragEnter(...) return checknz(comctl.ImageList_DragEnter(...)) end
function ImageList_DragLeave(...) return checknz(comctl.ImageList_DragLeave(...)) end
function ImageList_DragMove(...) return checknz(comctl.ImageList_DragMove(...)) end
function ImageList_SetDragCursorImage(...) return checknz(comctl.ImageList_SetDragCursorImage(...)) end
function ImageList_DragShowNolock(...) return checknz(comctl.ImageList_DragShowNolock(...)) end
function ImageList_GetDragImage(...) return checkh(comctl.ImageList_GetDragImage(...)) end

function ImageList_Read(...) return checkh(comctl.ImageList_Read(...)) end
function ImageList_Write(...) return checknz(comctl.ImageList_Write(...)) end
function ImageList_ReadEx(...) return checkh(comctl.ImageList_ReadEx(...)) end
function ImageList_WriteEx(...) return checkh(comctl.ImageList_WriteEx(...)) end

function ImageList_QueryInterface(...) return checkh(comctl.HIMAGELIST_QueryInterface(...)) end

--

ffi.cdef[[
typedef struct _IMAGEINFO
{
    HBITMAP image;
    HBITMAP mask;
    int     __Unused1;
    int     __Unused2;
    RECT    bounds;
} IMAGEINFO, *LPIMAGEINFO;

BOOL        ImageList_GetImageInfo(HIMAGELIST himl, int i, IMAGEINFO *pImageInfo);
]]

function ImageList_GetImageInfo(himl, i, info)
	info = types.IMAGEINFO(info)
	checknz(comctl.ImageList_GetImageInfo(himl, i, info))
	return info
end

--

ffi.cdef[[
typedef struct _IMAGELISTDRAWPARAMS
{
	 DWORD       cbSize;
	 HIMAGELIST  himl;
	 int         i;
	 HDC         hdcDst;
	 int         x;
	 int         y;
	 int         cx;
	 int         cy;
	 int         xBitmap;
	 int         yBitmap;
	 COLORREF    rgbBk;
	 COLORREF    rgbFg;
	 UINT        fStyle;
	 DWORD       dwRop;
	 DWORD       fState;
	 DWORD       Frame;
	 COLORREF    crEffect;
} IMAGELISTDRAWPARAMS, *LPIMAGELISTDRAWPARAMS;

BOOL        ImageList_Draw(HIMAGELIST himl, int i, HDC hdcDst, int x, int y, UINT fStyle);
BOOL        ImageList_DrawEx(HIMAGELIST himl, int i, HDC hdcDst, int x, int y, int dx, int dy, COLORREF rgbBk, COLORREF rgbFg, UINT fStyle);
BOOL        ImageList_DrawIndirect(IMAGELISTDRAWPARAMS* pimldp);
]]

function ImageList_Draw(himl, i, dc, x, y, IDL)
	return checknz(comctl.ImageList_Draw(himl, countfrom0(i), dc, x, y, flags(IDL)))
end
function ImageList_DrawEx(...)
	return checknz(comctl.ImageList_DrawEx(...))
end

function ImageList_DrawIndirect(params)
	return checknz(comctl.ImageList_DrawIndirect(IMAGELISTDRAWPARAMS(params)))
end

