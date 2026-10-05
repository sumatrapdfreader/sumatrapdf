/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// `-dbg-control <pipe>`: the automation channel orig drives its scripted tests
// with. The wire protocol (length-prefixed packets of typed arguments over a
// named pipe), the command numbering and the request loop are orig's, so the
// same client talks to both.
//
// ng: orig answers 100 commands, most of them through helpers in
// SumatraTest.cpp / the win32 layout probes, neither of which this port has
// (the matrix drops SumatraTest.cpp). Ported here are the commands whose
// answer this port can give - which is what finally calls the `*ResultTemp`
// hooks earlier steps added and left without a caller. The numbering is orig's
// so the rest can be filled in later; an unimplemented one answers
// "NOTPORTED <n>" instead of silently doing nothing.

#include "base/Base.h"
#include "base/UITask.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#if OS_WIN
#include "base/Win.h"
#elif !OS_WASM
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#endif
#include "base/CrashHandler.h"

#include "gui/UIModels.h"
#include "ProgressUpdateUI.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "Commands.h"
#include "CommandPalette.h"
#include "CommandAvailability.h"
#include "Menu.h"
#include "AppSettings.h"
#include "Flags.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "FileHistory.h"
#include "Favorites.h"
#include "PagePosition.h"
#include "SelectionTranslate.h"
#include "SearchAndDDE.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "Notifications.h"
#include "AIChatCommon.h"
#include "EutlTrust.h"
#include "PdfTools.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "gui/NativeFileDlg.h"
#include "gui/NativeMsgBox.h"
#include "gui/DocCanvas.h"
#include "gui/Sidebar.h"
#include "SumatraDialogs.h"
#include "NavFilesInFolder.h"
#include "AnnotFilterToolbar.h"
#include "PerfLog.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "gui/OleDragDrop.h"
#include "gui/NativeCursors.h"
#include "SumatraControl.h"

#include "SumatraLog.h"

static void AppendLayoutRect(str::Builder& out, Str name, bool visible, Rect rect) {
    out.Append(
        fmt("item name=%s visible=%d rect=%d,%d,%d,%d\n", name, visible ? 1 : 0, rect.x, rect.y, rect.dx, rect.dy));
}

// ng: the channel needs a listener thread, which wasm does not have
#if !OS_WASM

// action: "add" | "goto" | "goto-fav" | "next" | "prev" | "page"
static TempStr FavoriteNavResultTemp(Str action, int pageNo, int* exitCodeOut) {
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

    if (str::EqI(action, StrL("add"))) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        // ng: orig has a silent variant that skips the name dialog;
        // ApplyAddFavorite is that path here
        TempStr label = win->ctrl->GetPageLabeTemp(pageNo);
        ApplyAddFavorite(win, win->ctrl->GetFilePath(), pageNo, label, {});
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
static TempStr DisplayModeResultTemp(Str action, int* exitCodeOut) {
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
    if (len(action) == 0 || str::EqI(action, StrL("get")) || reportR2L) {
        // report only
    } else if (str::EqI(action, StrL("presentation"))) {
        // ng: orig calls ToggleFullScreen() directly; the port routes both
        // through their commands
        ExecuteCmd(win, CmdTogglePresentationMode);
    } else if (str::EqI(action, StrL("fullscreen"))) {
        ExecuteCmd(win, CmdToggleFullscreen);
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

// Boxes the current page actually declares (issue #814). Optional int arg is pageNo.
static TempStr PageBoxesResultTemp(int pageNo, int* exitCodeOut) {
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
    return finish(ToStrTemp(line), 0);
}

static TempStr DocumentSignaturesResultTemp(int* exitCodeOut) {
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

static TempStr DocumentFontListResultTemp(int* exitCodeOut) {
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

static TempStr DocumentPropertiesResultTemp(int* exitCodeOut) {
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
    int n = PropsCount(props);
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
    return finish(ToStrTemp(out), 0);
}

enum class ControlCmd : u16 {
    Ping = 1,
    Quit = 2,
    TestSynctex = 10,
    TestSearch = 11,
    TestDest = 12,
    TestNamedDest = 13,
    TestChm = 14,
    TestSelectionTranslate = 15,
    TestTripleClickLineSelect = 16,
    TestContextMenuSelection = 17,
    TestGoToFindMatch = 18,
    // IDs 19-21 unused (reserved on the -dbg-control wire protocol; do not renumber).
    // Assign new test commands starting at 23.
    TestInverseSearch = 22,
    TestImageResizeArrowKey = 23,
    TestFindResultPageColumnClip = 24,
    TestFileKind = 25,
    TestScrollToLink = 26,
    TestI18nErrorString = 27,
    TestPageInfoOverlay = 28,
    TestGetToc = 29,
    TestPageLinks = 30,
    TestWindowStateDuringLoad = 31,
    TestTocNavigate = 32,
    TestMarkdownTocNavigate = 33,
    TestFavoriteNav = 34,
    TestToolbarButtons = 35,
    TestKeyboardLinkFollow = 36,
    TestFindResultsOrder = 37,
    TestClickClearsSelection = 38,
    TestRectSelectionDrag = 39,
    TestSelectTextKeyboard = 40,
    TestAIChat = 41,
    TestAIChatReplay = 42,
    TestMarkdownFollowLink = 43,
    TestHomeListRows = 44,
    TestPageComments = 45,
    TestAdvSettingsRows = 46,
    TestDestZoomNav = 47,
    TestAnnotEditorLayout = 48,
    TestDisplayMode = 49,
    TestSidebarLayout = 50,
    TestCadEnhanceColors = 51,
    TestFindPageRange = 52,
    TestDocumentFontList = 53,
    WaitRenderIdle = 54,
    SetNotificationsEnabled = 55,
    TestHomeSelection = 56,
    TestImageRenderEdges = 57,
    TestInsertImage = 58,
    TestRenderPageColors = 59,
    TestListSigningCerts = 60,
    TestSignDocument = 61,
    TestGetPolicies = 62,
    TestPageBoxes = 63,
    TestDocumentSignatures = 64,
    TestCommandPalette = 65,
    TestFindHistory = 66,
    TestImageResizeEdges = 67,
    TestLinkDestHighlight = 68,
    TestConvertToImages = 69,
    TestLayout = 70,
    TestDpi = 71,
    TestSelectionVars = 72,
    TestSelectionToolbar = 73,
    TestMarkupAnnots = 74,
    TestCmykImageSave = 75,
    TestContextMenuPoint = 76,
    TestFindWindowContents = 77,
    TestFindUiState = 78,
    TestRenderViewPrint = 79,
    TestReadAloudPlaybackBar = 80,
    TestRotatedTextMouseDrag = 81,
    TestChapterInfo = 82,
    TestGoToLocation = 83,
    TestTocSidebarNav = 84,
    TestSelectionSurvivesRenumber = 85,
    TestConvertToPdf = 86,
    TestInvokeCommand = 87,
    TestCurrentTab = 88,
    TestCommandVisibility = 89,
    TestExtractPages = 90,
    TestAnnotFilter = 91,
    TestCanvasFlags = 92,
    CrashMe = 93,
    TestDocumentProperties = 94,
    TestHiddenTabGoToPage = 95,
    TestSaveSelectionAsImage = 96,
    TestReadingAutoScroll = 97,
    TestReadingBar = 98,
    TestSeedTextSelection = 99,
    TestTtsEngineCrash = 100,
    // ng: not one of orig's; the performance snapshot cmd/port-perf.ts reads
    TestPerfStats = 101,
    // ng: not one of orig's; the state of the six canvas overlays
    TestOverlayState = 102,
    // ng: not one of orig's; answers the open save-path dialog, which orig
    // does not have (it uses GetSaveFileNameW, which no script can drive)
    TestSavePathDialog = 103,
    TestNavFiles = 104,
    TestMergePdf = 115,
    TestMainMenu = 117,
    // ng: shows the "no longer the default app" bar for the given extensions
    // (".pdf,.epub"); the real check needs an installation and a UserChoice
    TestDefaultAppNotif = 118,
    // ng: orig's 101 / 102, which the port had given to its own test commands
    StartPerfLog = 119,
    StopPerfLog = 120,
    // ng: not orig's; gpui input with modifiers / hover / a held button, and
    // one line of shell state to check it against
    TestInput = 121,
    TestUiState = 122,
    // ng: Windows; exercises the OLE drag source / drop target in-process
    TestOleDragDrop = 123,
    // ng: Windows; the system file dialogs: off / on / state / cancel / accept
    TestNativeFileDlg = 124,
    // ng: not one of orig's; the tool windows (gui/ToolWindow.h) and input
    // into one of them
    TestToolWindow = 125,
    // ng: Windows; the system message boxes: on / off / state / answer <id>
    TestNativeMsgBox = 126,
};

enum class ControlArgType : u16 {
    End = 0,
    Int32 = 1,
    Bytes = 2,
    String = 3,
    List = 4,
};

struct ControlArg {
    ControlArgType type = ControlArgType::End;
    i32 intVal = 0;
    u8* bytes = nullptr;
    u32 bytesLen = 0;
    Str str;
    Vec<ControlArg*>* list = nullptr;
};

static void DeleteControlArg(ControlArg* arg) {
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

enum class RenderIdleState : u8 {
    NotReady = 0,
    Busy = 1,
    Idle = 2,
};

// ng: orig's manual-reset event (CreateEventW), from the portable primitives
struct DoneEvent {
    Mutex mutex;
    ConditionVariable cond;
    bool isSet = false;

    void Set() {
        mutex.Lock();
        isSet = true;
        mutex.Unlock();
        cond.WakeAll();
    }
    void Reset() {
        mutex.Lock();
        isSet = false;
        mutex.Unlock();
    }
    void Wait() {
        mutex.Lock();
        while (!isSet) {
            cond.Wait(&mutex);
        }
        mutex.Unlock();
    }
};

struct ControlRequest {
    u16 cmd = 0;
    u16 reqId = 0;
    Vec<ControlArg*> args;
    str::Builder results;
    DoneEvent done;
    RenderIdleState idleState = RenderIdleState::NotReady;
    char idleInfo[320]{};
};

static void DeleteControlRequest(ControlRequest* req) {
    if (!req) {
        return;
    }
    for (ControlArg* arg : req->args) {
        DeleteControlArg(arg);
    }
    delete req;
}

struct PacketReader {
    const u8* data = nullptr;
    size_t size = 0;
    size_t pos = 0;

    bool ReadU16(u16& v) {
        if (pos + 2 > size) {
            return false;
        }
        v = (u16)(data[pos] | (data[pos + 1] << 8));
        pos += 2;
        return true;
    }

    bool ReadU32(u32& v) {
        if (pos + 4 > size) {
            return false;
        }
        v = (u32)data[pos] | ((u32)data[pos + 1] << 8) | ((u32)data[pos + 2] << 16) | ((u32)data[pos + 3] << 24);
        pos += 4;
        return true;
    }

    bool ReadBytes(u8* dst, size_t n) {
        if (pos + n > size) {
            return false;
        }
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    }
};

static void AppendU16(str::Builder& s, u16 v) {
    u8 buf[2] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff)};
    s.Append(Str((char*)buf, (int)sizeof(buf)));
}

static void AppendU32(str::Builder& s, u32 v) {
    u8 buf[4] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff), (u8)((v >> 16) & 0xff), (u8)((v >> 24) & 0xff)};
    s.Append(Str((char*)buf, (int)sizeof(buf)));
}

static void AppendArgEnd(str::Builder& s) {
    AppendU16(s, (u16)ControlArgType::End);
}

static void AppendArgInt(str::Builder& s, i32 v) {
    AppendU16(s, (u16)ControlArgType::Int32);
    AppendU32(s, (u32)v);
}

static void AppendArgString(str::Builder& s, Str str) {
    if (len(str) == 0) {
        str = StrL("");
    }
    size_t n = (size_t)str.len;
    AppendU16(s, (u16)ControlArgType::String);
    AppendU32(s, (u32)n);
    s.Append(str);
    s.AppendChar(0);
}

static bool ParseArg(PacketReader& r, ControlArg** argOut);

static bool ParseArgList(PacketReader& r, Vec<ControlArg*>* args, bool explicitCount, u16 count = 0) {
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

static bool ParseArg(PacketReader& r, ControlArg** argOut) {
    u16 typeRaw = 0;
    if (!r.ReadU16(typeRaw)) {
        return false;
    }
    ControlArgType type = (ControlArgType)typeRaw;
    if (type == ControlArgType::End) {
        *argOut = nullptr;
        return true;
    }

    ControlArg* arg = new ControlArg();
    arg->type = type;
    if (type == ControlArgType::Int32) {
        u32 v = 0;
        if (!r.ReadU32(v)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->intVal = (i32)v;
        *argOut = arg;
        return true;
    }
    if (type == ControlArgType::Bytes || type == ControlArgType::String) {
        u32 n = 0;
        if (!r.ReadU32(n)) {
            DeleteControlArg(arg);
            return false;
        }
        u32 extra = (type == ControlArgType::String) ? 1 : 0;
        u8* data = AllocArray<u8>((int)(n + 1));
        if (!r.ReadBytes(data, (size_t)n + extra)) {
            free(data);
            DeleteControlArg(arg);
            return false;
        }
        if (type == ControlArgType::String) {
            arg->str = str::Dup(Str((char*)data, (int)n));
            free(data);
        } else {
            arg->bytes = data;
            arg->bytesLen = n;
        }
        *argOut = arg;
        return true;
    }
    if (type == ControlArgType::List) {
        u16 n = 0;
        if (!r.ReadU16(n)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->list = new Vec<ControlArg*>();
        if (!ParseArgList(r, arg->list, true, n)) {
            DeleteControlArg(arg);
            return false;
        }
        *argOut = arg;
        return true;
    }
    DeleteControlArg(arg);
    return false;
}

static ControlArg* ArgAt(ControlRequest* req, size_t idx, ControlArgType type) {
    if (idx >= (size_t)len(req->args)) {
        return nullptr;
    }
    ControlArg* arg = req->args[(int)idx];
    return arg->type == type ? arg : nullptr;
}

static Str StringArg(ControlRequest* req, size_t idx) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::String);
    return arg ? arg->str : Str();
}

static bool IntArg(ControlRequest* req, size_t idx, i32& valOut) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::Int32);
    if (!arg) {
        return false;
    }
    valOut = arg->intVal;
    return true;
}

static void AppendError(ControlRequest* req, Str msg) {
    req->results.Reset();
    AppendArgInt(req->results, -1);
    AppendArgString(req->results, msg);
    AppendArgEnd(req->results);
}

static void AppendTestResult(ControlRequest* req, int exitCode, Str result) {
    AppendArgInt(req->results, exitCode);
    AppendArgString(req->results, result);
    AppendArgEnd(req->results);
}

static MainWindow* FirstWindow() {
    return len(gWindows) > 0 ? gWindows[0] : nullptr;
}

static void ExecuteControlRequest(ControlRequest* req) {
    switch ((ControlCmd)req->cmd) {
        case ControlCmd::Ping:
            AppendArgString(req->results, StrL("pong"));
            AppendArgEnd(req->results);
            break;

        case ControlCmd::Quit:
            AppendArgInt(req->results, 0);
            AppendArgEnd(req->results);
            AppShellQuit();
            break;

        // A notification covers part of the document for a couple of seconds,
        // so a test that reads pixels either waits it out or turns them off.
        case ControlCmd::SetNotificationsEnabled: {
            i32 enabled = 0;
            if (!IntArg(req, 0, enabled)) {
                AppendError(req, StrL("SetNotificationsEnabled expects int enabled"));
                break;
            }
            SetNotificationsEnabled(enabled != 0);
            AppendTestResult(req, 0, enabled ? StrL("OK enabled") : StrL("OK disabled"));
            break;
        }

        case ControlCmd::TestFileKind: {
            Str path = StringArg(req, 0);
            Str expectedKind = StringArg(req, 1);
            if (len(path) == 0 || len(expectedKind) == 0) {
                AppendError(req, StrL("TestFileKind expects string path, string expectedKind"));
                break;
            }
            int exitCode = 0;
            Str res = FileKindResultTemp(path, expectedKind, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectionTranslate: {
            i32 backend = 0;
            Str srcLang = StringArg(req, 1);
            Str dstLang = StringArg(req, 2);
            Str text = StringArg(req, 3);
            if (!IntArg(req, 0, backend) || len(text) == 0) {
                AppendError(req, StrL("TestSelectionTranslate expects int backend, strings src, dst, text"));
                break;
            }
            int exitCode = 0;
            Str res = SelectionTranslateResultTemp(backend, srcLang, dstLang, text, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFavoriteNav: {
            i32 pageNo = 0;
            IntArg(req, 1, pageNo);
            int exitCode = 0;
            Str res = FavoriteNavResultTemp(StringArg(req, 0), pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestKeyboardLinkFollow: {
            int exitCode = 0;
            Str res = KeyboardLinkFollowResultTemp(StringArg(req, 0), StringArg(req, 1), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectTextKeyboard: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            AppendTestResult(req, 0, SelectTextKeyboardResultTemp(win));
            break;
        }

        case ControlCmd::TestAIChat: {
            i32 backend = 0;
            Str filePath = StringArg(req, 1);
            Str message = StringArg(req, 2);
            if (!IntArg(req, 0, backend) || len(message) == 0) {
                AppendError(req, StrL("TestAIChat expects int backend, string file, string message"));
                break;
            }
            int exitCode = 0;
            Str res = AIChatTestResultTemp(backend, filePath, message, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAIChatReplay: {
            int exitCode = 0;
            Str res = AIChatTestReplayResultTemp(StringArg(req, 0), StringArg(req, 1), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDisplayMode: {
            int exitCode = 0;
            Str res = DisplayModeResultTemp(StringArg(req, 0), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDocumentFontList: {
            int exitCode = 0;
            Str res = DocumentFontListResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGetPolicies: {
            str::Builder out;
            out.Append(fmt("restricted=%d\n", HasPermission(Perm::RestrictedUse) ? 1 : 0));
            out.Append(fmt("internet=%d\n", HasPermission(Perm::InternetAccess) ? 1 : 0));
            out.Append(fmt("disk=%d\n", HasPermission(Perm::DiskAccess) ? 1 : 0));
            out.Append(fmt("prefs=%d\n", HasPermission(Perm::SavePreferences) ? 1 : 0));
            out.Append(fmt("registry=%d\n", HasPermission(Perm::RegistryAccess) ? 1 : 0));
            out.Append(fmt("printer=%d\n", HasPermission(Perm::PrinterAccess) ? 1 : 0));
            out.Append(fmt("copy=%d\n", HasPermission(Perm::CopySelection) ? 1 : 0));
            out.Append(fmt("fullscreen=%d\n", HasPermission(Perm::FullscreenAccess) ? 1 : 0));
            AppendTestResult(req, 0, ToStrTemp(out));
            break;
        }

        case ControlCmd::TestPageBoxes: {
            i32 pageNo = 0;
            IntArg(req, 0, pageNo);
            int exitCode = 0;
            Str res = PageBoxesResultTemp(pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDocumentSignatures: {
            int exitCode = 0;
            Str res = DocumentSignaturesResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindHistory: {
            int exitCode = 0;
            Str res = FindHistoryResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindWindowContents: {
            i32 maxRows = 0;
            IntArg(req, 0, maxRows);
            int exitCode = 0;
            Str res = FindWindowContentsResultTemp(maxRows, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestLinkDestHighlight: {
            int exitCode = 0;
            Str res = LinkDestHighlightResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindUiState: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            AppendTestResult(req, 0, FindStateResultTemp(win));
            break;
        }

        case ControlCmd::TestConvertToImages: {
            Str tmpl = StringArg(req, 0);
            Str pages = StringArg(req, 1);
            if (len(tmpl) == 0) {
                AppendError(req, StrL("TestConvertToImages expects string template, string pages"));
                break;
            }
            // optional; 0 means the dialog's default
            i32 dpi = 0;
            IntArg(req, 2, dpi);
            int exitCode = 0;
            Str res = ConvertPagesToImagesResultTemp(tmpl, pages, dpi, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestConvertToPdf: {
            Str src = StringArg(req, 0);
            Str dst = StringArg(req, 1);
            if (len(src) == 0 || len(dst) == 0) {
                AppendError(req, StrL("TestConvertToPdf expects string srcPath, string destPath"));
                break;
            }
            int exitCode = 0;
            Str res = ConvertImageCollectionToPdfResultTemp(src, dst, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestExtractPages: {
            i32 annotsOnly = 0;
            Str dst = StringArg(req, 0);
            Str pages = StringArg(req, 1);
            IntArg(req, 2, annotsOnly);
            if (len(dst) == 0) {
                AppendError(req, StrL("TestExtractPages expects string destPath, string pages, int annotsOnly"));
                break;
            }
            int exitCode = 0;
            Str res = ExtractPdfPagesResultTemp(dst, pages, annotsOnly, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSaveSelectionAsImage: {
            i32 dpi = 0, pageNo = 0, x = 0, y = 0, dx = 0, dy = 0;
            Str dst = StringArg(req, 0);
            if (len(dst) == 0 || !IntArg(req, 1, dpi) || !IntArg(req, 2, pageNo) || !IntArg(req, 3, x) ||
                !IntArg(req, 4, y) || !IntArg(req, 5, dx) || !IntArg(req, 6, dy)) {
                AppendError(req, StrL("TestSaveSelectionAsImage expects string dest, int dpi, page, x, y, dx, dy"));
                break;
            }
            int exitCode = 0;
            Str res = SaveSelectionAsImageResultTemp(dst, dpi, pageNo, x, y, dx, dy, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMergePdf: {
            // action, arg, n: see MergePdfResultTemp()
            Str action = StringArg(req, 0);
            Str arg = StringArg(req, 1);
            i32 n = 0;
            IntArg(req, 2, n);
            int exitCode = 0;
            Str res = MergePdfResultTemp(action, arg, n, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestInvokeCommand: {
            Str name = StringArg(req, 0);
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            int cmdId = GetCommandIdByName(name);
            if (cmdId <= 0 && str::IndexOfChar(name, ' ') >= 0) {
                CustomCommand* custom = CreateCommandFromDefinition(name);
                if (custom) {
                    cmdId = custom->id;
                }
            }
            if (cmdId <= 0) {
                AppendError(req, StrL("TestInvokeCommand expects a command name"));
                break;
            }
            i32 x = 0;
            i32 y = 0;
            // ng: an optional fourth argument is the index of the main window
            // the command is for (with -1 -1 for "no point")
            i32 winIdx = 0;
            if (IntArg(req, 3, winIdx) && winIdx > 0 && winIdx < len(gWindows)) {
                win = gWindows[winIdx];
            }
            if (IntArg(req, 1, x) && IntArg(req, 2, y) && x >= 0 && y >= 0) {
                ExecuteCmdAtPoint(win, cmdId, Point{x, y});
            } else {
                ExecuteCmd(win, cmdId);
            }
            AppendTestResult(req, 0, fmt("OK handled=%d", gLastCmdFellThrough ? 0 : 1));
            break;
        }

        // ng: orig reports the win32 child window rects. The shell has no
        // child windows, so these are the rects it lays the same pieces out in
        // (same names, same format, so one script can diff the two exes)
        case ControlCmd::TestLayout: {
            // ng: with the name of an open tool window, that window's layout
            if (ToolWindowFind(StringArg(req, 0))) {
                AppendTestResult(req, 0, ToolWindowTestTemp(StrL("layout"), StringArg(req, 0), Str{}, 0, 0, 0, 0));
                break;
            }
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            str::Builder out;
            out.Append(StrL("OK count=0 watching=0\n"));
            // ng: the window's screen rect and the two modes that hide the
            // chrome, so a sweep can check fullscreen without a screenshot
            Rect scr = AppShellWindowScreenRect(win);
            out.Append(fmt("window rect=%d,%d,%d,%d maximized=%d fullscreen=%d presentation=%d\n", scr.x, scr.y, scr.dx,
                           scr.dy, win->isMaximized ? 1 : 0, win->isFullScreen ? 1 : 0, (int)win->presentation));
            Rect frame = win->frameRc;
            Rect canvas = win->canvasRc;
            int menuDy = (win->isMenuBarVisible && !AppShellNativeMenu()) ? kMenuBarDy : 0;
            int tabsDy = TabsAreVisible(win) ? kTabBarDy : 0;
            int toolbarDy = ToolbarDy(win);
            int sidebarDx = (win->uiState.tocVisible || win->uiState.favVisible) ? win->sidebarDx : 0;
            AppendLayoutRect(out, StrL("frame"), true, frame);
            AppendLayoutRect(out, StrL("canvas"), true, canvas);
            Rect menuRc{0, 0, frame.dx, menuDy};
            Rect tabsRc{0, menuDy, frame.dx, tabsDy};
            int topDy = menuDy + tabsDy;
            // orig's tabsInTitlebar: both are in the caption
            AppShellCaptionRects(win, &menuRc, &tabsRc, &topDy);
            AppendLayoutRect(out, StrL("menu"), menuDy > 0, menuRc);
            AppendLayoutRect(out, StrL("tabs"), tabsDy > 0, tabsRc);
            int border = AppShellFrameBorder(win);
            AppendLayoutRect(out, StrL("toolbar"), toolbarDy > 0,
                             Rect{border, topDy, frame.dx - 2 * border, toolbarDy});
            WindowTab* tab = win->CurrentTab();
            SidebarContent topContent = tab ? tab->sidebarContent : SidebarContent::Bookmarks;
            AppendLayoutRect(out, SidebarContentToStr(topContent), win->uiState.tocVisible,
                             Rect{0, topDy, sidebarDx, canvas.dy});
            AppendLayoutRect(out, SidebarContentToStr(win->sidebarBottomContent), win->uiState.favVisible,
                             Rect{0, topDy, sidebarDx, canvas.dy});
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                out.Append(
                    fmt("pages count=%d spacing=%d,%d\n", dm->PageCount(), dm->pageSpacing.dx, dm->pageSpacing.dy));
                int n = dm->PageCount() < 8 ? dm->PageCount() : 8;
                for (int pageNo = 1; pageNo <= n; pageNo++) {
                    PageInfo* pi = dm->GetPageInfo(pageNo);
                    if (!pi) {
                        continue;
                    }
                    Rect p = pi->pos;
                    Rect s = pi->pageOnScreen;
                    out.Append(fmt("page n=%d shown=%d pos=%d,%d,%d,%d screen=%d,%d,%d,%d\n", pageNo,
                                   pi->isShown ? 1 : 0, p.x, p.y, p.dx, p.dy, s.x, s.y, s.dx, s.dy));
                }
            }
            AppendTestResult(req, 0, ToStrTemp(out));
            break;
        }

        case ControlCmd::TestPerfStats: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            i32 reset = 0;
            IntArg(req, 0, reset);
            TempStr frames = AppShellFrameStatsTemp(win, reset != 0);
            double wrapMs = 0;
            int wrapCount = 0;
            CanvasBmpWrapStats(wrapMs, wrapCount);
            int nEntries = 0;
            i64 cacheBytes = gRenderCache ? gRenderCache->CacheBytes(nEntries) : 0;
            AppendTestResult(
                req, 0,
                fmt("%s bmpWrapMs=%.0f bmpWrapCount=%d cacheEntries=%d/%d cacheKB=%d firstPaintMs=%.0f", frames, wrapMs,
                    wrapCount, nEntries, kMaxBitmapsCached, (int)(cacheBytes / 1024), CanvasFirstPaintMs()));
            break;
        }

        // ng: the six canvas overlays and the two tips are drawn, so there is
        // nothing to read back from the model; this reports their flags and
        // the text of the two notifications instead of a screenshot
        case ControlCmd::TestReadAloudPlaybackBar: {
            // ng: an optional "show" / "hide" (ReadAloudPlaybackBarTestTemp)
            int exitCode = 0;
            Str res = ReadAloudPlaybackBarTestTemp(StringArg(req, 0), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestReadingAutoScroll: {
            int exitCode = 0;
            Str res = ReadingAutoScrollBarStateTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestOverlayState: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            NotificationWnd* pageInfo = GetNotificationForGroup(win, kNotifPageInfo);
            NotificationWnd* cursorPos = GetNotificationForGroup(win, kNotifCursorPos);
            str::Builder out;
            out.Append(fmt("pageInfo wanted=%d shown=%d msg=%s\n", win->pageInfoWanted ? 1 : 0, pageInfo ? 1 : 0,
                           pageInfo ? NotificationGetMessageTemp(pageInfo) : StrL("")));
            out.Append(fmt("cursorPos shown=%d next=%s msg=%s\n", cursorPos ? 1 : 0, NextCursorPositionUnitName(win),
                           cursorPos ? NotificationGetMessageTemp(cursorPos) : StrL("")));
            out.Append(fmt("pageBoxes=%d images=%d transparencyGrid=%d fitContentArea=%d links=%d\n",
                           win->showPageBoxes ? 1 : 0, ShowImageOutlines() ? 1 : 0, ShowTransparencyGrid() ? 1 : 0,
                           ShowFitContentArea() ? 1 : 0, gSettings->showLinks ? 1 : 0));
            out.Append(fmt("drawn shapes=%d\n", CanvasOverlayShapesDrawn()));
            out.Append(fmt("commandPalette=%d\n", IsCommandPaletteVisible() ? 1 : 0));
            AppendTestResult(req, 0, ToStrTemp(out));
            break;
        }

        case ControlCmd::TestNavFiles: {
            Str action = StringArg(req, 0);
            i32 idx = -1;
            IntArg(req, 1, idx);
            int exitCode = 0;
            Str res = NavFilesInFolderStateTemp(action, idx, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAnnotFilter: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            Str action = StringArg(req, 0);
            if (str::EqI(action, StrL("set"))) {
                ApplyAnnotFilterText(win, StringArg(req, 1));
            } else if (str::EqI(action, StrL("paint"))) {
                AppShellInvalidate(win);
            } else {
                // ng: not orig's: click / dblclick / key / delete / scroll
                i32 arg = -1;
                i32 mods = 0;
                IntArg(req, 1, arg);
                IntArg(req, 2, mods);
                AnnotFilterTestAction(win, action, arg, mods);
            }
            AppendTestResult(req, 0, AnnotFilterToolbarStateTemp(win));
            break;
        }

        case ControlCmd::TestSavePathDialog: {
            Str path = StringArg(req, 0);
            if (!IsSavePathDialogVisible()) {
                AppendTestResult(req, 2, StrL("NOTREADY no-dialog"));
                break;
            }
            bool ok = TestFinishSavePathDialog(path);
            AppendTestResult(req, 0, fmt("OK done=%d path=%s", ok ? 1 : 0, path));
            break;
        }

        case ControlCmd::TestCurrentTab: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            WindowTab* tab = win->CurrentTab();
            if (!tab || len(tab->filePath) == 0) {
                AppendTestResult(req, 2, StrL("NOTREADY no-tab"));
                break;
            }
            int page = tab->ctrl ? tab->ctrl->CurrentPageNo() : 0;
            AppendTestResult(req, 0, fmt("path=%s page=%d", tab->filePath, page));
            break;
        }

        case ControlCmd::TestCommandVisibility: {
            Str name = StringArg(req, 0);
            int cmdId = GetCommandIdByName(name);
            if (cmdId <= 0) {
                AppendError(req, StrL("TestCommandVisibility expects a command name"));
                break;
            }
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            CommandSurface surface = CommandSurface::Menu;
            Str surf = StringArg(req, 1);
            if (str::EqI(surf, StrL("palette"))) {
                surface = CommandSurface::Palette;
            } else if (str::EqI(surf, StrL("toolbar"))) {
                surface = CommandSurface::Toolbar;
            }
            Point pt{};
            i32 x = 0;
            i32 y = 0;
            if (IntArg(req, 2, x) && IntArg(req, 3, y)) {
                pt = Point{x, y};
            }
            AppCommandCtx ctx = NewAppCommandCtx(win, pt);
            CommandVisibility vis = GetCommandVisibility(cmdId, ctx, surface);
            Str visName = StrL("show");
            if (vis == CommandVisibility::Hide) {
                visName = StrL("hide");
            } else if (vis == CommandVisibility::Disable) {
                visName = StrL("disable");
            }
            AppendTestResult(req, 0, fmt("cmd=%s vis=%s", name, visName));
            break;
        }

#if OS_WIN
        case ControlCmd::TestDefaultAppNotif: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            StrVec exts;
            Split(&exts, StringArg(req, 0), StrL(","), true);
            ShowDefaultAppNotification(win, exts);
            AppendTestResult(req, 0, fmt("OK exts=%d", len(exts)));
            break;
        }
#endif

        case ControlCmd::TestMainMenu: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            AppendTestResult(req, 0, MainMenuResultTemp(win));
            break;
        }

        case ControlCmd::TestDocumentProperties: {
            int exitCode = 0;
            Str res = DocumentPropertiesResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestInput: {
            MainWindow* win = FirstWindow();
            i32 a = 0, b = 0, c = 0, d = 0;
            IntArg(req, 1, a);
            IntArg(req, 2, b);
            IntArg(req, 3, c);
            IntArg(req, 4, d);
            AppendTestResult(req, 0, AppShellTestInput(win, StringArg(req, 0), a, b, c, d));
            break;
        }

        case ControlCmd::TestUiState:
            AppendTestResult(req, 0, AppShellUiStateTemp(FirstWindow()));
            break;

        case ControlCmd::TestToolWindow: {
            i32 a = 0, b = 0, c = 0, d = 0;
            IntArg(req, 3, a);
            IntArg(req, 4, b);
            IntArg(req, 5, c);
            IntArg(req, 6, d);
            Str res = ToolWindowTestTemp(StringArg(req, 0), StringArg(req, 1), StringArg(req, 2), a, b, c, d);
            AppendTestResult(req, 0, res);
            break;
        }

#if OS_WIN
        case ControlCmd::TestNativeMsgBox: {
            i32 id = 0;
            IntArg(req, 1, id);
            AppendTestResult(req, 0, NativeMsgBoxTestTemp(StringArg(req, 0), id));
            break;
        }

        case ControlCmd::TestNativeFileDlg: {
            AppendTestResult(req, 0, NativeFileDlgTestTemp(FirstWindow(), StringArg(req, 0), StringArg(req, 1)));
            break;
        }

        case ControlCmd::TestOleDragDrop: {
            i32 x = 0, y = 0;
            IntArg(req, 2, x);
            IntArg(req, 3, y);
            if (str::Eq(StringArg(req, 0), StrL("cursor"))) {
                AppendTestResult(req, 0, NativeCursorTestTemp(FirstWindow(), StringArg(req, 1)));
                break;
            }
            // ng: a fifth argument names the tool window to drop on
            AppendTestResult(
                req, 0,
                OleDragDropTestTemp(FirstWindow(), StringArg(req, 0), StringArg(req, 1), x, y, StringArg(req, 4)));
            break;
        }
#endif

        case ControlCmd::StartPerfLog:
            StartPerfLog();
            AppendTestResult(req, 0, StrL("OK"));
            break;

        case ControlCmd::StopPerfLog:
            StopPerfLog();
            AppendTestResult(req, 0, StrL("OK"));
            break;

        default:
            // ng: orig answers the rest through SumatraTest.cpp and the win32
            // layout probes; neither is in this port (step 18)
            AppendTestResult(req, 1, fmt("NOTPORTED %d", (int)req->cmd));
            break;
    }
    req->done.Set();
}

static void SnapshotRenderIdle(ControlRequest* req) {
    req->idleState = RenderIdleState::NotReady;
    req->idleInfo[0] = 0;
#if OS_WIN
    if (gIsStartup) {
        // LoadOnStartup applies -zoom after the first paint; a snapshot
        // during that window would see the default-zoom tiles as "done"
        str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), StrL("startup"));
        req->done.Set();
        return;
    }
#endif
    MainWindow* win = FirstWindow();
    if (!win || !win->IsDocLoaded()) {
        str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), StrL("no-doc"));
        req->done.Set();
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        // ebook / CHM / etc.: nothing in RenderCache to wait for
        req->idleState = RenderIdleState::Idle;
        str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), StrL("no-fixed"));
        req->done.Set();
        return;
    }
    // ng: orig repaints the canvas here (that is what queues the missing
    // target tiles); the shell's tick does that on its own, so we only drain
    // the queue the finished renders come back through
    AppShellInvalidate(win);
    uitask::DrainQueue();

    float zoomV = dm->GetZoomVirtual(true);
    int pageNo = dm->FirstVisiblePageNo();
    if (pageNo < 1) {
        pageNo = 1;
    }
    float zoomR = dm->GetZoomReal(pageNo);
    int res = gRenderCache ? (int)gRenderCache->GetTileRes(dm, pageNo) : 0;
    Size vp = dm->GetViewPort().Size();
    Str whyNot;
    bool busy = gRenderCache && gRenderCache->IsBusyFor(dm);
    bool ready = false;
    // LoadDocument Relayouts before the canvas has a real size; fit zoom then
    // stays unset and no page is visible. That is not idle (issue-1203).
    if (dm->zoomReal < 0.01f || dm->GetCanvasSize().IsEmpty() || vp.IsEmpty()) {
        whyNot = StrL("no-layout");
    } else {
        ready = gRenderCache && !busy && gRenderCache->VisibleTargetTilesReady(dm, &whyNot);
    }
    if (busy) {
        whyNot = StrL("rendering");
    }
    if (win->scrollAnimActive) {
        whyNot = StrL("scrolling");
        ready = false;
    }
    int nQ = gRenderCache ? gRenderCache->requestCount : -1;
    TempStr busyInfo = gRenderCache ? gRenderCache->BusyInfoTemp(dm) : str::DupTemp(StrL(""));
    str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)),
                fmt("zoomV=%.1f zoomR=%.3f res=%d vp=%dx%d ready=%d q=%d why=%s %s", zoomV, zoomR, res, vp.dx, vp.dy,
                    ready ? 1 : 0, nQ, whyNot, busyInfo));
    req->idleState = ready ? RenderIdleState::Idle : (gRenderCache ? RenderIdleState::Busy : RenderIdleState::NotReady);
    req->done.Set();
}

// Block on the control thread until visible tiles are cached at target
// resolution, or until timeoutMs. Optional first int arg is the timeout.
static void RunWaitRenderIdle(ControlRequest* req) {
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

#if OS_WIN
using ControlConn = HANDLE;
#else
// a connected unix domain socket
using ControlConn = int;
#endif

#if OS_WIN
static bool ReadExact(HANDLE h, void* data, DWORD n) {
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

static bool WriteExact(HANDLE h, Str data) {
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

#else
static bool ReadExact(int fd, void* data, u32 n) {
    u8* d = (u8*)data;
    u32 total = 0;
    while (total < n) {
        ssize_t nRead = read(fd, d + total, n - total);
        if (nRead < 0 && errno == EINTR) {
            continue;
        }
        if (nRead <= 0) {
            return false;
        }
        total += (u32)nRead;
    }
    return true;
}

static bool WriteExact(int fd, Str data) {
    int total = 0;
    while (total < data.len) {
        ssize_t nWritten = send(fd, data.s + total, (size_t)(data.len - total), MSG_NOSIGNAL);
        if (nWritten < 0 && errno == EINTR) {
            continue;
        }
        if (nWritten <= 0) {
            return false;
        }
        total += (int)nWritten;
    }
    return true;
}
#endif

static ControlRequest* ReadControlRequest(ControlConn h) {
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

static bool WriteControlResponse(ControlConn h, ControlRequest* req) {
    str::Builder payload;
    AppendU16(payload, req->reqId);
    payload.Append(ToStr(req->results));

    str::Builder packet;
    AppendU32(packet, (u32)len(payload));
    packet.Append(ToStr(payload));
    return WriteExact(h, ToStr(packet));
}

// returns true if the app is quitting, so the listener thread should exit
// instead of blocking in ConnectNamedPipe (ASan shutdown hangs on that)
static bool ProcessControlConnection(ControlConn h) {
    for (;;) {
        ControlRequest* req = ReadControlRequest(h);
        if (!req) {
            return false;
        }
        bool isQuit = (ControlCmd)req->cmd == ControlCmd::Quit;
        if ((ControlCmd)req->cmd == ControlCmd::CrashMe) {
            log(StrL("ControlCmd::CrashMe\n"));
            CrashMe();
        }
        // WaitRenderIdle polls on this thread so the UI thread stays free to
        // paint (and thereby request the tiles we are waiting for)
        if ((ControlCmd)req->cmd == ControlCmd::WaitRenderIdle) {
            RunWaitRenderIdle(req);
        } else {
            uitask::Post(MkFunc0<ControlRequest>(ExecuteControlRequest, req), "SumatraControl");
            req->done.Wait();
        }
        bool ok = WriteControlResponse(h, req);
        DeleteControlRequest(req);
        if (!ok || isQuit) {
            return isQuit;
        }
    }
}

#if OS_WIN
static WStr FullPipeNameOwned(Str pipeName) {
    if (str::StartsWith(pipeName, StrL(R"(\\.\pipe\)"))) {
        return ToWStr(pipeName);
    }
    TempStr fullName = str::JoinTemp(StrL(R"(\\.\pipe\)"), pipeName);
    return ToWStr(fullName);
}

struct ControlThreadArg {
    Str pipeName;
};

static void SumatraControlThread(ControlThreadArg* arg) {
    WStr pipeNameW = FullPipeNameOwned(arg->pipeName);
    str::FreePtr(&arg->pipeName);
    delete arg;

    for (;;) {
        HANDLE pipe = CreateNamedPipeW(pipeNameW.s, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       1, 64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            logf("CreateNamedPipeW failed for control pipe, err=%u\n", (unsigned)GetLastError());
            return;
        }
        BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        bool stop = false;
        if (connected) {
            stop = ProcessControlConnection(pipe);
        }
        // DisconnectNamedPipe discards data the client hasn't read yet; wait
        // until it has, or the Quit reply is lost and the client sees EPIPE
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        if (stop) {
            return;
        }
    }
}

#else

// ng: off Windows the channel is a unix domain socket: <name> is its path, or,
// without a '/', /tmp/<name>.sock (cmd/dbg-control.ts does the same)
static TempStr ControlSocketPathTemp(Str pipeName) {
    if (str::IndexOfChar(pipeName, '/') >= 0) {
        return str::DupTemp(pipeName);
    }
    return fmt("/tmp/%s.sock", pipeName);
}

struct ControlThreadArg {
    Str pipeName;
};

static void SumatraControlThread(ControlThreadArg* arg) {
    TempStr sockPath = ControlSocketPathTemp(arg->pipeName);
    str::FreePtr(&arg->pipeName);
    delete arg;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (len(sockPath) >= (int)sizeof(addr.sun_path)) {
        logf("control socket path is too long: '%s'\n", sockPath);
        return;
    }
    memcpy(addr.sun_path, sockPath.s, (size_t)len(sockPath));

    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) {
        logf("socket() failed for control socket, errno=%d\n", errno);
        return;
    }
    // a stale socket file from a run that did not clean up
    unlink(addr.sun_path);
    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(listener, 1) != 0) {
        logf("bind() / listen() failed for control socket '%s', errno=%d\n", sockPath, errno);
        close(listener);
        return;
    }
    for (;;) {
        int conn = accept(listener, nullptr, nullptr);
        if (conn < 0) {
            if (errno == EINTR) {
                continue;
            }
            logf("accept() failed for control socket, errno=%d\n", errno);
            break;
        }
        bool stop = ProcessControlConnection(conn);
        // the client reads the reply (to Quit, too) before it sees the close
        shutdown(conn, SHUT_WR);
        close(conn);
        if (stop) {
            break;
        }
    }
    close(listener);
    unlink(addr.sun_path);
}
#endif

void StartSumatraControl(Str pipeName) {
    if (len(pipeName) == 0) {
        return;
    }
    logf("StartSumatraControl: pipe '%s'\n", pipeName);
#if OS_WIN
    // a script cannot answer a system message box: it gets the port's own
    // until it asks for the system's (TestNativeMsgBox on)
    NativeMsgBoxSetEnabled(false);
#endif
    auto* arg = new ControlThreadArg{str::Dup(pipeName)};
    RunAsync(MkFunc0(SumatraControlThread, arg), StrL("SumatraControl"));
}

#else

// ng: no threads, so nothing to listen on
void StartSumatraControl(Str pipeName) {
    if (len(pipeName) > 0) {
        log(StrL("-dbg-control is not supported on this platform\n"));
    }
}

#endif
