--proc/headercontrol: standard header control.
setfenv(1, require'winapi')
require'winapi.winuser'

--creation

WC_HEADER = 'SysHeader32'

--commands

HDM_FIRST               = 0x1200
HDM_GETITEMCOUNT        = (HDM_FIRST + 0)

function Header_GetItemCount(hwndHD)
	return checkpoz(SNDMSG(hwndHD, HDM_GETITEMCOUNT))
end


--notifications

--[[

#define HDS_HORZ                0x0000
#define HDS_BUTTONS             0x0002
#define HDS_HOTTRACK            0x0004
#define HDS_HIDDEN              0x0008
#define HDS_DRAGDROP            0x0040
#define HDS_FULLDRAG            0x0080
#define HDS_FILTERBAR           0x0100
#define HDS_FLAT                0x0200
#define HDS_CHECKBOXES          0x0400
#define HDS_NOSIZING            0x0800
#define HDS_OVERFLOW            0x1000
#define HDFT_ISSTRING       0x0000      // HD_ITEM.pvFilter points to a HD_TEXTFILTER
#define HDFT_ISNUMBER       0x0001      // HD_ITEM.pvFilter points to a INT
#define HDFT_ISDATE         0x0002      // HD_ITEM.pvFilter points to a DWORD (dos date)
#define HDFT_HASNOVALUE     0x8000      // clear the filter, by setting this bit

typedef struct _HD_TEXTFILTERW
{
    LPWSTR pszText;                     // [in] pointer to the buffer contiaining the filter (UNICODE)
    INT cchTextMax;                     // [in] max size of buffer/edit control buffer
} HD_TEXTFILTERW, *LPHD_TEXTFILTERW;

typedef struct _HD_ITEMW
{
    UINT    mask;
    int     cxy;
    LPWSTR  pszText;
    HBITMAP hbm;
    int     cchTextMax;
    int     fmt;
    LPARAM  lParam;
    int     iImage;        // index of bitmap in ImageList
    int     iOrder;
    UINT    type;           // [in] filter type (defined what pvFilter is a pointer to)
    void *  pvFilter;       // [in] fillter data see above
    UINT   state;
} HDITEMW, *LPHDITEMW;

#define HDI_WIDTH               0x0001
#define HDI_HEIGHT              HDI_WIDTH
#define HDI_TEXT                0x0002
#define HDI_FORMAT              0x0004
#define HDI_LPARAM              0x0008
#define HDI_BITMAP              0x0010
#define HDI_IMAGE               0x0020
#define HDI_DI_SETITEM          0x0040
#define HDI_ORDER               0x0080
#define HDI_FILTER              0x0100
#define HDI_STATE               0x0200

// HDF_ flags are shared with the listview control (LVCFMT_ flags)

#define HDF_LEFT                0x0000 // Same as LVCFMT_LEFT
#define HDF_RIGHT               0x0001 // Same as LVCFMT_RIGHT
#define HDF_CENTER              0x0002 // Same as LVCFMT_CENTER
#define HDF_JUSTIFYMASK         0x0003 // Same as LVCFMT_JUSTIFYMASK
#define HDF_RTLREADING          0x0004 // Same as LVCFMT_LEFT

#define HDF_BITMAP              0x2000
#define HDF_STRING              0x4000
#define HDF_OWNERDRAW           0x8000 // Same as LVCFMT_COL_HAS_IMAGES
#if (_WIN32_IE >= 0x0300)
#define HDF_IMAGE               0x0800 // Same as LVCFMT_IMAGE
#define HDF_BITMAP_ON_RIGHT     0x1000 // Same as LVCFMT_BITMAP_ON_RIGHT
#endif

#if (_WIN32_WINNT >= 0x0501)
#define HDF_SORTUP              0x0400
#define HDF_SORTDOWN            0x0200
#endif

#if _WIN32_WINNT >= 0x0600
#define HDF_CHECKBOX            0x0040
#define HDF_CHECKED             0x0080
#define HDF_FIXEDWIDTH          0x0100 // Can't resize the column; same as LVCFMT_FIXED_WIDTH
#define HDF_SPLITBUTTON      0x1000000 // Column is a split button; same as LVCFMT_SPLITBUTTON
#endif

#if _WIN32_WINNT >= 0x0600
#define HDIS_FOCUSED            0x00000001
#endif


#define HDM_INSERTITEMA         (HDM_FIRST + 1)
#define HDM_INSERTITEMW         (HDM_FIRST + 10)

#ifdef UNICODE
#define HDM_INSERTITEM          HDM_INSERTITEMW
#else
#define HDM_INSERTITEM          HDM_INSERTITEMA
#endif

#define Header_InsertItem(hwndHD, i, phdi) \
    (int)SNDMSG((hwndHD), HDM_INSERTITEM, (WPARAM)(int)(i), (LPARAM)(const HD_ITEM *)(phdi))


#define HDM_DELETEITEM          (HDM_FIRST + 2)
#define Header_DeleteItem(hwndHD, i) \
    (BOOL)SNDMSG((hwndHD), HDM_DELETEITEM, (WPARAM)(int)(i), 0L)


#define HDM_GETITEMA            (HDM_FIRST + 3)
#define HDM_GETITEMW            (HDM_FIRST + 11)

#ifdef UNICODE
#define HDM_GETITEM             HDM_GETITEMW
#else
#define HDM_GETITEM             HDM_GETITEMA
#endif

#define Header_GetItem(hwndHD, i, phdi) \
    (BOOL)SNDMSG((hwndHD), HDM_GETITEM, (WPARAM)(int)(i), (LPARAM)(HD_ITEM *)(phdi))


#define HDM_SETITEMA            (HDM_FIRST + 4)
#define HDM_SETITEMW            (HDM_FIRST + 12)

#ifdef UNICODE
#define HDM_SETITEM             HDM_SETITEMW
#else
#define HDM_SETITEM             HDM_SETITEMA
#endif

#define Header_SetItem(hwndHD, i, phdi) \
    (BOOL)SNDMSG((hwndHD), HDM_SETITEM, (WPARAM)(int)(i), (LPARAM)(const HD_ITEM *)(phdi))

#if (_WIN32_IE >= 0x0300)
#define HD_LAYOUT  HDLAYOUT
#else
#define HDLAYOUT   HD_LAYOUT
#endif

typedef struct _HD_LAYOUT
{
    RECT *prc;
    WINDOWPOS *pwpos;
} HDLAYOUT, *LPHDLAYOUT;


#define HDM_LAYOUT              (HDM_FIRST + 5)
#define Header_Layout(hwndHD, playout) \
    (BOOL)SNDMSG((hwndHD), HDM_LAYOUT, 0, (LPARAM)(HD_LAYOUT *)(playout))


#define HHT_NOWHERE             0x0001
#define HHT_ONHEADER            0x0002
#define HHT_ONDIVIDER           0x0004
#define HHT_ONDIVOPEN           0x0008
#if (_WIN32_IE >= 0x0500)
#define HHT_ONFILTER            0x0010
#define HHT_ONFILTERBUTTON      0x0020
#endif
#define HHT_ABOVE               0x0100
#define HHT_BELOW               0x0200
#define HHT_TORIGHT             0x0400
#define HHT_TOLEFT              0x0800
#if _WIN32_WINNT >= 0x0600
#define HHT_ONITEMSTATEICON     0x1000
#define HHT_ONDROPDOWN          0x2000
#define HHT_ONOVERFLOW          0x4000
#endif

#if (_WIN32_IE >= 0x0300)
#define HD_HITTESTINFO HDHITTESTINFO
#else
#define HDHITTESTINFO  HD_HITTESTINFO
#endif

typedef struct _HD_HITTESTINFO
{
    POINT pt;
    UINT flags;
    int iItem;
} HDHITTESTINFO, *LPHDHITTESTINFO;

#define HDSIL_NORMAL            0
#define HDSIL_STATE             1

#define HDM_HITTEST             (HDM_FIRST + 6)

#if (_WIN32_IE >= 0x0300)

#define HDM_GETITEMRECT         (HDM_FIRST + 7)
#define Header_GetItemRect(hwnd, iItem, lprc) \
        (BOOL)SNDMSG((hwnd), HDM_GETITEMRECT, (WPARAM)(iItem), (LPARAM)(lprc))

#define HDM_SETIMAGELIST        (HDM_FIRST + 8)
#define Header_SetImageList(hwnd, himl) \
        (HIMAGELIST)SNDMSG((hwnd), HDM_SETIMAGELIST, HDSIL_NORMAL, (LPARAM)(himl))
#define Header_SetStateImageList(hwnd, himl) \
        (HIMAGELIST)SNDMSG((hwnd), HDM_SETIMAGELIST, HDSIL_STATE, (LPARAM)(himl))

#define HDM_GETIMAGELIST        (HDM_FIRST + 9)
#define Header_GetImageList(hwnd) \
        (HIMAGELIST)SNDMSG((hwnd), HDM_GETIMAGELIST, HDSIL_NORMAL, 0)
#define Header_GetStateImageList(hwnd) \
        (HIMAGELIST)SNDMSG((hwnd), HDM_GETIMAGELIST, HDSIL_STATE, 0)

#define HDM_ORDERTOINDEX        (HDM_FIRST + 15)
#define Header_OrderToIndex(hwnd, i) \
        (int)SNDMSG((hwnd), HDM_ORDERTOINDEX, (WPARAM)(i), 0)

#define HDM_CREATEDRAGIMAGE     (HDM_FIRST + 16)  // wparam = which item (by index)
#define Header_CreateDragImage(hwnd, i) \
        (HIMAGELIST)SNDMSG((hwnd), HDM_CREATEDRAGIMAGE, (WPARAM)(i), 0)

#define HDM_GETORDERARRAY       (HDM_FIRST + 17)
#define Header_GetOrderArray(hwnd, iCount, lpi) \
        (BOOL)SNDMSG((hwnd), HDM_GETORDERARRAY, (WPARAM)(iCount), (LPARAM)(lpi))

#define HDM_SETORDERARRAY       (HDM_FIRST + 18)
#define Header_SetOrderArray(hwnd, iCount, lpi) \
        (BOOL)SNDMSG((hwnd), HDM_SETORDERARRAY, (WPARAM)(iCount), (LPARAM)(lpi))
// lparam = int array of size HDM_GETITEMCOUNT
// the array specifies the order that all items should be displayed.
// e.g.  { 2, 0, 1}
// says the index 2 item should be shown in the 0ths position
//      index 0 should be shown in the 1st position
//      index 1 should be shown in the 2nd position


#define HDM_SETHOTDIVIDER          (HDM_FIRST + 19)
#define Header_SetHotDivider(hwnd, fPos, dw) \
        (int)SNDMSG((hwnd), HDM_SETHOTDIVIDER, (WPARAM)(fPos), (LPARAM)(dw))
// convenience message for external dragdrop
// wParam = BOOL  specifying whether the lParam is a dwPos of the cursor
//              position or the index of which divider to hotlight
// lParam = depends on wParam  (-1 and wParm = FALSE turns off hotlight)
#endif      // _WIN32_IE >= 0x0300

#if (_WIN32_IE >= 0x0500)

#define HDM_SETBITMAPMARGIN          (HDM_FIRST + 20)
#define Header_SetBitmapMargin(hwnd, iWidth) \
        (int)SNDMSG((hwnd), HDM_SETBITMAPMARGIN, (WPARAM)(iWidth), 0)

#define HDM_GETBITMAPMARGIN          (HDM_FIRST + 21)
#define Header_GetBitmapMargin(hwnd) \
        (int)SNDMSG((hwnd), HDM_GETBITMAPMARGIN, 0, 0)
#endif


#if (_WIN32_IE >= 0x0400)
#define HDM_SETUNICODEFORMAT   CCM_SETUNICODEFORMAT
#define Header_SetUnicodeFormat(hwnd, fUnicode)  \
    (BOOL)SNDMSG((hwnd), HDM_SETUNICODEFORMAT, (WPARAM)(fUnicode), 0)

#define HDM_GETUNICODEFORMAT   CCM_GETUNICODEFORMAT
#define Header_GetUnicodeFormat(hwnd)  \
    (BOOL)SNDMSG((hwnd), HDM_GETUNICODEFORMAT, 0, 0)
#endif

#if (_WIN32_IE >= 0x0500)
#define HDM_SETFILTERCHANGETIMEOUT  (HDM_FIRST+22)
#define Header_SetFilterChangeTimeout(hwnd, i) \
        (int)SNDMSG((hwnd), HDM_SETFILTERCHANGETIMEOUT, 0, (LPARAM)(i))

#define HDM_EDITFILTER          (HDM_FIRST+23)
#define Header_EditFilter(hwnd, i, fDiscardChanges) \
        (int)SNDMSG((hwnd), HDM_EDITFILTER, (WPARAM)(i), MAKELPARAM(fDiscardChanges, 0))

// Clear filter takes -1 as a column value to indicate that all
// the filter should be cleared.  When this happens you will
// only receive a single filter changed notification.

#define HDM_CLEARFILTER         (HDM_FIRST+24)
#define Header_ClearFilter(hwnd, i) \
        (int)SNDMSG((hwnd), HDM_CLEARFILTER, (WPARAM)(i), 0)
#define Header_ClearAllFilters(hwnd) \
        (int)SNDMSG((hwnd), HDM_CLEARFILTER, (WPARAM)-1, 0)
#endif

#if (_WIN32_IE >= 0x0600)
#define HDM_TRANSLATEACCELERATOR    CCM_TRANSLATEACCELERATOR
#endif

#if (_WIN32_WINNT >= 0x600)

#define HDM_GETITEMDROPDOWNRECT (HDM_FIRST+25)  // rect of item's drop down button
#define Header_GetItemDropDownRect(hwnd, iItem, lprc) \
        (BOOL)SNDMSG((hwnd), HDM_GETITEMDROPDOWNRECT, (WPARAM)(iItem), (LPARAM)(lprc))

#define HDM_GETOVERFLOWRECT (HDM_FIRST+26)  // rect of overflow button
#define Header_GetOverflowRect(hwnd, lprc) \
        (BOOL)SNDMSG((hwnd), HDM_GETOVERFLOWRECT, 0, (LPARAM)(lprc))

#define HDM_GETFOCUSEDITEM (HDM_FIRST+27)
#define Header_GetFocusedItem(hwnd) \
        (int)SNDMSG((hwnd), HDM_GETFOCUSEDITEM, (WPARAM)(0), (LPARAM)(0))

#define HDM_SETFOCUSEDITEM (HDM_FIRST+28)
#define Header_SetFocusedItem(hwnd, iItem) \
        (BOOL)SNDMSG((hwnd), HDM_SETFOCUSEDITEM, (WPARAM)(0), (LPARAM)(iItem))

#endif // _WIN32_WINNT >= 0x600

#define HDN_ITEMCHANGINGA       (HDN_FIRST-0)
#define HDN_ITEMCHANGINGW       (HDN_FIRST-20)
#define HDN_ITEMCHANGEDA        (HDN_FIRST-1)
#define HDN_ITEMCHANGEDW        (HDN_FIRST-21)
#define HDN_ITEMCLICKA          (HDN_FIRST-2)
#define HDN_ITEMCLICKW          (HDN_FIRST-22)
#define HDN_ITEMDBLCLICKA       (HDN_FIRST-3)
#define HDN_ITEMDBLCLICKW       (HDN_FIRST-23)
#define HDN_DIVIDERDBLCLICKA    (HDN_FIRST-5)
#define HDN_DIVIDERDBLCLICKW    (HDN_FIRST-25)
#define HDN_BEGINTRACKA         (HDN_FIRST-6)
#define HDN_BEGINTRACKW         (HDN_FIRST-26)
#define HDN_ENDTRACKA           (HDN_FIRST-7)
#define HDN_ENDTRACKW           (HDN_FIRST-27)
#define HDN_TRACKA              (HDN_FIRST-8)
#define HDN_TRACKW              (HDN_FIRST-28)
#if (_WIN32_IE >= 0x0300)
#define HDN_GETDISPINFOA        (HDN_FIRST-9)
#define HDN_GETDISPINFOW        (HDN_FIRST-29)
#define HDN_BEGINDRAG           (HDN_FIRST-10)
#define HDN_ENDDRAG             (HDN_FIRST-11)
#endif
#if (_WIN32_IE >= 0x0500)
#define HDN_FILTERCHANGE        (HDN_FIRST-12)
#define HDN_FILTERBTNCLICK      (HDN_FIRST-13)
#endif

#if (_WIN32_IE >= 0x0600)
#define HDN_BEGINFILTEREDIT     (HDN_FIRST-14)
#define HDN_ENDFILTEREDIT       (HDN_FIRST-15)
#endif

#if _WIN32_WINNT >= 0x0600
#define HDN_ITEMSTATEICONCLICK  (HDN_FIRST-16)
#define HDN_ITEMKEYDOWN         (HDN_FIRST-17)
#define HDN_DROPDOWN            (HDN_FIRST-18)
#define HDN_OVERFLOWCLICK       (HDN_FIRST-19)
#endif

#ifdef UNICODE
#define HDN_ITEMCHANGING         HDN_ITEMCHANGINGW
#define HDN_ITEMCHANGED          HDN_ITEMCHANGEDW
#define HDN_ITEMCLICK            HDN_ITEMCLICKW
#define HDN_ITEMDBLCLICK         HDN_ITEMDBLCLICKW
#define HDN_DIVIDERDBLCLICK      HDN_DIVIDERDBLCLICKW
#define HDN_BEGINTRACK           HDN_BEGINTRACKW
#define HDN_ENDTRACK             HDN_ENDTRACKW
#define HDN_TRACK                HDN_TRACKW
#if (_WIN32_IE >= 0x0300)
#define HDN_GETDISPINFO          HDN_GETDISPINFOW
#endif
#else
#define HDN_ITEMCHANGING         HDN_ITEMCHANGINGA
#define HDN_ITEMCHANGED          HDN_ITEMCHANGEDA
#define HDN_ITEMCLICK            HDN_ITEMCLICKA
#define HDN_ITEMDBLCLICK         HDN_ITEMDBLCLICKA
#define HDN_DIVIDERDBLCLICK      HDN_DIVIDERDBLCLICKA
#define HDN_BEGINTRACK           HDN_BEGINTRACKA
#define HDN_ENDTRACK             HDN_ENDTRACKA
#define HDN_TRACK                HDN_TRACKA
#if (_WIN32_IE >= 0x0300)
#define HDN_GETDISPINFO          HDN_GETDISPINFOA
#endif
#endif



#if (_WIN32_IE >= 0x0300)
#define HD_NOTIFYA              NMHEADERA
#define HD_NOTIFYW              NMHEADERW
#else
#define tagNMHEADERA            _HD_NOTIFY
#define NMHEADERA               HD_NOTIFYA
#define tagHMHEADERW            _HD_NOTIFYW
#define NMHEADERW               HD_NOTIFYW
#endif
#define HD_NOTIFY               NMHEADER

typedef struct tagNMHEADERA
{
    NMHDR   hdr;
    int     iItem;
    int     iButton;
    HDITEMA *pitem;
}  NMHEADERA, *LPNMHEADERA;


typedef struct tagNMHEADERW
{
    NMHDR   hdr;
    int     iItem;
    int     iButton;
    HDITEMW *pitem;
} NMHEADERW, *LPNMHEADERW;

#ifdef UNICODE
#define NMHEADER                NMHEADERW
#define LPNMHEADER              LPNMHEADERW
#else
#define NMHEADER                NMHEADERA
#define LPNMHEADER              LPNMHEADERA
#endif

typedef struct tagNMHDDISPINFOW
{
    NMHDR   hdr;
    int     iItem;
    UINT    mask;
    LPWSTR  pszText;
    int     cchTextMax;
    int     iImage;
    LPARAM  lParam;
} NMHDDISPINFOW, *LPNMHDDISPINFOW;

typedef struct tagNMHDDISPINFOA
{
    NMHDR   hdr;
    int     iItem;
    UINT    mask;
    LPSTR   pszText;
    int     cchTextMax;
    int     iImage;
    LPARAM  lParam;
} NMHDDISPINFOA, *LPNMHDDISPINFOA;


#ifdef UNICODE
#define NMHDDISPINFO            NMHDDISPINFOW
#define LPNMHDDISPINFO          LPNMHDDISPINFOW
#else
#define NMHDDISPINFO            NMHDDISPINFOA
#define LPNMHDDISPINFO          LPNMHDDISPINFOA
#endif

#if (_WIN32_IE >= 0x0500)
typedef struct tagNMHDFILTERBTNCLICK
{
    NMHDR hdr;
    INT iItem;
    RECT rc;
} NMHDFILTERBTNCLICK, *LPNMHDFILTERBTNCLICK;
#endif

#endif      // NOHEADER
]]
