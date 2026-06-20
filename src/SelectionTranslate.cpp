/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/Log.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "TextSelection.h"
#include "Selection.h"
#include "SelectionTranslate.h"
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

static const char* gPopularLanguages[] = {
    "English",
    "Chinese (Simplified)",
    "Chinese (Traditional)",
    "Spanish",
    "Arabic",
    "Hindi",
    "Portuguese",
    "Bengali",
    "Russian",
    "Japanese",
    "Punjabi",
    "German",
    "French",
    "Korean",
    "Turkish",
    "Vietnamese",
    "Italian",
    "Polish",
    "Ukrainian",
    "Dutch",
    "Thai",
    "Indonesian",
    "Czech",
    "Swedish",
    "Romanian",
    "Greek",
    "Hebrew",
    "Danish",
    "Finnish",
    "Norwegian",
    "Hungarian",
    "Slovak",
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
    int yAfterLangs = 0;
    int pad = 0;
    int gap = 0;
    int btnDy = 0;
    int btnW = 0;
};

struct SelectionTranslateTaskData {
    SelectionTranslateDialog* dlg = nullptr;
    AIChatBackend backend = AIChatBackend::Grok;
    AutoFreeStr srcLang;
    AutoFreeStr dstLang;
    AutoFreeStr text;
};

struct SelectionTranslateDoneData {
    HWND hwndDlg = nullptr;
    bool ok = false;
    AutoFreeStr msg;
};

static const char* PrimaryLangIdToEnglishName(WORD primary) {
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
            return nullptr;
    }
}

static const char* DefaultDestinationLanguageTemp() {
    LANGID langId = GetUserDefaultUILanguage();
    const char* name = PrimaryLangIdToEnglishName(PRIMARYLANGID(langId));
    if (name) {
        return name;
    }
    if (SUBLANGID(langId) == SUBLANG_CHINESE_TRADITIONAL) {
        return "Chinese (Traditional)";
    }
    return "English";
}

static void PopulateLanguageCombo(HWND hwnd, const char* initial) {
    SendMessageW(hwnd, CB_RESETCONTENT, 0, 0);
    for (const char* lang : gPopularLanguages) {
        CbAddString(hwnd, lang);
    }
    if (!str::IsEmptyOrWhiteSpace(initial)) {
        SetWindowTextA(hwnd, initial);
        LRESULT idx = SendMessageW(hwnd, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)initial);
        if (idx != CB_ERR) {
            SendMessageW(hwnd, CB_SETCURSEL, (WPARAM)idx, 0);
        }
    }
}

static char* GetWindowTextUtf8Temp(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) {
        return nullptr;
    }
    AutoFreeWStr ws = AllocArray<WCHAR>(len + 1);
    GetWindowTextW(hwnd, ws, len + 1);
    return ToUtf8Temp(ws);
}

static bool LanguagesAreSameTemp(const char* a, const char* b) {
    if (str::IsEmptyOrWhiteSpace(a) || str::IsEmptyOrWhiteSpace(b)) {
        return false;
    }
    TempStr aa = str::DupTemp(a);
    TempStr bb = str::DupTemp(b);
    str::TrimWSInPlace(aa, str::TrimOpt::Both);
    str::TrimWSInPlace(bb, str::TrimOpt::Both);
    return str::EqI(aa, bb);
}

static const char* BackendLogName(AIChatBackend backend) {
    switch (backend) {
        case AIChatBackend::Grok:
            return "grok";
        case AIChatBackend::Claude:
            return "claude";
        case AIChatBackend::Codex:
            return "codex";
    }
    return "ai";
}

static void LogTranslation(AIChatBackend backend, const char* direction, const char* text) {
    if (!text) {
        text = "";
    }
    logf("selection-translate %s %s: %s", BackendLogName(backend), direction, text);
}

static bool TranslationLooksLikeError(const char* text) {
    if (str::IsEmptyOrWhiteSpace(text)) {
        return true;
    }
    if (str::FindI(text, "failed to authenticate")) {
        return true;
    }
    if (str::FindI(text, "authentication_failed")) {
        return true;
    }
    if (str::FindI(text, "api error")) {
        return true;
    }
    if (str::StartsWithI(text, "error:")) {
        return true;
    }
    if (str::FindI(text, "model is not supported")) {
        return true;
    }
    return false;
}

static TempStr FormatTranslationErrorForDisplayTemp(AIChatBackend backend, const char* raw) {
    if (str::IsEmptyOrWhiteSpace(raw)) {
        return str::DupTemp(_TRA("Translation failed."));
    }
    if (str::FindI(raw, "failed to authenticate") || str::FindI(raw, "authentication_failed") ||
        str::FindI(raw, "invalid authentication credentials")) {
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
    if (str::FindI(raw, "model is not supported")) {
        return str::DupTemp(_TRA("The configured AI model is not available for your account."));
    }
    if (str::FindI(raw, "did not contain text")) {
        return str::DupTemp(_TRA("Translation response did not contain text."));
    }
    return str::DupTemp(raw);
}

static TempStr StripTrailingSlashTemp(TempStr path) {
    if (!path) {
        return nullptr;
    }
    TempStr p = str::DupTemp(path);
    while (str::Len(p) > 0 && (p[str::Len(p) - 1] == '\\' || p[str::Len(p) - 1] == '/')) {
        p[str::Len(p) - 1] = 0;
    }
    return p;
}

static TempStr NormalizeTextForPromptTemp(const char* text) {
    if (!text) {
        return nullptr;
    }
    StrBuilder buf;
    for (const char* s = text; *s; s++) {
        char c = *s;
        if (c == '\r' || c == '\n' || c == '\t') {
            if (buf.Size() > 0 && buf.LastChar() != ' ') {
                buf.AppendChar(' ');
            }
        } else {
            buf.AppendChar(c);
        }
    }
    str::TrimWSInPlace(buf.Get(), str::TrimOpt::Both);
    return str::DupTemp(buf.Get());
}

static TempStr BuildTranslationPromptTemp(const char* srcLang, const char* dstLang, const char* text) {
    TempStr normalized = NormalizeTextForPromptTemp(text);
    return str::FormatTemp(
        "Translate the following text from %s to %s. Return only the translation with no "
        "explanation, commentary, or quotation marks. Text: %s",
        srcLang, dstLang, normalized);
}

static void ReadPipeToStrBuilder(HANDLE hPipe, StrBuilder& out) {
    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        out.Append(buf, bytesRead);
    }
}

static void AppendGrokTranslationText(const char* line, StrBuilder& out) {
    TempStr eventType = AIChatJsonStrTemp(line, "type");
    if (eventType && str::Eq(eventType, "text")) {
        TempStr text = AIChatJsonStrTemp(line, "data");
        if (text && str::Len(text) > 0) {
            out.Append(text);
        }
    }
}

static void AppendClaudeTranslationText(const char* line, StrBuilder& out) {
    TempStr eventType = AIChatJsonStrTemp(line, "type");
    if (!eventType) {
        return;
    }
    if (str::Eq(eventType, "result")) {
        bool isError = str::Find(line, "\"is_error\":true") != nullptr;
        TempStr text = AIChatJsonStrTemp(line, "result");
        if (text && str::Len(text) > 0) {
            if (isError) {
                out.Reset();
                out.Append(text);
            } else if (out.Size() == 0) {
                out.Append(text);
            }
        }
        return;
    }
    if (str::Find(line, "authentication_failed") || str::Find(line, "\"is_error\":true")) {
        return;
    }
    if (str::Eq(eventType, "assistant") && str::Find(line, "\"type\":\"text\"")) {
        TempStr text = AIChatJsonStrTemp(line, "text");
        if (text && str::Len(text) > 0 && !TranslationLooksLikeError(text)) {
            out.Append(text);
        }
    } else if (str::Eq(eventType, "content_block_delta")) {
        TempStr text = AIChatJsonStrTemp(line, "text");
        if (text && str::Len(text) > 0) {
            out.Append(text);
        }
    }
}

static void AppendCodexTranslationText(const char* line, StrBuilder& out) {
    if (!line || *line != '{') {
        return;
    }
    TempStr eventType = AIChatJsonStrTemp(line, "type");
    if (!eventType || !str::Eq(eventType, "item.completed")) {
        return;
    }
    TempStr text = AIChatJsonStrTemp(line, "text");
    if (text && str::Len(text) > 0) {
        out.Append(text);
        return;
    }
    if (str::Find(line, "\"type\":\"agent_message\"")) {
        const char* p = str::Find(line, "\"type\":\"agent_message\"");
        text = AIChatJsonStrTemp(p, "text");
        if (text && str::Len(text) > 0) {
            out.Append(text);
        }
    }
}

static void ParseTranslationOutput(AIChatBackend backend, const char* output, StrBuilder& translationOut) {
    if (str::IsEmptyOrWhiteSpace(output)) {
        return;
    }
    const char* s = output;
    const char* end = output + str::Len(output);
    while (s < end) {
        const char* lineEnd = s;
        while (lineEnd < end && *lineEnd != '\n' && *lineEnd != '\r') {
            lineEnd++;
        }
        if (lineEnd > s) {
            TempStr line = str::DupTemp(s, (int)(lineEnd - s));
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
        s = lineEnd;
        while (s < end && (*s == '\n' || *s == '\r')) {
            s++;
        }
    }
    str::TrimWSInPlace(translationOut.Get(), str::TrimOpt::Both);
    if (translationOut.Size() == 0 && output && !str::Find(output, "{\"type\":")) {
        TempStr trimmed = str::DupTemp(output);
        str::TrimWSInPlace(trimmed, str::TrimOpt::Both);
        if (!str::IsEmptyOrWhiteSpace(trimmed)) {
            translationOut.Append(trimmed);
        }
    }
}

static TempStr BuildGrokTranslateCmdLineTemp(const char* exePath, const char* prompt, const char* cwd) {
    const char* model = gGlobalPrefs->grokBuild.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = "grok-composer-2.5-fast";
    }
    TempStr escapedPrompt = str::ReplaceTemp(prompt, "\"", "\\\"");
    const char* permsFlag = gGlobalPrefs->grokBuild.alwaysApprove ? "--always-approve" : "";
    return str::FormatTemp("\"%s\" -p \"%s\" --cwd \"%s\" --output-format streaming-json --model %s --effort low %s",
                           exePath, escapedPrompt, cwd, model, permsFlag);
}

static TempStr BuildClaudeTranslateCmdLineTemp(const char* exePath, const char* prompt) {
    const char* model = gGlobalPrefs->claudeCode.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = "claude-sonnet-4-20250514";
    }
    TempStr escapedPrompt = str::ReplaceTemp(prompt, "\"", "\\\"");
    const char* permsFlag = gGlobalPrefs->claudeCode.skipPermissions ? "--dangerously-skip-permissions" : "";
    char* sessionId = AIChatGenerateSessionId();
    defer {
        str::Free(sessionId);
    };
    return str::FormatTemp("\"%s\" -p --verbose --output-format stream-json --model %s %s --session-id %s \"%s\"",
                           exePath, model, permsFlag, sessionId, escapedPrompt);
}

static TempStr BuildCodexTranslateCmdLineTemp(const char* exePath, const char* prompt, const char* cwd) {
    const char* model = gGlobalPrefs->codexBuild.model;
    bool hasModel = !str::IsEmptyOrWhiteSpace(model);
    TempStr escapedPrompt = str::ReplaceTemp(prompt, "\"", "\\\"");
    const char* skipFlag = gGlobalPrefs->codexBuild.skipSandbox ? "--dangerously-bypass-approvals-and-sandbox" : "";
    if (skipFlag[0]) {
        if (hasModel) {
            return str::FormatTemp("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s read-only %s \"%s\"",
                                   exePath, cwd, model, skipFlag, escapedPrompt);
        }
        return str::FormatTemp("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -s read-only %s \"%s\"", exePath,
                               cwd, skipFlag, escapedPrompt);
    }
    if (hasModel) {
        return str::FormatTemp("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s read-only \"%s\"", exePath,
                               cwd, model, escapedPrompt);
    }
    return str::FormatTemp("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -s read-only \"%s\"", exePath, cwd,
                           escapedPrompt);
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
    return nullptr;
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

static const char* BackendDisplayName(AIChatBackend backend) {
    switch (backend) {
        case AIChatBackend::Grok:
            return "Grok Build";
        case AIChatBackend::Claude:
            return "Claude Code";
        case AIChatBackend::Codex:
            return "OpenAI Codex";
    }
    return "AI";
}

static bool RunTranslation(AIChatBackend backend, const char* srcLang, const char* dstLang, const char* text,
                           AutoFreeStr& msgOut) {
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
        LogTranslation(backend, "<<< error", msgOut.Get());
        return false;
    }

    StrBuilder output(4096);
    ReadPipeToStrBuilder(launch.hReadPipe, output);
    CloseHandle(launch.hReadPipe);
    launch.hReadPipe = nullptr;

    DWORD waitRes = WaitForSingleObject(launch.hProcess, 5 * 60 * 1000);
    if (waitRes == WAIT_TIMEOUT) {
        TerminateProcess(launch.hProcess, 1);
        AIChatCloseProcess(&launch.hProcess, false);
        msgOut = str::Dup(_TRA("Translation timed out."));
        LogTranslation(backend, "<<< error", msgOut.Get());
        return false;
    }
    AIChatCloseProcess(&launch.hProcess, false);

    LogTranslation(backend, "<<< raw", output.Get());

    StrBuilder translation(1024);
    ParseTranslationOutput(backend, output.Get(), translation);
    LogTranslation(backend, "<<< parsed", translation.Get());
    if (translation.Size() == 0) {
        msgOut = str::Dup(_TRA("Translation response did not contain text."));
        LogTranslation(backend, "<<< error", msgOut.Get());
        return false;
    }
    if (TranslationLooksLikeError(translation.Get())) {
        msgOut.Set(translation.StealData());
        LogTranslation(backend, "<<< error", msgOut.Get());
        return false;
    }
    msgOut.Set(translation.StealData());
    return true;
}

char* TestSelectionTranslateResult(int backend, const char* srcLang, const char* dstLang, const char* text,
                                   int* exitCode) {
    AIChatBackend chatBackend = AIChatBackend::Grok;
    if (backend == 0) {
        chatBackend = AIChatBackend::Claude;
    } else if (backend == 2) {
        chatBackend = AIChatBackend::Codex;
    }
    AutoFreeStr msg;
    bool ok = RunTranslation(chatBackend, srcLang, dstLang, text, msg);
    if (exitCode) {
        *exitCode = ok ? 0 : 1;
    }
    return str::Dup(msg.Get());
}

static void SetDialogClientSize(HWND hwnd, int clientW, int clientH) {
    RECT rc{0, 0, clientW, clientH};
    DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
}

static void LayoutButtons(SelectionTranslateDialog* dlg, int y) {
    int x = dlg->pad;
    int innerDx = dlg->contentDx;
    SetWindowPos(dlg->hwndTranslateBtn, nullptr, x + innerDx - 2 * dlg->btnW - dlg->gap, y, dlg->btnW, dlg->btnDy,
                 SWP_NOZORDER);
    SetWindowPos(dlg->hwndCloseBtn, nullptr, x + innerDx - dlg->btnW, y, dlg->btnW, dlg->btnDy, SWP_NOZORDER);
}

static void ShowTranslationResult(SelectionTranslateDialog* dlg, const char* text, bool isError) {
    if (!dlg || !dlg->hwnd) {
        return;
    }
    HWND hwnd = dlg->hwnd;
    int labelDy = DpiScale(hwnd, 16);
    int resultDy = DpiScale(hwnd, 120);
    int x = dlg->pad;
    int y = dlg->yAfterLangs;
    const char* label = isError ? _TRA("Error:") : _TRA("Translation:");

    if (!dlg->resultVisible) {
        dlg->hwndResultLabel =
            CreateWindowExW(0, L"STATIC", ToWStrTemp(label), WS_CHILD | WS_VISIBLE, x, y, dlg->contentDx, labelDy, hwnd,
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
        SetWindowTextA(dlg->hwndResultLabel, label);
    }
    SetWindowTextA(dlg->hwndResultText, text);
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
    SetWindowTextA(dlg->hwndTranslateBtn, _TRA("Translate"));
    TempStr display = data->ok ? data->msg.Get() : FormatTranslationErrorForDisplayTemp(dlg->backend, data->msg.Get());
    ShowTranslationResult(dlg, display, !data->ok);
    UpdateTranslateButtonState(dlg);
}

static void SelectionTranslateThread(SelectionTranslateTaskData* data) {
    AutoDelete del(data);
    AutoFreeStr result;
    bool ok = RunTranslation(data->backend, data->srcLang.Get(), data->dstLang.Get(), data->text.Get(), result);
    if (!ok && result.empty()) {
        result = str::Dup(_TRA("Translation failed."));
    }

    auto done = new SelectionTranslateDoneData();
    done->hwndDlg = data->dlg->hwnd;
    done->ok = ok;
    done->msg = result.Release();
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
    SetWindowTextA(dlg->hwndTranslateBtn, _TRA("Translating..."));
    if (dlg->resultVisible) {
        ShowWindow(dlg->hwndResultLabel, SW_HIDE);
        ShowWindow(dlg->hwndResultText, SW_HIDE);
    }

    auto task = new SelectionTranslateTaskData();
    task->dlg = dlg;
    task->backend = dlg->backend;
    task->srcLang.SetCopy(srcLang);
    task->dstLang.SetCopy(dstLang);
    task->text.SetCopy(text);
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
        case WM_DESTROY:
            if (dlg->hwndOwner) {
                EnableWindow(dlg->hwndOwner, TRUE);
            }
            delete dlg;
            return 0;
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

    TempStr title = str::FormatTemp(_TRA("Translate with %s"), BackendDisplayName(backend));

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kSelectionTranslateWinClass, ToWStrTemp(title),
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
    int comboDy = DpiScale(hwnd, 24);
    dlg->btnDy = DpiScale(hwnd, 28);
    dlg->btnW = DpiScale(hwnd, 96);
    int langLabelDx = DpiScale(hwnd, 44);
    int clientW = DpiScale(hwndOwner, 500);
    int x = dlg->pad;
    int y = dlg->pad;
    dlg->contentDx = clientW - 2 * dlg->pad;
    int colDx = (dlg->contentDx - dlg->gap) / 2;
    int comboDx = colDx - langLabelDx;

    auto createLabel = [&](const char* text, int lx, int ly, int ldx) {
        HWND h = CreateWindowExW(0, L"STATIC", ToWStrTemp(text), WS_CHILD | WS_VISIBLE, lx, ly, ldx, labelDy, hwnd,
                                 nullptr, GetModuleHandleW(nullptr), nullptr);
        SetWindowFont(h, dlg->hFont, TRUE);
        return h;
    };

    createLabel(_TRA("Translate:"), x, y, dlg->contentDx);
    y += labelDy + dlg->gap;
    int srcTextDy = DpiScale(hwnd, 140);
    dlg->hwndSrcText =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ToWStrTemp(selText),
                        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, x, y, dlg->contentDx,
                        srcTextDy, hwnd, (HMENU)(INT_PTR)kIdSrcText, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndSrcText, dlg->hFont, TRUE);
    y += srcTextDy + dlg->gap;

    int col2X = x + colDx + dlg->gap;
    createLabel(_TRA("From:"), x, y, langLabelDx);
    dlg->hwndSrcLang = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                                       x + langLabelDx, y - DpiScale(hwnd, 2), comboDx, DpiScale(hwnd, 200), hwnd,
                                       (HMENU)(INT_PTR)kIdSrcLang, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndSrcLang, dlg->hFont, TRUE);
    PopulateLanguageCombo(dlg->hwndSrcLang, "English");

    createLabel(_TRA("To:"), col2X, y, langLabelDx);
    dlg->hwndDstLang = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
                                       col2X + langLabelDx, y - DpiScale(hwnd, 2), comboDx, DpiScale(hwnd, 200), hwnd,
                                       (HMENU)(INT_PTR)kIdDstLang, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndDstLang, dlg->hFont, TRUE);
    PopulateLanguageCombo(dlg->hwndDstLang, DefaultDestinationLanguageTemp());
    y += comboDy + dlg->gap;
    dlg->yAfterLangs = y;

    dlg->hwndTranslateBtn = CreateWindowExW(0, L"BUTTON", ToWStrTemp(_TRA("Translate")),
                                            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, y, dlg->btnW, dlg->btnDy, hwnd,
                                            (HMENU)(INT_PTR)kIdTranslateBtn, GetModuleHandleW(nullptr), nullptr);
    SetWindowFont(dlg->hwndTranslateBtn, dlg->hFont, TRUE);
    dlg->hwndCloseBtn =
        CreateWindowExW(0, L"BUTTON", ToWStrTemp(_TRA("Close")), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, y, dlg->btnW,
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