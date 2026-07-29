/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// The audiobook Characters panel.
//
// A panel, not a window: a WS_CHILD of the frame docked down the left side
// beside the document, with a splitter to resize it and its width remembered
// in Audiobook.SidebarDx. RelayoutFrame carves its strip out of the client
// area (see SumatraPDF.cpp), exactly as it does for the table of contents.
//
// It lists every character the Chatterbox engine found in the book, how many
// lines each speaks, and which voice is cast to it. A character with no voice
// is read by the narrator and is flagged here, with a Test button to hear it
// and a Train button to make a voice for it.
//
// The panel opens whatever the engine is doing, including when it isn't
// running yet: opening it starts the engine idle (TTS model loaded, nothing
// read) and the panel then follows along. Working out who speaks each line
// takes minutes on a real book, so it's the Analyse button here rather than
// something that happens on the way to reading.
//
// The engine holds the state; the panel polls it from the engine's local
// control API and posts changes back.

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/Http.h"
#include "base/JsonParser.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "wingui/LabelWithCloseWnd.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "AudiobookCharacters.h"

#include "base/Log.h"

constexpr const WCHAR* kCharsClassName = L"SUMATRA_AUDIOBOOK_CHARS";

constexpr UINT kMsgRefreshRows = WM_APP + 0x40;
constexpr UINT kMsgClosePanel = WM_APP + 0x41;

constexpr int kIdVoiceFirst = 2000; // one per row
constexpr int kIdTestFirst = 3000;
constexpr int kIdTrainFirst = 4000;
constexpr int kIdAnalyze = 1002;
constexpr int kIdModel = 1003;
constexpr int kIdAnalyzer = 1010;
constexpr int kIdAmount = 1004;
constexpr int kIdStopAnalyze = 1005;
constexpr int kIdAddEndpoint = 1006;
constexpr int kIdAddEdit = 1007;
constexpr int kIdScan = 1008;
constexpr int kIdScanStop = 1009;
constexpr int kIdSort = 1011;
constexpr int kIdCloseChars = 1012;
constexpr int kIdRemoveEpFirst = 5000;
constexpr int kIdAddFoundFirst = 6000;
constexpr int kMaxRows = 256;
constexpr int kMaxEps = 8;
constexpr int kMaxFound = 12;

// how much of the book to read. Whole book first: it's the normal choice, and
// the rest are for sampling a long book or trying the setup out cheaply.
struct AnalyzeAmount {
    const WCHAR* label;
    int percent;
};
static const AnalyzeAmount kAmounts[] = {
    {L"the whole book", 100},
    {L"the first 50%", 50},
    {L"the first 25%", 25},
    {L"the first 10%", 10},
};

constexpr UINT_PTR kPollTimerId = 1;
constexpr int kPollMs = 1000;

constexpr int kAudiobookControlPortDefault = 7862;

struct CharRow {
    Str name;
    int lines = 0;
    int first = 0;
    Str voice;
    HWND hName = nullptr;
    HWND hVoice = nullptr;
    HWND hTest = nullptr;
    HWND hTrain = nullptr;
};

static const WCHAR* kSortLabels[] = {
    L"By first appearance", L"By last appearance", L"By most lines",
    L"By fewest lines",     L"By name (A-Z)",      L"By name (Z-A)",
};

static int SortIdx() {
    Str s = gGlobalPrefs->audiobook.charSort;
    if (str::EqI(s, StrL("appearance-desc"))) {
        return 1;
    }
    if (str::EqI(s, StrL("lines"))) {
        return 2;
    }
    if (str::EqI(s, StrL("lines-asc"))) {
        return 3;
    }
    if (str::EqI(s, StrL("name"))) {
        return 4;
    }
    if (str::EqI(s, StrL("name-desc"))) {
        return 5;
    }
    return 0;
}

static const char* SortKey(int idx) {
    switch (idx) {
        case 1:
            return "appearance-desc";
        case 2:
            return "lines";
        case 3:
            return "lines-asc";
        case 4:
            return "name";
        case 5:
            return "name-desc";
        default:
            return "appearance";
    }
}

static bool RowBefore(const CharRow& a, const CharRow& b, int mode) {
    int c = 0;
    switch (mode) {
        case 0:
            c = a.first - b.first;
            break;
        case 1:
            c = b.first - a.first;
            break;
        case 2:
            c = b.lines - a.lines;
            break;
        case 3:
            c = a.lines - b.lines;
            break;
        case 4:
            c = str::CmpNatural(a.name, b.name);
            break;
        case 5:
            c = str::CmpNatural(b.name, a.name);
            break;
    }
    if (c != 0) {
        return c < 0;
    }
    return str::CmpNatural(a.name, b.name) < 0;
}

struct CharsPanel {
    MainWindow* win = nullptr;
    HWND hwndContainer = nullptr;
    HWND hwnd = nullptr;
    HWND hwndFooter = nullptr;
    LabelWithCloseWnd* label = nullptr;
    HWND hSubtitle = nullptr;
    HWND hProgress = nullptr;
    UINT_PTR containerSubclassId = 0;
    int footerDy = 0;
    Str subtitle;
    HFONT font = nullptr;
    HFONT fontBold = nullptr;
    int port = kAudiobookControlPortDefault;

    StrVec voices;
    CharRow rows[kMaxRows];
    int nRows = 0;

    // the local LLM that works out who speaks each line
    StrVec lmModels;
    Str lmModel; // "" = decided automatically
    HWND hModel = nullptr;

    // the computers analysis is shared out over. [0] is always this one.
    struct Endpoint {
        Str url;
        Str model;
        bool ok = false;
        bool local = false;
    };
    Endpoint eps[kMaxEps];
    int nEps = 0;
    HWND hAddEdit = nullptr;

    // computers found by looking round the network, offered to be added
    struct Found {
        Str url;
        Str model;
        Str kind;
        bool known = false; // already in the list above
    };
    Found found[kMaxFound];
    int nFound = 0;
    bool scanning = false;
    int scanDone = 0;
    int scanTotal = 0;
    Str scanMsg;
    HWND hScanStatus = nullptr;

    bool engineUp = false;
    // the engine answers, but it's reading a different book: another
    // SumatraPDF window has it. Its cast is not ours to show.
    bool otherBook = false;
    Str enginePdf;

    bool analyzed = false;
    bool analyzing = false;
    // a stopped or part-of-the-book run: usable, and Analyse continues it
    bool analyzeComplete = true;
    int linesRead = 0;
    int linesTotal = 0;
    int amountIdx = 0; // index into kAmounts
    Str analyzeStatus;
    Str analyzeError;

    // we only ever try to start an engine once per panel session: a failed
    // launch (no Chatterbox install) would otherwise retry every poll and
    // pour notifications into the window
    bool triedStart = false;

    // a book can have more characters than the panel is tall
    int scrollY = 0;
    int wheelAccum = 0; // leftover wheel delta, finer than one notch
    int contentDy = 0;

    // what the controls were built for; rebuilding on every poll would flicker
    // and fight an open combo box, so only rebuild when this changes
    Str builtFor;
};

// one panel per app: the engine only ever reads one book at a time
static CharsPanel* gPanel = nullptr;

// --- talking to the engine ------------------------------------------------

static TempStr ControlGet(int port, Str path) {
    HttpRsp rsp;
    TempStr url = fmt("http://127.0.0.1:%d%s", port, path);
    if (!HttpGet(Str(url), &rsp) || !IsHttpRspOk(&rsp)) {
        return nullptr;
    }
    // ToStr, not TakeStr: TakeStr hands over ownership and we'd drop it
    return str::DupTemp(ToStr(rsp.data));
}

static bool ControlPost(int port, const char* path, Str body) {
    str::Builder hdrs;
    hdrs.Append("Content-Type: application/json\r\n");
    str::Builder b;
    if (len(body) > 0) {
        b.Append(body);
    }
    return HttpPost(StrL("127.0.0.1"), port, Str(path), &hdrs, &b);
}

// --- parsing /state -------------------------------------------------------

// Paths look like /characters[0]/name, /characters[0]/lines,
// /characters[0]/voice, /voices[0], /narrator, /analyzed
struct StateParser : json::ValueVisitor {
    CharsPanel* st;
    Str narrator;

    explicit StateParser(CharsPanel* s) : st(s) {}

    static int IndexOf(Str path, const char* prefix) {
        // "/characters[12]/name" -> 12 (or -1)
        if (!str::StartsWith(path, Str(prefix))) {
            return -1;
        }
        const char* s = path.s + strlen(prefix);
        if (*s != '[') {
            return -1;
        }
        return atoi(s + 1);
    }

    bool Visit(Str path, Str value, json::Type) override {
        int idx = IndexOf(path, "/characters");
        if (idx >= 0 && idx < kMaxRows) {
            if (idx + 1 > st->nRows) {
                st->nRows = idx + 1;
            }
            CharRow& r = st->rows[idx];
            if (str::EndsWith(path, StrL("/name"))) {
                r.name = str::Dup(value);
            } else if (str::EndsWith(path, StrL("/lines"))) {
                r.lines = atoi(value.s);
            } else if (str::EndsWith(path, StrL("/first"))) {
                r.first = atoi(value.s);
            } else if (str::EndsWith(path, StrL("/voice"))) {
                r.voice = str::Dup(value);
            }
            return true;
        }
        if (IndexOf(path, "/voices") >= 0) {
            st->voices.Append(value);
            return true;
        }
        if (IndexOf(path, "/lm_models") >= 0) {
            st->lmModels.Append(value);
            return true;
        }
        int fi = IndexOf(path, "/scan/found");
        if (fi >= 0 && fi < kMaxFound) {
            if (fi + 1 > st->nFound) {
                st->nFound = fi + 1;
            }
            CharsPanel::Found& f = st->found[fi];
            if (str::EndsWith(path, StrL("/url"))) {
                f.url = str::Dup(value);
            } else if (str::EndsWith(path, StrL("/model"))) {
                f.model = str::Dup(value);
            } else if (str::EndsWith(path, StrL("/kind"))) {
                f.kind = str::Dup(value);
            } else if (str::EndsWith(path, StrL("/known"))) {
                f.known = str::Eq(value, StrL("true"));
            }
            return true;
        }
        if (str::Eq(path, StrL("/scan/scanning"))) {
            st->scanning = str::Eq(value, StrL("true"));
            return true;
        }
        if (str::Eq(path, StrL("/scan/done"))) {
            st->scanDone = atoi(value.s);
            return true;
        }
        if (str::Eq(path, StrL("/scan/total"))) {
            st->scanTotal = atoi(value.s);
            return true;
        }
        if (str::Eq(path, StrL("/scan/message"))) {
            str::ReplaceWithCopy(&st->scanMsg, value);
            return true;
        }
        int ei = IndexOf(path, "/endpoints");
        if (ei >= 0 && ei < kMaxEps) {
            if (ei + 1 > st->nEps) {
                st->nEps = ei + 1;
            }
            CharsPanel::Endpoint& e = st->eps[ei];
            if (str::EndsWith(path, StrL("/url"))) {
                e.url = str::Dup(value);
            } else if (str::EndsWith(path, StrL("/model"))) {
                e.model = str::Dup(value);
            } else if (str::EndsWith(path, StrL("/ok"))) {
                e.ok = str::Eq(value, StrL("true"));
            } else if (str::EndsWith(path, StrL("/local"))) {
                e.local = str::Eq(value, StrL("true"));
            }
            return true;
        }
        if (str::Eq(path, StrL("/lm_model"))) {
            str::ReplaceWithCopy(&st->lmModel, value);
            return true;
        }
        if (str::Eq(path, StrL("/pdf"))) {
            str::ReplaceWithCopy(&st->enginePdf, value);
            return true;
        }
        if (str::Eq(path, StrL("/narrator"))) {
            narrator = str::Dup(value);
        } else if (str::Eq(path, StrL("/analyzed"))) {
            st->analyzed = str::Eq(value, StrL("true"));
        } else if (str::Eq(path, StrL("/analyzing"))) {
            st->analyzing = str::Eq(value, StrL("true"));
        } else if (str::Eq(path, StrL("/analyze_status"))) {
            str::ReplaceWithCopy(&st->analyzeStatus, value);
        } else if (str::Eq(path, StrL("/analyze_error"))) {
            str::ReplaceWithCopy(&st->analyzeError, value);
        } else if (str::Eq(path, StrL("/analyze_complete"))) {
            st->analyzeComplete = str::Eq(value, StrL("true"));
        } else if (str::Eq(path, StrL("/lines_read"))) {
            st->linesRead = atoi(value.s);
        } else if (str::Eq(path, StrL("/lines_total"))) {
            st->linesTotal = atoi(value.s);
        }
        return true;
    }
};

static void FreeRows(CharsPanel* st) {
    for (int i = 0; i < st->nRows; i++) {
        str::Free(st->rows[i].name);
        str::Free(st->rows[i].voice);
        st->rows[i] = CharRow{};
    }
    st->nRows = 0;
}

static void FreeFound(CharsPanel* st) {
    for (int i = 0; i < st->nFound; i++) {
        str::Free(st->found[i].url);
        str::Free(st->found[i].model);
        str::Free(st->found[i].kind);
        st->found[i] = CharsPanel::Found{};
    }
    st->nFound = 0;
}

static void FreeEndpoints(CharsPanel* st) {
    for (int i = 0; i < st->nEps; i++) {
        str::Free(st->eps[i].url);
        str::Free(st->eps[i].model);
        st->eps[i] = CharsPanel::Endpoint{};
    }
    st->nEps = 0;
}

// Push the current list (minus this computer, which is implicit) to the
// engine and remember it, so it survives a restart.
static void SaveEndpoints(CharsPanel* st) {
    str::Builder b;
    str::Builder pref;
    b.Append("{\"urls\":[");
    bool first = true;
    for (int i = 0; i < st->nEps; i++) {
        if (st->eps[i].local || len(st->eps[i].url) == 0) {
            continue;
        }
        if (!first) {
            b.Append(",");
            pref.Append(",");
        }
        b.Append(fmt("\"%s\"", st->eps[i].url));
        pref.Append(st->eps[i].url);
        first = false;
    }
    b.Append("]}");
    ControlPost(st->port, "/endpoints", ToStr(b));
    str::ReplaceWithCopy(&gGlobalPrefs->audiobook.lmUrls, ToStr(pref));
    SaveSettings();
}

// Poll the engine. Returns false if it isn't answering (yet).
static bool LoadState(CharsPanel* st) {
    FreeRows(st);
    FreeEndpoints(st);
    FreeFound(st);
    st->analyzed = false;
    st->analyzing = false;
    st->scanning = false;

    TempStr js = ControlGet(st->port, StrL("/state"));
    if (len(js) == 0) {
        st->engineUp = false;
        return false;
    }
    st->engineUp = true;

    // The engine re-asks the TTS server for the voice list on every /state, so
    // one busy moment there answers with none. Parse into a fresh list and
    // only take it if it has something: otherwise a hiccup would blank every
    // dropdown mid-poll and read as "the voice was lost".
    StrVec prevVoices;
    for (int i = 0; i < st->voices.size; i++) {
        prevVoices.Append(st->voices.At(i));
    }
    st->voices.Reset();
    // same for the LLM list: LM Studio may be busy loading a model
    StrVec prevModels;
    for (int i = 0; i < st->lmModels.size; i++) {
        prevModels.Append(st->lmModels.At(i));
    }
    st->lmModels.Reset();

    StateParser p(st);
    json::Parse(Str(js), &p);

    // One engine per machine, but a panel in every window: if the engine has
    // someone else's book, none of what it just told us is about ours.
    st->otherBook = false;
    WindowTab* tab = st->win ? st->win->CurrentTab() : nullptr;
    Str ourPdf = tab ? tab->filePath : Str();
    if (len(st->enginePdf) > 0 && len(ourPdf) > 0 && !path::IsSame(st->enginePdf, ourPdf)) {
        st->otherBook = true;
        FreeRows(st);
        FreeEndpoints(st);
        FreeFound(st);
        st->analyzed = false;
        st->analyzing = false;
        return true;
    }

    if (st->voices.size == 0) {
        for (int i = 0; i < prevVoices.size; i++) {
            st->voices.Append(prevVoices.At(i));
        }
    }
    if (st->lmModels.size == 0) {
        for (int i = 0; i < prevModels.size; i++) {
            st->lmModels.Append(prevModels.At(i));
        }
    }

    // "Already added" is worked out here, not taken from the engine: the
    // engine decided it when the scan ran, and anything added since - by this
    // panel or another window - would still be offered as new.
    for (int i = 0; i < st->nFound; i++) {
        for (int j = 0; j < st->nEps; j++) {
            if (str::EqI(st->found[i].url, st->eps[j].url)) {
                st->found[i].known = true;
                break;
            }
        }
    }

    int mode = SortIdx();
    for (int i = 1; i < st->nRows; i++) {
        CharRow r = st->rows[i];
        int j = i - 1;
        while (j >= 0 && RowBefore(r, st->rows[j], mode)) {
            st->rows[j + 1] = st->rows[j];
            j--;
        }
        st->rows[j + 1] = r;
    }

    // the narrator is a cast slot too - show it first
    if (st->nRows < kMaxRows) {
        for (int i = st->nRows; i > 0; i--) {
            st->rows[i] = st->rows[i - 1];
        }
        st->rows[0] = CharRow{};
        st->rows[0].name = str::Dup(StrL("narrator"));
        st->rows[0].voice = str::Dup(p.narrator);
        st->nRows++;
    }
    return true;
}

// --- UI -------------------------------------------------------------------

static LRESULT CALLBACK ComboWheelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL) {
        return SendMessageW(GetParent(hwnd), msg, wp, lp);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static HWND MkText(HWND parent, const WCHAR* s, int x, int y, int dx, int dy, HFONT f, UINT extra = 0) {
    HWND h = CreateWindowExW(0, WC_STATICW, s, WS_CHILD | WS_VISIBLE | SS_LEFT | extra, x, y, dx, dy, parent, nullptr,
                             GetModuleHandle(nullptr), nullptr);
    HwndSetFont(h, f);
    return h;
}

static HWND MkButton(HWND parent, const WCHAR* s, int id, int x, int y, int dx, int dy, HFONT f) {
    HWND h = CreateWindowExW(0, WC_BUTTONW, s, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, x, y, dx, dy, parent,
                             (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    HwndSetFont(h, f);
    return h;
}

// One line saying what the engine is doing, so the panel is never a dead end.
static TempStr StatusTextTemp(CharsPanel* st) {
    if (st->otherBook) {
        TempStr other = path::GetBaseNameTemp(st->enginePdf);
        return fmt(
            "Chatterbox is in use by another window, reading:\n%s\n\n"
            "Close it there (or its Characters panel) to use it for this document.",
            other);
    }
    if (!st->engineUp) {
        return fmt("%s", StrL("Starting Chatterbox and loading voices..."));
    }
    if (st->analyzing) {
        Str s = st->analyzeStatus;
        if (len(s) > 0) {
            return fmt("Finding who speaks each line:\n%s", s);
        }
        return fmt("%s", StrL("Finding who speaks each line..."));
    }
    if (len(st->analyzeError) > 0) {
        return fmt("Analysis failed:\n%s", st->analyzeError);
    }
    if (!st->analyzed) {
        return fmt("%s", StrL("This book hasn't been analysed, so it reads in the narrator's voice."));
    }
    if (!st->analyzeComplete && st->linesTotal > 0) {
        // partial: say what it means out loud rather than leaving the reader
        // to wonder why later chapters are all narrator
        return fmt(
            "Analysed %d of %d spoken lines. The rest reads in the narrator's voice - "
            "Continue analysing picks up where it stopped.",
            st->linesRead, st->linesTotal);
    }
    return fmt("%s", StrL("A character with no voice is read by the narrator."));
}

// One line under the scan button. Progress is a count of doors knocked on,
// which is the only honest measure: most addresses are empty and answer only
// by timing out.
static TempStr ScanStatusTextTemp(CharsPanel* st) {
    if (st->scanning) {
        if (st->scanTotal > 0) {
            return fmt("Looking... %d of %d", st->scanDone, st->scanTotal);
        }
        return fmt("%s", StrL("Looking..."));
    }
    if (len(st->scanMsg) > 0) {
        return fmt("%s", st->scanMsg);
    }
    return fmt("%s", StrL(""));
}

// A rebuild is only worth it when the shape changes, not on every poll.
static TempStr SignatureTemp(CharsPanel* st) {
    str::Builder b;
    b.Append(fmt("%d|%d|%d|%d|%d|%d|%d|%d|%d|%s", (int)st->engineUp, (int)st->otherBook, (int)st->analyzed,
                 (int)st->analyzing, (int)st->analyzeComplete, st->nRows, st->voices.size, st->lmModels.size,
                 ClientRect(st->hwnd).dx, st->lmModel));
    // the computer list and each one's reachability: a machine coming or
    // going has to redraw the rows
    for (int i = 0; i < st->nEps; i++) {
        b.Append(fmt("|%s,%d", st->eps[i].url, (int)st->eps[i].ok));
    }
    // scan progress is deliberately absent: it ticks every poll, and rebuilding
    // the panel a second apart would destroy the box you're typing into. The
    // count goes straight to the status line instead. What's found does change
    // the shape.
    b.Append(fmt("|s%d,%d", (int)st->scanning, st->nFound));
    for (int i = 0; i < st->nFound; i++) {
        b.Append(fmt("|f%s,%d", st->found[i].url, (int)st->found[i].known));
    }
    return str::DupTemp(ToStr(b));
}

static void DestroyChildren(HWND hwnd) {
    HWND child = GetWindow(hwnd, GW_CHILD);
    while (child) {
        HWND next = GetWindow(child, GW_HWNDNEXT);
        DestroyWindow(child);
        child = next;
    }
}

// Lay the panel out down its width. A sidebar is narrow, so each character is
// a stack (name / voice / buttons) rather than a wide row.
static void BuildRows(CharsPanel* st) {
    HWND hwnd = st->hwnd;
    HFONT f = st->font;
    DestroyChildren(hwnd);
    st->hScanStatus = nullptr;
    st->hAddEdit = nullptr;

    int dxPanel = ClientRect(hwnd).dx;
    if (dxPanel <= 0) {
        return;
    }
    int pad = DpiScale(hwnd, 8);
    int rowH = DpiScale(hwnd, 22);
    int comboH = DpiScale(hwnd, 24);
    int gap = DpiScale(hwnd, 4);
    int w = dxPanel - pad * 2;
    if (w < DpiScale(hwnd, 60)) {
        w = DpiScale(hwnd, 60);
    }
    int y = pad - st->scrollY;

    if (!st->otherBook && st->nRows > 2) {
        HWND hSort =
            CreateWindowExW(0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, pad, y, w,
                            comboH * 8, hwnd, (HMENU)(INT_PTR)kIdSort, GetModuleHandle(nullptr), nullptr);
        SetWindowSubclass(hSort, ComboWheelProc, 0, 0);
        HwndSetFont(hSort, f);
        for (const WCHAR* s : kSortLabels) {
            ComboBox_AddString(hSort, s);
        }
        ComboBox_SetCurSel(hSort, SortIdx());
        y += comboH + gap;
    }

    // Someone else's book: show why, and nothing else. Its cast and its
    // Analyse button would both act on a document this window doesn't have.
    if (st->otherBook) {
        st->contentDy = 0;
        HwndScheduleRepaint(hwnd);
        return;
    }

    for (int i = 0; i < st->nRows; i++) {
        CharRow& r = st->rows[i];
        bool isNarrator = str::Eq(r.name, StrL("narrator"));
        TempStr label;
        if (isNarrator) {
            label = str::DupTemp(StrL("Narrator"));
        } else if (r.lines > 0) {
            label = fmt("%s  (%d lines)", r.name, r.lines);
        } else {
            label = str::DupTemp(r.name);
        }
        if (len(r.voice) == 0) {
            label = fmt("%s  - needs a voice", label);
        }
        r.hName = MkText(hwnd, CWStrTemp(label), pad, y, w, rowH, f, SS_ENDELLIPSIS);
        y += rowH;

        r.hVoice = CreateWindowExW(
            0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, pad, y, w,
            comboH * 8, hwnd, (HMENU)(INT_PTR)(kIdVoiceFirst + i), GetModuleHandle(nullptr), nullptr);
        SetWindowSubclass(r.hVoice, ComboWheelProc, 0, 0);
        HwndSetFont(r.hVoice, f);
        ComboBox_AddString(r.hVoice, L"(none)");
        int sel = 0;
        for (int v = 0; v < st->voices.size; v++) {
            Str vn = st->voices.At(v);
            ComboBox_AddString(r.hVoice, CWStrTemp(vn));
            if (str::Eq(vn, r.voice)) {
                sel = v + 1;
            }
        }
        ComboBox_SetCurSel(r.hVoice, sel);
        y += comboH + gap / 2;

        int wBtn = (w - gap) / 2;
        r.hTest = MkButton(hwnd, L"Test", kIdTestFirst + i, pad, y, wBtn, rowH, f);
        r.hTrain = MkButton(hwnd, L"Train...", kIdTrainFirst + i, pad + wBtn + gap, y, wBtn, rowH, f);
        EnableWindow(r.hTest, len(r.voice) > 0);
        y += rowH + pad;
    }

    if (st->engineUp && st->nRows <= 1 && !st->analyzing) {
        // only the narrator: nothing has been analysed for this book yet
        MkText(hwnd, L"No characters found yet.", pad, y, w, rowH, f);
        y += rowH + gap;
    }

    bool useBookNlp = str::EqI(gGlobalPrefs->audiobook.analyzer, StrL("booknlp"));

    // The computers doing the work. Reading a novel is ~50 LLM calls of ~45s;
    // a second machine genuinely halves it. Shown with live status because
    // the usual outcome of adding one is silence: LM Studio only listens on
    // 127.0.0.1 until "Serve on Local Network" is switched on over there, and
    // without saying so the reader just sees no speed-up and no reason.
    //
    // One entry per *server*, not per machine: a computer running LM Studio
    // and Ollama at once is two, each with its own models. Irrelevant under
    // BookNLP, which runs locally with no servers to share out to.
    if (st->engineUp && !useBookNlp) {
        MkText(hwnd, L"Analyse on", pad, y, w, rowH, st->fontBold);
        y += rowH;
        for (int i = 0; i < st->nEps; i++) {
            CharsPanel::Endpoint& e = st->eps[i];
            TempStr name = e.local ? str::DupTemp(StrL("This computer")) : str::DupTemp(e.url);
            TempStr line;
            if (!e.ok) {
                line = fmt("%s - not reachable", name);
            } else if (len(e.model) > 0) {
                line = fmt("%s - %s", name, e.model);
            } else {
                line = fmt("%s - no model", name);
            }
            int wRemove = e.local ? 0 : DpiScale(hwnd, 60);
            MkText(hwnd, CWStrTemp(line), pad, y, w - wRemove - (wRemove ? gap : 0), rowH, f, SS_ENDELLIPSIS);
            if (!e.local) {
                MkButton(hwnd, L"Remove", kIdRemoveEpFirst + i, pad + w - wRemove, y, wRemove, rowH, f);
            }
            y += rowH;
        }
        // add another
        int wAdd = DpiScale(hwnd, 52);
        st->hAddEdit = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL, pad, y,
            w - wAdd - gap, rowH, hwnd, (HMENU)(INT_PTR)kIdAddEdit, GetModuleHandle(nullptr), nullptr);
        HwndSetFont(st->hAddEdit, f);
        Edit_SetCueBannerText(st->hAddEdit, L"http://192.168.1.20:11434");
        MkButton(hwnd, L"Add", kIdAddEndpoint, pad + w - wAdd, y, wAdd, rowH, f);
        y += rowH + gap;

        // Typing an address in assumes you know it. Usually you don't - you
        // know the machine in the study is switched on. Nothing on the network
        // announces itself, so this goes and knocks on every door.
        if (st->scanning) {
            MkButton(hwnd, L"Stop looking", kIdScanStop, pad, y, w, rowH, f);
        } else {
            MkButton(hwnd, L"Look for computers on my network", kIdScan, pad, y, w, rowH, f);
        }
        y += rowH;
        st->hScanStatus = MkText(hwnd, CWStrTemp(ScanStatusTextTemp(st)), pad, y, w, rowH, f, SS_ENDELLIPSIS);
        y += rowH;

        // What it found, each offered as one click. Shown with its model so
        // it's recognisable as the machine you meant.
        for (int i = 0; i < st->nFound; i++) {
            CharsPanel::Found& fo = st->found[i];
            if (fo.known) {
                continue; // already in the list above
            }
            int wAddF = DpiScale(hwnd, 52);
            TempStr line = len(fo.model) > 0 ? fmt("%s - %s", fo.url, fo.model) : str::DupTemp(fo.url);
            MkText(hwnd, CWStrTemp(line), pad, y, w - wAddF - gap, rowH, f, SS_ENDELLIPSIS);
            MkButton(hwnd, L"Add", kIdAddFoundFirst + i, pad + w - wAddF, y, wAddF, rowH, f);
            y += rowH;
        }
        y += gap / 2;

        // No hard line breaks: the panel is a splitter away from any width,
        // so a break that fits today wraps to a clipped third line tomorrow.
        // Let the static wrap it, and give it the room to.
        MkText(hwnd,
               L"Another computer needs LM Studio with \"Serve on Local "
               L"Network\" turned on, or Ollama running.",
               pad, y, w, rowH * 3, f);
        y += rowH * 3 + gap;
    }

    st->contentDy = y + st->scrollY;
    HwndScheduleRepaint(hwnd);
}

static void UpdateProgressBar(CharsPanel* st) {
    if (!st->hProgress) {
        return;
    }
    LONG_PTR style = GetWindowLongPtrW(st->hProgress, GWL_STYLE);
    if (st->linesTotal > 0) {
        if (style & PBS_MARQUEE) {
            SetWindowLongPtrW(st->hProgress, GWL_STYLE, style & ~PBS_MARQUEE);
            SendMessageW(st->hProgress, PBM_SETMARQUEE, FALSE, 0);
        }
        SendMessageW(st->hProgress, PBM_SETRANGE32, 0, st->linesTotal);
        SendMessageW(st->hProgress, PBM_SETPOS, st->linesRead, 0);
    } else if (!(style & PBS_MARQUEE)) {
        SetWindowLongPtrW(st->hProgress, GWL_STYLE, style | PBS_MARQUEE);
        SendMessageW(st->hProgress, PBM_SETMARQUEE, TRUE, 0);
    }
}

static void BuildFooter(CharsPanel* st) {
    HWND hwnd = st->hwndFooter;
    HFONT f = st->font;
    DestroyChildren(hwnd);
    st->hModel = nullptr;
    st->hProgress = nullptr;

    if (!st->engineUp || st->otherBook) {
        st->footerDy = 0;
        return;
    }

    int dxPanel = ClientRect(hwnd).dx;
    int pad = DpiScale(hwnd, 8);
    int rowH = DpiScale(hwnd, 22);
    int comboH = DpiScale(hwnd, 24);
    int gap = DpiScale(hwnd, 4);
    int sepDy = 2;
    int w = dxPanel - pad * 2;
    if (w < DpiScale(hwnd, 60)) {
        w = DpiScale(hwnd, 60);
    }

    CreateWindowExW(0, WC_STATICW, nullptr, WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 0, 0, dxPanel, sepDy, hwnd, nullptr,
                    GetModuleHandle(nullptr), nullptr);
    int y = sepDy + gap;

    // Who works out the speakers. BookNLP is local and needs no LLM server;
    // the LLM path handles the hardest untagged lines a little better. The
    // choice sticks and travels with the next Analyse (no engine restart).
    bool useBookNlp = str::EqI(gGlobalPrefs->audiobook.analyzer, StrL("booknlp"));
    MkText(hwnd, L"Find speakers with", pad, y, w, rowH, f);
    y += rowH;
    HWND hMethod =
        CreateWindowExW(0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, pad, y, w,
                        comboH * 4, hwnd, (HMENU)(INT_PTR)kIdAnalyzer, GetModuleHandle(nullptr), nullptr);
    SetWindowSubclass(hMethod, ComboWheelProc, 0, 0);
    HwndSetFont(hMethod, f);
    ComboBox_AddString(hMethod, L"A local LLM (LM Studio / Ollama)");
    ComboBox_AddString(hMethod, L"BookNLP (local, no LLM needed)");
    ComboBox_SetCurSel(hMethod, useBookNlp ? 1 : 0);
    EnableWindow(hMethod, !st->analyzing);
    y += comboH + gap;

    // Which LLM finds the speakers. Only worth showing when there's a choice
    // to make: with one model there's nothing to pick, and naming it is what
    // makes the server load it, so it just works. BookNLP uses no LLM, so this
    // is hidden when it's chosen.
    if (!useBookNlp && st->lmModels.size > 1) {
        MkText(hwnd, L"Analyse with", pad, y, w, rowH, f);
        y += rowH;
        st->hModel = CreateWindowExW(0, WC_COMBOBOXW, nullptr,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, pad, y, w,
                                     comboH * 8, hwnd, (HMENU)(INT_PTR)kIdModel, GetModuleHandle(nullptr), nullptr);
        SetWindowSubclass(st->hModel, ComboWheelProc, 0, 0);
        HwndSetFont(st->hModel, f);
        ComboBox_AddString(st->hModel, L"(choose automatically)");
        int sel = 0;
        for (int i = 0; i < st->lmModels.size; i++) {
            Str id = st->lmModels.At(i);
            ComboBox_AddString(st->hModel, CWStrTemp(id));
            if (str::Eq(id, st->lmModel)) {
                sel = i + 1;
            }
        }
        ComboBox_SetCurSel(st->hModel, sel);
        EnableWindow(st->hModel, !st->analyzing);
        y += comboH + gap;
    }

    // How much of the book to read. A novel is hundreds of LLM calls, so
    // "just the first quarter" is a real answer - the rest still reads, in
    // the narrator's voice.
    if (!st->analyzing) {
        MkText(hwnd, L"Analyse", pad, y, w, rowH, f);
        y += rowH;
        HWND hAmount =
            CreateWindowExW(0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, pad, y, w,
                            comboH * 6, hwnd, (HMENU)(INT_PTR)kIdAmount, GetModuleHandle(nullptr), nullptr);
        SetWindowSubclass(hAmount, ComboWheelProc, 0, 0);
        HwndSetFont(hAmount, f);
        for (const AnalyzeAmount& a : kAmounts) {
            ComboBox_AddString(hAmount, a.label);
        }
        ComboBox_SetCurSel(hAmount, st->amountIdx);
        y += comboH + gap;
    }

    // Analyse is the only thing that ever runs the LLM: it costs minutes, so
    // it's a button, not a surprise on the way to reading. While it runs the
    // same slot is Stop - which keeps the work rather than binning it.
    if (st->analyzing) {
        MkButton(hwnd, L"Stop and keep what's read", kIdStopAnalyze, pad, y, w, DpiScale(hwnd, 26), f);
        y += DpiScale(hwnd, 26) + gap;
        int progDy = DpiScale(hwnd, 14);
        st->hProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE, pad, y, w, progDy, hwnd,
                                        nullptr, GetModuleHandle(nullptr), nullptr);
        y += progDy + gap;
        UpdateProgressBar(st);
    } else {
        const WCHAR* label = L"Analyse document";
        if (st->analyzed && !st->analyzeComplete) {
            label = L"Continue analysing";
        } else if (st->analyzed) {
            label = L"Re-analyse document";
        }
        MkButton(hwnd, label, kIdAnalyze, pad, y, w, DpiScale(hwnd, 26), f);
        y += DpiScale(hwnd, 26) + gap;
    }

    st->footerDy = y - gap + pad;
    HwndScheduleRepaint(hwnd);
}

static void LayoutCharsContainer(CharsPanel* st) {
    if (!st || !st->hwndContainer || !st->label) {
        return;
    }
    Rect rc = ClientRect(st->hwndContainer);
    if (rc.dx <= 0 || rc.dy <= 0) {
        return;
    }
    int pad = DpiScale(st->hwndContainer, 8);
    int labelDy = st->label->GetIdealSize().dy;
    MoveWindow(st->label->hwnd, 0, 0, rc.dx, labelDy, TRUE);

    int subW = rc.dx - pad * 2;
    if (subW < DpiScale(st->hwndContainer, 10)) {
        subW = DpiScale(st->hwndContainer, 10);
    }
    int subDy = DpiScale(st->hwndContainer, 16);
    TempStr s = HwndGetTextTemp(st->hSubtitle);
    if (len(s) > 0) {
        HDC hdc = GetDC(st->hSubtitle);
        Size sz = HdcMeasureText(hdc, Str(s), subW, DT_WORDBREAK | DT_NOPREFIX, st->font);
        ReleaseDC(st->hSubtitle, hdc);
        if (sz.dy > subDy) {
            subDy = sz.dy;
        }
    }
    MoveWindow(st->hSubtitle, pad, labelDy + pad / 2, subW, subDy, TRUE);

    int top = labelDy + pad / 2 + subDy + pad / 2;
    int footDy = limitValue(st->footerDy, 0, rc.dy > top ? rc.dy - top : 0);
    int scrollDy = rc.dy - top - footDy;
    if (scrollDy < 0) {
        scrollDy = 0;
    }
    MoveWindow(st->hwnd, 0, top, rc.dx, scrollDy, TRUE);
    MoveWindow(st->hwndFooter, 0, top + scrollDy, rc.dx, footDy, TRUE);
}

// scroll range follows the content: a novel has more characters than fit
static void UpdateScrollbar(CharsPanel* st) {
    if (!st->hwnd) {
        return;
    }
    int dy = ClientRect(st->hwnd).dy;
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = st->contentDy > 0 ? st->contentDy : 0;
    si.nPage = dy;
    si.nPos = st->scrollY;
    SetScrollInfo(st->hwnd, SB_VERT, &si, TRUE);
}

// How far one notch of the wheel goes: whatever the reader told Windows they
// want, rather than a number invented here. The "one screen" setting is a real
// value, not a silly one.
static int WheelStepPx(CharsPanel* st) {
    UINT lines = 3;
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    int lineH = DpiScale(st->hwnd, 16);
    if (lines == WHEEL_PAGESCROLL) {
        int dy = ClientRect(st->hwnd).dy;
        return dy > lineH ? dy - lineH : lineH;
    }
    if (lines == 0) {
        lines = 3;
    }
    return (int)lines * lineH;
}

static int MaxScroll(CharsPanel* st) {
    int dy = ClientRect(st->hwnd).dy;
    int m = st->contentDy - dy;
    return m > 0 ? m : 0;
}

// Scrolling moves what's already there. It used to rebuild the panel, which
// meant every wheel notch destroyed and re-created every control in it - on a
// novel's worth of characters, hundreds of windows per notch, and it showed.
// The children already sit at their content position minus scrollY, so a
// rebuild would put them exactly where ScrollWindowEx does.
static void ScrollTo(CharsPanel* st, int pos) {
    pos = limitValue(pos, 0, MaxScroll(st));
    int dy = st->scrollY - pos;
    if (dy == 0) {
        return;
    }
    st->scrollY = pos;
    ScrollWindowEx(st->hwnd, 0, dy, nullptr, nullptr, nullptr, nullptr, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    UpdateScrollbar(st);
}

// Poll the engine and refresh: rebuild if the shape changed, otherwise just
// retitle the status line (so an open combo box isn't yanked away).
static void RefreshFromEngine(CharsPanel* st, bool force) {
    if (!st->hwnd) {
        return;
    }
    LoadState(st);
    // the window that had the engine has closed it: this one can have it now
    if (!st->engineUp && !st->triedStart) {
        st->triedStart = true;
        AudiobookEnsureEngineForCurrentTab();
        LoadState(st);
    }
    TempStr sig = SignatureTemp(st);
    if (force || !str::Eq(st->builtFor, sig)) {
        str::ReplaceWithCopy(&st->builtFor, sig);
        BuildRows(st);
        BuildFooter(st);
        LayoutCharsContainer(st);
        UpdateScrollbar(st);
    }
    TempStr status = StatusTextTemp(st);
    if (!str::Eq(st->subtitle, Str(status))) {
        str::ReplaceWithCopy(&st->subtitle, Str(status));
        HwndSetText(st->hSubtitle, Str(status));
        LayoutCharsContainer(st);
        HwndScheduleRepaint(st->hSubtitle);
    }
    // ticks every poll, so it updates in place rather than through a rebuild
    if (st->hScanStatus) {
        HwndSetText(st->hScanStatus, ScanStatusTextTemp(st));
        HwndScheduleRepaint(st->hScanStatus);
    }
    UpdateProgressBar(st);
}

// Take a full "http://host:port" into the pool, from the box or from what the
// scan turned up. Both routes have to dedupe and persist the same way.
static void AddEndpointUrl(CharsPanel* st, Str url) {
    if (len(url) == 0 || st->nEps >= kMaxEps) {
        return;
    }
    for (int i = 0; i < st->nEps; i++) {
        if (str::EqI(st->eps[i].url, url)) {
            return; // already there
        }
    }
    CharsPanel::Endpoint& e = st->eps[st->nEps++];
    e = CharsPanel::Endpoint{};
    e.url = str::Dup(url);
    e.local = false;
    SaveEndpoints(st);
    PostMessageW(st->hwnd, kMsgRefreshRows, 0, 0);
}

// Add the computer typed into the box. Accepts "192.168.1.20" and fills in
// the rest: nobody should have to remember LM Studio's port to share a job
// with the machine in the next room.
static void OnAddEndpoint(CharsPanel* st) {
    if (!st->hAddEdit || st->nEps >= kMaxEps) {
        return;
    }
    TempStr typed = HwndGetTextTemp(st->hAddEdit);
    str::Builder b;
    for (int i = 0; i < len(typed); i++) {
        char c = typed.s[i];
        if (c != ' ' && c != '\t' && c != '"') {
            b.AppendChar(c);
        }
    }
    TempStr url = str::DupTemp(ToStr(b));
    if (len(url) == 0) {
        return;
    }
    if (!str::StartsWithI(url, StrL("http://")) && !str::StartsWithI(url, StrL("https://"))) {
        url = fmt("http://%s", url);
    }
    // No port given: reuse the one this machine's LM Studio is on rather than
    // LM Studio's stock 1234. Someone running a non-default port here almost
    // certainly set the same one up on the other machine.
    Str hostPart = url;
    hostPart.s += 7; // past "http://"
    hostPart.len -= 7;
    if (!str::Contains(hostPart, StrL(":"))) {
        int port = 1234;
        // "http://127.0.0.1:11434" -> ":11434"
        Str tail = str::SliceFromCharLast(gGlobalPrefs->audiobook.lmStudioUrl, ':');
        if (len(tail) > 1) {
            int p = atoi(CStrTemp(tail) + 1);
            if (p > 0 && p < 65536) {
                port = p;
            }
        }
        url = fmt("%s:%d", url, port);
    }
    AddEndpointUrl(st, url);
    if (st->hAddEdit) {
        HwndSetText(st->hAddEdit, StrL(""));
    }
}

// The choice sticks: it's a property of this machine's LLM setup, not of the
// book, so it belongs in settings rather than being asked again every launch.
static void OnModelChanged(CharsPanel* st) {
    int sel = ComboBox_GetCurSel(st->hModel);
    Str model = (sel <= 0) ? Str() : st->lmModels.At(sel - 1);
    str::ReplaceWithCopy(&st->lmModel, model);
    TempStr body = fmt("{\"model\":\"%s\"}", model);
    ControlPost(st->port, "/model", Str(body));
    str::ReplaceWithCopy(&gGlobalPrefs->audiobook.lmModel, model);
    SaveSettings();
}

static void OnVoiceChanged(CharsPanel* st, int row, HWND hCombo) {
    CharRow& r = st->rows[row];
    int sel = ComboBox_GetCurSel(hCombo);
    if (sel < 0) {
        return;
    }
    Str voice = (sel == 0) ? Str() : st->voices.At(sel - 1);
    str::Free(r.voice);
    r.voice = str::Dup(voice);
    TempStr body = (len(voice) > 0) ? fmt("{\"character\":\"%s\",\"voice\":\"%s\"}", r.name, voice)
                                    : fmt("{\"character\":\"%s\",\"voice\":null}", r.name);
    ControlPost(st->port, "/cast", Str(body));
    if (r.hTest) {
        EnableWindow(r.hTest, len(voice) > 0);
    }
    PostMessageW(st->hwnd, kMsgRefreshRows, 0, 0);
}

static LRESULT CALLBACK CharsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CharsPanel* st = gPanel;
    if (!st || (st->hwnd != hwnd && st->hwndFooter != hwnd)) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case kMsgRefreshRows:
            RefreshFromEngine(st, true);
            return 0;

        case WM_TIMER:
            if (wp == kPollTimerId) {
                RefreshFromEngine(st, false);
                return 0;
            }
            break;

        case WM_SIZE:
            // the splitter moved: the stack has to re-flow to the new width
            if (hwnd == st->hwndFooter) {
                BuildFooter(st);
            } else {
                BuildRows(st);
                UpdateScrollbar(st);
            }
            return 0;

        case WM_VSCROLL: {
            int pos = st->scrollY;
            int line = DpiScale(hwnd, 24);
            switch (LOWORD(wp)) {
                case SB_LINEUP:
                    pos -= line;
                    break;
                case SB_LINEDOWN:
                    pos += line;
                    break;
                case SB_PAGEUP:
                    pos -= ClientRect(hwnd).dy;
                    break;
                case SB_PAGEDOWN:
                    pos += ClientRect(hwnd).dy;
                    break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION:
                    pos = HIWORD(wp);
                    break;
                default:
                    break;
            }
            ScrollTo(st, pos);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            // Deltas are not always a whole notch: a precision touchpad sends
            // a stream of small ones, and the old "delta / WHEEL_DELTA" threw
            // every one of them away as zero - the finer the gesture, the less
            // it did. Bank the remainder and move by however many pixels have
            // accumulated, so a slow drag creeps and a flick flies.
            st->wheelAccum += GET_WHEEL_DELTA_WPARAM(wp);
            int step = WheelStepPx(st);
            int px = MulDiv(st->wheelAccum, step, WHEEL_DELTA);
            if (px != 0) {
                st->wheelAccum -= MulDiv(px, WHEEL_DELTA, step);
                ScrollTo(st, st->scrollY - px);
            }
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);
            int code = HIWORD(wp);
            if (id == kIdAnalyze) {
                int pct = kAmounts[st->amountIdx].percent;
                // "Re-analyse" has to say so: a finished analysis comes back
                // from the cache before a single chunk is sent, so without
                // this the button reads the old answer and changes nothing.
                // "Continue analysing" must NOT force - resuming is the point.
                bool force = st->analyzed && st->analyzeComplete;
                Str analyzer = gGlobalPrefs->audiobook.analyzer;
                if (len(analyzer) == 0) {
                    analyzer = StrL("llm");
                }
                TempStr body = fmt("{\"percent\":%d,\"force\":%s,\"analyzer\":\"%s\"}", pct,
                                   force ? StrL("true") : StrL("false"), analyzer);
                if (ControlPost(st->port, "/analyze", Str(body))) {
                    st->analyzing = true;
                    PostMessageW(st->hwnd, kMsgRefreshRows, 0, 0);
                }
                return 0;
            }
            if (id == kIdStopAnalyze) {
                ControlPost(st->port, "/analyze_stop", {});
                PostMessageW(st->hwnd, kMsgRefreshRows, 0, 0);
                return 0;
            }
            if (id == kIdAddEndpoint) {
                OnAddEndpoint(st);
                return 0;
            }
            if (id == kIdScan) {
                ControlPost(st->port, "/scan", {});
                st->scanning = true;
                PostMessageW(st->hwnd, kMsgRefreshRows, 0, 0);
                return 0;
            }
            if (id == kIdScanStop) {
                ControlPost(st->port, "/scan_stop", {});
                PostMessageW(st->hwnd, kMsgRefreshRows, 0, 0);
                return 0;
            }
            if (id >= kIdAddFoundFirst && id < kIdAddFoundFirst + kMaxFound) {
                int i = id - kIdAddFoundFirst;
                if (i >= 0 && i < st->nFound) {
                    AddEndpointUrl(st, st->found[i].url);
                }
                return 0;
            }
            if (id >= kIdRemoveEpFirst && id < kIdRemoveEpFirst + kMaxEps) {
                int i = id - kIdRemoveEpFirst;
                if (i >= 0 && i < st->nEps && !st->eps[i].local) {
                    str::Free(st->eps[i].url);
                    str::Free(st->eps[i].model);
                    for (int j = i; j < st->nEps - 1; j++) {
                        st->eps[j] = st->eps[j + 1];
                    }
                    st->eps[st->nEps - 1] = CharsPanel::Endpoint{};
                    st->nEps--;
                    SaveEndpoints(st);
                    PostMessageW(st->hwnd, kMsgRefreshRows, 0, 0);
                }
                return 0;
            }
            if (id == kIdSort && code == CBN_SELCHANGE) {
                int sel = ComboBox_GetCurSel((HWND)lp);
                if (sel >= 0 && sel < dimofi(kSortLabels)) {
                    str::ReplaceWithCopy(&gGlobalPrefs->audiobook.charSort, Str(SortKey(sel)));
                    SaveSettings();
                }
                PostMessageW(hwnd, kMsgRefreshRows, 0, 0);
                return 0;
            }
            if (id == kIdAmount && code == CBN_SELCHANGE) {
                int sel = ComboBox_GetCurSel((HWND)lp);
                if (sel >= 0 && sel < dimofi(kAmounts)) {
                    st->amountIdx = sel;
                }
                return 0;
            }
            if (id == kIdModel && code == CBN_SELCHANGE) {
                OnModelChanged(st);
                return 0;
            }
            if (id == kIdAnalyzer && code == CBN_SELCHANGE) {
                int sel = ComboBox_GetCurSel((HWND)lp);
                const char* which = (sel == 1) ? "booknlp" : "llm";
                str::ReplaceWithCopy(&gGlobalPrefs->audiobook.analyzer, Str(which));
                SaveSettings();
                // the LLM-only rows appear or vanish with the choice
                PostMessageW(hwnd, kMsgRefreshRows, 0, 0);
                return 0;
            }
            if (id >= kIdVoiceFirst && id < kIdVoiceFirst + kMaxRows && code == CBN_SELCHANGE) {
                OnVoiceChanged(st, id - kIdVoiceFirst, (HWND)lp);
                return 0;
            }
            if (id >= kIdTestFirst && id < kIdTestFirst + kMaxRows) {
                CharRow& r = st->rows[id - kIdTestFirst];
                TempStr body = fmt("{\"character\":\"%s\"}", r.name);
                ControlPost(st->port, "/test", Str(body));
                return 0;
            }
            if (id >= kIdTrainFirst && id < kIdTrainFirst + kMaxRows) {
                CharRow& r = st->rows[id - kIdTrainFirst];
                WindowTab* tab = st->win ? st->win->CurrentTab() : nullptr;
                AudiobookTrainVoice(r.name, tab ? tab->filePath : Str());
                return 0;
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wp, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);

        case WM_ERASEBKGND: {
            Rect rc = ClientRect(hwnd);
            RECT r = ToRECT(rc);
            FillRect((HDC)wp, &r, GetSysColorBrush(COLOR_BTNFACE));
            return 1;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK WndProcCharsContainer(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    CharsPanel* st = gPanel;
    if (!st || st->hwndContainer != hwnd) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    switch (msg) {
        case WM_SIZE:
            LayoutCharsContainer(st);
            return 0;

        case WM_COMMAND:
            if (LOWORD(wp) == kIdCloseChars) {
                PostMessageW(hwnd, kMsgClosePanel, 0, 0);
                return 0;
            }
            break;

        case kMsgClosePanel:
            ToggleAudiobookPanel(st->win);
            return 0;

        case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)wp, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);

        case WM_ERASEBKGND: {
            Rect rc = ClientRect(hwnd);
            RECT r = ToRECT(rc);
            FillRect((HDC)wp, &r, GetSysColorBrush(COLOR_BTNFACE));
            return 1;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void RegisterCharsClass() {
    static bool done = false;
    if (done) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CharsWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = kCharsClassName;
    RegisterClassExW(&wc);
    done = true;
}

// --- public API -----------------------------------------------------------

constexpr int kAudiobookMinDx = 180;

static void OnAudiobookSplitterMove(Splitter::MoveEvent* ev) {
    Splitter* splitter = ev->w;
    MainWindow* win = FindMainWindowByHwnd(splitter->hwnd);
    if (!win) {
        return;
    }
    Point pcur = HwndGetCursorPos(win->hwndFrame);
    Rect rFrame = ClientRect(win->hwndFrame);
    int dx = pcur.x; // docked left: the width is the cursor's x
    if (dx < kAudiobookMinDx || dx > rFrame.dx / 2) {
        ev->resizeAllowed = false;
        return;
    }
    win->audiobookDx = dx;
    gGlobalPrefs->audiobook.sidebarDx = dx;
    if (ev->finishedDragging) {
        // write it now rather than trusting a clean exit to do it
        SaveSettings();
        ScheduleUiUpdate(win, kUiRelayout | kUiNoToolbars);
    }
}

bool IsAudiobookPanelVisible(MainWindow* win) {
    return win && win->uiState.audiobookVisible;
}

static void CreateAudiobookPanel(MainWindow* win) {
    if (win->hwndAudiobookBox) {
        return;
    }
    RegisterCharsClass();
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gGlobalPrefs->audiobook.sidebarDx;
    if (dx <= 0) {
        dx = DpiScale(win->hwndFrame, 260);
    }
    win->audiobookDx = dx;

    // WS_CHILD: this is docked in the frame beside the document, not a window
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    win->hwndAudiobookBox =
        CreateWindowExW(0, WC_STATICW, L"", style, 0, 0, dx, 0, win->hwndFrame, nullptr, hmod, nullptr);
    if (!win->hwndAudiobookBox) {
        return;
    }

    {
        Splitter::CreateArgs args;
        args.parent = win->hwndFrame;
        args.type = SplitterType::Vert;
        args.isLive = false;
        win->audiobookSplitter = new Splitter();
        win->audiobookSplitter->onMove = MkFunc1Void(OnAudiobookSplitterMove);
        win->audiobookSplitter->Create(args);
    }

    auto* st = new CharsPanel();
    st->win = win;
    st->hwndContainer = win->hwndAudiobookBox;
    st->font = GetDefaultGuiFont();
    st->fontBold = GetDefaultGuiFont(true, false);
    st->port = kAudiobookControlPortDefault;
    gPanel = st;

    auto* l = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = st->hwndContainer;
        args.cmdId = kIdCloseChars;
        args.isRtl = IsUIRtl();
        args.font = GetAppSidebarLabelFont(win->hwndFrame);
        l->Create(args);
    }
    st->label = l;
    l->SetPaddingXY(2, 2);
    l->SetLabel(StrL("Characters"));

    st->hSubtitle = CreateWindowExW(0, WC_STATICW, L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, 0, 0, 0,
                                    st->hwndContainer, nullptr, hmod, nullptr);
    HwndSetFont(st->hSubtitle, st->font);

    st->hwnd = CreateWindowExW(0, kCharsClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL, 0, 0, 0,
                               0, st->hwndContainer, nullptr, hmod, nullptr);
    st->hwndFooter = CreateWindowExW(0, kCharsClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 0, 0,
                                     st->hwndContainer, nullptr, hmod, nullptr);

    st->containerSubclassId = NextSubclassId();
    SetWindowSubclass(st->hwndContainer, WndProcCharsContainer, st->containerSubclassId, 0);

    // Ask first, then start. The control port is machine-wide: if another
    // window's engine already holds it, launching ours would just fail to
    // bind and exit, and we'd sit polling theirs. LoadState spots that and
    // sets otherBook, which is what the panel then explains.
    LoadState(st);
    if (!st->engineUp) {
        // nothing there: start ours idle. It loads the TTS model and serves
        // the cast without reading a word; the poll fills the panel in.
        st->triedStart = true;
        AudiobookEnsureEngineForCurrentTab();
        LoadState(st);
    }
    str::ReplaceWithCopy(&st->builtFor, SignatureTemp(st));
    BuildRows(st);
    BuildFooter(st);
    str::ReplaceWithCopy(&st->subtitle, Str(StatusTextTemp(st)));
    HwndSetText(st->hSubtitle, st->subtitle);
    LayoutCharsContainer(st);
    UpdateScrollbar(st);
    // the engine takes seconds to come up and analysis takes minutes: follow
    // both rather than making the reader hit refresh
    SetTimer(st->hwnd, kPollTimerId, kPollMs, nullptr);
}

void DestroyAudiobookPanel(MainWindow* win) {
    if (!win) {
        return;
    }
    if (gPanel && gPanel->win == win) {
        if (gPanel->hwnd) {
            KillTimer(gPanel->hwnd, kPollTimerId);
        }
        if (gPanel->hwndContainer && gPanel->containerSubclassId) {
            RemoveWindowSubclass(gPanel->hwndContainer, WndProcCharsContainer, gPanel->containerSubclassId);
        }
        FreeRows(gPanel);
        FreeEndpoints(gPanel);
        FreeFound(gPanel);
        str::Free(gPanel->analyzeStatus);
        str::Free(gPanel->analyzeError);
        str::Free(gPanel->lmModel);
        str::Free(gPanel->enginePdf);
        str::Free(gPanel->scanMsg);
        str::Free(gPanel->builtFor);
        str::Free(gPanel->subtitle);
        delete gPanel->label;
        delete gPanel;
        gPanel = nullptr;
    }
    if (win->hwndAudiobookBox) {
        DestroyWindow(win->hwndAudiobookBox);
        win->hwndAudiobookBox = nullptr;
    }
    delete win->audiobookSplitter;
    win->audiobookSplitter = nullptr;
    win->uiState.audiobookVisible = false;
}

// The command entry point: show/hide the docked panel.
void ToggleAudiobookPanel(MainWindow* win) {
    if (!win) {
        return;
    }
    if (win->uiState.audiobookVisible) {
        DestroyAudiobookPanel(win);
        ScheduleUiUpdate(win, kUiRelayout);
        return;
    }
    CreateAudiobookPanel(win);
    if (!win->hwndAudiobookBox) {
        return;
    }
    win->uiState.audiobookVisible = true;
    ScheduleUiUpdate(win, kUiRelayout);
}

// re-flow after the frame moved us (splitter drag / window resize)
void RelayoutAudiobookPanel(MainWindow* win) {
    if (!win || !win->hwndAudiobookBox || !gPanel || gPanel->win != win) {
        return;
    }
    LayoutCharsContainer(gPanel);
}
