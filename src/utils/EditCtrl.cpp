#include "BaseUtil.h"
#include "EditCtrl.h"

#include "BitManip.h"
#include "WinUtil.h"

// TODO:
// - expose EN_UPDATE
// (http://msdn.microsoft.com/en-us/library/windows/desktop/bb761687(v=vs.85).aspx)
// - add border and possibly other decorations by handling WM_NCCALCSIZE, WM_NCPAINT and
// WM_NCHITTEST
//   etc., http://www.catch22.net/tuts/insert-buttons-edit-control
// - include value we remember in WM_NCCALCSIZE in GetIdealSize()

static LRESULT CALLBACK EditParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                       UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    EditCtrl *w = (EditCtrl *)dwRefData;
    CrashIf(GetParent(w->hwnd) != (HWND)lp);
    if ((WM_CTLCOLOREDIT == msg) && (w->bgBrush != NULL)) {
        HDC hdc = (HDC)wp;
        // SetBkColor(hdc, w->bgCol);
        SetBkMode(hdc, TRANSPARENT);
        if (w->txtCol != NO_COLOR) {
            SetTextColor(hdc, w->txtCol);
        }
        return (INT_PTR)w->bgBrush;
    }
    if (w->onTextChanged && (WM_COMMAND == msg) && (EN_CHANGE == HIWORD(wp))) {
        w->onTextChanged(w);
        return 0;
    }
    // TODO: handle WM_CTLCOLORSTATIC for read-only/disabled controls
    return DefSubclassProc(hwnd, msg, wp, lp);
}

#if 0
static bool HasWsBorder(HWND hwnd)
{
    DWORD style = GetWindowStyle(hwnd);
    return bit::IsMaskSet<DWORD>(style, WS_BORDER);
}
#endif

static LRESULT CALLBACK
EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    EditCtrl *w = (EditCtrl *)dwRefData;
    CrashIf(w->hwnd != (HWND)lp);

    if (w->preFilter) {
        bool discard = false;
        auto res = w->preFilter(hwnd, msg, wp, lp, discard);
        if (discard) {
            return res;
        }
    }

    if (WM_NCDESTROY == msg) {
        RemoveWindowSubclass(GetParent(w->hwnd), EditParentProc, 0);
        RemoveWindowSubclass(w->hwnd, EditProc, 0);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    // Node: this is sent during creation, which is too early for us (we didn't
    // subclass the window yet)
    // currently, we force it with SetWindowPos(... SMP_FRAMECHANGED)
    if (WM_NCCALCSIZE == msg) {
        NCCALCSIZE_PARAMS *p = (NCCALCSIZE_PARAMS *)lp;
        RECT orig = p->rgrc[0];
        LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
        RECT curr = p->rgrc[0];
        w->ncDx = RectDx(orig) - RectDx(curr);
        w->ncDy = RectDy(orig) - RectDy(curr);
        return res;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

void SetFont(EditCtrl *w, HFONT f) { SetWindowFont(w->hwnd, f, TRUE); }

void SetColors(EditCtrl *w, COLORREF txtCol, COLORREF bgCol) {
    DeleteObject(w->bgBrush);
    w->bgBrush = nullptr;
    if (txtCol != NO_CHANGE) {
        w->txtCol = txtCol;
    }
    if (bgCol != NO_CHANGE) {
        w->bgCol = bgCol;
    }
    if (w->bgCol != NO_COLOR) {
        w->bgBrush = CreateSolidBrush(bgCol);
    }
    InvalidateRect(w->hwnd, nullptr, FALSE);
}

void SetText(EditCtrl *w, const WCHAR *s) { SetWindowTextW(w->hwnd, s); }

void SetCueText(EditCtrl *w, const WCHAR *s) { Edit_SetCueBannerText(w->hwnd, s); }

// caller must free() the result
WCHAR *GetTextW(EditCtrl *w) { return win::GetText(w->hwnd); }

// caller must free() the result
char *GetText(EditCtrl *w) {
    ScopedMem<WCHAR> su(GetTextW(w));
    return str::conv::ToUtf8(su.Get());
}

EditCtrl *AllocEditCtrl(HWND parent, RECT *initialPosition) {
    auto w = AllocStruct<EditCtrl>();
    w->parent = parent;
    if (initialPosition) {
        w->initialPos = *initialPosition;
    } else {
        SetRect(&w->initialPos, 0, 0, 120, 28);
    }

    w->dwExStyle = 0;
    w->dwStyle = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL;

    w->txtCol = NO_COLOR;
    w->bgCol = NO_COLOR;
    w->bgBrush = nullptr;

    return w;
}

bool CreateEditCtrl(EditCtrl *w) {
    // Note: has to remember this here because when I GetWindowStyle() later on,
    // WS_BORDER is not set, which is a mystery, because it is being drawn.
    // also, WS_BORDER seems to be painted in client areay
    w->hasBorder = bit::IsMaskSet<DWORD>(w->dwStyle, WS_BORDER);

    RECT rc = w->initialPos;
    w->hwnd = CreateWindowExW(w->dwExStyle, WC_EDIT, L"", w->dwStyle, rc.left, rc.top, RectDx(rc),
                              RectDy(rc), w->parent, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!w->hwnd) {
        return false;
    }
    SetFont(w, GetDefaultGuiFont());
    SetWindowSubclass(w->hwnd, EditProc, 0, (DWORD_PTR)w);
    SetWindowSubclass(GetParent(w->hwnd), EditParentProc, 0, (DWORD_PTR)w);
    return true;
}

void DeleteEditCtrl(EditCtrl *w) {
    if (!w)
        return;

    DeleteObject(w->bgBrush);
    free(w);
}

SIZE GetIdealSize(EditCtrl *w) {
    // force sending WM_NCCALCSIZE
    SetWindowPos(w->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE);
    WCHAR *txt = GetTextW(w);
    if (str::Len(txt) == 0) {
        free(txt);
        txt = str::Dup(L"Sample");
    }
    SizeI s = TextSizeInHwnd(w->hwnd, txt);
    free(txt);
    SIZE res;
    res.cx = s.dx + w->ncDx;
    res.cy = s.dy + w->ncDy;

    if (w->hasBorder) {
        res.cx += 4;
        res.cy += 4;
    }

    return res;
}
