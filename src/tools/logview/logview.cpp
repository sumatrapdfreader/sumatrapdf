/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// logview: a native Win32 log viewer for SumatraPDF's SumatraLog.cpp pipe client.
//
// It is the *server* side of the named pipe \\.\pipe\LOCAL\ArsLexis-Logger that
// src/SumatraLog.cpp connects to (as a writer). Each process that connects gets
// its own tab; we collect its log lines separately. This is the native
// equivalent of the web-based logview (C:\Users\kjk\src\hack\winapps\logview-web).
//
// Features:
//  - filter edit control at the top (space separated terms, case-insensitive AND).
//    matching terms are highlighted in the log.
//  - one tab per connected client; the "app: <name>" line names the tab.
//  - virtualized, scrollable log view: only the lines visible per the scrollbar
//    are drawn, so millions of lines stay cheap.
//  - auto-follows the tail only while scrolled to the bottom; scroll up and the
//    view stays put as new lines arrive, scroll back to the end to resume.
//  - ":v <name> <type> <value>" lines are shown as a live key/value overlay.
//
// Keyboard: '/' focus filter, Esc clear+leave filter, '1'..'9' select tab,
// PgUp/PgDn/Up/Dn/Home/End scroll the log (even while typing in the filter).
// The filter is applied 300ms after you stop typing (debounced).

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/ScopedWin.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

// ---- logging required by the base library (we don't link SumatraLog.cpp) ----

void log(Str s) {
    if (!s) {
        return;
    }
    OutputDebugStringA(s.s);
}

void loga(Str s) {
    log(s);
}

// base's ReportIf() references this crash-reporting hook; we don't crash-report.
void _uploadDebugReport(Str, Str, bool, bool) {}

// opt into the v6 common controls (themed edit box / buttons = modern look).
// combined with InitCommonControlsEx() at startup.
#pragma comment(linker,                                                                                          \
                "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
                "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ------------------------------------------------------------------ constants

constexpr const WCHAR* kPipeName = L"\\\\.\\pipe\\LOCAL\\ArsLexis-Logger";
constexpr int kPipeBufSize = 1024 * 64;
constexpr const WCHAR* kAboutURL = L"https://www.sumatrapdfreader.org/docs/Logview";

constexpr int WM_APP_NEW_LOGS = WM_APP + 1;

// filtering is debounced: applied 300ms after the last keystroke in the filter box
constexpr UINT_PTR kFilterTimerId = 1;
constexpr UINT kFilterDebounceMs = 300;

constexpr Color kColLogBg = kColWhite;
constexpr Color kColLogText = kColBlack;
constexpr Color kColHili = kColYellow;
constexpr Color kColTabBar = MkRgb(0xd8, 0xd8, 0xd8);
constexpr Color kColTabSel = kColWhite;
constexpr Color kColKbd = MkRgb(0x80, 0x80, 0x80);
constexpr Color kColValBg = MkRgb(0xf3, 0xf3, 0xf3);

enum {
    IdcClear = 100, // 'c' button
    IdcAbout,       // '?' button
};

// -------------------------------------------------------------------- model

// parsed from a ":v <name> <type> <value>" line; shown in the values overlay.
struct Value {
    Str name; // owned
    Str val;  // owned
};

struct Tab {
    int connNo = 0;
    Str name;           // owned; defaults to "logs", set from "app: <name>"
    StrVec logs;        // all log lines received from this client
    Vec<int> filtered;  // indices into logs that match the current filter
    Vec<Value> values;  // live key/value pairs from ":v" lines
    int scrollTop = 0;  // first visible line (index into filtered)
    int scrollX = 0;    // horizontal scroll in pixels
    int maxWidth = 0;   // widest line seen so far, in pixels (grows monotonically)
    i64 logBytes = 0;   // total bytes of all log lines (for the size readout)
    bool follow = true; // auto-scroll to the tail; only user scrolling clears it
};

// lines handed from a pipe reader thread to the GUI thread
struct PendingLine {
    int connNo;
    char* text; // heap allocated (str::Dup); GUI thread frees after copying
};

static Vec<Tab*> gTabs;
static int gSel = -1;

static Str gFilter;     // owned copy of the (trimmed) filter text, UTF-8
static Vec<Str> gTerms; // views into gFilter, one per space-separated term

static Mutex gQueueMutex;
static Vec<PendingLine> gQueue;

struct LogViewWnd;
struct LogLinesWnd;
struct TabBarCtrl;

// windows / gdi
static LogViewWnd* gWnd = nullptr;
static HWND gHwndMain = nullptr;
static HWND gHwndLog = nullptr;
static int gWheelAccum = 0; // accumulated mouse-wheel delta (for hi-res wheels)
static PlatformFont* gUiFont = nullptr;
static PlatformFont* gMonoFont = nullptr;
static int gLineDy = 16; // height of one log row, in pixels

// tab hit-testing rects, rebuilt on each tab-bar paint (in window coords)
struct TabHit {
    RECT rc;      // whole tab
    RECT rcClose; // the 'x' close area
};
static Vec<TabHit> gTabHits;

static int gDpi = 96;
static int DpiScale(int x) {
    return MulDiv(x, gDpi, 96);
}

// ------------------------------------------------------------------ helpers

static void InvalidateTabBar();

static Tab* SelTab() {
    if (gSel < 0 || gSel >= len(gTabs)) {
        return nullptr;
    }
    return gTabs[gSel];
}

// true if line contains every filter term (case-insensitive). empty filter matches all.
static bool LineMatches(Str line) {
    int n = len(gTerms);
    for (int i = 0; i < n; i++) {
        if (!str::ContainsI(line, gTerms[i])) {
            return false;
        }
    }
    return true;
}

// split gFilter into gTerms (views into gFilter's buffer). called after gFilter changes.
static void RebuildTerms() {
    gTerms.Reset();
    char* s = gFilter.s;
    int n = gFilter.len;
    int i = 0;
    while (i < n) {
        while (i < n && s[i] == ' ') {
            i++;
        }
        int start = i;
        while (i < n && s[i] != ' ') {
            i++;
        }
        if (i > start) {
            gTerms.Append(Str(s + start, i - start));
        }
    }
}

// recompute filtered[] for a tab from scratch.
static void RebuildFiltered(Tab* tab) {
    tab->filtered.Reset();
    int n = len(tab->logs);
    for (int i = 0; i < n; i++) {
        if (LineMatches(tab->logs[i])) {
            tab->filtered.Append(i);
        }
    }
}

static int VisibleRows() {
    RECT rc;
    GetClientRect(gHwndLog, &rc);
    int dy = rc.bottom - rc.top;
    if (gLineDy <= 0) {
        return 1;
    }
    return dy / gLineDy;
}

// configure the log window's scrollbars for the selected tab and clamp scroll.
static void UpdateLogScrollbars() {
    Tab* tab = SelTab();
    RECT rc;
    GetClientRect(gHwndLog, &rc);
    int clientW = rc.right - rc.left;
    int nLines = tab ? len(tab->filtered) : 0;
    int rows = VisibleRows();

    int maxTop = nLines - rows;
    if (maxTop < 0) {
        maxTop = 0;
    }
    int top = tab ? tab->scrollTop : 0;
    if (top > maxTop) {
        top = maxTop;
    }
    if (top < 0) {
        top = 0;
    }
    if (tab) {
        tab->scrollTop = top;
    }

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = nLines > 0 ? nLines - 1 : 0;
    si.nPage = (UINT)rows;
    si.nPos = top;
    SetScrollInfo(gHwndLog, SB_VERT, &si, TRUE);

    int maxW = tab ? tab->maxWidth : 0;
    int sx = tab ? tab->scrollX : 0;
    if (sx > maxW - clientW) {
        sx = maxW - clientW;
    }
    if (sx < 0) {
        sx = 0;
    }
    if (tab) {
        tab->scrollX = sx;
    }
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = maxW > 0 ? maxW : 0;
    si.nPage = (UINT)clientW;
    si.nPos = sx;
    SetScrollInfo(gHwndLog, SB_HORZ, &si, TRUE);
}

static int MaxScrollTop(Tab* tab) {
    int rows = VisibleRows();
    int maxTop = len(tab->filtered) - rows;
    return maxTop < 0 ? 0 : maxTop;
}

// true if the view is scrolled to (or past) the last line.
static bool IsAtBottom(Tab* tab) {
    return tab->scrollTop >= MaxScrollTop(tab);
}

static void ScrollToBottom(Tab* tab) {
    tab->scrollTop = MaxScrollTop(tab);
    tab->follow = true;
}

static void SetCountText(Str s);

static void UpdateCountLabel() {
    Tab* tab = SelTab();
    WCHAR buf[128];
    if (!tab) {
        buf[0] = 0;
    } else {
        int n = len(tab->filtered);
        int total = len(tab->logs);
        WCHAR sizeBuf[32];
        FormatSizeHumanIntoWBuf((u64)tab->logBytes, WStr(sizeBuf, 32));
        if (n == total) {
            wsprintfW(buf, L"%d lines · %s", total, sizeBuf);
        } else {
            wsprintfW(buf, L"%d of %d lines · %s", n, total, sizeBuf);
        }
    }
    SetCountText(ToUtf8Temp(WStr(buf, (int)wcslen(buf))));
}

// ------------------------------------------------------- ingesting log lines

static Tab* FindOrCreateTab(int connNo) {
    int n = len(gTabs);
    for (int i = 0; i < n; i++) {
        if (gTabs[i]->connNo == connNo) {
            return gTabs[i];
        }
    }
    Tab* tab = new Tab();
    tab->connNo = connNo;
    tab->name = str::Dup(StrL("logs"));
    gTabs.Append(tab);
    gSel = len(gTabs) - 1; // select the newest client, like the web ui
    InvalidateTabBar();
    return tab;
}

static const Str kValuePrefix = StrL(":v ");
static const Str kAppPrefix = StrL("app: ");

// parse "<name> <type> <value>" (the part after ":v "). returns false on bad input.
static bool ParseValue(Str s, Str* nameOut, Str* valOut) {
    int idx = str::IndexOfChar(s, ' ');
    if (idx < 0) {
        return false;
    }
    Str name = Str(s.s, idx);
    Str rest = Str(s.s + idx + 1, s.len - idx - 1);
    idx = str::IndexOfChar(rest, ' ');
    if (idx < 0) {
        return false;
    }
    // rest is "<type> <value>"; skip the type
    Str val = Str(rest.s + idx + 1, rest.len - idx - 1);
    *nameOut = name;
    *valOut = val;
    return true;
}

static void HandleValueLine(Tab* tab, Str line) {
    Str payload = Str(line.s + kValuePrefix.len, line.len - kValuePrefix.len);
    Str name, val;
    if (!ParseValue(payload, &name, &val)) {
        return;
    }
    int n = len(tab->values);
    for (int i = 0; i < n; i++) {
        if (str::Eq(tab->values[i].name, name)) {
            str::Free(tab->values[i].val);
            tab->values[i].val = str::Dup(val);
            return;
        }
    }
    Value v;
    v.name = str::Dup(name);
    v.val = str::Dup(val);
    tab->values.Append(v);
}

// measure a line's pixel width; used to grow the horizontal scroll range.
static int MeasureLineWidth(HDC hdc, Str line) {
    WStr w = ToWStrTemp(line);
    SIZE sz{};
    GetTextExtentPoint32W(hdc, w.s, w.len, &sz);
    return sz.cx;
}

// process one received line for a client. mirrors plog() in the web ui.
static void IngestLine(HDC measureDC, int connNo, Str raw) {
    Str line = str::TrimSuffixWhitespace(raw); // trimEnd

    Tab* tab = FindOrCreateTab(connNo);

    if (str::StartsWith(line, kValuePrefix)) {
        HandleValueLine(tab, line);
        return; // value lines are not added to the log
    }

    Str stored = tab->logs.Append(line);
    tab->logBytes += stored.len + 1; // +1 for the newline that was trimmed
    if (str::StartsWith(line, kAppPrefix)) {
        str::Free(tab->name);
        tab->name = str::Dup(Str(line.s + kAppPrefix.len, line.len - kAppPrefix.len));
        InvalidateTabBar();
    }

    int w = MeasureLineWidth(measureDC, stored);
    if (w > tab->maxWidth) {
        tab->maxWidth = w;
    }

    // keep the selected tab's filtered[] in sync incrementally
    if (tab == SelTab() && LineMatches(stored)) {
        tab->filtered.Append(len(tab->logs) - 1);
    }
}

// drain the cross-thread queue and update the ui. runs on the gui thread.
static void DrainQueue() {
    gQueueMutex.Lock();
    Vec<PendingLine> local;
    local.Append(gQueue);
    gQueue.Reset();
    gQueueMutex.Unlock();

    int n = len(local);
    if (n == 0) {
        return;
    }

    HDC hdc = GetDC(gHwndLog);
    {
        ScopedSelectFont selectFont(hdc, gMonoFont->GetHFont());
        for (int i = 0; i < n; i++) {
            PendingLine& pl = local[i];
            IngestLine(hdc, pl.connNo, Str(pl.text));
            free(pl.text);
        }
    }
    ReleaseDC(gHwndLog, hdc);

    // pause is automatic: follow the tail only while the tab is in follow mode
    // (set when the view is at the bottom, cleared when the user scrolls up).
    // otherwise the incoming lines pile up below without moving the viewport.
    Tab* tab = SelTab();
    if (tab && tab->follow) {
        ScrollToBottom(tab);
    }
    UpdateCountLabel();
    UpdateLogScrollbars();
    InvalidateRect(gHwndLog, nullptr, TRUE);
    ResetTempArena();
}

// -------------------------------------------------------------- pipe server

struct ConnInfo {
    HANDLE hPipe;
    int connNo;
};

// accumulates raw bytes and splits them into '\n'-terminated lines.
struct LineSplitter {
    char* buf = nullptr;
    int len = 0;
    int cap = 0;

    ~LineSplitter() { free(buf); }
    void Append(const char* d, int n) {
        if (len + n > cap) {
            int newCap = (len + n) * 2 + 1024;
            buf = (char*)realloc(buf, (size_t)newCap);
            cap = newCap;
        }
        memcpy(buf + len, d, (size_t)n);
        len += n;
    }
};

// push all complete lines out of the splitter to the shared queue.
static void FlushLines(LineSplitter* ls, int connNo, bool* posted) {
    int start = 0;
    for (int i = 0; i < ls->len; i++) {
        if (ls->buf[i] != '\n') {
            continue;
        }
        Str line = Str(ls->buf + start, i - start);
        char* dup = str::Dup(line).s;
        gQueueMutex.Lock();
        gQueue.Append(PendingLine{connNo, dup});
        gQueueMutex.Unlock();
        *posted = true;
        start = i + 1;
    }
    // keep the unconsumed tail
    if (start > 0) {
        ls->len -= start;
        memmove(ls->buf, ls->buf + start, (size_t)ls->len);
    }
}

static DWORD WINAPI ReaderThread(void* arg) {
    ConnInfo* ci = (ConnInfo*)arg;
    HANDLE hPipe = ci->hPipe;
    int connNo = ci->connNo;
    free(ci);

    LineSplitter ls;
    char buf[kPipeBufSize];
    for (;;) {
        DWORD nRead = 0;
        BOOL ok = ReadFile(hPipe, buf, kPipeBufSize, &nRead, nullptr);
        if (!ok && GetLastError() == ERROR_MORE_DATA) {
            // message bigger than our buffer; take this chunk and keep reading
            ls.Append(buf, (int)nRead);
            continue;
        }
        if (!ok || nRead == 0) {
            break;
        }
        ls.Append(buf, (int)nRead);
        bool posted = false;
        FlushLines(&ls, connNo, &posted);
        if (posted) {
            PostMessageW(gHwndMain, WM_APP_NEW_LOGS, 0, 0);
        }
    }

    // note the disconnect in the client's log
    {
        char* dup = str::Dup(StrL("--- client disconnected ---")).s;
        gQueueMutex.Lock();
        gQueue.Append(PendingLine{connNo, dup});
        gQueueMutex.Unlock();
        PostMessageW(gHwndMain, WM_APP_NEW_LOGS, 0, 0);
    }

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    return 0;
}

static DWORD WINAPI PipeAcceptThread(void*) {
    int connNo = 1;
    for (;;) {
        HANDLE hPipe =
            CreateNamedPipeW(kPipeName, PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                             PIPE_UNLIMITED_INSTANCES, kPipeBufSize, kPipeBufSize, 0, nullptr);
        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }
        BOOL ok = ConnectNamedPipe(hPipe, nullptr);
        if (!ok && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(hPipe);
            continue;
        }
        ConnInfo* ci = new ConnInfo{hPipe, connNo++};
        HANDLE h = CreateThread(nullptr, 0, ReaderThread, ci, 0, nullptr);
        if (h) {
            CloseHandle(h);
        } else {
            CloseHandle(hPipe);
            delete ci;
        }
    }
}

// -------------------------------------------------------------- log drawing

// mark [start, start+termLen) in mask for every case-insensitive occurrence of
// each term in the (already lowercased) wide line lc.
static void ComputeHighlight(const WCHAR* lc, int n, char* mask) {
    int nTerms = len(gTerms);
    for (int t = 0; t < nTerms; t++) {
        WStr tw = ToWStrTemp(gTerms[t]);
        if (tw.len == 0) {
            continue;
        }
        // lowercase the term in place
        CharLowerBuffW(tw.s, (DWORD)tw.len);
        for (int i = 0; i + tw.len <= n; i++) {
            bool match = true;
            for (int j = 0; j < tw.len; j++) {
                if (lc[i + j] != tw.s[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                for (int j = 0; j < tw.len; j++) {
                    mask[i + j] = 1;
                }
            }
        }
    }
}

static void DrawLogLine(HDC hdc, int x, int y, Str line) {
    WStr w = ToWStrTemp(line);
    if (w.len == 0) {
        return;
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kColLogText);

    if (len(gTerms) == 0) {
        ExtTextOutW(hdc, x, y, 0, nullptr, w.s, w.len, nullptr);
        return;
    }

    // draw the plain text first, then overpaint matched runs with a yellow bg
    ExtTextOutW(hdc, x, y, 0, nullptr, w.s, w.len, nullptr);

    WCHAR* lc = AllocArrayTemp<WCHAR>(w.len + 1);
    memcpy(lc, w.s, sizeof(WCHAR) * (size_t)w.len);
    lc[w.len] = 0;
    CharLowerBuffW(lc, (DWORD)w.len);

    char* mask = AllocArrayTemp<char>(w.len);
    ComputeHighlight(lc, w.len, mask);

    int i = 0;
    while (i < w.len) {
        if (!mask[i]) {
            i++;
            continue;
        }
        int start = i;
        while (i < w.len && mask[i]) {
            i++;
        }
        SIZE sz{};
        GetTextExtentPoint32W(hdc, w.s, start, &sz);
        int runX = x + sz.cx;
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, kColHili);
        ExtTextOutW(hdc, runX, y, 0, nullptr, w.s + start, i - start, nullptr);
        SetBkMode(hdc, TRANSPARENT);
    }
}

// the values overlay in the top-right corner (live ":v" key/value pairs).
static void DrawValuesOverlay(HDC hdc, RECT client, Tab* tab) {
    int nVals = len(tab->values);
    if (nVals == 0) {
        return;
    }
    int pad = DpiScale(4);
    int boxW = DpiScale(240);
    int rowH = gLineDy;
    int boxH = nVals * rowH + pad * 2;
    RECT box;
    box.right = client.right - DpiScale(20);
    box.left = box.right - boxW;
    box.top = client.top + DpiScale(4);
    box.bottom = box.top + boxH;

    HBRUSH br = CreateSolidBrush(kColValBg);
    FillRect(hdc, &box, br);
    DeleteObject(br);
    FrameRect(hdc, &box, (HBRUSH)GetStockObject(GRAY_BRUSH));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kColLogText);
    int y = box.top + pad;
    for (int i = 0; i < nVals; i++) {
        WStr name = ToWStrTemp(tab->values[i].name);
        WStr val = ToWStrTemp(tab->values[i].val);
        ExtTextOutW(hdc, box.left + pad, y, 0, nullptr, name.s, name.len, nullptr);
        SIZE sz{};
        GetTextExtentPoint32W(hdc, val.s, val.len, &sz);
        int vx = box.right - pad - sz.cx;
        ExtTextOutW(hdc, vx, y, 0, nullptr, val.s, val.len, nullptr);
        y += rowH;
    }
}

static void PaintLog(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdcWin = BeginPaint(hwnd, &ps);
    RECT client;
    GetClientRect(hwnd, &client);
    int clientW = client.right;
    int clientH = client.bottom;

    // double buffer to avoid flicker
    HDC hdc = CreateCompatibleDC(hdcWin);
    HBITMAP bmp = CreateCompatibleBitmap(hdcWin, clientW, clientH);
    HBITMAP oldBmp = (HBITMAP)SelectObject(hdc, bmp);
    {
        ScopedSelectFont selectFont(hdc, gMonoFont->GetHFont());

        HBRUSH bgBrush = CreateSolidBrush(kColLogBg);
        FillRect(hdc, &client, bgBrush);
        DeleteObject(bgBrush);

        Tab* tab = SelTab();
        if (tab && len(tab->filtered) > 0) {
            int rows = clientH / gLineDy + 1;
            int top = tab->scrollTop;
            int nFiltered = len(tab->filtered);
            int x = -tab->scrollX;
            for (int row = 0; row < rows; row++) {
                int fi = top + row;
                if (fi >= nFiltered) {
                    break;
                }
                int lineIdx = tab->filtered[fi];
                Str line = tab->logs[lineIdx];
                DrawLogLine(hdc, x, row * gLineDy, line);
            }
            DrawValuesOverlay(hdc, client, tab);
        } else {
            // empty state
            const WCHAR* msg = L"No logs yet";
            WCHAR buf[256];
            if (tab && len(tab->logs) > 0 && gFilter.len > 0) {
                WStr wf = ToWStrTemp(gFilter);
                wsprintfW(buf, L"No results matching '%s'", wf.s);
                msg = buf;
            }
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, MkRgb(0x80, 0x80, 0x80));
            RECT rc = client;
            rc.top = clientH / 4;
            ScopedSelectFont selectUiFont(hdc, gUiFont->GetHFont());
            DrawTextW(hdc, msg, -1, &rc, DT_CENTER | DT_SINGLELINE);
        }

        BitBlt(hdcWin, 0, 0, clientW, clientH, hdc, 0, 0, SRCCOPY);
    }

    SelectObject(hdc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hdc);
    EndPaint(hwnd, &ps);
    ResetTempArena();
}

// ------------------------------------------------------------- log window

// scroll the vertical view by dLines (negative = up); clamps and repaints.
static void ScrollByLines(HWND hwnd, int dLines) {
    Tab* tab = SelTab();
    if (!tab) {
        return;
    }
    int top = tab->scrollTop + dLines;
    int maxTop = MaxScrollTop(tab);
    if (top > maxTop) {
        top = maxTop;
    }
    if (top < 0) {
        top = 0;
    }
    if (top != tab->scrollTop) {
        tab->scrollTop = top;
        tab->follow = IsAtBottom(tab); // re-arm follow only when the user lands at the end
        SetScrollPos(hwnd, SB_VERT, top, TRUE);
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}

static void LogVScroll(HWND hwnd, int code) {
    Tab* tab = SelTab();
    if (!tab) {
        return;
    }
    int rows = VisibleRows();
    int nLines = len(tab->filtered);
    int maxTop = nLines - rows;
    if (maxTop < 0) {
        maxTop = 0;
    }
    int top = tab->scrollTop;
    switch (code) {
        case SB_LINEUP:
            top -= 1;
            break;
        case SB_LINEDOWN:
            top += 1;
            break;
        case SB_PAGEUP:
            top -= rows;
            break;
        case SB_PAGEDOWN:
            top += rows;
            break;
        case SB_TOP:
            top = 0;
            break;
        case SB_BOTTOM:
            top = maxTop;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(hwnd, SB_VERT, &si);
            top = si.nTrackPos;
            break;
        }
    }
    if (top > maxTop) {
        top = maxTop;
    }
    if (top < 0) {
        top = 0;
    }
    if (top != tab->scrollTop) {
        tab->scrollTop = top;
        tab->follow = IsAtBottom(tab); // re-arm follow only when the user lands at the end
        SetScrollPos(hwnd, SB_VERT, top, TRUE);
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}

static void LogHScroll(HWND hwnd, int code) {
    Tab* tab = SelTab();
    if (!tab) {
        return;
    }
    RECT rc;
    GetClientRect(hwnd, &rc);
    int clientW = rc.right;
    int maxX = tab->maxWidth - clientW;
    if (maxX < 0) {
        maxX = 0;
    }
    int x = tab->scrollX;
    int step = DpiScale(32);
    switch (code) {
        case SB_LINELEFT:
            x -= step;
            break;
        case SB_LINERIGHT:
            x += step;
            break;
        case SB_PAGELEFT:
            x -= clientW;
            break;
        case SB_PAGERIGHT:
            x += clientW;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(hwnd, SB_HORZ, &si);
            x = si.nTrackPos;
            break;
        }
    }
    if (x > maxX) {
        x = maxX;
    }
    if (x < 0) {
        x = 0;
    }
    if (x != tab->scrollX) {
        tab->scrollX = x;
        SetScrollPos(hwnd, SB_HORZ, x, TRUE);
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}

// The log lines keep an HWND of their own: they are virtualized and scrolled
// with the system scrollbars, which a virtual control has no use for. As a
// ControlBase it is placed by the layout like everything else
struct LogLinesWnd : ControlBase {
    LogLinesWnd() { kind = "logLines"; }
    HWND Create(HWND parent);
    void WndProc(ControlBase::WndProcEvent* ev);
};

HWND LogLinesWnd::Create(HWND parent) {
    shouldEraseBackground = false;
    onWndProc = MkMethod1<LogLinesWnd, ControlBase::WndProcEvent*, &LogLinesWnd::WndProc>(this);
    CreateCustomArgs args;
    args.parent = parent;
    args.className = L"LogViewLines";
    args.style = WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL;
    CreateCustom(args);
    return hwnd;
}

void LogLinesWnd::WndProc(ControlBase::WndProcEvent* ev) {
    HWND hwnd = ev->hwnd;
    UINT msg = ev->msg;
    WPARAM wp = ev->wparam;
    switch (msg) {
        case WM_PAINT:
            PaintLog(hwnd);
            ev->result = 0;
            ev->didHandle = true;
            return;
        case WM_SIZE: {
            Tab* tab = SelTab();
            if (tab && tab->follow) {
                ScrollToBottom(tab); // fewer/more visible rows: stay pinned to the tail
            }
            UpdateLogScrollbars();
            ev->result = 0;
            ev->didHandle = true;
            return;
        }
        case WM_VSCROLL:
            LogVScroll(hwnd, LOWORD(wp));
            ev->result = 0;
            ev->didHandle = true;
            return;
        case WM_HSCROLL:
            LogHScroll(hwnd, LOWORD(wp));
            ev->result = 0;
            ev->didHandle = true;
            return;
        case WM_MOUSEWHEEL: {
            // accumulate so hi-resolution wheels (delta < WHEEL_DELTA) also scroll,
            // and scroll by whole notches (3 lines each). wheel up (positive delta)
            // moves the view toward the top.
            gWheelAccum += GET_WHEEL_DELTA_WPARAM(wp);
            int notches = gWheelAccum / WHEEL_DELTA;
            if (notches != 0) {
                gWheelAccum -= notches * WHEEL_DELTA;
                ScrollByLines(hwnd, -notches * 3);
            }
            ev->result = 0;
            ev->didHandle = true;
            return;
        }
        case WM_LBUTTONDOWN:
            SetFocus(); // so subsequent wheel messages target the log view
            ev->result = 0;
            ev->didHandle = true;
            return;
    }
}

// ------------------------------------------------------------- tabs window

static void SelectTab(int idx);
static void CloseTab(int idx);

// the tab bar is a virtual control: the main window paints it and gives it its
// input, so it needs no HWND / window class of its own
struct TabBarCtrl : VirtCtrl {
    TabBarCtrl() {
        kind = "logViewTabs";
        onMouseDown = MkMethod1<TabBarCtrl, VirtMouseEvent*, &TabBarCtrl::OnMouseDown>(this);
    }
    Size GetIdealSize() override { return {0, DpiScale(26)}; }
    void Paint(VirtPaintCtx&) override;
    void OnMouseDown(VirtMouseEvent*);
};

void TabBarCtrl::Paint(VirtPaintCtx& ctx) {
    Gfx* gfx = ctx.gfx;
    Rect client = ctx.bounds;
    gfx->FillRect(client, kColTabBar);

    gTabHits.Reset();
    int pad = DpiScale(8);
    int x = client.x;
    int n = len(gTabs);
    Str closeStr = StrL(" x");
    for (int i = 0; i < n; i++) {
        Tab* tab = gTabs[i];
        TempStr kbd = fmt(" [%d]", i + 1);

        Size szName = gfx->MeasureText(tab->name, gUiFont);
        Size szKbd = gfx->MeasureText(kbd, gUiFont);
        Size szClose = gfx->MeasureText(closeStr, gUiFont);

        int tabW = pad + szName.dx + szKbd.dx + DpiScale(6) + szClose.dx + pad;
        Rect rc{x, client.y, tabW, client.dy};

        if (i == gSel) {
            gfx->FillRect(rc, kColTabSel);
        }

        int tx = x + pad;
        int ty = client.y + ((client.dy - szName.dy) / 2);
        u32 fmtTxt = gfxTextSingleLine | gfxTextNoClip;
        gfx->DrawTextAt(tab->name, {tx, ty}, fmtTxt, gUiFont, kColLogText);
        tx += szName.dx;
        gfx->DrawTextAt(kbd, {tx, ty}, fmtTxt, gUiFont, kColKbd);
        tx += szKbd.dx + DpiScale(6);

        Rect rcClose{tx, ty, szClose.dx, szClose.dy};
        gfx->DrawTextAt(closeStr, {tx, ty}, fmtTxt, gUiFont, MkRgb(0x60, 0x60, 0x60));

        gTabHits.Append(TabHit{ToRECT(rc), ToRECT(rcClose)});
        x += tabW;
    }

    ResetTempArena();
}

void TabBarCtrl::OnMouseDown(VirtMouseEvent* ev) {
    POINT pt{ev->ptWindow.x, ev->ptWindow.y};
    int n = len(gTabHits);
    for (int i = 0; i < n; i++) {
        if (PtInRect(&gTabHits[i].rcClose, pt)) {
            CloseTab(i);
            ev->didHandle = true;
            return;
        }
        if (PtInRect(&gTabHits[i].rc, pt)) {
            SelectTab(i);
            ev->didHandle = true;
            return;
        }
    }
    ev->didHandle = true;
}

static void SelectTab(int idx) {
    if (idx < 0 || idx >= len(gTabs)) {
        return;
    }
    gSel = idx;
    Tab* tab = gTabs[idx];
    RebuildFiltered(tab);
    UpdateCountLabel();
    UpdateLogScrollbars();
    InvalidateTabBar();
    InvalidateRect(gHwndLog, nullptr, TRUE);
}

static void FreeTab(Tab* tab) {
    str::Free(tab->name);
    int n = len(tab->values);
    for (int i = 0; i < n; i++) {
        str::Free(tab->values[i].name);
        str::Free(tab->values[i].val);
    }
    delete tab;
}

static void CloseTab(int idx) {
    if (idx < 0 || idx >= len(gTabs)) {
        return;
    }
    FreeTab(gTabs[idx]);
    gTabs.RemoveAt(idx);
    if (len(gTabs) == 0) {
        gSel = -1;
    } else if (gSel >= len(gTabs)) {
        gSel = len(gTabs) - 1;
    }
    if (gSel >= 0) {
        RebuildFiltered(gTabs[gSel]);
    }
    UpdateCountLabel();
    UpdateLogScrollbars();
    InvalidateTabBar();
    InvalidateRect(gHwndLog, nullptr, TRUE);
}

// ------------------------------------------------------------- main window

// the whole ui: a filter row, the tab bar and the log lines, laid out in a VBox
struct LogViewWnd : WindowBase {
    Edit* filterEdit = nullptr;
    VirtText* countText = nullptr;
    VirtButton* btnClear = nullptr;
    VirtButton* btnAbout = nullptr;
    TabBarCtrl* tabBar = nullptr;
    LogLinesWnd* logLines = nullptr;

    bool Create();
    void WndProc(WindowBase::WndProcEvent* ev);
    void OnTimer(WindowBase::TimerEvent* ev);
    void OnDestroy(WindowBase::DestroyEvent* ev);
};

static void InvalidateTabBar() {
    if (gWnd && gWnd->tabBar) {
        gWnd->tabBar->Invalidate();
    }
}

static void SetCountText(Str s) {
    if (!gWnd || !gWnd->countText) {
        return;
    }
    if (str::Eq(gWnd->countText->s, s)) {
        return;
    }
    gWnd->countText->SetText(s);
    // the label is as wide as its text, so the row has to be laid out again
    gWnd->DoLayout();
    HwndInvalidate(gWnd->hwnd);
}

static void OnFilterChanged() {
    TempStr utf8 = gWnd->filterEdit->GetTextTemp();

    str::Free(gFilter);
    gFilter = str::Dup(utf8);
    RebuildTerms();

    Tab* tab = SelTab();
    if (tab) {
        RebuildFiltered(tab);
        ScrollToBottom(tab); // show the latest matching lines
    }
    UpdateCountLabel();
    UpdateLogScrollbars();
    InvalidateRect(gHwndLog, nullptr, TRUE);
    ResetTempArena();
}

static void ClearSelectedTab() {
    Tab* tab = SelTab();
    if (!tab) {
        return;
    }
    tab->logs.Reset();
    tab->filtered.Reset();
    tab->scrollTop = 0;
    tab->scrollX = 0;
    tab->maxWidth = 0;
    tab->logBytes = 0;
    UpdateCountLabel();
    UpdateLogScrollbars();
    InvalidateRect(gHwndLog, nullptr, TRUE);
}

static bool IsFilterFocused() {
    return gWnd && gWnd->filterEdit && GetFocus() == gWnd->filterEdit->hwnd;
}

// the filter is applied kFilterDebounceMs after the last keystroke
static void FilterTextChanged() {
    SetTimer(gHwndMain, kFilterTimerId, kFilterDebounceMs, nullptr);
}

static void AboutClicked(VirtMouseEvent*) {
    ShellExecuteW(gHwndMain, L"open", kAboutURL, nullptr, nullptr, SW_SHOWNORMAL);
}

static VirtButton* NewToolButton(Str text, const VirtMouseHandler& onClick) {
    auto* b = new VirtButton(text, gUiFont);
    b->SetColor(kColBtnText, kColLogText);
    b->SetColor(kColBtnBg, MkRgb(0xe6, 0xe6, 0xe6));
    b->SetColor(kColBtnBgHover, MkRgb(0xd0, 0xd0, 0xd0));
    b->SetColor(kColBtnBorder, MkRgb(0xa0, 0xa0, 0xa0));
    b->textPadding = {DpiScale(3), DpiScale(8), DpiScale(3), DpiScale(8)};
    b->onClick = onClick;
    return b;
}

bool LogViewWnd::Create() {
    onWndProc = MkMethod1<LogViewWnd, WindowBase::WndProcEvent*, &LogViewWnd::WndProc>(this);
    onTimer = MkMethod1<LogViewWnd, WindowBase::TimerEvent*, &LogViewWnd::OnTimer>(this);
    onDestroy = MkMethod1<LogViewWnd, WindowBase::DestroyEvent*, &LogViewWnd::OnDestroy>(this);
    {
        CreateCustomArgs args;
        args.className = L"LogViewMain";
        args.title = "Logview - SumatraPDF";
        args.pos = {CW_USEDEFAULT, CW_USEDEFAULT, DpiScale(1000), DpiScale(700)};
        args.bgColor = GetSysColor(COLOR_BTNFACE);
        args.icon = LoadIconW(nullptr, IDI_APPLICATION);
        args.visible = false;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    gHwndMain = hwnd;

    int m = DpiScale(4);

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = gUiFont;
        args.withBorder = true;
        args.cueText = "filter '/'";
        filterEdit = new Edit();
        filterEdit->Create(args);
        filterEdit->onTextChanged = MkFunc0Void(FilterTextChanged);
    }

    countText = NewVirtText({
        .s = "",
        .font = gUiFont,
        .textColor = kColLogText,
        .align = VirtTextAlign::Right,
    });
    btnClear = NewToolButton("c", MkFunc0Void(ClearSelectedTab));
    btnAbout = NewToolButton("?", MkFunc1Void<VirtMouseEvent*>(AboutClicked));

    tabBar = new TabBarCtrl();

    logLines = new LogLinesWnd();
    logLines->Create(hwnd);
    gHwndLog = logLines->hwnd;

    auto* filterRow = new HBox();
    filterRow->alignCross = CrossAxisAlign::CrossCenter;
    filterRow->AddChild(filterEdit, 1);
    // 1rem between the size readout and the 'c' button
    filterRow->AddChild(new Padding(countText, Insets{0, DpiScale(16), 0, DpiScale(8)}));
    filterRow->AddChild(btnClear);
    filterRow->AddChild(new Padding(btnAbout, Insets{0, 0, 0, m}));

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(new Padding(filterRow, Insets{m, m, m, m}));
    vbox->AddChild(tabBar);
    vbox->AddChild(logLines, 1);
    layout = vbox;
    return true;
}

void LogViewWnd::OnTimer(WindowBase::TimerEvent* ev) {
    if (ev->timerId == kFilterTimerId) {
        KillTimer(hwnd, kFilterTimerId);
        OnFilterChanged();
    }
}

void LogViewWnd::OnDestroy(WindowBase::DestroyEvent*) {
    PostQuitMessage(0);
}

// custom app message for the log drain queue
void LogViewWnd::WndProc(WindowBase::WndProcEvent* ev) {
    if (ev->msg == WM_APP_NEW_LOGS) {
        DrainQueue();
        ev->result = 0;
        ev->didHandle = true;
    }
}

// -------------------------------------------------------------------- setup

static void CreateFonts() {
    HDC hdc = GetDC(nullptr);
    gDpi = GetDeviceCaps(hdc, LOGPIXELSY);

    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    gUiFont = GetPlatformFont(CreateFontIndirectW(&ncm.lfMessageFont));

    gMonoFont = GetPlatformFont("Consolas", 10, PlatformFontStyle::Regular);

    {
        ScopedSelectFont selectFont(hdc, gMonoFont->GetHFont());
        TEXTMETRICW tm{};
        GetTextMetricsW(hdc, &tm);
        gLineDy = tm.tmHeight + DpiScale(2);
    }
    ReleaseDC(nullptr, hdc);
}

// handle app-level keys in the message loop, mirroring the web ui's shortcuts.
static bool HandleKey(MSG* msg) {
    if (msg->message != WM_KEYDOWN) {
        return false;
    }
    int key = (int)msg->wParam;

    // navigation keys scroll the log regardless of where focus is (including the
    // filter edit), so you can filter and page through results without clicking.
    switch (key) {
        case VK_PRIOR:
            LogVScroll(gHwndLog, SB_PAGEUP);
            return true;
        case VK_NEXT:
            LogVScroll(gHwndLog, SB_PAGEDOWN);
            return true;
        case VK_UP:
            LogVScroll(gHwndLog, SB_LINEUP);
            return true;
        case VK_DOWN:
            LogVScroll(gHwndLog, SB_LINEDOWN);
            return true;
        case VK_HOME:
            LogVScroll(gHwndLog, SB_TOP);
            return true;
        case VK_END:
            LogVScroll(gHwndLog, SB_BOTTOM);
            return true;
    }

    bool filterFocused = IsFilterFocused();

    if (filterFocused) {
        if (key == VK_ESCAPE) {
            gWnd->filterEdit->SetText("");
            SetFocus(gHwndLog);
            return true;
        }
        return false; // let the edit control handle typing
    }

    if (key == VK_OEM_2) { // '/' key
        SetFocus(gWnd->filterEdit->hwnd);
        gWnd->filterEdit->SelectAll();
        return true;
    }
    if (key >= '1' && key <= '9') {
        int idx = key - '1';
        if (idx < len(gTabs)) {
            SelectTab(idx);
        }
        return true;
    }
    return false;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int nCmdShow) {
    SetProcessDPIAware();

    // register the v6 common controls so the edit box / buttons pick up the
    // current visual style (the manifest dependency above supplies comctl32 v6).
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // the virtual controls measure and draw text with gdiplus
    ScopedGdiPlus gdiPlus(true);

    CreateFonts();

    gWnd = new LogViewWnd();
    if (!gWnd->Create()) {
        return 1;
    }
    gWnd->DoLayout();
    ShowWindow(gHwndMain, nCmdShow);
    UpdateWindow(gHwndMain);

    CreateThread(nullptr, 0, PipeAcceptThread, nullptr, 0, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (HandleKey(&msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
