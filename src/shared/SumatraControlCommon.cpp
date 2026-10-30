/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "DocController.h"
#include "DocProperties.h"
#include "DocumentProperties.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "AppSettings.h"
#include "Flags.h"
#include "SumatraTest.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AnnotPlacement.h"
#include "WindowTab.h"
#include "TextSelection.h"
#include "Selection.h"
#include "SelectionHandlers.h"
#include "FileHistory.h"
#include "Favorites.h"
#include "PagePosition.h"
#include "SelectionTranslate.h"
#include "ImageSaveCropResize.h"
#include "base/GuessFileType.h"
#include "FindWindow.h"
#include "FindBar.h"
#include "Toolbar.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "SelectionToolbar.h"
#include "HomePage.h"
#include "Notifications.h"
#include "AIChatCommon.h"
#include "SumatraDialogs.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "Annotation.h"
#include "Menu.h"
#include "EngineAll.h"
#include "EutlTrust.h"
#include "CommandPalette.h"
#include "PdfTools.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"
#include "NavFilesInFolder.h"
#include "PerfLog.h"
#include "SumatraControl.h"
#include "SumatraControlCommon.h"

// Parts of the -dbg-control channel that orig and ng have in common. ng has no
// listener thread on wasm, so nothing calls them there.
#if !OS_WASM

// Boxes the current page actually declares (issue #814). Optional int arg is pageNo.
TempStr PageBoxesResultTemp(int pageNo, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("ERROR not-fixed-page"), 1);
    }
    if (pageNo < 1) {
        pageNo = win->ctrl->CurrentPageNo();
    }
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
    }
    Vec<PdfPageBox> boxes;
    engine->GetPdfPageBoxes(pageNo, boxes);
    str::Builder line;
    line.Append(fmt("OK page=%d show=%d", pageNo, win->showPageBoxes ? 1 : 0));
    for (const PdfPageBox& box : boxes) {
        line.Append(fmt(" %s=%.2f,%.2f,%.2f,%.2f", Str(PdfPageBoxName(box.kind)), box.rect.x, box.rect.y, box.rect.dx,
                        box.rect.dy));
    }
    RectF media = engine->PageMediabox(pageNo);
    RectF content = engine->PageContentBox(pageNo);
    line.Append(fmt(" engineMedia=%.2f,%.2f,%.2f,%.2f", media.x, media.y, media.dx, media.dy));
    line.Append(fmt(" engineContent=%.2f,%.2f,%.2f,%.2f", content.x, content.y, content.dx, content.dy));
    return finish(ToStrTemp(line), 0);
}

void AppendLayoutRect(str::Builder& out, Str name, bool visible, Rect rect) {
    out.Append(
        fmt("item name=%s visible=%d rect=%d,%d,%d,%d\n", name, visible ? 1 : 0, rect.x, rect.y, rect.dx, rect.dy));
}

// Expand SelectionHandlers placeholders against the current tab's selection
// (discussion #6015 ${selectionPosition}).
TempStr SelectionVarsResultTemp(Str pattern, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0 || !gWindows[0]) {
        return finish(StrL("NOTREADY no-window\n"), 2);
    }
    WindowTab* tab = gWindows[0]->CurrentTab();
    bool isTextOnly = false;
    TempStr sel = tab ? GetSelectedTextTemp(tab, StrL("\n"), isTextOnly) : TempStr{};
    if (len(sel) == 0) {
        sel = StrL("");
    }
    if (str::IsEmptyOrWhiteSpace(pattern)) {
        pattern = StrL("${selectionPosition}");
    }
    TempStr expanded = ExpandSelectionVarsTemp(pattern, sel, false, 0, nullptr, tab);
    out.Append(StrL("pattern="));
    out.Append(pattern);
    out.AppendChar('\n');
    out.Append(StrL("expanded="));
    out.Append(expanded);
    out.AppendChar('\n');
    if (tab && tab->selectionOnPage) {
        out.Append(fmt("nrects=%d\n", len(*tab->selectionOnPage)));
        for (SelectionOnPage& onPage : *tab->selectionOnPage) {
            RectF r = onPage.rect;
            out.Append(fmt("rect=%g,%g,%g,%g page=%d\n", r.x, r.y, r.dx, r.dy, onPage.pageNo));
            if (onPage.HasQuad()) {
                QuadF q = onPage.quad;
                out.Append(fmt("quad=%g,%g %g,%g %g,%g %g,%g\n", q.ul.x, q.ul.y, q.ur.x, q.ur.y, q.ll.x, q.ll.y, q.lr.x,
                               q.lr.y));
            }
        }
    } else {
        out.Append(StrL("nrects=0\n"));
    }
    return finish({}, 0);
}

TempStr DocumentSignaturesResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    EutlRegisterLookup();
    Props props;
    engine->GetProperties(props);
    Str sigs = GetPropValueTemp(props, DocProp::Signatures);
    if (len(sigs) == 0) {
        return finish(StrL("ERROR no-signatures"), 1);
    }
    return finish(str::DupTemp(sigs), 0);
}

TempStr DocumentFontListResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    TempStr fonts = engine->GetPropertyTemp(DocProp::FontList);
    if (len(fonts) == 0) {
        return finish(StrL("ERROR no-fonts"), 1);
    }
    return finish(fmt("OK fonts=%s", fonts), 0);
}

TempStr DocumentPropertiesResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    Props props;
    engine->GetProperties(props);
    str::Builder out;
    out.Append(StrL("OK"));
    int n = len(props);
    for (int i = 0; i < n; i++) {
        TempStr name = PropNameTemp(props[i].prop);
        if (len(name) == 0) {
            continue;
        }
        out.Append(StrL("\n"));
        out.Append(name);
        out.Append(StrL("="));
        out.Append(props[i].val);
    }
    // what Save As offers, and the sniffed type Properties shows
    out.Append(fmt("\ndefaultExt=%s", engine->defaultExt));
    FileType ft = GuessFileTypeFromFile(engine->FilePath());
    out.Append(fmt("\nfileTypeExt=%s", GetExtForFileTypeTemp(ft)));
    return finish(ToStrTemp(out), 0);
}

void DeleteControlArg(ControlArg* arg) {
    if (!arg) {
        return;
    }
    free(arg->bytes);
    str::FreePtr(&arg->str);
    if (arg->list) {
        for (ControlArg* el : *arg->list) {
            DeleteControlArg(el);
        }
        delete arg->list;
    }
    delete arg;
}

void AppendU16(str::Builder& s, u16 v) {
    u8 buf[2] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff)};
    s.Append(Str((char*)buf, (int)sizeof(buf)));
}

void AppendU32(str::Builder& s, u32 v) {
    u8 buf[4] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff), (u8)((v >> 16) & 0xff), (u8)((v >> 24) & 0xff)};
    s.Append(Str((char*)buf, (int)sizeof(buf)));
}

void AppendArgEnd(str::Builder& s) {
    AppendU16(s, (u16)ControlArgType::End);
}

void AppendArgInt(str::Builder& s, i32 v) {
    AppendU16(s, (u16)ControlArgType::Int32);
    AppendU32(s, (u32)v);
}

void AppendArgString(str::Builder& s, Str str) {
    if (len(str) == 0) {
        str = StrL("");
    }
    size_t n = (size_t)str.len;
    AppendU16(s, (u16)ControlArgType::String);
    AppendU32(s, (u32)n);
    s.Append(str);
    s.AppendChar(0);
}

void DeleteControlRequest(ControlRequest* req) {
    if (!req) {
        return;
    }
    for (ControlArg* arg : req->args) {
        DeleteControlArg(arg);
    }
    delete req;
}

static ControlArg* ArgAt(ControlRequest* req, size_t idx, ControlArgType type) {
    if (idx >= (size_t)len(req->args)) {
        return nullptr;
    }
    ControlArg* arg = req->args[(int)idx];
    if (arg->type != type) {
        return nullptr;
    }
    return arg;
}

Str StringArg(ControlRequest* req, size_t idx) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::String);
    return arg ? arg->str : Str{};
}

bool IntArg(ControlRequest* req, size_t idx, i32& valOut) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::Int32);
    if (!arg) {
        return false;
    }
    valOut = arg->intVal;
    return true;
}

void AppendError(ControlRequest* req, Str msg) {
    req->results.Reset();
    AppendArgInt(req->results, -1);
    AppendArgString(req->results, msg);
    AppendArgEnd(req->results);
}

void AppendTestResult(ControlRequest* req, int exitCode, Str result) {
    AppendArgInt(req->results, exitCode);
    AppendArgString(req->results, result);
    AppendArgEnd(req->results);
}

bool ParseArgList(PacketReader& r, Vec<ControlArg*>* args, bool explicitCount, u16 count) {
    for (u16 i = 0; !explicitCount || i < count; i++) {
        ControlArg* arg = nullptr;
        if (!ParseArg(r, &arg)) {
            return false;
        }
        if (!arg) {
            return !explicitCount;
        }
        VecAppend(*args, arg);
    }
    return true;
}

// Block on the control thread until visible tiles are cached at target
// resolution, or until timeoutMs. Optional first int arg is the timeout.
void RunWaitRenderIdle(ControlRequest* req) {
    i32 timeoutMs = 15000;
    IntArg(req, 0, timeoutMs);
    if (timeoutMs < 1) {
        timeoutMs = 1;
    }
    u64 deadline = GetTickCount64() + (u64)timeoutMs;
    for (;;) {
        req->done.Reset();
        uitask::Post(MkFunc0<ControlRequest>(SnapshotRenderIdle, req), "WaitRenderIdle");
        req->done.Wait();
        if (req->idleState == RenderIdleState::Idle) {
            AppendTestResult(req, 0, req->idleInfo[0] ? Str(req->idleInfo) : StrL("idle"));
            return;
        }
        if (GetTickCount64() >= deadline) {
            Str kind = req->idleState == RenderIdleState::NotReady ? StrL("timeout-notready") : StrL("timeout-busy");
            AppendTestResult(req, 1, req->idleInfo[0] ? fmt("%s %s", kind, Str(req->idleInfo)) : kind);
            return;
        }
        SleepInMs(20);
    }
}

// Block on the control thread until the restored session's tabs have loaded.
void RunWaitSessionRestored(ControlRequest* req) {
    i32 timeoutMs = 15000;
    IntArg(req, 0, timeoutMs);
    if (timeoutMs < 1) {
        timeoutMs = 1;
    }
    u64 deadline = GetTickCount64() + (u64)timeoutMs;
    for (;;) {
        req->done.Reset();
        uitask::Post(MkFunc0<ControlRequest>(SnapshotSessionRestore, req), "WaitSessionRestored");
        req->done.Wait();
        if (req->idleState == RenderIdleState::Idle) {
            AppendTestResult(req, 0, req->idleInfo[0] ? Str(req->idleInfo) : StrL("restored"));
            return;
        }
        if (GetTickCount64() >= deadline) {
            AppendTestResult(req, 1, req->idleInfo[0] ? fmt("timeout %s", Str(req->idleInfo)) : StrL("timeout"));
            return;
        }
        SleepInMs(20);
    }
}

#if OS_WIN

bool ReadExact(HANDLE h, void* data, DWORD n) {
    u8* d = (u8*)data;
    DWORD total = 0;
    while (total < n) {
        DWORD nRead = 0;
        if (!ReadFile(h, d + total, n - total, &nRead, nullptr) || nRead == 0) {
            return false;
        }
        total += nRead;
    }
    return true;
}

bool WriteExact(HANDLE h, Str data) {
    const u8* d = (const u8*)data.s;
    int total = 0;
    while (total < data.len) {
        DWORD nWritten = 0;
        if (!WriteFile(h, d + total, (DWORD)(data.len - total), &nWritten, nullptr) || nWritten == 0) {
            return false;
        }
        total += (int)nWritten;
    }
    return true;
}

static WStr FullPipeNameOwned(Str pipeName) {
    if (str::StartsWith(pipeName, StrL(R"(\\.\pipe\)"))) {
        return ToWStr(pipeName);
    }
    TempStr fullName = str::JoinTemp(StrL(R"(\\.\pipe\)"), pipeName);
    return ToWStr(fullName);
}

static WStr gControlPipeName;
static volatile LONG gControlStop = 0;
static HANDLE gControlListenerExited = nullptr;

struct ControlConnArg {
    HANDLE pipe;
};

// One thread per client: the async client stays connected while tests also
// make short synchronous requests (scroll position) on a second connection.
static void ControlConnThread(ControlConnArg* arg) {
    HANDLE pipe = arg->pipe;
    delete arg;
    bool stop = ProcessControlConnection(pipe);
    // DisconnectNamedPipe discards data the client hasn't read yet; wait
    // until it has, or the Quit reply is lost and the client sees EPIPE
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    if (!stop) {
        return;
    }
    // wake the listener blocked in ConnectNamedPipe so it can exit. It may be
    // between instances (no pipe to open yet) or all instances busy: retry
    InterlockedExchange(&gControlStop, 1);
    for (int i = 0; i < 300; i++) {
        if (WaitForSingleObject(gControlListenerExited, 0) == WAIT_OBJECT_0) {
            return;
        }
        HANDLE h = CreateFileW(gControlPipeName.s, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
        WaitForSingleObject(gControlListenerExited, 10);
    }
}

void SumatraControlThread(ControlThreadArg* arg) {
    gControlPipeName = FullPipeNameOwned(arg->pipeName);
    str::FreePtr(&arg->pipeName);
    delete arg;
    gControlListenerExited = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    constexpr DWORD kMaxConns = 8;
    while (!gControlStop) {
        HANDLE pipe =
            CreateNamedPipeW(gControlPipeName.s, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                             kMaxConns, 64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            // all kMaxConns instances connected: wait for one to disconnect
            if (err == ERROR_PIPE_BUSY) {
                SleepInMs(50);
                continue;
            }
            logf("CreateNamedPipeW failed for control pipe, err=%u\n", (unsigned)err);
            break;
        }
        BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected || gControlStop) {
            CloseHandle(pipe);
            continue;
        }
        RunAsync(MkFunc0(ControlConnThread, new ControlConnArg{pipe}), StrL("SumatraControlConn"));
    }
    SetEvent(gControlListenerExited);
}

#endif // OS_WIN

ControlRequest* ReadControlRequest(ControlConn h) {
    u32 size = 0;
    if (!ReadExact(h, &size, sizeof(size))) {
        return nullptr;
    }
    if (size < 4 || size > 16 * 1024 * 1024) {
        return nullptr;
    }
    u8* data = AllocArray<u8>((int)size);
    if (!ReadExact(h, data, size)) {
        free(data);
        return nullptr;
    }

    PacketReader r{data, size};
    ControlRequest* req = new ControlRequest();
    if (!r.ReadU16(req->cmd) || !r.ReadU16(req->reqId) || !ParseArgList(r, &req->args, false)) {
        DeleteControlRequest(req);
        free(data);
        return nullptr;
    }
    free(data);
    return req;
}

ControlServeResult ServeControlRequest(ControlConn h, ControlExecFn execOnUi) {
    ControlServeResult res;
    ControlRequest* req = ReadControlRequest(h);
    if (!req) {
        return res;
    }
    res.hadRequest = true;
    res.isQuit = ControlCmdEndsListener(req->cmd);
    if (req->cmd == (u16)ControlFrameCmd::CrashMe) {
        log(StrL("ControlCmd::CrashMe\n"));
        CrashMe();
    }
    switch (ControlFrameActionFor(req->cmd)) {
        case ControlFrameAction::WaitRenderIdle:
            RunWaitRenderIdle(req);
            break;
        case ControlFrameAction::WaitSessionRestored:
            RunWaitSessionRestored(req);
            break;
        case ControlFrameAction::RunOnUi:
            if (execOnUi) {
                execOnUi(req);
            }
            break;
    }
    res.wrote = WriteControlResponse(h, req);
    DeleteControlRequest(req);
    return res;
}

bool WriteControlResponse(ControlConn h, ControlRequest* req) {
    str::Builder payload;
    AppendU16(payload, req->reqId);
    payload.Append(ToStr(req->results));

    str::Builder packet;
    AppendU32(packet, (u32)len(payload));
    packet.Append(ToStr(payload));
    return WriteExact(h, ToStr(packet));
}

// "OK page=<n> x=<x> y=<y>": the first window's scroll position, in page units
TempStr ScrollStateResultTemp(int* exitCodeOut) {
    *exitCodeOut = 2;
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    DisplayModel* dm = win && win->IsDocLoaded() ? win->AsFixed() : nullptr;
    if (!dm) {
        return fmt("NOTREADY no-doc\n");
    }
    ScrollState ss = dm->GetScrollState();
    *exitCodeOut = 0;
    return fmt("OK page=%d x=%g y=%g\n", ss.page, ss.x, ss.y);
}

TempStr FavoriteNavResultTemp(Str action, int pageNo, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }

    TempStr menuIds;
    if (str::EqI(action, StrL("add"))) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        AddFavoriteSilent(win, pageNo);
    } else if (str::EqI(action, StrL("goto"))) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        win->ctrl->GoToPage(pageNo, true);
    } else if (str::EqI(action, StrL("goto-fav"))) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        FileState* fs = FileHistoryFindByPath(win->ctrl->GetFilePath());
        Favorite* fav = nullptr;
        if (fs && fs->favorites) {
            for (Favorite* f : *fs->favorites) {
                if (ParseStoredPagePos(f->pageNo).pageNo == pageNo) {
                    fav = f;
                    break;
                }
            }
        }
        if (!fav) {
            return finish(fmt("ERROR no-fav page=%d", pageNo), 1);
        }
        JumpToFavorite(win, fav);
    } else if (str::EqI(action, StrL("next"))) {
        GoToNextFavorite(win, true);
    } else if (str::EqI(action, StrL("prev"))) {
        GoToNextFavorite(win, false);
    } else if (str::EqI(action, StrL("page"))) {
        // report only
    } else if (ControlFavoritesMenu(win, action, &menuIds)) {
        return finish(menuIds, 0);
    } else {
        return finish(fmt("ERROR unknown-action action=%s", action), 1);
    }

    int cur = win->ctrl->CurrentPageNo();
    int y = -1;
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        ScrollState ss = dm->GetScrollState();
        y = (int)ss.y;
        cur = ss.page;
    }
    return finish(fmt("OK page=%d y=%d", cur, y), 0);
}

// action: "get" | "r2l" | "presentation" | "fullscreen"
// Reports the current page layout and whether presentation / windowed
// fullscreen is on. presentation/fullscreen toggle that mode first.
TempStr DisplayModeResultTemp(Str action, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }

    bool reportR2L = str::EqI(action, StrL("r2l"));
    if (str::EqI(action, StrL("zoom-real"))) {
        return finish(fmt("OK zoomReal=%g", win->ctrl->GetZoomVirtual(true)), 0);
    }
    if (len(action) == 0 || str::EqI(action, StrL("get")) || reportR2L) {
        // report only
    } else if (str::EqI(action, StrL("presentation"))) {
        ControlTogglePresentation(win);
    } else if (str::EqI(action, StrL("fullscreen"))) {
        ControlToggleFullScreen(win);
    } else {
        return finish(fmt("ERROR unknown-action action=%s", action), 1);
    }

    if (reportR2L) {
        DisplayModel* dm = win->AsFixed();
        if (!dm) {
            return finish(StrL("ERROR not-fixed-page"), 1);
        }
        AppCommandCtx ctx = NewAppCommandCtx(win);
        bool available =
            GetCommandVisibility(CmdToggleMangaMode, ctx, CommandSurface::Palette) == CommandVisibility::Show;
        return finish(fmt("OK r2l=%d available=%d", dm->GetDisplayR2L() ? 1 : 0, available ? 1 : 0), 0);
    }

    Str mode = DisplayModeToString(win->ctrl->GetDisplayMode());
    Str zoomLabel;
    ZoomToString(&zoomLabel, win->ctrl->GetZoomVirtual(false), nullptr);
    TempStr res = fmt("OK mode=%s presentation=%d fullscreen=%d zoom=%s", mode, win->InPresentation() ? 1 : 0,
                      win->isFullScreen ? 1 : 0, zoomLabel);
    str::Free(zoomLabel);
    return finish(res, 0);
}

#endif // !OS_WASM
