/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "DocController.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "GlobalPrefs.h"
#include "Flags.h"
#include "SumatraTest.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "FileHistory.h"
#include "Favorites.h"
#include "SelectionTranslate.h"
#include "ImageSaveCropResize.h"
#include "base/GuessFileType.h"
#include "FindWindow.h"
#include "Toolbar.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "HomePage.h"
#include "AIChatCommon.h"
#include "AdvancedSettingsDialog.h"
#include "EditAnnotations.h"

extern bool gIsStartup;

// Silent add for -dbg-control tests (no name dialog, no settings flush).
static void AddFavoriteSilent(MainWindow* win, int pageNo) {
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath || !win->ctrl->ValidPageNo(pageNo)) {
        return;
    }
    Str path = tab->filePath;
    FileState* fs = FileHistoryFindByPath(path);
    if (!fs) {
        fs = NewFileState(path);
        FileHistoryAppend(fs);
    }
    if (!fs->favorites) {
        return;
    }
    for (Favorite* fav : *fs->favorites) {
        if (fav->pageNo == pageNo) {
            return;
        }
    }
    TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNo);
    TempStr plainLabel = fmt("%d", pageNo);
    bool needsLabel = pageLabel && !str::Eq(plainLabel, pageLabel);
    Str pl = needsLabel ? pageLabel : Str{};
    fs->favorites->Append(NewFavorite(pageNo, {}, pl));
}

// Drive favorites on the already-open document for tests/issue-3744.ts.
// action: "add" | "goto" | "next" | "prev" | "page". pageNo is used by add/goto.
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

    if (str::EqI(action, "add")) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        AddFavoriteSilent(win, pageNo);
    } else if (str::EqI(action, "goto")) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        win->ctrl->GoToPage(pageNo, true);
    } else if (str::EqI(action, "next")) {
        GoToNextFavorite(win, true);
    } else if (str::EqI(action, "prev")) {
        GoToNextFavorite(win, false);
    } else if (str::EqI(action, "page")) {
        // report only
    } else {
        return finish(fmt("ERROR unknown-action action=%s", action), 1);
    }

    int cur = win->ctrl->CurrentPageNo();
    return finish(fmt("OK page=%d", cur), 0);
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
    if (!action || str::EqI(action, StrL("get")) || reportR2L) {
        // report only
    } else if (str::EqI(action, StrL("presentation"))) {
        ToggleFullScreen(win, win->AsFixed() != nullptr);
    } else if (str::EqI(action, StrL("fullscreen"))) {
        ToggleFullScreen(win, false);
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

// Reports sidebar vs canvas client x positions so tests can check SidebarOnRight.
static TempStr SidebarLayoutResultTemp(int* exitCodeOut) {
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
    if (!win || !win->hwndFrame) {
        return finish(StrL("NOTREADY no-window"), 2);
    }

    auto clientX = [&](HWND hwnd) -> int {
        if (!hwnd || !HwndIsVisible(hwnd)) {
            return -1;
        }
        return HwndScreenToClient(win->hwndFrame, HwndWindowRect(hwnd).TL()).x;
    };

    bool pref = gGlobalPrefs && gGlobalPrefs->sidebarOnRight;
    bool tocVis = win->hwndTocBox && HwndIsVisible(win->hwndTocBox);
    bool favVis = win->hwndFavBox && HwndIsVisible(win->hwndFavBox);
    int tocX = clientX(win->hwndTocBox);
    int favX = clientX(win->hwndFavBox);
    int canvasX = clientX(win->hwndCanvas);
    return finish(fmt("OK pref=%d tocVis=%d favVis=%d tocX=%d favX=%d canvasX=%d", pref ? 1 : 0, tocVis ? 1 : 0,
                      favVis ? 1 : 0, tocX, favX, canvasX),
                  0);
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
    if (!fonts) {
        return finish(StrL("ERROR no-fonts"), 1);
    }
    return finish(fmt("OK fonts=%s", fonts), 0);
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

struct ControlRequest {
    u16 cmd = 0;
    u16 reqId = 0;
    Vec<ControlArg*> args;
    str::Builder results;
    HANDLE done = nullptr;
    RenderIdleState idleState = RenderIdleState::NotReady;
    char idleInfo[160]{};
};

static void DeleteControlRequest(ControlRequest* req) {
    if (!req) {
        return;
    }
    for (ControlArg* arg : req->args) {
        DeleteControlArg(arg);
    }
    SafeCloseHandle(&req->done);
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
    s.Append(Str((char*)(buf), (int)(sizeof(buf))));
}

static void AppendU32(str::Builder& s, u32 v) {
    u8 buf[4] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff), (u8)((v >> 16) & 0xff), (u8)((v >> 24) & 0xff)};
    s.Append(Str((char*)(buf), (int)(sizeof(buf))));
}

static void AppendArgEnd(str::Builder& s) {
    AppendU16(s, (u16)ControlArgType::End);
}

static void AppendArgInt(str::Builder& s, i32 v) {
    AppendU16(s, (u16)ControlArgType::Int32);
    AppendU32(s, (u32)v);
}

static void AppendArgString(str::Builder& s, Str str) {
    if (!str) {
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
        args->Append(arg);
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
    } else if (type == ControlArgType::Bytes) {
        u32 n = 0;
        if (!r.ReadU32(n)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->bytes = AllocArray<u8>((int)n + 1);
        arg->bytesLen = n;
        if (!r.ReadBytes(arg->bytes, n)) {
            DeleteControlArg(arg);
            return false;
        }
    } else if (type == ControlArgType::String) {
        u32 n = 0;
        if (!r.ReadU32(n)) {
            DeleteControlArg(arg);
            return false;
        }
        char* strBuf = AllocArray<char>((int)n + 1);
        if (!r.ReadBytes((u8*)strBuf, n)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->str = Str(strBuf, (int)n);
        u8 zero = 1;
        if (!r.ReadBytes(&zero, 1) || zero != 0) {
            DeleteControlArg(arg);
            return false;
        }
    } else if (type == ControlArgType::List) {
        u16 count = 0;
        if (!r.ReadU16(count)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->list = new Vec<ControlArg*>();
        if (!ParseArgList(r, arg->list, true, count)) {
            DeleteControlArg(arg);
            return false;
        }
    } else {
        DeleteControlArg(arg);
        return false;
    }
    *argOut = arg;
    return true;
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

static Str StringArg(ControlRequest* req, size_t idx) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::String);
    return arg ? arg->str : Str{};
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

static void ExecuteControlRequest(ControlRequest* req) {
    switch ((ControlCmd)req->cmd) {
        case ControlCmd::Ping:
            AppendArgString(req->results, "pong");
            AppendArgEnd(req->results);
            break;

        case ControlCmd::Quit:
            AppendArgInt(req->results, 0);
            AppendArgEnd(req->results);
            PostAppExit();
            break;

        case ControlCmd::TestSynctex: {
            i32 line = 0;
            Str pdf = StringArg(req, 0);
            Str src = StringArg(req, 1);
            if (!pdf || !src || !IntArg(req, 2, line)) {
                AppendError(req, "TestSynctex expects string pdf, string source, int line");
                break;
            }
            AppendTestResult(req, 0, SynctexResultTemp(pdf, src, line));
            break;
        }

        case ControlCmd::TestInverseSearch: {
            i32 page = 0, x = 0, y = 0;
            Str pdf = StringArg(req, 0);
            if (!pdf || !IntArg(req, 1, page) || !IntArg(req, 2, x) || !IntArg(req, 3, y)) {
                AppendError(req, "TestInverseSearch expects string pdf, int page, int x, int y");
                break;
            }
            AppendTestResult(req, 0, InverseSearchResultTemp(pdf, page, x, y));
            break;
        }

        case ControlCmd::TestSearch: {
            Str pdf = StringArg(req, 0);
            Str needle = StringArg(req, 1);
            Str password = StringArg(req, 2);
            if (!pdf || !needle) {
                AppendError(req, "TestSearch expects string pdf, string needle, optional string password");
                break;
            }
            if (!password && gCli) {
                password = gCli->password;
            }
            AppendTestResult(req, 0, SearchResultTemp(pdf, needle, password));
            break;
        }

        case ControlCmd::TestDest: {
            i32 destNo = 0;
            Str pdf = StringArg(req, 0);
            if (!pdf || !IntArg(req, 1, destNo)) {
                AppendError(req, "TestDest expects string pdf, int destinationNumber");
                break;
            }
            AppendTestResult(req, 0, DestResultTemp(pdf, destNo));
            break;
        }

        case ControlCmd::TestNamedDest: {
            Str pdf = StringArg(req, 0);
            Str name = StringArg(req, 1);
            if (!pdf || !name) {
                AppendError(req, "TestNamedDest expects string pdf, string name");
                break;
            }
            AppendTestResult(req, 0, NamedDestResultTemp(pdf, name));
            break;
        }

        case ControlCmd::TestChm: {
            Str chm = StringArg(req, 0);
            if (!chm) {
                AppendError(req, "TestChm expects string chmPath");
                break;
            }
            int exitCode = 0;
            Str res = ChmResultTemp(chm, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectionTranslate: {
            i32 backend = 0;
            Str srcLang = StringArg(req, 1);
            Str dstLang = StringArg(req, 2);
            Str text = StringArg(req, 3);
            if (!IntArg(req, 0, backend) || !srcLang || !dstLang || !text) {
                AppendError(req,
                            "TestSelectionTranslate expects int backend, string srcLang, string dstLang, string text");
                break;
            }
            int exitCode = 0;
            Str res = SelectionTranslateResultTemp(backend, srcLang, dstLang, text, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTripleClickLineSelect: {
            Str pdf = StringArg(req, 0);
            Str clickWord = StringArg(req, 1);
            Str expectedLine = StringArg(req, 2);
            if (!pdf || !clickWord || !expectedLine) {
                AppendError(req, "TestTripleClickLineSelect expects string pdf, string clickWord, string expectedLine");
                break;
            }
            int exitCode = 0;
            Str res = TripleClickLineSelectResultTemp(pdf, clickWord, expectedLine, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestContextMenuSelection: {
            Str word1 = StringArg(req, 0);
            Str word2 = StringArg(req, 1);
            Str cursorWord = StringArg(req, 2);
            if (!word1 || !word2 || !cursorWord) {
                AppendError(req, "TestContextMenuSelection expects string word1, string word2, string cursorWord");
                break;
            }
            int exitCode = 0;
            Str res = ContextMenuSelectionResultTemp(word1, word2, cursorWord, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGoToFindMatch: {
            Str word = StringArg(req, 0);
            Str typed = StringArg(req, 1);
            if (!word || !typed) {
                AppendError(req, "TestGoToFindMatch expects string word, string typed");
                break;
            }
            int exitCode = 0;
            Str res = GoToFindMatchResultTemp(word, typed, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestImageResizeArrowKey: {
            Str imagePath = StringArg(req, 0);
            if (!imagePath) {
                AppendError(req, "TestImageResizeArrowKey expects string imagePath");
                break;
            }
            int exitCode = 0;
            Str res = ImageResizeArrowKeyResultTemp(imagePath, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestClickClearsSelection: {
            Str word = StringArg(req, 0);
            if (!word) {
                AppendError(req, "TestClickClearsSelection expects string word");
                break;
            }
            int exitCode = 0;
            Str res = ClickClearsSelectionResultTemp(word, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestRectSelectionDrag: {
            Str word = StringArg(req, 0);
            if (!word) {
                AppendError(req, "TestRectSelectionDrag expects string word");
                break;
            }
            int exitCode = 0;
            Str res = RectSelectionDragResultTemp(word, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindResultsOrder: {
            Str term = StringArg(req, 0);
            if (!term) {
                AppendError(req, "TestFindResultsOrder expects string term, int startPage");
                break;
            }
            i32 startPage = 0;
            IntArg(req, 1, startPage);
            int exitCode = 0;
            Str res = FindResultsOrderResultTemp(term, startPage, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindResultPageColumnClip: {
            int exitCode = 0;
            Str res = FindResultPageColumnClipResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFileKind: {
            Str path = StringArg(req, 0);
            Str expectedKind = StringArg(req, 1);
            if (!path || !expectedKind) {
                AppendError(req, "TestFileKind expects string path, string expectedKind");
                break;
            }
            int exitCode = 0;
            Str res = FileKindResultTemp(path, expectedKind, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestScrollToLink: {
            i32 minDelta = 50;
            IntArg(req, 0, minDelta);
            int exitCode = 0;
            Str res = ScrollToLinkResultTemp(minDelta, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestI18nErrorString: {
            int exitCode = 0;
            Str res = I18nErrorStringResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageInfoOverlay: {
            Str pathTwo = StringArg(req, 0);
            Str pathOne = StringArg(req, 1);
            if (!pathTwo || !pathOne) {
                AppendError(req, "TestPageInfoOverlay expects string pathTwoPages, string pathOnePage");
                break;
            }
            int exitCode = 0;
            Str res = PageInfoOverlayResultTemp(pathTwo, pathOne, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGetToc: {
            Str path = StringArg(req, 0);
            if (!path) {
                AppendError(req, "TestGetToc expects string path");
                break;
            }
            int exitCode = 0;
            Str res = GetTocResultTemp(path, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageLinks: {
            Str path = StringArg(req, 0);
            i32 pageNo = 1;
            if (!path || !IntArg(req, 1, pageNo)) {
                AppendError(req, "TestPageLinks expects string path, int pageNo");
                break;
            }
            int exitCode = 0;
            Str res = PageLinksResultTemp(path, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestCadEnhanceColors: {
            Str path = StringArg(req, 0);
            i32 pageNo = 1;
            i32 zoomPercent = 25;
            if (!path || !IntArg(req, 1, pageNo)) {
                AppendError(req, "TestCadEnhanceColors expects string path, int pageNo [, int zoomPercent]");
                break;
            }
            IntArg(req, 2, zoomPercent); // optional
            int exitCode = 0;
            Str res = CadEnhanceColorsResultTemp(path, pageNo, zoomPercent, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageComments: {
            Str path = StringArg(req, 0);
            i32 pageNo = 1;
            if (!path || !IntArg(req, 1, pageNo)) {
                AppendError(req, "TestPageComments expects string path, int pageNo");
                break;
            }
            int exitCode = 0;
            Str res = PageCommentsResultTemp(path, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestWindowStateDuringLoad: {
            int exitCode = 0;
            Str res = WindowStateDuringLoadResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTocNavigate: {
            i32 destNo = 1;
            if (!IntArg(req, 0, destNo)) {
                AppendError(req, "TestTocNavigate expects int destNo (1-based)");
                break;
            }
            int exitCode = 0;
            Str res = TocNavigateResultTemp(destNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDestZoomNav: {
            i32 destNo = 1;
            i32 startZoomPerc = 0;
            if (!IntArg(req, 0, destNo)) {
                AppendError(req, "TestDestZoomNav expects int destNo (1-based) [, int startZoomPerc]");
                break;
            }
            IntArg(req, 1, startZoomPerc); // optional
            int exitCode = 0;
            Str res = DestZoomNavResultTemp(destNo, startZoomPerc, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAnnotEditorLayout: {
            i32 clientDy = 0;
            i32 selectItem = 0;
            IntArg(req, 0, clientDy);   // optional
            IntArg(req, 1, selectItem); // optional, 1-based
            int exitCode = 0;
            Str res = AnnotEditorLayoutResultTemp(clientDy, selectItem, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDisplayMode: {
            Str action = StringArg(req, 0);
            int exitCode = 0;
            Str res = DisplayModeResultTemp(action, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDocumentFontList: {
            int exitCode = 0;
            Str res = DocumentFontListResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindPageRange: {
            Str pdf = StringArg(req, 0);
            Str needle = StringArg(req, 1);
            i32 first = 0;
            i32 last = 0;
            IntArg(req, 2, first);
            IntArg(req, 3, last);
            Str spec = StringArg(req, 4); // optional "3,4-6,18-"
            if (!pdf || !needle) {
                AppendError(
                    req, "TestFindPageRange expects string pdf, string needle [, int first, int last [, string spec]]");
                break;
            }
            int exitCode = 0;
            Str res = FindPageRangeResultTemp(pdf, needle, first, last, spec, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSidebarLayout: {
            int exitCode = 0;
            Str res = SidebarLayoutResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMarkdownTocNavigate: {
            i32 destNo = 0;
            i32 minScrollY = 1;
            if (!IntArg(req, 0, destNo) || !IntArg(req, 1, minScrollY)) {
                AppendError(req, "TestMarkdownTocNavigate expects int destNo, int minScrollY");
                break;
            }
            int exitCode = 0;
            Str res = MarkdownTocNavigateResultTemp(destNo, minScrollY, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMarkdownFollowLink: {
            Str href = StringArg(req, 0);
            i32 follow = 0;
            if (!IntArg(req, 1, follow)) {
                AppendError(req, "TestMarkdownFollowLink expects string href, int follow");
                break;
            }
            int exitCode = 0;
            Str res = MarkdownFollowLinkResultTemp(href, follow != 0, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestHomeListRows: {
            int exitCode = 0;
            Str res = HomeListRowsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAdvSettingsRows: {
            Str action = StringArg(req, 0);
            i32 arg = 0;
            IntArg(req, 1, arg); // optional; only "scroll" uses it
            if (!action) {
                AppendError(req, "TestAdvSettingsRows expects string action [, int rows]");
                break;
            }
            int exitCode = 0;
            Str res = AdvSettingsRowsResultTemp(action, arg, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFavoriteNav: {
            Str action = StringArg(req, 0);
            i32 pageNo = 0;
            IntArg(req, 1, pageNo); // optional for next/prev/page
            if (!action) {
                AppendError(req, "TestFavoriteNav expects string action [, int pageNo]");
                break;
            }
            int exitCode = 0;
            Str res = FavoriteNavResultTemp(action, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestToolbarButtons: {
            int exitCode = 0;
            Str res = ToolbarButtonsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestKeyboardLinkFollow: {
            int exitCode = 0;
            Str res = KeyboardLinkFollowResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectTextKeyboard: {
            int exitCode = 0;
            Str res = SelectTextKeyboardResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAIChat: {
            i32 backend = 0;
            Str filePath = StringArg(req, 1);
            Str message = StringArg(req, 2);
            if (!IntArg(req, 0, backend) || !filePath || !message) {
                AppendError(req, "TestAIChat expects int backend, string filePath, string message");
                break;
            }
            int exitCode = 0;
            Str res = AIChatTestResultTemp(backend, filePath, message, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAIChatReplay: {
            Str userMsg = StringArg(req, 0);
            Str response = StringArg(req, 1);
            if (!userMsg || !response) {
                AppendError(req, "TestAIChatReplay expects string userMsg, string response");
                break;
            }
            int exitCode = 0;
            Str res = AIChatTestReplayResultTemp(userMsg, response, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        default:
            AppendError(req, "unknown control command");
            break;
    }
    SetEvent(req->done);
}

// Snapshot for WaitRenderIdle. Must run on the UI thread: window/doc state
// and the cache walk both belong there. Does not block; the control thread
// polls so WM_PAINT can still request missing tiles.
static void SnapshotRenderIdle(ControlRequest* req) {
    req->idleState = RenderIdleState::NotReady;
    req->idleInfo[0] = 0;
    if (gIsStartup) {
        // LoadOnStartup applies -zoom after the first paint; a snapshot
        // during that window would see the default-zoom tiles as "done"
        str::BufSet(Str(req->idleInfo, dimof(req->idleInfo)), StrL("startup"));
        SetEvent(req->done);
        return;
    }
    if (len(gWindows) == 0) {
        str::BufSet(Str(req->idleInfo, dimof(req->idleInfo)), StrL("no-window"));
        SetEvent(req->done);
        return;
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        str::BufSet(Str(req->idleInfo, dimof(req->idleInfo)), StrL("no-doc"));
        SetEvent(req->done);
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        // ebook / CHM / etc.: nothing in RenderCache to wait for
        req->idleState = RenderIdleState::Idle;
        str::BufSet(Str(req->idleInfo, dimof(req->idleInfo)), StrL("no-fixed"));
        SetEvent(req->done);
        return;
    }
    // Paint first: that's what queues missing target tiles. Checking the
    // cache before this paint sees a leftover preview and no in-flight work.
    // DrainQueue runs RenderFinished from tiles that completed during the
    // paint; that posts another repaint which may request the next resolution.
    if (win->hwndCanvas) {
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        UpdateWindow(win->hwndCanvas);
    }
    uitask::DrainQueue();
    if (win->hwndCanvas) {
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        UpdateWindow(win->hwndCanvas);
    }
    float zoomV = dm->GetZoomVirtual(true);
    int pageNo = dm->FirstVisiblePageNo();
    if (pageNo < 1) {
        pageNo = 1;
    }
    float zoomR = dm->GetZoomReal(pageNo);
    USHORT res = gRenderCache ? gRenderCache->GetTileRes(dm, pageNo) : (USHORT)0;
    Size vp = dm->GetViewPort().Size();
    bool ready = gRenderCache && !gRenderCache->IsBusyFor(dm) && gRenderCache->VisibleTargetTilesReady(dm);
    int nQ = gRenderCache ? gRenderCache->requestCount : -1;
    str::BufSet(Str(req->idleInfo, dimof(req->idleInfo)), fmt("zoomV=%.1f zoomR=%.3f res=%d vp=%dx%d ready=%d q=%d",
                                                              zoomV, zoomR, (int)res, vp.dx, vp.dy, ready ? 1 : 0, nQ));
    req->idleState = ready ? RenderIdleState::Idle : (gRenderCache ? RenderIdleState::Busy : RenderIdleState::NotReady);
    SetEvent(req->done);
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
        ResetEvent(req->done);
        uitask::Post(MkFunc0<ControlRequest>(SnapshotRenderIdle, req), "WaitRenderIdle");
        WaitForSingleObject(req->done, INFINITE);
        if (req->idleState == RenderIdleState::Idle) {
            AppendTestResult(req, 0, req->idleInfo[0] ? Str(req->idleInfo) : StrL("idle"));
            return;
        }
        if (GetTickCount64() >= deadline) {
            Str kind = req->idleState == RenderIdleState::NotReady ? StrL("timeout-notready") : StrL("timeout-busy");
            AppendTestResult(req, 1, req->idleInfo[0] ? fmt("%s %s", kind, Str(req->idleInfo)) : kind);
            return;
        }
        Sleep(20);
    }
}

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

static ControlRequest* ReadControlRequest(HANDLE h) {
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
    req->done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    free(data);
    return req;
}

static bool WriteControlResponse(HANDLE h, ControlRequest* req) {
    str::Builder payload;
    AppendU16(payload, req->reqId);
    payload.Append(ToStr(req->results));

    str::Builder packet;
    AppendU32(packet, (u32)len(payload));
    packet.Append(ToStr(payload));
    return WriteExact(h, ToStr(packet));
}

static void ProcessControlConnection(HANDLE h) {
    for (;;) {
        ControlRequest* req = ReadControlRequest(h);
        if (!req) {
            return;
        }
        // WaitRenderIdle polls on this thread so the UI thread stays free to
        // paint (and thereby request the tiles we are waiting for)
        if ((ControlCmd)req->cmd == ControlCmd::WaitRenderIdle) {
            RunWaitRenderIdle(req);
        } else {
            uitask::Post(MkFunc0<ControlRequest>(ExecuteControlRequest, req), "SumatraControl");
            WaitForSingleObject(req->done, INFINITE);
        }
        bool ok = WriteControlResponse(h, req);
        DeleteControlRequest(req);
        if (!ok) {
            return;
        }
    }
}

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
        if (connected) {
            ProcessControlConnection(pipe);
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

void StartSumatraControl(Str pipeName) {
    if (len(pipeName) == 0) {
        return;
    }
    auto* arg = new ControlThreadArg{str::Dup(pipeName)};
    RunAsync(MkFunc0(SumatraControlThread, arg), "SumatraControl");
}
