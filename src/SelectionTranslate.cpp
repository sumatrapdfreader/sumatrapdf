/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "TextSelection.h"
#include "Selection.h"
#include "AIChatCommon.h"
#include "GrokBuild.h"
#include "ClaudeCode.h"
#include "CodexBuild.h"
#include "Translations.h"

constexpr UINT WM_SELECTION_TRANSLATE_DONE = WM_USER + 211;

constexpr int kIdSrcText = 100;
constexpr int kIdSrcLang = 101;
constexpr int kIdDstLang = 102;
constexpr int kIdResultText = 103;
constexpr int kIdTranslateBtn = 104;
constexpr int kIdCloseBtn = 105;

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

struct SelectionTranslateDialog {
    HWND hwnd = nullptr;
    HWND hwndOwner = nullptr;
    HWND hwndSrcText = nullptr;
    HWND hwndSrcLang = nullptr;
    HWND hwndDstLang = nullptr;
    HWND hwndResultLabel = nullptr;
    HWND hwndResultText = nullptr;
    HWND hwndTranslateBtn = nullptr;
    HWND hwndCloseBtn = nullptr;
    HFONT hFont = nullptr;
    AIChatBackend backend = AIChatBackend::Grok;
    bool translating = false;
    bool resultVisible = false;
    int contentDx = 0;
    int labelShift = 0;
    int yAfterLangs = 0;
    int pad = 0;
    int gap = 0;
    int btnDy = 0;
    int btnW = 0;
};

struct SelectionTranslateTaskData {
    SelectionTranslateDialog* dlg = nullptr;
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

static void PopulateLanguageCombo(HWND hwnd, Str initial, bool includeAuto) {
    SendMessageW(hwnd, CB_RESETCONTENT, 0, 0);
    if (includeAuto) {
        CbAddString(hwnd, kSrcLangAuto);
    }
    for (Str lang : gPopularLanguages) {
        CbAddString(hwnd, lang);
    }
    if (!str::IsEmptyOrWhiteSpace(initial)) {
        HwndSetText(hwnd, initial);
        WCHAR* initialW = CWStrTemp(initial);
        LRESULT idx = SendMessageW(hwnd, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)initialW);
        if (idx != CB_ERR) {
            SendMessageW(hwnd, CB_SETCURSEL, (WPARAM)idx, 0);
        }
    }
}

static bool IsSrcLangAutoTemp(Str srcLang) {
    if (str::IsEmptyOrWhiteSpace(srcLang)) {
        return false;
    }
    TempStr lang = str::DupTemp(srcLang);
    str::TrimWSInPlace(lang, str::TrimOpt::Both);
    return str::EqI(lang, kSrcLangAuto);
}

static TempStr GetWindowTextUtf8Temp(HWND hwnd) {
    int n = GetWindowTextLengthW(hwnd);
    if (n <= 0) {
        return nullptr;
    }
    WCHAR* ws = AllocArrayTemp<WCHAR>(n + 1);
    GetWindowTextW(hwnd, ws, n + 1);
    return ToUtf8Temp(WStr(ws));
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

static void SetDialogClientSize(HWND hwnd, int clientW, int clientH) {
    RECT rc{0, 0, clientW, clientH};
    DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
}

static int ComboVisibleDy(HWND hwndCombo) {
    int itemH = (int)SendMessageW(hwndCombo, CB_GETITEMHEIGHT, (WPARAM)-1, 0);
    return itemH + 8;
}

static int ComboLabelYOffset(HWND hwndCombo, HWND hwnd, HFONT font) {
    COMBOBOXINFO cbi{};
    cbi.cbSize = sizeof(COMBOBOXINFO);
    if (!GetComboBoxInfo(hwndCombo, &cbi)) {
        return DpiScale(hwnd, 4);
    }
    int editDy = cbi.rcItem.bottom - cbi.rcItem.top;
    AutoReleaseDC dc(hwnd);
    ScopedSelectFont selectFont(dc, font);
    TEXTMETRIC tm{};
    int textDy = GetTextMetrics(dc, &tm) ? tm.tmHeight : FontDyPx(hwnd, font);
    // static text draws flush to the top of its rect; nudge up to match combo text visually
    constexpr int kComboLabelAdjust = 2;
    return cbi.rcItem.top + (editDy - textDy) / 2 - DpiScale(hwnd, kComboLabelAdjust);
}

static void LayoutButtons(SelectionTranslateDialog* dlg, int y) {
    int x = dlg->pad;
    int innerDx = dlg->contentDx;
    SetWindowPos(dlg->hwndTranslateBtn, nullptr, x + innerDx - 2 * dlg->btnW - dlg->gap, y, dlg->btnW, dlg->btnDy,
                 SWP_NOZORDER);
    SetWindowPos(dlg->hwndCloseBtn, nullptr, x + innerDx - dlg->btnW, y, dlg->btnW, dlg->btnDy, SWP_NOZORDER);
}

static void ShowTranslationResult(SelectionTranslateDialog* dlg, Str text, bool isError) {
    if (!dlg || !dlg->hwnd) {
        return;
    }
    HWND hwnd = dlg->hwnd;
    int labelDy = DpiScale(hwnd, 16);
    int resultDy = DpiScale(hwnd, 120);
    int x = dlg->pad;
    int y = dlg->yAfterLangs;
    Str label = isError ? Str(_TRA("Error:")) : Str(_TRA("Translation:"));

    if (!dlg->resultVisible) {
        dlg->hwndResultLabel = CreateWindowExW(0, L"STATIC", CWStrTemp(label), WS_CHILD | WS_VISIBLE,
                                               x + dlg->labelShift, y, dlg->contentDx - dlg->labelShift, labelDy, hwnd,
                                               nullptr, GetModuleHandleW(nullptr), nullptr);
        SetWindowFont(dlg->hwndResultLabel, dlg->hFont, TRUE);
        y += labelDy + dlg->gap;
        dlg->hwndResultText = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, x, y, dlg->contentDx,
            resultDy, hwnd, (HMENU)(INT_PTR)kIdResultText, GetModuleHandleW(nullptr), nullptr);
        SetWindowFont(dlg->hwndResultText, dlg->hFont, TRUE);
        y += resultDy + dlg->gap;
        LayoutButtons(dlg, y);
        int clientH = y + dlg->btnDy + dlg->pad;
        SetDialogClientSize(hwnd, dlg->contentDx + 2 * dlg->pad, clientH);
        CenterDialog(hwnd, dlg->hwndOwner);
        dlg->resultVisible = true;
    }
    if (dlg->hwndResultLabel) {
        HwndSetText(dlg->hwndResultLabel, label);
    }
    HwndSetText(dlg->hwndResultText, text);
    ShowWindow(dlg->hwndResultLabel, SW_SHOW);
    ShowWindow(dlg->hwndResultText, SW_SHOW);
}

static void UpdateTranslateButtonState(SelectionTranslateDialog* dlg) {
    if (!dlg || !dlg->hwndTranslateBtn) {
        return;
    }
    TempStr srcLang = GetWindowTextUtf8Temp(dlg->hwndSrcLang);
    TempStr dstLang = GetWindowTextUtf8Temp(dlg->hwndDstLang);
    TempStr srcText = GetWindowTextUtf8Temp(dlg->hwndSrcText);
    bool sameLang = LanguagesAreSameTemp(srcLang, dstLang);
    bool hasText = !str::IsEmptyOrWhiteSpace(srcText);
    bool enable = !dlg->translating && hasText && !sameLang;
    EnableWindow(dlg->hwndTranslateBtn, enable);
}

static void OnTranslateDone(SelectionTranslateDoneData* data) {
    AutoDelete del(data);
    SelectionTranslateDialog* dlg = (SelectionTranslateDialog*)GetWindowLongPtrW(data->hwndDlg, GWLP_USERDATA);
    if (!dlg || !IsWindow(dlg->hwnd)) {
        return;
    }
    dlg->translating = false;
    EnableWindow(dlg->hwndSrcText, TRUE);
    EnableWindow(dlg->hwndSrcLang, TRUE);
    EnableWindow(dlg->hwndDstLang, TRUE);
    HwndSetText(dlg->hwndTranslateBtn, _TRA("Translate"));
    TempStr display = data->ok ? data->msg : FormatTranslationErrorForDisplayTemp(dlg->backend, data->msg);
    ShowTranslationResult(dlg, display, !data->ok);
    if (data->ok) {
        MaybeSaveTranslateToLang(GetWindowTextUtf8Temp(dlg->hwndDstLang));
    }
    UpdateTranslateButtonState(dlg);
}

static void SelectionTranslateThread(SelectionTranslateTaskData* data) {
    AutoDelete del(data);
    Str result;
    bool ok = RunTranslation(data->backend, data->srcLang, data->dstLang, data->text, result);
    if (!ok && len(result) == 0) {
        result = str::Dup(_TRA("Translation failed."));
    }

    auto done = new SelectionTranslateDoneData();
    done->hwndDlg = data->dlg->hwnd;
    done->ok = ok;
    done->msg = result; // transfer ownership; freed in ~SelectionTranslateDoneData
    uitask::Post(MkFunc0(OnTranslateDone, done), "SelectionTranslateDone");
}

static void StartTranslation(SelectionTranslateDialog* dlg) {
    if (!dlg || dlg->translating) {
        return;
    }
    TempStr srcLang = GetWindowTextUtf8Temp(dlg->hwndSrcLang);
    TempStr dstLang = GetWindowTextUtf8Temp(dlg->hwndDstLang);
    TempStr text = GetWindowTextUtf8Temp(dlg->hwndSrcText);
    if (str::IsEmptyOrWhiteSpace(text) || LanguagesAreSameTemp(srcLang, dstLang)) {
        return;
    }

    dlg->translating = true;
    EnableWindow(dlg->hwndSrcText, FALSE);
    EnableWindow(dlg->hwndSrcLang, FALSE);
    EnableWindow(dlg->hwndDstLang, FALSE);
    EnableWindow(dlg->hwndTranslateBtn, FALSE);
    HwndSetText(dlg->hwndTranslateBtn, _TRA("Translating..."));
    if (dlg->resultVisible) {
        ShowWindow(dlg->hwndResultLabel, SW_HIDE);
        ShowWindow(dlg->hwndResultText, SW_HIDE);
    }

    auto task = new SelectionTranslateTaskData();
    task->dlg = dlg;
    task->backend = dlg->backend;
    task->srcLang = str::Dup(srcLang);
    task->dstLang = str::Dup(dstLang);
    task->text = str::Dup(text);
    RunAsync(MkFunc0(SelectionTranslateThread, task), "SelectionTranslate");
}

static LRESULT CALLBACK SelectionTranslateDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SelectionTranslateDialog* dlg = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        dlg = (SelectionTranslateDialog*)cs->lpCreateParams;
        dlg->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)dlg);
        return 0;
    }
    dlg = (SelectionTranslateDialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!dlg) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_COMMAND: {
            HWND ctl = (HWND)lp;
            int code = HIWORD(wp);
            if (ctl == dlg->hwndTranslateBtn && code == BN_CLICKED) {
                StartTranslation(dlg);
                return 0;
            }
            if (ctl == dlg->hwndCloseBtn && code == BN_CLICKED) {
                DestroyWindow(hwnd);
                return 0;
            }
            if ((ctl == dlg->hwndSrcLang || ctl == dlg->hwndDstLang || ctl == dlg->hwndSrcText) &&
                (code == CBN_EDITCHANGE || code == EN_CHANGE)) {
                UpdateTranslateButtonState(dlg);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY: {
            HWND hwndOwner = dlg->hwndOwner;
            if (hwndOwner) {
                EnableWindow(hwndOwner, TRUE);
            }
            delete dlg;
            if (hwndOwner) {
                HwndToForeground(hwndOwner);
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static constexpr const WCHAR* kSelectionTranslateWinClass = L"SUMATRA_SELECTION_TRANSLATE";
static bool gSelectionTranslateWinClassRegistered = false;

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

    bool isTextOnlySelection = false;
    TempStr selText = GetSelectedTextTemp(tab, "\n", isTextOnlySelection);
    if (str::IsEmptyOrWhiteSpace(selText)) {
        return;
    }

    if (!gSelectionTranslateWinClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = SelectionTranslateDlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = kSelectionTranslateWinClass;
        RegisterClassExW(&wc);
        gSelectionTranslateWinClassRegistered = true;
    }

    HWND hwndOwner = tab->win->hwndFrame;
    EnableWindow(hwndOwner, FALSE);

    auto dlg = new SelectionTranslateDialog();
    dlg->hwndOwner = hwndOwner;
    dlg->backend = backend;
    dlg->hFont = GetDefaultGuiFont();

    TempStr title = fmt(_TRA("Translate with %s").s, BackendDisplayName(backend));

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kSelectionTranslateWinClass, CWStrTemp(title),
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                                100, 100, hwndOwner, nullptr, GetModuleHandleW(nullptr), dlg);
    if (!hwnd) {
        EnableWindow(hwndOwner, TRUE);
        delete dlg;
        return;
    }

    dlg->pad = DpiScale(hwnd, 12);
    dlg->gap = DpiScale(hwnd, 8);
    int labelDy = DpiScale(hwnd, 16);
    dlg->btnDy = DpiScale(hwnd, 28);
    dlg->btnW = DpiScale(hwnd, 96);
    int clientW = DpiScale(hwndOwner, 500);
    int x = dlg->pad;
    int y = dlg->pad;
    dlg->contentDx = clientW - 2 * dlg->pad;
    int colDx = (dlg->contentDx - dlg->gap) / 2;

    auto createLabel = [&](Str text, int lx, int ly, int ldx) {
        HWND h = CreateWindowExW(0, L"STATIC", CWStrTemp(text.s), WS_CHILD | WS_VISIBLE, lx, ly, ldx, labelDy, hwnd,
                                 nullptr, GetModuleHandleW(nullptr), nullptr);
        SetWindowFont(h, dlg->hFont, TRUE);
        return h;
    };

    int translateLabelY = y;
    y += labelDy + dlg->gap;
    int srcTextDy = DpiScale(hwnd, 140);
    dlg->hwndSrcText =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", CWStrTemp(selText),
                        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, x, y, dlg->contentDx,
                        srcTextDy, hwnd, (HMENU)(INT_PTR)kIdSrcText, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndSrcText, dlg->hFont, TRUE);
    int editBorder = GetSystemMetrics(SM_CXEDGE);
    LRESULT margins = SendMessageW(dlg->hwndSrcText, EM_GETMARGINS, 0, 0);
    dlg->labelShift = editBorder + LOWORD(margins);
    createLabel(Str(_TRA("Translate:")), x + dlg->labelShift, translateLabelY, dlg->contentDx - dlg->labelShift);
    y += srcTextDy + dlg->gap;

    int charDx = HwndMeasureText(hwnd, " ", dlg->hFont).dx;
    Size fromLabelSize = HwndMeasureText(hwnd, _TRA("From:"), dlg->hFont);
    Size toLabelSize = HwndMeasureText(hwnd, _TRA("To:"), dlg->hFont);
    int col2X = x + colDx + dlg->gap;
    int langRowY = y;
    int fromLabelX = x + dlg->labelShift;
    int srcComboX = fromLabelX + fromLabelSize.dx + charDx;
    int srcComboDx = x + colDx - srcComboX;
    dlg->hwndSrcLang = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                                       srcComboX, langRowY, srcComboDx, DpiScale(hwnd, 200), hwnd,
                                       (HMENU)(INT_PTR)kIdSrcLang, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndSrcLang, dlg->hFont, TRUE);
    int comboRowDy = ComboVisibleDy(dlg->hwndSrcLang);
    MoveWindow(dlg->hwndSrcLang, srcComboX, langRowY, srcComboDx, comboRowDy + DpiScale(hwnd, 200), TRUE);
    PopulateLanguageCombo(dlg->hwndSrcLang, kSrcLangAuto, true);
    int labelYOffset = ComboLabelYOffset(dlg->hwndSrcLang, hwnd, dlg->hFont);
    createLabel(Str(_TRA("From:")), fromLabelX, langRowY + labelYOffset, fromLabelSize.dx);

    int toLabelX = col2X + dlg->labelShift;
    int dstComboX = toLabelX + toLabelSize.dx + charDx;
    int dstComboDx = x + dlg->contentDx - dstComboX;
    dlg->hwndDstLang = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                                       dstComboX, langRowY, dstComboDx, DpiScale(hwnd, 200), hwnd,
                                       (HMENU)(INT_PTR)kIdDstLang, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndDstLang, dlg->hFont, TRUE);
    MoveWindow(dlg->hwndDstLang, dstComboX, langRowY, dstComboDx, comboRowDy + DpiScale(hwnd, 200), TRUE);
    PopulateLanguageCombo(dlg->hwndDstLang, DefaultDestinationLanguageTemp(), false);
    createLabel(Str(_TRA("To:")), toLabelX, langRowY + labelYOffset, toLabelSize.dx);
    y += comboRowDy + dlg->gap;
    dlg->yAfterLangs = y;

    dlg->hwndTranslateBtn = CreateWindowExW(0, L"BUTTON", CWStrTemp(_TRA("Translate")),
                                            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, y, dlg->btnW, dlg->btnDy, hwnd,
                                            (HMENU)(INT_PTR)kIdTranslateBtn, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndTranslateBtn, dlg->hFont, TRUE);
    dlg->hwndCloseBtn =
        CreateWindowExW(0, L"BUTTON", CWStrTemp(_TRA("Close")), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, y, dlg->btnW,
                        dlg->btnDy, hwnd, (HMENU)(INT_PTR)kIdCloseBtn, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndCloseBtn, dlg->hFont, TRUE);
    LayoutButtons(dlg, y);

    int clientH = y + dlg->btnDy + dlg->pad;
    SetDialogClientSize(hwnd, clientW, clientH);
    CenterDialog(hwnd, hwndOwner);

    UpdateTranslateButtonState(dlg);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}
