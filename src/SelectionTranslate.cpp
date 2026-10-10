/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/AutoWin.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Http.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "TextSelection.h"
#include "Selection.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkMode.h"
#include "SelectionTranslate.h"
#include "SelectionTranslateCommon.h"

static const Str gPopularLanguages[] = {
    StrL("English"),
    StrL("Chinese (Simplified)"),
    StrL("Chinese (Traditional)"),
    StrL("Spanish"),
    StrL("Arabic"),
    StrL("Hindi"),
    StrL("Portuguese"),
    StrL("Bengali"),
    StrL("Russian"),
    StrL("Japanese"),
    StrL("Punjabi"),
    StrL("German"),
    StrL("French"),
    StrL("Korean"),
    StrL("Turkish"),
    StrL("Vietnamese"),
    StrL("Italian"),
    StrL("Polish"),
    StrL("Ukrainian"),
    StrL("Dutch"),
    StrL("Thai"),
    StrL("Indonesian"),
    StrL("Czech"),
    StrL("Swedish"),
    StrL("Romanian"),
    StrL("Greek"),
    StrL("Hebrew"),
    StrL("Danish"),
    StrL("Finnish"),
    StrL("Norwegian"),
    StrL("Hungarian"),
    StrL("Slovak"),
};

struct SelectionTranslateWnd : WindowBase {
    ~SelectionTranslateWnd() override;

    HWND hwndOwner = nullptr;
    // the labels and the buttons are virtual controls; the text fields and the
    // drop-downs are real HWNDs
    VirtText* staticPrompt = nullptr;
    DropDown* dropEngine = nullptr;
    Edit* editSrcText = nullptr;
    VirtText* staticFromLabel = nullptr;
    DropDown* dropSrcLang = nullptr;
    VirtText* staticToLabel = nullptr;
    DropDown* dropDstLang = nullptr;
    VirtButton* btnTranslate = nullptr;
    VirtButton* btnClose = nullptr;
    VirtText* staticResultLabel = nullptr;
    Edit* editResult = nullptr;

    // engine pre-selected in the dropdown when the dialog opens
    TranslateEngine engine = TranslateEngine::Google;
    // AI backend of the in-flight translation (for error formatting)
    AIChatBackend backend = AIChatBackend::Grok;
    bool translating = false;
    bool resultVisible = false;
    // true after the first size-to-content layout
    bool sizeInitialized = false;

    bool Create(HWND owner, Str selText, Str title);
    // initial: size to content and center; later: reflow keeping (or growing) current size
    void Relayout(bool initial = false);
    VirtButton* NewButton(Str text, bool isDefault);
    // the label changes width ("Translate" / "Translating..."), so the row is
    // laid out again
    void SetTranslateButtonText(Str);
    void UpdateTranslateButtonState();
    void ShowTranslationResult(Str text, bool isError);
    void StartTranslation(VirtMouseEvent* ev = nullptr);
    void OnTranslationFinished(bool ok, Str msg);
    void OnCloseClicked(VirtMouseEvent* ev = nullptr);

    void UpdateFont();

    void OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev);
    void OnDpiChanged(WindowBase::DpiChangedEvent* ev);
};

static SelectionTranslateWnd* gSelectionTranslateWnd = nullptr;

SelectionTranslateWnd::~SelectionTranslateWnd() = default;

struct SelectionTranslateTaskData {
    // the dialog can be closed and deleted (via ScheduleDelete) while the
    // translation thread runs, so remember only its HWND, never the object.
    // OnTranslateDone re-validates the HWND against gSelectionTranslateWnd.
    HWND hwndDlg = nullptr;
    AIChatBackend backend = AIChatBackend::Grok;
    Str srcLang;
    Str dstLang;
    Str text;
    ~SelectionTranslateTaskData() {
        str::Free(srcLang);
        str::Free(dstLang);
        str::Free(text);
    }
};

struct SelectionTranslateDoneData {
    HWND hwndDlg = nullptr;
    bool ok = false;
    Str msg;
    ~SelectionTranslateDoneData() { str::Free(msg); }
};

static void SelectionTranslateThread(SelectionTranslateTaskData* data);

static void PopulateLanguageDropDown(DropDown* dd, Str initial, bool includeAuto) {
    if (!dd) {
        return;
    }
    StrVec items;
    if (includeAuto) {
        items.Append(kSrcLangAuto);
    }
    for (Str lang : gPopularLanguages) {
        items.Append(lang);
    }
    dd->SetItems(items);
    if (!str::IsEmptyOrWhiteSpace(initial)) {
        dd->SetText(initial);
        for (int i = 0; i < len(items); i++) {
            if (str::EqI(items[i], initial)) {
                CbSetCurrentSelection(dd, i);
                return;
            }
        }
    }
}

// clang-format off
// Parallel LANG_* ids and English names; keep in the same order.
static const WORD gPrimaryLangIds[] = {
    LANG_ENGLISH,
    LANG_CHINESE,
    LANG_GERMAN,
    LANG_FRENCH,
    LANG_SPANISH,
    LANG_ITALIAN,
    LANG_PORTUGUESE,
    LANG_RUSSIAN,
    LANG_JAPANESE,
    LANG_KOREAN,
    LANG_ARABIC,
    LANG_HINDI,
    LANG_TURKISH,
    LANG_VIETNAMESE,
    LANG_POLISH,
    LANG_UKRAINIAN,
    LANG_DUTCH,
    LANG_THAI,
    LANG_INDONESIAN,
    LANG_CZECH,
    LANG_SWEDISH,
    LANG_ROMANIAN,
    LANG_GREEK,
    LANG_HEBREW,
    LANG_DANISH,
    LANG_FINNISH,
    LANG_NORWEGIAN,
    LANG_HUNGARIAN,
    LANG_SLOVAK,
    LANG_BENGALI,
};

static SeqStrings gPrimaryLangNames =
    "English\0"
    "Chinese (Simplified)\0"
    "German\0"
    "French\0"
    "Spanish\0"
    "Italian\0"
    "Portuguese\0"
    "Russian\0"
    "Japanese\0"
    "Korean\0"
    "Arabic\0"
    "Hindi\0"
    "Turkish\0"
    "Vietnamese\0"
    "Polish\0"
    "Ukrainian\0"
    "Dutch\0"
    "Thai\0"
    "Indonesian\0"
    "Czech\0"
    "Swedish\0"
    "Romanian\0"
    "Greek\0"
    "Hebrew\0"
    "Danish\0"
    "Finnish\0"
    "Norwegian\0"
    "Hungarian\0"
    "Slovak\0"
    "Bengali\0";
// clang-format on

static Str PrimaryLangIdToEnglishName(WORD primary) {
    int n = dimofi(gPrimaryLangIds);
    for (int i = 0; i < n; i++) {
        if (gPrimaryLangIds[i] == primary) {
            return SeqStrByIndex(gPrimaryLangNames, i);
        }
    }
    return {};
}

static TempStr OsDefaultDestinationLanguageTemp() {
    LANGID langId = GetUserDefaultUILanguage();
    Str name = PrimaryLangIdToEnglishName(PRIMARYLANGID(langId));
    if (name) {
        return name;
    }
    if (SUBLANGID(langId) == SUBLANG_CHINESE_TRADITIONAL) {
        return StrL("Chinese (Traditional)");
    }
    return StrL("English");
}

static TempStr DefaultDestinationLanguageTemp() {
    if (gSettings && !str::IsEmptyOrWhiteSpace(gSettings->translateToLang)) {
        return gSettings->translateToLang;
    }
    return OsDefaultDestinationLanguageTemp();
}

static void LogTranslation(AIChatBackend backend, Str direction, Str text) {
    logf("selection-translate %s %s: %s", BackendLogName(backend), direction, text);
}

static void ReadPipeToStrBuilder(HANDLE hPipe, str::Builder& out) {
    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        out.Append(Str(buf, (int)bytesRead));
    }
}

static void PopulateEngineDropDown(DropDown* dd, TranslateEngine selected) {
    StrVec items;
    int selIdx = 0;
    for (TranslateEngine engine : gAllEngines) {
        if (!IsEngineAvailable(engine)) {
            continue;
        }
        if (engine == selected) {
            selIdx = len(items);
        }
        items.Append(EngineDisplayName(engine));
    }
    if (len(items) == 0) {
        items.Append(EngineDisplayName(TranslateEngine::Google));
    }
    dd->SetItems(items);
    CbSetCurrentSelection(dd, selIdx);
}

static bool RunTranslation(AIChatBackend backend, Str srcLang, Str dstLang, Str text, Str& msgOut) {
    TempStr exePath = FindBackendExecutableTemp(backend);
    if (len(exePath) == 0) {
        msgOut = str::Dup(Tr("The selected AI CLI is not installed."));
        return false;
    }

    TempStr prompt = BuildTranslationPromptTemp(srcLang, dstLang, text);
    TempStr cwd = StripTrailingSlashTemp(GetTempDirTemp());
    TempStr cmdLine;
    if (backend == AIChatBackend::Grok) {
        cmdLine = BuildGrokTranslateCmdLineTemp(exePath, prompt, cwd);
    } else if (backend == AIChatBackend::Claude) {
        cmdLine = BuildClaudeTranslateCmdLineTemp(exePath, prompt);
    } else if (backend == AIChatBackend::Codex) {
        cmdLine = BuildCodexTranslateCmdLineTemp(exePath, prompt, cwd);
    } else if (backend == AIChatBackend::AntiGravity) {
        cmdLine = BuildAntiGravityTranslateCmdLineTemp(exePath, prompt);
    }

    LogTranslation(backend, StrL(">>> backend"), BackendDisplayName(backend));
    LogTranslation(backend, StrL(">>> srcLang"), srcLang);
    LogTranslation(backend, StrL(">>> dstLang"), dstLang);
    LogTranslation(backend, StrL(">>> prompt"), prompt);
    LogTranslation(backend, StrL(">>> cmd"), cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, cwd, &launch)) {
        msgOut = str::Dup(Tr("Failed to launch the AI CLI."));
        LogTranslation(backend, StrL("<<< error"), msgOut);
        return false;
    }

    str::Builder output;
    output.Reserve(4096);
    ReadPipeToStrBuilder(launch.hReadPipe, output);
    CloseHandle(launch.hReadPipe);
    launch.hReadPipe = nullptr;

    DWORD waitRes = WaitForSingleObject(launch.hProcess, 5 * 60 * 1000);
    if (waitRes == WAIT_TIMEOUT) {
        TerminateProcess(launch.hProcess, 1);
        AIChatCloseProcess(&launch.hProcess, false);
        msgOut = str::Dup(Tr("Translation timed out."));
        LogTranslation(backend, StrL("<<< error"), msgOut);
        return false;
    }
    AIChatCloseProcess(&launch.hProcess, false);

    LogTranslation(backend, StrL("<<< raw"), ToStr(output));

    str::Builder translation;
    translation.Reserve(1024);
    ParseTranslationOutput(backend, ToStr(output), translation);
    LogTranslation(backend, StrL("<<< parsed"), ToStr(translation));
    if (len(translation) == 0) {
        msgOut = str::Dup(Tr("Translation response did not contain text."));
        LogTranslation(backend, StrL("<<< error"), msgOut);
        return false;
    }
    if (TranslationLooksLikeError(ToStr(translation))) {
        msgOut = translation.TakeStr();
        LogTranslation(backend, StrL("<<< error"), msgOut);
        return false;
    }
    msgOut = translation.TakeStr();
    return true;
}

// backend: 0=Claude, 1=Grok, 2=Codex, 3=AntiGravity
TempStr SelectionTranslateResultTemp(int backend, Str srcLang, Str dstLang, Str text, int* exitCode) {
    AIChatBackend chatBackend = AIChatBackend::Grok;
    if (backend == 0) {
        chatBackend = AIChatBackend::Claude;
    } else if (backend == 2) {
        chatBackend = AIChatBackend::Codex;
    } else if (backend == 3) {
        chatBackend = AIChatBackend::AntiGravity;
    }
    Str msg;
    bool ok = RunTranslation(chatBackend, srcLang, dstLang, text, msg);
    if (exitCode) {
        *exitCode = ok ? 0 : 1;
    }
    TempStr res = str::DupTemp(msg);
    str::Free(msg);
    return res;
}

// re-pick the app font for the window's current DPI and push it to every child.
// GetAppFont() caches a PlatformFont per DPI.
void SelectionTranslateWnd::UpdateFont() {
    SetFont(GetAppFont());
    HwndSetFontForWindowAndItsChildren(hwnd, GetHFont());
    VirtText* virts[] = {staticPrompt, staticFromLabel, staticToLabel, staticResultLabel, btnTranslate, btnClose};
    for (VirtText* w : virts) {
        if (w) {
            w->font = font;
        }
    }
}

VirtButton* SelectionTranslateWnd::NewButton(Str text, bool isDefault) {
    return NewThemedButton(hwnd, text, font, isDefault);
}

void SelectionTranslateWnd::SetTranslateButtonText(Str s) {
    if (!btnTranslate) {
        return;
    }
    btnTranslate->SetText(s);
    Relayout();
    btnTranslate->Invalidate();
}

// Moving the dialog to a monitor with a different scaling changes this window's
// DPI, so re-pick the font and re-run the layout: each control's ideal size is
// measured from its font, so they resize with it. Note the Padding insets were
// DpiScale()d once when the layout tree was built and keep their old scale.
void SelectionTranslateWnd::OnDpiChanged(WindowBase::DpiChangedEvent* ev) {
    RECT* r = ev->suggested;
    if (r) {
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    UpdateFont();
    Relayout();
    HwndInvalidate(hwnd, true);
    ev->didHandle = true;
}

void SelectionTranslateWnd::Relayout(bool initial) {
    if (!layout) {
        return;
    }
    if (initial || !sizeInitialized) {
        LayoutAndSizeToContent(layout, 0, 0, hwnd);
        // pick up the virtual controls so we paint them and they get their input
        DoLayout(HwndClientRect(hwnd).Size());
        HwndCenterDialog(hwnd, hwndOwner);
        sizeInitialized = true;
        return;
    }
    // keep current client size (or grow if new content needs more space, e.g. result)
    Rect rc = HwndClientRect(hwnd);
    LayoutAndSizeToContent(layout, rc.dx, rc.dy, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
}

void SelectionTranslateWnd::OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev) {
    if (!hwnd || !layout || !ev->mmi) {
        return;
    }
    int clientMinDx = layout->MinIntrinsicWidth(Inf);
    int clientMinDy = layout->MinIntrinsicHeight(Inf);
    clientMinDx = std::max(clientMinDx, DpiScale(200));
    clientMinDy = std::max(clientMinDy, DpiScale(150));
    RECT r{0, 0, clientMinDx, clientMinDy};
    DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongW(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&r, style, FALSE, exStyle);
    ev->mmi->ptMinTrackSize.x = r.right - r.left;
    ev->mmi->ptMinTrackSize.y = r.bottom - r.top;
}

void SelectionTranslateWnd::UpdateTranslateButtonState() {
    if (!btnTranslate) {
        return;
    }
    TempStr srcLang = dropSrcLang ? dropSrcLang->GetTextTemp() : TempStr{};
    TempStr dstLang = dropDstLang ? dropDstLang->GetTextTemp() : TempStr{};
    TempStr srcText = editSrcText ? editSrcText->GetTextTemp() : TempStr{};
    bool sameLang = LanguagesAreSameTemp(srcLang, dstLang);
    bool hasText = !str::IsEmptyOrWhiteSpace(srcText);
    bool enable = !translating && hasText && !sameLang;
    btnTranslate->SetIsEnabled(enable);
}

void SelectionTranslateWnd::ShowTranslationResult(Str text, bool isError) {
    if (!hwnd) {
        return;
    }
    Str label = isError ? Str(Tr("Error:")) : Str(Tr("Translation:"));
    if (!resultVisible) {
        if (staticResultLabel) {
            staticResultLabel->SetVisibility(Visibility::Visible);
        }
        if (editResult) {
            editResult->SetVisibility(Visibility::Visible);
        }
        resultVisible = true;
        Relayout();
    }
    if (staticResultLabel) {
        staticResultLabel->SetText(label);
    }
    if (editResult) {
        editResult->SetText(text);
    }
}

void SelectionTranslateWnd::StartTranslation(VirtMouseEvent*) {
    if (translating) {
        return;
    }
    TempStr srcLang = dropSrcLang ? dropSrcLang->GetTextTemp() : TempStr{};
    TempStr dstLang = dropDstLang ? dropDstLang->GetTextTemp() : TempStr{};
    TempStr text = editSrcText ? editSrcText->GetTextTemp() : TempStr{};
    if (str::IsEmptyOrWhiteSpace(text) || LanguagesAreSameTemp(srcLang, dstLang)) {
        return;
    }

    TranslateEngine curEngine = ResolveEngine(dropEngine ? EngineFromName(dropEngine->GetTextTemp()) : engine);
    MaybeSaveTranslatePrefs(curEngine, srcLang, dstLang);

    if (!EngineIsAI(curEngine)) {
        // Google / DeepL translate in the browser; keep the dialog open so the
        // user can tweak languages or pick a different engine
        TempStr url = BuildTranslateUrlTemp(curEngine, srcLang, dstLang, text);
        if (url) {
            SumatraLaunchBrowser(url);
        }
        return;
    }

    backend = BackendFromEngine(curEngine);
    translating = true;
    if (editSrcText) {
        editSrcText->SetIsEnabled(false);
    }
    if (dropSrcLang) {
        dropSrcLang->SetIsEnabled(false);
    }
    if (dropDstLang) {
        dropDstLang->SetIsEnabled(false);
    }
    if (btnTranslate) {
        btnTranslate->SetIsEnabled(false);
        SetTranslateButtonText(Tr("Translating..."));
    }
    if (resultVisible) {
        if (staticResultLabel) {
            staticResultLabel->SetVisibility(Visibility::Collapse);
        }
        if (editResult) {
            editResult->SetVisibility(Visibility::Collapse);
        }
        resultVisible = false;
        Relayout();
    }

    auto* task = new SelectionTranslateTaskData();
    task->hwndDlg = hwnd;
    task->backend = backend;
    task->srcLang = str::Dup(srcLang);
    task->dstLang = str::Dup(dstLang);
    task->text = str::Dup(text);
    RunAsync(MkFunc0(SelectionTranslateThread, task), StrL("SelectionTranslate"));
}

void SelectionTranslateWnd::OnTranslationFinished(bool ok, Str msg) {
    translating = false;
    if (editSrcText) {
        editSrcText->SetIsEnabled(true);
    }
    if (dropSrcLang) {
        dropSrcLang->SetIsEnabled(true);
    }
    if (dropDstLang) {
        dropDstLang->SetIsEnabled(true);
    }
    if (btnTranslate) {
        SetTranslateButtonText(Tr("Translate"));
    }
    TempStr display = ok ? msg : FormatTranslationErrorForDisplayTemp(backend, msg);
    ShowTranslationResult(display, !ok);
    UpdateTranslateButtonState();
}

void SelectionTranslateWnd::OnCloseClicked(VirtMouseEvent*) {
    Close();
}

static void OnTranslateDone(SelectionTranslateDoneData* data) {
    AutoDelete del(data);
    if (!gSelectionTranslateWnd || !IsWindow(gSelectionTranslateWnd->hwnd) ||
        gSelectionTranslateWnd->hwnd != data->hwndDlg) {
        return;
    }
    gSelectionTranslateWnd->OnTranslationFinished(data->ok, data->msg);
}

static void SelectionTranslateThread(SelectionTranslateTaskData* data) {
    AutoDelete del(data);
    Str result;
    bool ok = RunTranslation(data->backend, data->srcLang, data->dstLang, data->text, result);
    if (!ok && len(result) == 0) {
        result = str::Dup(Tr("Translation failed."));
    }

    auto* done = new SelectionTranslateDoneData();
    done->hwndDlg = data->hwndDlg;
    done->ok = ok;
    done->msg = result;
    uitask::Post(MkFunc0(OnTranslateDone, done), "SelectionTranslateDone");
}

static void TeardownSelectionTranslateWnd() {
    if (!gSelectionTranslateWnd) {
        return;
    }
    SelectionTranslateWnd* w = gSelectionTranslateWnd;
    gSelectionTranslateWnd = nullptr;
    HWND hwndOwner = w->hwndOwner;
    if (hwndOwner) {
        EnableWindow(hwndOwner, TRUE);
        HwndToForeground(hwndOwner);
    }
    w->ScheduleDelete();
}

static void OnSelectionTranslateClose(WindowBase::CloseEvent* ev) {
    if (gSelectionTranslateWnd == (SelectionTranslateWnd*)ev->e->self) {
        TeardownSelectionTranslateWnd();
    }
}

static void OnSelectionTranslateDestroy(WindowBase::DestroyEvent* ev) {
    if (gSelectionTranslateWnd == (SelectionTranslateWnd*)ev->e->self) {
        TeardownSelectionTranslateWnd();
    }
}

bool SelectionTranslateWnd::Create(HWND owner, Str selText, Str title) {
    hwndOwner = owner;
    bool isRtl = IsUIRtl();

    {
        CreateCustomArgs args;
        args.title = title;
        args.visible = false;
        // resizable: thick frame; CLIPCHILDREN avoids flicker while resizing
        args.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN;
        args.font = GetFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.text = selText;
        args.isMultiLine = true;
        args.withBorder = true;
        args.idealSizeLines = 7;
        // prefer ~40 chars wide; cap so long selection text does not widen the dialog
        args.idealWidthChars = 40;
        args.maxWidthChars = 120;
        args.isRtl = isRtl;
        editSrcText = new Edit();
        editSrcText->Create(args);
        editSrcText->onTextChanged =
            MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
    }

    // The two text fields come first and the controls that act on them
    // (engine, languages, buttons) follow, so the dialog reads top to bottom.
    // flex so source/result edits absorb extra height when the window is resized
    vbox->AddChild(editSrcText, 1);

    {
        staticResultLabel = NewVirtText({
            .s = Tr("Translation:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(8, 0, 0, 0),
        });
        staticResultLabel->SetVisibility(Visibility::Collapse);
        vbox->AddChild(staticResultLabel);
    }
    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetFont();
        args.isMultiLine = true;
        args.withBorder = true;
        args.idealSizeLines = 6;
        args.idealWidthChars = 40;
        args.maxWidthChars = 120;
        args.isRtl = isRtl;
        editResult = new Edit();
        editResult->Create(args);
        SendMessageW(editResult->hwnd, EM_SETREADONLY, TRUE, 0);
        editResult->SetVisibility(Visibility::Collapse);
        editResult->SetInsetsPt(4, 0, 0, 0);
        vbox->AddChild(editResult, 1);
    }

    {
        auto* engineRow = new HBox();
        engineRow->alignMain = MainAxisAlign::MainStart;
        engineRow->alignCross = CrossAxisAlign::CrossCenter;
        staticPrompt = NewVirtText({.s = Tr("Translate with"), .font = font, .isRtl = isRtl});
        engineRow->AddChild(staticPrompt);
        {
            DropDown::CreateArgs args;
            args.parent = hwnd;
            args.font = GetFont();
            args.isRtl = isRtl;
            dropEngine = new DropDown();
            dropEngine->Create(args);
            PopulateEngineDropDown(dropEngine, engine);
            dropEngine->onSelectionChanged =
                MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
            dropEngine->SetInsetsPt(0, 0, 0, 4);
            engineRow->AddChild(dropEngine, 1);
        }
        vbox->AddChild(new Padding(engineRow, DpiScaledInsets(8, 0, 0, 0)));
    }

    {
        auto* langRow = new HBox();
        langRow->alignMain = MainAxisAlign::MainStart;
        langRow->alignCross = CrossAxisAlign::CrossCenter;

        staticFromLabel = NewVirtText({.s = Tr("From:"), .font = font, .isRtl = isRtl});
        langRow->AddChild(staticFromLabel);
        {
            DropDown::CreateArgs args;
            args.parent = hwnd;
            args.font = GetFont();
            args.isEditable = true;
            args.isRtl = isRtl;
            dropSrcLang = new DropDown();
            dropSrcLang->Create(args);
            PopulateLanguageDropDown(dropSrcLang, DefaultSourceLanguageTemp(), true);
            dropSrcLang->onTextChanged =
                MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
            dropSrcLang->onSelectionChanged =
                MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
            dropSrcLang->SetInsetsPt(0, 0, 0, 4);
            langRow->AddChild(dropSrcLang, 1);
        }
        staticToLabel = NewVirtText({
            .s = Tr("To:"),
            .font = font,
            .isRtl = isRtl,
            .padding = DpiScaledInsets(0, 0, 0, 4),
        });
        langRow->AddChild(staticToLabel);
        {
            DropDown::CreateArgs args;
            args.parent = hwnd;
            args.font = GetFont();
            args.isEditable = true;
            args.isRtl = isRtl;
            dropDstLang = new DropDown();
            dropDstLang->Create(args);
            PopulateLanguageDropDown(dropDstLang, DefaultDestinationLanguageTemp(), false);
            dropDstLang->onTextChanged =
                MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
            dropDstLang->onSelectionChanged =
                MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
            langRow->AddChild(dropDstLang, 1);
        }
        vbox->AddChild(new Padding(langRow, DpiScaledInsets(8, 0, 0, 0)));
    }

    {
        auto* btnRow = new HBox();
        btnRow->alignMain = MainAxisAlign::MainEnd;
        btnRow->alignCross = CrossAxisAlign::CrossCenter;
        btnRow->gap = font->averageCharWidth;

        btnClose = NewButton(Tr("Close"), false);
        btnClose->onClick =
            MkMethod1<SelectionTranslateWnd, VirtMouseEvent*, &SelectionTranslateWnd::OnCloseClicked>(this);
        btnRow->AddChild(btnClose);

        btnTranslate = NewButton(Tr("Translate"), true);
        btnTranslate->onClick =
            MkMethod1<SelectionTranslateWnd, VirtMouseEvent*, &SelectionTranslateWnd::StartTranslation>(this);
        btnTranslate->padding = DpiScaledInsets(0, 4, 0, 4);
        btnRow->AddChild(btnTranslate);
        vbox->AddChild(new Padding(btnRow, DpiScaledInsets(8, 0, 0, 0)));
    }

    layout = new Padding(vbox, DpiScaledInsets(12, 12));
    Relayout(true);
    UpdateTheme();
    UpdateTranslateButtonState();
    SetIsVisible(true);
    EditSetFocus(editSrcText);
    return true;
}

void ShowSelectionTranslateDialog(WindowTab* tab, TranslateEngine engineIn) {
    if (!tab || !tab->win || !tab->selectionOnPage) {
        return;
    }
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }
    TranslateEngine engine = ResolveEngine(engineIn);
    if (gSelectionTranslateWnd) {
        if (gSelectionTranslateWnd->hwnd && IsWindow(gSelectionTranslateWnd->hwnd)) {
            HwndSetFocus(gSelectionTranslateWnd->hwnd);
            return;
        }
        TeardownSelectionTranslateWnd();
    }

    bool isTextOnlySelection = false;
    TempStr selText = GetSelectedTextTemp(tab, StrL("\n"), isTextOnlySelection);
    if (str::IsEmptyOrWhiteSpace(selText)) {
        return;
    }

    HWND hwndOwner = tab->win->hwndFrame;
    EnableWindow(hwndOwner, FALSE);

    auto* wnd = new SelectionTranslateWnd();
    wnd->hwndOwner = hwndOwner;
    wnd->engine = engine;
    wnd->SetFont(GetAppFont());
    wnd->closeOnEsc = true;
    wnd->closeOnCtrlW = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnSelectionTranslateClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnSelectionTranslateDestroy);
    wnd->onGetMinMaxInfo =
        MkMethod1<SelectionTranslateWnd, WindowBase::GetMinMaxInfoEvent*, &SelectionTranslateWnd::OnGetMinMaxInfo>(wnd);
    wnd->onDpiChanged =
        MkMethod1<SelectionTranslateWnd, WindowBase::DpiChangedEvent*, &SelectionTranslateWnd::OnDpiChanged>(wnd);
    Str title = Tr("Translate");
    if (!wnd->Create(hwndOwner, selText, title)) {
        EnableWindow(hwndOwner, TRUE);
        delete wnd;
        return;
    }

    gSelectionTranslateWnd = wnd;
    HwndToForeground(wnd->hwnd);
}
