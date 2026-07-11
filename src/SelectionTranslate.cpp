/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "TextSelection.h"
#include "Selection.h"
#include "AIChatCommon.h"
#include "GrokBuild.h"
#include "ClaudeCode.h"
#include "CodexBuild.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkModeSubclass.h"

static const Str kSrcLangAuto = StrL("Auto");

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

struct SelectionTranslateWnd : Wnd {
    ~SelectionTranslateWnd() override;

    HFONT font = nullptr;
    HWND hwndOwner = nullptr;
    Static* staticPrompt = nullptr;
    Edit* editSrcText = nullptr;
    Static* staticFromLabel = nullptr;
    DropDown* dropSrcLang = nullptr;
    Static* staticToLabel = nullptr;
    DropDown* dropDstLang = nullptr;
    Button* btnTranslate = nullptr;
    Button* btnClose = nullptr;
    Static* staticResultLabel = nullptr;
    Edit* editResult = nullptr;

    AIChatBackend backend = AIChatBackend::Grok;
    bool translating = false;
    bool resultVisible = false;

    bool Create(HWND owner, Str selText, Str title);
    void Relayout();
    void UpdateTheme();
    void UpdateTranslateButtonState();
    void ShowTranslationResult(Str text, bool isError);
    void StartTranslation();
    void OnTranslationFinished(bool ok, Str msg);
    void OnCloseClicked();
    void ScheduleDelete();
};

static SelectionTranslateWnd* gSelectionTranslateWnd = nullptr;

SelectionTranslateWnd::~SelectionTranslateWnd() = default;

static void SafeDeleteSelectionTranslateWnd() {
    if (!gSelectionTranslateWnd) {
        return;
    }
    auto* wnd = gSelectionTranslateWnd;
    gSelectionTranslateWnd = nullptr;
    delete wnd;
}

void SelectionTranslateWnd::ScheduleDelete() {
    auto fn = MkFunc0Void(SafeDeleteSelectionTranslateWnd);
    uitask::Post(fn, "SafeDeleteSelectionTranslateWnd");
}

struct SelectionTranslateTaskData {
    SelectionTranslateWnd* wnd = nullptr;
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
                dd->SetCurrentSelection(i);
                return;
            }
        }
    }
}

static Str PrimaryLangIdToEnglishName(WORD primary) {
    switch (primary) {
        case LANG_ENGLISH:
            return "English";
        case LANG_CHINESE:
            return "Chinese (Simplified)";
        case LANG_GERMAN:
            return "German";
        case LANG_FRENCH:
            return "French";
        case LANG_SPANISH:
            return "Spanish";
        case LANG_ITALIAN:
            return "Italian";
        case LANG_PORTUGUESE:
            return "Portuguese";
        case LANG_RUSSIAN:
            return "Russian";
        case LANG_JAPANESE:
            return "Japanese";
        case LANG_KOREAN:
            return "Korean";
        case LANG_ARABIC:
            return "Arabic";
        case LANG_HINDI:
            return "Hindi";
        case LANG_TURKISH:
            return "Turkish";
        case LANG_VIETNAMESE:
            return "Vietnamese";
        case LANG_POLISH:
            return "Polish";
        case LANG_UKRAINIAN:
            return "Ukrainian";
        case LANG_DUTCH:
            return "Dutch";
        case LANG_THAI:
            return "Thai";
        case LANG_INDONESIAN:
            return "Indonesian";
        case LANG_CZECH:
            return "Czech";
        case LANG_SWEDISH:
            return "Swedish";
        case LANG_ROMANIAN:
            return "Romanian";
        case LANG_GREEK:
            return "Greek";
        case LANG_HEBREW:
            return "Hebrew";
        case LANG_DANISH:
            return "Danish";
        case LANG_FINNISH:
            return "Finnish";
        case LANG_NORWEGIAN:
            return "Norwegian";
        case LANG_HUNGARIAN:
            return "Hungarian";
        case LANG_SLOVAK:
            return "Slovak";
        case LANG_BENGALI:
            return "Bengali";
        default:
            return {};
    }
}

static const TempStr OsDefaultDestinationLanguageTemp() {
    LANGID langId = GetUserDefaultUILanguage();
    Str name = PrimaryLangIdToEnglishName(PRIMARYLANGID(langId));
    if (name) {
        return name;
    }
    if (SUBLANGID(langId) == SUBLANG_CHINESE_TRADITIONAL) {
        return "Chinese (Traditional)";
    }
    return "English";
}

static const TempStr DefaultDestinationLanguageTemp() {
    if (gGlobalPrefs && !str::IsEmptyOrWhiteSpace(gGlobalPrefs->translateToLang)) {
        return gGlobalPrefs->translateToLang;
    }
    return OsDefaultDestinationLanguageTemp();
}

static TempStr NormalizeLangNameTemp(Str lang) {
    if (str::IsEmptyOrWhiteSpace(lang)) {
        return {};
    }
    TempStr normalized = str::DupTemp(lang);
    str::TrimWSInPlace(normalized, str::TrimOpt::Both);
    return normalized;
}

static void MaybeSaveTranslateToLang(Str dstLang) {
    if (!gGlobalPrefs || str::IsEmptyOrWhiteSpace(dstLang)) {
        return;
    }
    TempStr normalized = NormalizeLangNameTemp(dstLang);
    if (!normalized) {
        return;
    }
    Str saved = gGlobalPrefs->translateToLang;
    if (saved && str::EqI(saved, normalized)) {
        return;
    }
    str::ReplacePtr(&gGlobalPrefs->translateToLang, str::Dup(normalized));
    SaveSettings();
}

static bool IsSrcLangAutoTemp(Str srcLang) {
    if (str::IsEmptyOrWhiteSpace(srcLang)) {
        return false;
    }
    TempStr lang = str::DupTemp(srcLang);
    str::TrimWSInPlace(lang, str::TrimOpt::Both);
    return str::EqI(lang, kSrcLangAuto);
}

static bool LanguagesAreSameTemp(Str a, Str b) {
    if (IsSrcLangAutoTemp(a) || IsSrcLangAutoTemp(b)) {
        return false;
    }
    if (str::IsEmptyOrWhiteSpace(a) || str::IsEmptyOrWhiteSpace(b)) {
        return false;
    }
    TempStr aa = str::DupTemp(a);
    TempStr bb = str::DupTemp(b);
    str::TrimWSInPlace(aa, str::TrimOpt::Both);
    str::TrimWSInPlace(bb, str::TrimOpt::Both);
    return str::EqI(aa, bb);
}

static Str BackendLogName(AIChatBackend backend) {
    switch (backend) {
        case AIChatBackend::Grok:
            return StrL("grok");
        case AIChatBackend::Claude:
            return StrL("claude");
        case AIChatBackend::Codex:
            return StrL("codex");
    }
    return StrL("ai");
}

static void LogTranslation(AIChatBackend backend, Str direction, Str text) {
    logf("selection-translate %s %s: %s", BackendLogName(backend), direction, text);
}

static bool TranslationLooksLikeError(Str text) {
    if (str::IsEmptyOrWhiteSpace(text)) {
        return true;
    }
    if (str::ContainsI(text, StrL("failed to authenticate"))) {
        return true;
    }
    if (str::ContainsI(text, StrL("authentication_failed"))) {
        return true;
    }
    if (str::ContainsI(text, StrL("api error"))) {
        return true;
    }
    if (str::StartsWithI(text, "error:")) {
        return true;
    }
    if (str::ContainsI(text, StrL("model is not supported"))) {
        return true;
    }
    return false;
}

static TempStr FormatTranslationErrorForDisplayTemp(AIChatBackend backend, Str raw) {
    if (str::IsEmptyOrWhiteSpace(raw)) {
        return str::DupTemp(_TRA("Translation failed."));
    }
    if (str::ContainsI(raw, StrL("failed to authenticate")) || str::ContainsI(raw, StrL("authentication_failed")) ||
        str::ContainsI(raw, StrL("invalid authentication credentials"))) {
        switch (backend) {
            case AIChatBackend::Claude:
                return str::DupTemp(
                    _TRA("Claude Code is not signed in. Open a terminal, run \"claude auth login\", "
                         "then try again."));
            case AIChatBackend::Grok:
                return str::DupTemp(_TRA("Grok Build is not signed in. Sign in to Grok Build, then try again."));
            case AIChatBackend::Codex:
                return str::DupTemp(_TRA("OpenAI Codex is not signed in. Sign in to Codex, then try again."));
        }
    }
    if (str::ContainsI(raw, StrL("model is not supported"))) {
        return str::DupTemp(_TRA("The configured AI model is not available for your account."));
    }
    if (str::ContainsI(raw, StrL("did not contain text"))) {
        return str::DupTemp(_TRA("Translation response did not contain text."));
    }
    return str::DupTemp(raw);
}

static TempStr StripTrailingSlashTemp(TempStr path) {
    if (!path) {
        return {};
    }
    TempStr p = str::DupTemp(path);
    while (len(p) > 0 && (p.s[len(p) - 1] == '\\' || p.s[len(p) - 1] == '/')) {
        p.s[len(p) - 1] = 0;
    }
    return p;
}

static TempStr NormalizeTextForPromptTemp(Str text) {
    if (!text) {
        return {};
    }
    str::Builder buf;
    for (int i = 0; i < text.len; i++) {
        char c = text.s[i];
        if (c == '\r' || c == '\n' || c == '\t') {
            if (len(buf) > 0 && buf.LastChar() != ' ') {
                buf.AppendChar(' ');
            }
        } else {
            buf.AppendChar(c);
        }
    }
    str::TrimWSInPlace(ToStr(buf), str::TrimOpt::Both);
    return ToStrTemp(buf);
}

static TempStr BuildTranslationPromptTemp(Str srcLang, Str dstLang, Str text) {
    TempStr normalized = NormalizeTextForPromptTemp(text);
    if (IsSrcLangAutoTemp(srcLang)) {
        return fmt(
            "Detect the language of the following text and translate it to %s. Return only the "
            "translation with no explanation, commentary, or quotation marks. Text: %s",
            dstLang, normalized);
    }
    return fmt(
        "Translate the following text from %s to %s. Return only the translation with no "
        "explanation, commentary, or quotation marks. Text: %s",
        srcLang, dstLang, normalized);
}

static void ReadPipeToStrBuilder(HANDLE hPipe, str::Builder& out) {
    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        out.Append(Str(buf, (int)bytesRead));
    }
}

static void AppendGrokTranslationText(Str line, str::Builder& out) {
    TempStr eventType = AIChatJsonStrTemp(line, "type");
    if (eventType && str::Eq(eventType, "text")) {
        TempStr text = AIChatJsonStrTemp(line, "data");
        if (len(text) > 0) {
            out.Append(text);
        }
    }
}

static void AppendClaudeTranslationText(Str line, str::Builder& out) {
    TempStr eventType = AIChatJsonStrTemp(line, "type");
    if (!eventType) {
        return;
    }
    if (str::Eq(eventType, "result")) {
        bool isError = str::Contains(line, StrL("\"is_error\":true"));
        TempStr text = AIChatJsonStrTemp(line, "result");
        if (len(text) > 0) {
            if (isError) {
                out.Reset();
                out.Append(text);
            } else if (len(out) == 0) {
                out.Append(text);
            }
        }
        return;
    }
    if (str::Contains(line, StrL("authentication_failed")) || str::Contains(line, StrL("\"is_error\":true"))) {
        return;
    }
    if (str::Eq(eventType, "assistant") && str::Contains(line, StrL("\"type\":\"text\""))) {
        TempStr text = AIChatJsonStrTemp(line, "text");
        if (len(text) > 0 && !TranslationLooksLikeError(text)) {
            out.Append(text);
        }
    } else if (str::Eq(eventType, "content_block_delta")) {
        TempStr text = AIChatJsonStrTemp(line, "text");
        if (len(text) > 0) {
            out.Append(text);
        }
    }
}

static void AppendCodexTranslationText(Str line, str::Builder& out) {
    if (!line || line.s[0] != '{') {
        return;
    }
    TempStr eventType = AIChatJsonStrTemp(line, "type");
    if (!eventType || !str::Eq(eventType, "item.completed")) {
        return;
    }
    TempStr text = AIChatJsonStrTemp(line, "text");
    if (len(text) > 0) {
        out.Append(text);
        return;
    }
    Str agentMsg;
    if (str::Cut(line, StrL("\"type\":\"agent_message\""), nullptr, &agentMsg)) {
        text = AIChatJsonStrTemp(agentMsg, "text");
        if (len(text) > 0) {
            out.Append(text);
        }
    }
}

static void ParseTranslationOutput(AIChatBackend backend, Str output, str::Builder& translationOut) {
    if (str::IsEmptyOrWhiteSpace(output)) {
        return;
    }
    int off = 0;
    while (off < output.len) {
        int lineStart = off;
        while (off < output.len && output.s[off] != '\n' && output.s[off] != '\r') {
            off++;
        }
        if (off > lineStart) {
            TempStr line = str::DupTemp(Str(output.s + lineStart, off - lineStart));
            switch (backend) {
                case AIChatBackend::Grok:
                    AppendGrokTranslationText(line, translationOut);
                    break;
                case AIChatBackend::Claude:
                    AppendClaudeTranslationText(line, translationOut);
                    break;
                case AIChatBackend::Codex:
                    AppendCodexTranslationText(line, translationOut);
                    break;
            }
        }
        while (off < output.len && (output.s[off] == '\n' || output.s[off] == '\r')) {
            off++;
        }
    }
    str::TrimWSInPlace(ToStr(translationOut), str::TrimOpt::Both);
    if (len(translationOut) == 0 && output && !str::Contains(output, StrL("{\"type\":"))) {
        TempStr trimmed = str::DupTemp(output.s);
        str::TrimWSInPlace(trimmed, str::TrimOpt::Both);
        if (!str::IsEmptyOrWhiteSpace(trimmed)) {
            translationOut.Append(trimmed);
        }
    }
}

static TempStr BuildGrokTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd) {
    Str model = gGlobalPrefs->grokBuild.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = "grok-composer-2.5-fast";
    }
    TempStr escapedPrompt = str::ReplaceTemp(prompt, StrL("\""), StrL("\\\""));
    Str permsFlag = gGlobalPrefs->grokBuild.alwaysApprove ? StrL("--always-approve") : Str{};
    return fmt("\"%s\" -p \"%s\" --cwd \"%s\" --output-format streaming-json --model %s --effort low %s", exePath,
               escapedPrompt, cwd, model, permsFlag);
}

static TempStr BuildClaudeTranslateCmdLineTemp(Str exePath, Str prompt) {
    Str model = gGlobalPrefs->claudeCode.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = "claude-sonnet-4-20250514";
    }
    TempStr escapedPrompt = str::ReplaceTemp(prompt, StrL("\""), StrL("\\\""));
    Str permsFlag = gGlobalPrefs->claudeCode.skipPermissions ? StrL("--dangerously-skip-permissions") : Str{};
    TempStr sessionId = AIChatGenerateSessionIdTemp();
    return fmt("\"%s\" -p --verbose --output-format stream-json --model %s %s --session-id %s \"%s\"", exePath, model,
               permsFlag, sessionId, escapedPrompt);
}

static TempStr BuildCodexTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd) {
    Str model = gGlobalPrefs->codexBuild.model;
    bool hasModel = !str::IsEmptyOrWhiteSpace(model);
    TempStr escapedPrompt = str::ReplaceTemp(prompt, StrL("\""), StrL("\\\""));
    Str skipFlag = gGlobalPrefs->codexBuild.skipSandbox ? StrL("--dangerously-bypass-approvals-and-sandbox") : Str{};
    if (skipFlag) {
        if (hasModel) {
            return fmt("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s read-only %s \"%s\"", exePath, cwd,
                       model, skipFlag, escapedPrompt);
        }
        return fmt("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -s read-only %s \"%s\"", exePath, cwd, skipFlag,
                   escapedPrompt);
    }
    if (hasModel) {
        return fmt("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s read-only \"%s\"", exePath, cwd, model,
                   escapedPrompt);
    }
    return fmt("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -s read-only \"%s\"", exePath, cwd, escapedPrompt);
}

static TempStr FindBackendExecutableTemp(AIChatBackend backend) {
    switch (backend) {
        case AIChatBackend::Grok:
            return GrokBuildExecutablePathTemp();
        case AIChatBackend::Claude:
            return ClaudeCodeExecutablePathTemp();
        case AIChatBackend::Codex:
            return CodexBuildExecutablePathTemp();
    }
    return {};
}

static bool IsBackendInstalled(AIChatBackend backend) {
    switch (backend) {
        case AIChatBackend::Grok:
            return IsGrokBuildInstalled();
        case AIChatBackend::Claude:
            return IsClaudeCodeInstalled();
        case AIChatBackend::Codex:
            return IsCodexBuildInstalled();
    }
    return false;
}

static Str BackendDisplayName(AIChatBackend backend) {
    switch (backend) {
        case AIChatBackend::Grok:
            return StrL("Grok Build");
        case AIChatBackend::Claude:
            return StrL("Claude Code");
        case AIChatBackend::Codex:
            return StrL("OpenAI Codex");
    }
    return StrL("AI");
}

static bool RunTranslation(AIChatBackend backend, Str srcLang, Str dstLang, Str text, Str& msgOut) {
    TempStr exePath = FindBackendExecutableTemp(backend);
    if (!exePath) {
        msgOut = str::Dup(_TRA("The selected AI CLI is not installed."));
        return false;
    }

    TempStr prompt = BuildTranslationPromptTemp(srcLang, dstLang, text);
    TempStr cwd = StripTrailingSlashTemp(GetTempDirTemp());
    TempStr cmdLine;
    switch (backend) {
        case AIChatBackend::Grok:
            cmdLine = BuildGrokTranslateCmdLineTemp(exePath, prompt, cwd);
            break;
        case AIChatBackend::Claude:
            cmdLine = BuildClaudeTranslateCmdLineTemp(exePath, prompt);
            break;
        case AIChatBackend::Codex:
            cmdLine = BuildCodexTranslateCmdLineTemp(exePath, prompt, cwd);
            break;
    }

    LogTranslation(backend, ">>> backend", BackendDisplayName(backend));
    LogTranslation(backend, ">>> srcLang", srcLang);
    LogTranslation(backend, ">>> dstLang", dstLang);
    LogTranslation(backend, ">>> prompt", prompt);
    LogTranslation(backend, ">>> cmd", cmdLine);

    AIChatProcessLaunchResult launch;
    if (!AIChatLaunchProcessWithStdoutPipe(cmdLine, cwd, &launch)) {
        msgOut = str::Dup(_TRA("Failed to launch the AI CLI."));
        LogTranslation(backend, "<<< error", msgOut);
        return false;
    }

    str::Builder output(4096);
    ReadPipeToStrBuilder(launch.hReadPipe, output);
    CloseHandle(launch.hReadPipe);
    launch.hReadPipe = nullptr;

    DWORD waitRes = WaitForSingleObject(launch.hProcess, 5 * 60 * 1000);
    if (waitRes == WAIT_TIMEOUT) {
        TerminateProcess(launch.hProcess, 1);
        AIChatCloseProcess(&launch.hProcess, false);
        msgOut = str::Dup(_TRA("Translation timed out."));
        LogTranslation(backend, "<<< error", msgOut);
        return false;
    }
    AIChatCloseProcess(&launch.hProcess, false);

    LogTranslation(backend, "<<< raw", ToStr(output));

    str::Builder translation(1024);
    ParseTranslationOutput(backend, ToStr(output), translation);
    LogTranslation(backend, "<<< parsed", ToStr(translation));
    if (len(translation) == 0) {
        msgOut = str::Dup(_TRA("Translation response did not contain text."));
        LogTranslation(backend, "<<< error", msgOut);
        return false;
    }
    if (TranslationLooksLikeError(ToStr(translation))) {
        msgOut = translation.TakeStr();
        LogTranslation(backend, "<<< error", msgOut);
        return false;
    }
    msgOut = translation.TakeStr();
    return true;
}

TempStr SelectionTranslateResultTemp(int backend, Str srcLang, Str dstLang, Str text, int* exitCode) {
    AIChatBackend chatBackend = AIChatBackend::Grok;
    if (backend == 0) {
        chatBackend = AIChatBackend::Claude;
    } else if (backend == 2) {
        chatBackend = AIChatBackend::Codex;
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

void SelectionTranslateWnd::UpdateTheme() {
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    COLORREF colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    auto setColors = [&](Wnd* w) {
        if (w) {
            w->SetColors(colTxt, colBg);
        }
    };
    setColors(staticPrompt);
    setColors(editSrcText);
    setColors(staticFromLabel);
    setColors(dropSrcLang);
    setColors(staticToLabel);
    setColors(dropDstLang);
    setColors(btnTranslate);
    setColors(btnClose);
    setColors(staticResultLabel);
    setColors(editResult);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void SelectionTranslateWnd::Relayout() {
    if (!layout) {
        return;
    }
    int minDx = DpiScale(hwnd, 500);
    LayoutAndSizeToContent(layout, minDx, 0, hwnd);
    CenterDialog(hwnd, hwndOwner);
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
    Str label = isError ? Str(_TRA("Error:")) : Str(_TRA("Translation:"));
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

void SelectionTranslateWnd::StartTranslation() {
    if (translating) {
        return;
    }
    TempStr srcLang = dropSrcLang ? dropSrcLang->GetTextTemp() : TempStr{};
    TempStr dstLang = dropDstLang ? dropDstLang->GetTextTemp() : TempStr{};
    TempStr text = editSrcText ? editSrcText->GetTextTemp() : TempStr{};
    if (str::IsEmptyOrWhiteSpace(text) || LanguagesAreSameTemp(srcLang, dstLang)) {
        return;
    }

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
        btnTranslate->SetText(_TRA("Translating..."));
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

    auto task = new SelectionTranslateTaskData();
    task->wnd = this;
    task->backend = backend;
    task->srcLang = str::Dup(srcLang);
    task->dstLang = str::Dup(dstLang);
    task->text = str::Dup(text);
    RunAsync(MkFunc0(SelectionTranslateThread, task), "SelectionTranslate");
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
        btnTranslate->SetText(_TRA("Translate"));
    }
    TempStr display = ok ? msg : FormatTranslationErrorForDisplayTemp(backend, msg);
    ShowTranslationResult(display, !ok);
    if (ok && dropDstLang) {
        MaybeSaveTranslateToLang(dropDstLang->GetTextTemp());
    }
    UpdateTranslateButtonState();
}

void SelectionTranslateWnd::OnCloseClicked() {
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
        result = str::Dup(_TRA("Translation failed."));
    }

    auto done = new SelectionTranslateDoneData();
    done->hwndDlg = data->wnd ? data->wnd->hwnd : nullptr;
    done->ok = ok;
    done->msg = result;
    uitask::Post(MkFunc0(OnTranslateDone, done), "SelectionTranslateDone");
}

static void OnSelectionTranslateDestroy(Wnd::DestroyEvent*) {
    if (!gSelectionTranslateWnd) {
        return;
    }
    HWND hwndOwner = gSelectionTranslateWnd->hwndOwner;
    if (hwndOwner) {
        EnableWindow(hwndOwner, TRUE);
        HwndToForeground(hwndOwner);
    }
    gSelectionTranslateWnd->ScheduleDelete();
}

bool SelectionTranslateWnd::Create(HWND owner, Str selText, Str title) {
    hwndOwner = owner;
    bool isRtl = IsUIRtl();

    {
        CreateCustomArgs args;
        args.title = title;
        args.visible = false;
        args.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
        args.exStyle = WS_EX_DLGMODALFRAME;
        args.font = font;
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
        args.font = font;
        args.text = selText;
        args.isMultiLine = true;
        args.withBorder = true;
        args.idealSizeLines = 7;
        args.isRtl = isRtl;
        editSrcText = new Edit();
        editSrcText->Create(args);
        editSrcText->onTextChanged =
            MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
    }

    if (editSrcText) {
        int labelShift = editSrcText->GetLeftTextMargin();
        Static::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.text = _TRA("Translate:");
        args.isRtl = isRtl;
        staticPrompt = new Static();
        staticPrompt->Create(args);
        auto* promptPad = new Padding(staticPrompt, {0, 0, 0, labelShift});
        vbox->AddChild(promptPad);
    }

    vbox->AddChild(editSrcText);

    {
        auto* langRow = new HBox();
        langRow->alignMain = MainAxisAlign::MainStart;
        langRow->alignCross = CrossAxisAlign::CrossCenter;

        {
            Static::CreateArgs args;
            args.parent = hwnd;
            args.font = font;
            args.text = _TRA("From:");
            args.isRtl = isRtl;
            staticFromLabel = new Static();
            staticFromLabel->Create(args);
            langRow->AddChild(staticFromLabel);
        }
        {
            DropDown::CreateArgs args;
            args.parent = hwnd;
            args.font = font;
            args.isEditable = true;
            args.isRtl = isRtl;
            dropSrcLang = new DropDown();
            dropSrcLang->Create(args);
            PopulateLanguageDropDown(dropSrcLang, kSrcLangAuto, true);
            dropSrcLang->onTextChanged =
                MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
            dropSrcLang->onSelectionChanged =
                MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::UpdateTranslateButtonState>(this);
            dropSrcLang->SetInsetsPt(0, 0, 0, 4);
            langRow->AddChild(dropSrcLang, 1);
        }
        {
            Static::CreateArgs args;
            args.parent = hwnd;
            args.font = font;
            args.text = _TRA("To:");
            args.isRtl = isRtl;
            staticToLabel = new Static();
            staticToLabel->Create(args);
            staticToLabel->SetInsetsPt(0, 0, 0, 4);
            langRow->AddChild(staticToLabel);
        }
        {
            DropDown::CreateArgs args;
            args.parent = hwnd;
            args.font = font;
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
        vbox->AddChild(new Padding(langRow, DpiScaledInsets(hwnd, 8, 0, 0, 0)));
    }

    {
        auto* btnRow = new HBox();
        btnRow->alignMain = MainAxisAlign::MainEnd;
        btnRow->alignCross = CrossAxisAlign::CrossCenter;

        btnClose = CreateButton(hwnd, _TRA("Close"),
                                MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::OnCloseClicked>(this), isRtl);
        btnRow->AddChild(btnClose);
        {
            auto* btn = new Button();
            btn->isDefault = true;
            btn->onClick = MkMethod0<SelectionTranslateWnd, &SelectionTranslateWnd::StartTranslation>(this);
            Button::CreateArgs args;
            args.parent = hwnd;
            args.font = font;
            args.text = _TRA("Translate");
            args.isRtl = isRtl;
            btn->Create(args);
            btn->SetInsetsPt(0, 0, 0, 4);
            btnTranslate = btn;
            btnRow->AddChild(btnTranslate);
        }
        vbox->AddChild(new Padding(btnRow, DpiScaledInsets(hwnd, 8, 0, 0, 0)));
    }

    {
        Static::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.text = _TRA("Translation:");
        args.isRtl = isRtl;
        staticResultLabel = new Static();
        staticResultLabel->Create(args);
        staticResultLabel->SetVisibility(Visibility::Collapse);
        staticResultLabel->SetInsetsPt(8, 0, 0, 0);
        vbox->AddChild(staticResultLabel);
    }
    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isMultiLine = true;
        args.withBorder = true;
        args.idealSizeLines = 6;
        args.isRtl = isRtl;
        editResult = new Edit();
        editResult->Create(args);
        SendMessageW(editResult->hwnd, EM_SETREADONLY, TRUE, 0);
        editResult->SetVisibility(Visibility::Collapse);
        editResult->SetInsetsPt(4, 0, 0, 0);
        vbox->AddChild(editResult);
    }

    layout = new Padding(vbox, DpiScaledInsets(hwnd, 12, 12));
    Relayout();
    UpdateTheme();
    UpdateTranslateButtonState();
    SetIsVisible(true);
    if (editSrcText) {
        HwndSetFocus(editSrcText->hwnd);
    }
    return true;
}

void ShowSelectionTranslateDialog(WindowTab* tab, AIChatBackend backend) {
    if (!tab || !tab->win || !tab->selectionOnPage) {
        return;
    }
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }
    if (!IsBackendInstalled(backend)) {
        return;
    }
    if (gSelectionTranslateWnd) {
        HwndSetFocus(gSelectionTranslateWnd->hwnd);
        return;
    }

    bool isTextOnlySelection = false;
    TempStr selText = GetSelectedTextTemp(tab, "\n", isTextOnlySelection);
    if (str::IsEmptyOrWhiteSpace(selText)) {
        return;
    }

    HWND hwndOwner = tab->win->hwndFrame;
    EnableWindow(hwndOwner, FALSE);

    auto* wnd = new SelectionTranslateWnd();
    wnd->hwndOwner = hwndOwner;
    wnd->backend = backend;
    wnd->font = GetAppFont(hwndOwner);
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnSelectionTranslateDestroy);
    TempStr title = fmt(_TRA("Translate with %s").s, BackendDisplayName(backend));
    if (!wnd->Create(hwndOwner, selText, title)) {
        EnableWindow(hwndOwner, TRUE);
        delete wnd;
        return;
    }

    gSelectionTranslateWnd = wnd;
    HwndToForeground(wnd->hwnd);
}
