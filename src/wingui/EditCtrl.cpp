#include "utils/BaseUtil.h"

#include "utils/WinUtil.h"
#include "EditCtrl.h"

#include "utils/BitManip.h"

constexpr UINT_PTR SUBCLASS_ID = 1;

static void Unsubclass(EditCtrl* w);

// TODO:
// - expose EN_UPDATE
// (http://msdn.microsoft.com/en-us/library/windows/desktop/bb761687(v=vs.85).aspx)
// - add border and possibly other decorations by handling WM_NCCALCSIZE, WM_NCPAINT and
// WM_NCHITTEST
//   etc., http://www.catch22.net/tuts/insert-buttons-edit-control
// - include value we remember in WM_NCCALCSIZE in GetIdealSize()

static LRESULT CALLBACK EditParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                       DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    EditCtrl* w = (EditCtrl*)dwRefData;
    CrashIf(GetParent(w->hwnd) != (HWND)lp);
    if ((WM_CTLCOLOREDIT == msg) && (w->bgBrush != nullptr)) {
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

static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    EditCtrl* w = (EditCtrl*)dwRefData;
    CrashIf(w->hwnd != (HWND)lp);

    if (w->preFilter) {
        bool discard = false;
        auto res = w->preFilter(hwnd, msg, wp, lp, discard);
        if (discard) {
            return res;
        }
    }

    if (WM_NCDESTROY == msg) {
        Unsubclass(w);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    // Node: this is sent during creation, which is too early for us (we didn't
    // subclass the window yet)
    // currently, we force it with SetWindowPos(... SMP_FRAMECHANGED)
    if (WM_NCCALCSIZE == msg) {
        NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lp;
        RECT orig = p->rgrc[0];
        LRESULT res = DefSubclassProc(hwnd, msg, wp, lp);
        RECT curr = p->rgrc[0];
        w->ncDx = RectDx(orig) - RectDx(curr);
        w->ncDy = RectDy(orig) - RectDy(curr);
        return res;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void Subclass(EditCtrl* w) {
    BOOL ok = SetWindowSubclass(w->hwnd, EditProc, SUBCLASS_ID, (DWORD_PTR)w);
    CrashIf(!ok);
    w->hwndSubclassId = SUBCLASS_ID;

    ok = SetWindowSubclass(w->parent, EditParentProc, SUBCLASS_ID, (DWORD_PTR)w);
    CrashIf(!ok);
    w->hwndParentSubclassId = SUBCLASS_ID;
}

static void Unsubclass(EditCtrl* w) {
    if (!w) {
        return;
    }

    if (w->hwndSubclassId != 0) {
        BOOL ok = RemoveWindowSubclass(w->hwnd, EditProc, SUBCLASS_ID);
        CrashIf(false && !ok);
        w->hwndSubclassId = 0;
    }

    if (w->hwndParentSubclassId != 0) {
        BOOL ok = RemoveWindowSubclass(w->parent, EditParentProc, SUBCLASS_ID);
        CrashIf(false && !ok);
        w->hwndParentSubclassId = 0;
    }
}

void EditCtrl::SetFont(HFONT f) {
    SetWindowFont(this->hwnd, f, TRUE);
}

void EditCtrl::SetColors(COLORREF txtCol, COLORREF bgCol) {
    DeleteObject(this->bgBrush);
    this->bgBrush = nullptr;
    if (txtCol != NO_CHANGE) {
        this->txtCol = txtCol;
    }
    if (bgCol != NO_CHANGE) {
        this->bgCol = bgCol;
    }
    if (this->bgCol != NO_COLOR) {
        this->bgBrush = CreateSolidBrush(bgCol);
    }
    InvalidateRect(this->hwnd, nullptr, FALSE);
}

void EditCtrl::SetText(const WCHAR* s) {
    SetWindowTextW(this->hwnd, s);
}

bool EditCtrl::SetCueText(const WCHAR* s) {
    return Edit_SetCueBannerText(this->hwnd, s) == TRUE;
}

// caller must free() the result
WCHAR* EditCtrl::GetTextW() {
    return win::GetText(this->hwnd);
}

// caller must free() the result
char* EditCtrl::GetText() {
    AutoFreeW su(GetTextW());
    return str::conv::ToUtf8(su.Get()).StealData();
}

EditCtrl::EditCtrl(HWND parent, RECT* initialPosition) {
    this->parent = parent;
    if (initialPosition) {
        this->initialPos = *initialPosition;
    } else {
        SetRect(&this->initialPos, 0, 0, 120, 28);
    }
}

bool EditCtrl::Create() {
    // Note: has to remember this here because when I GetWindowStyle() later on,
    // WS_BORDER is not set, which is a mystery, because it is being drawn.
    // also, WS_BORDER seems to be painted in client areay
    this->hasBorder = bit::IsMaskSet<DWORD>(this->dwStyle, WS_BORDER);

    RECT rc = this->initialPos;
    this->hwnd = CreateWindowExW(this->dwExStyle, WC_EDIT, L"", this->dwStyle, rc.left, rc.top, RectDx(rc), RectDy(rc),
                                 this->parent, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!this->hwnd) {
        return false;
    }
    SetFont(GetDefaultGuiFont());
    Subclass(this);
    return true;
}

EditCtrl::~EditCtrl() {
    DeleteObject(this->bgBrush);
}

SIZE EditCtrl::GetIdealSize() {
    // force sending WM_NCCALCSIZE
    UINT flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(this->hwnd, nullptr, 0, 0, 0, 0, flags);
    WCHAR* txt = this->GetTextW();
    if (str::Len(txt) == 0) {
        free(txt);
        txt = str::Dup(L"Sample");
    }
    SizeI s = TextSizeInHwnd(this->hwnd, txt);
    free(txt);
    SIZE res;
    res.cx = s.dx + this->ncDx;
    res.cy = s.dy + this->ncDy;

    if (this->hasBorder) {
        res.cx += 4;
        res.cy += 4;
    }

    return res;
}

void EditCtrl::SetPos(RECT* r) {
    MoveWindow(this->hwnd, r);
}
