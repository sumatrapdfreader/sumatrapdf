/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

#include "utils/Log.h"

//--- Tooltip

// https://docs.microsoft.com/en-us/windows/win32/controls/tooltip-control-reference

Kind kindTooltip = "tooltip";

LONG gTolltipID = 0;

static int GetNextTooltipID() {
    LONG res = InterlockedIncrement(&gTolltipID);
    return (int)res;
}

int TooltipGetCount(HWND hwnd) {
    int n = (int)SendMessageW(hwnd, TTM_GETTOOLCOUNT, 0, 0);
    return n;
}

void TooltipoRemoveTool(HWND hwnd, HWND owner, int id) {
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    SendMessageW(hwnd, TTM_DELTOOL, 0, (LPARAM)&ti);
}

int TooltipGetId(HWND hwnd, int idx) {
    WCHAR buf[90]; // per docs returns max 80 chars
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.lpszText = buf;
    BOOL ok = SendMessageW(hwnd, TTM_ENUMTOOLS, idx, (LPARAM)&ti);
    ReportIf(!ok);
    if (!ok) {
        return -1;
    }
    return (int)ti.uId;
}

void TooltipRemoveAll(HWND hwnd, HWND owner) {
    for (;;) {
        int n = TooltipGetCount(hwnd);
        if (n <= 0) {
            return;
        }
        int id = TooltipGetId(hwnd, 0);
        if (id < 0) {
            return;
        }
        TooltipoRemoveTool(hwnd, owner, id);
    }
}

void TooltipAddTools(HWND hwnd, HWND owner, TooltipInfo* tools, int nTools) {
    for (int i = 0; i < nTools; i++) {
        TooltipInfo& tti = tools[i];

        TempWStr ws = ToWStrTemp(tti.s);
        TOOLINFOW ti = {};
        ti.cbSize = sizeof(ti);
        ti.hwnd = owner;
        ti.uId = (UINT_PTR)tti.id;
        ti.lpszText = (WCHAR*)ws;
        ti.rect = ToRECT(tti.r);
        ti.uFlags = TTF_SUBCLASS; // TODO: do I need this ?
        SendMessageW(hwnd, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }
}

static TempStr TooltipGetTextTemp(HWND hwnd, HWND owner, int id) {
    WCHAR buf[512];
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.lpszText = buf;
    SendMessageW(hwnd, TTM_GETTEXT, 512, (LPARAM)&ti);
    return ToUtf8Temp(buf);
}

static const int MULTILINE_INFOTIP_WIDTH_PX = 500;

static void SetMaxWidthForText(HWND hwnd, const char* s, bool multiline) {
    int dx = -1;
    if (multiline || str::FindChar(s, '\n')) {
        // TODO: dpi scale
        dx = MULTILINE_INFOTIP_WIDTH_PX;
    }
    SendMessageW(hwnd, TTM_SETMAXTIPWIDTH, 0, dx);
}

static bool TooltipUpdateText(HWND hwnd, HWND owner, int id, const char* s, bool multiline) {
    // avoid flickering
    char* s2 = TooltipGetTextTemp(hwnd, owner, id);
    if (str::Eq(s, s2)) {
        return false;
    }

    SetMaxWidthForText(hwnd, s, multiline);
    TempWStr ws = ToWStrTemp(s);
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.lpszText = (WCHAR*)ws;
    ti.uFlags = TTF_SUBCLASS; // TODO: do I need this ?
    SendMessageW(hwnd, TTM_UPDATETIPTEXT, 0, (LPARAM)&ti);
    return true;
}

void TooltipUpdateRect(HWND hwnd, HWND owner, int id, const Rect& rc) {
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = owner;
    ti.uId = (UINT_PTR)id;
    ti.rect = ToRECT(rc);
    SendMessageW(hwnd, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);
}

Tooltip::Tooltip() {
    kind = kindTooltip;
}

HWND Tooltip::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.className = TOOLTIPS_CLASS;
    cargs.font = args.font;
    cargs.style = WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP;
    cargs.exStyle = WS_EX_TOPMOST;

    parent = args.parent;

    Wnd::CreateControl(cargs);
    SetDelayTime(TTDT_AUTOPOP, 32767);
    return hwnd;
}
Size Tooltip::GetIdealSize() {
    // not used as this is top-level window
    return {100, 32};
}

void Tooltip::SetMaxWidth(int dx) {
    SendMessageW(hwnd, TTM_SETMAXTIPWIDTH, 0, dx);
}

int Tooltip::Add(const char* s, const Rect& rc, bool multiline) {
    int id = GetNextTooltipID();
    SetMaxWidthForText(hwnd, s, multiline);
    TempWStr ws = ToWStrTemp(s);
    TOOLINFOW ti = {};
    ti.cbSize = sizeof(ti);
    ti.hwnd = parent;
    ti.uId = (UINT_PTR)id;
    ti.uFlags = TTF_SUBCLASS;
    ti.rect = ToRECT(rc);
    ti.lpszText = (WCHAR*)ws;
    BOOL ok = SendMessageW(hwnd, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    if (!ok) {
        return -1;
    }
    tooltipIds.Append(id);
    return id;
}

TempStr Tooltip::GetTextTemp(int id) {
    return TooltipGetTextTemp(hwnd, parent, id);
}

void Tooltip::Update(int id, const char* s, const Rect& rc, bool multiline) {
    TooltipUpdateText(hwnd, parent, id, s, multiline);
    TooltipUpdateRect(hwnd, parent, id, rc);
}

// this assumes we only have at most one tool per this tooltip
int Tooltip::SetSingle(const char* s, const Rect& rc, bool multiline) {
    if (str::Len(s) > 256) {
        // pathological cases make for tooltips that take too long to display
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/2814
        s = str::JoinTemp(str::DupTemp(s, 256), "...");
    }
    int n = Count();
    // if want to use more tooltips, use Add() and Update()
    ReportIf(n > 1);
    if (n == 0) {
        return Add(s, rc, multiline);
    }
    int id = tooltipIds[0];
    Update(id, s, rc, multiline);
    return id;
}

int Tooltip::Count() {
    int n = TooltipGetCount(hwnd);
    int n2 = tooltipIds.Size();
    ReportIf(n != n2);
    return n;
}

void Tooltip::Delete(int id) {
    if (Count() == 0) {
        return;
    }

    int removeIdx = 0;
    if (id == 0) {
        // 0 means delete a single tool
        // should only be used if we only have single tool
        ReportIf(Count() > 1);
        id = tooltipIds[0];
    } else {
        removeIdx = tooltipIds.Find(id);
        ReportIf(removeIdx < 0);
    }

    TOOLINFOW ti{};
    ti.cbSize = sizeof(ti);
    ti.hwnd = parent;
    ti.uId = (UINT_PTR)id;
    int n1 = (int)SendMessageW(hwnd, TTM_GETTOOLCOUNT, 0, 0);
    SendMessageW(hwnd, TTM_DELTOOLW, 0, (LPARAM)&ti);
    int n2 = (int)SendMessageW(hwnd, TTM_GETTOOLCOUNT, 0, 0);
    ReportIf(n1 != n2 + 1);
    tooltipIds.RemoveAt(removeIdx);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/ttm-setdelaytime
// type is: TTDT_AUTOPOP, TTDT_INITIAL, TTDT_RESHOW, TTDT_AUTOMATIC
// timeInMs is max 32767 (~32 secs)
void Tooltip::SetDelayTime(int type, int timeInMs) {
    ReportIf(!IsValidDelayType(type));
    ReportIf(timeInMs < 0);
    ReportIf(timeInMs > 32767); // TODO: or is it 65535?
    SendMessageW(hwnd, TTM_SETDELAYTIME, type, (LPARAM)timeInMs);
}
