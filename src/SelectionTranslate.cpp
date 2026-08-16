/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgsIter.h"
#include "base/ScopedWin.h"
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
#include "GlobalPrefs.h"
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
#include "DarkMode_win.h"
#include "SelectionTranslate.h"

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

// maps the language names offered in the From/To dropdowns to ISO 639 codes
// used in Google / DeepL urls (name, code pairs)
static const char* gLangNameToCode =
    "English\0en\0Chinese (Simplified)\0zh-CN\0Chinese (Traditional)\0zh-TW\0Spanish\0es\0Arabic\0ar\0Hindi\0hi\0"
    "Portuguese\0pt\0Bengali\0bn\0Russian\0ru\0Japanese\0ja\0Punjabi\0pa\0German\0de\0French\0fr\0Korean\0ko\0"
    "Turkish\0tr\0Vietnamese\0vi\0Italian\0it\0Polish\0pl\0Ukrainian\0uk\0Dutch\0nl\0Thai\0th\0Indonesian\0id\0"
    "Czech\0cs\0Swedish\0sv\0Romanian\0ro\0Greek\0el\0Hebrew\0he\0Danish\0da\0Finnish\0fi\0Norwegian\0no\0"
    "Hungarian\0hu\0Slovak\0sk\0";

// empty result means unknown language (the dropdowns are editable, so the
// user can type anything) => auto-detect for source, English for destination
static TempStr LangCodeForUrlTemp(Str name) {
    if (str::IsEmptyOrWhiteSpace(name)) {
        return {};
    }
    TempStr n = str::DupTemp(name);
    str::TrimWSInPlace(n, str::TrimOpt::Both);
    int idx = SeqStrIndexIS(gLangNameToCode, n);
    if (idx < 0 || idx % 2 != 0) {
        return {};
    }
    return SeqStrByIndex(gLangNameToCode, idx + 1);
}

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

static TempStr OsDefaultDestinationLanguageTemp() {
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

static TempStr DefaultDestinationLanguageTemp() {
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

static TempStr DefaultSourceLanguageTemp() {
    if (gGlobalPrefs && !str::IsEmptyOrWhiteSpace(gGlobalPrefs->translateFromLang)) {
        return gGlobalPrefs->translateFromLang;
    }
    return kSrcLangAuto;
}

// update one remembered translate pref; returns true if it changed
static bool UpdateTranslatePref(Str* pref, Str value) {
    TempStr normalized = NormalizeLangNameTemp(value);
    if (!normalized || (*pref && str::EqI(*pref, normalized))) {
        return false;
    }
    str::ReplacePtr(pref, str::Dup(normalized));
    return true;
}

static Str EngineDisplayName(TranslateEngine engine);

// remember engine / from / to across launches of the dialog
static void MaybeSaveTranslatePrefs(TranslateEngine engine, Str srcLang, Str dstLang) {
    if (!gGlobalPrefs) {
        return;
    }
    bool changed = UpdateTranslatePref(&gGlobalPrefs->translateEngine, EngineDisplayName(engine));
    changed |= UpdateTranslatePref(&gGlobalPrefs->translateFromLang, srcLang);
    changed |= UpdateTranslatePref(&gGlobalPrefs->translateToLang, dstLang);
    if (changed) {
        SaveSettings();
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
    if (backend == AIChatBackend::Grok) {
        return StrL("grok");
    }
    if (backend == AIChatBackend::Claude) {
        return StrL("claude");
    }
    if (backend == AIChatBackend::Codex) {
        return StrL("codex");
    }
    if (backend == AIChatBackend::AntiGravity) {
        return StrL("antigravity");
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
    if (str::StartsWithI(text, StrL("error:"))) {
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
        if (backend == AIChatBackend::Claude) {
            return str::DupTemp(
                _TRA("Claude Code is not signed in. Open a terminal, run \"claude auth login\", "
                     "then try again."));
        }
        if (backend == AIChatBackend::Grok) {
            return str::DupTemp(_TRA("Grok Build is not signed in. Sign in to Grok Build, then try again."));
        }
        if (backend == AIChatBackend::Codex) {
            return str::DupTemp(_TRA("OpenAI Codex is not signed in. Sign in to Codex, then try again."));
        }
        if (backend == AIChatBackend::AntiGravity) {
            return str::DupTemp(_TRA(
                "Antigravity CLI is not signed in. Open a terminal, run \"antigravity auth login\", then try again."));
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
    TempStr s = ToStrTemp(buf);
    str::TrimWSInPlace(s, str::TrimOpt::Both);
    return s;
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
    if (eventType && str::Eq(eventType, StrL("text"))) {
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
    if (str::Eq(eventType, StrL("result"))) {
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
    if (str::Eq(eventType, StrL("assistant")) && str::Contains(line, StrL("\"type\":\"text\""))) {
        TempStr text = AIChatJsonStrTemp(line, "text");
        if (len(text) > 0 && !TranslationLooksLikeError(text)) {
            out.Append(text);
        }
    } else if (str::Eq(eventType, StrL("content_block_delta"))) {
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
    if (!eventType || !str::Eq(eventType, StrL("item.completed"))) {
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

// antigravity's stream-json isn't claude's: text arrives as `text_delta` in
// `event:step_update` lines with `step_type:agent_response`, and errors as an
// `event:result` with status ERROR (see AIAntiGravity.cpp::ParseStreamLine).
static void AppendAntiGravityTranslationText(Str line, str::Builder& out) {
    TempStr eventName = AIChatJsonStrTemp(line, "event");
    if (!eventName) {
        return;
    }
    if (str::Eq(eventName, StrL("step_update"))) {
        if (str::Contains(line, StrL("\"step_type\":\"agent_response\""))) {
            TempStr delta = AIChatJsonStrTemp(line, "text_delta");
            if (len(delta) > 0) {
                out.Append(delta);
            }
        }
        return;
    }
    if (str::Eq(eventName, StrL("result"))) {
        TempStr status = AIChatJsonStrTemp(line, "status");
        if (status && str::Eq(status, StrL("ERROR"))) {
            TempStr err = AIChatJsonStrTemp(line, "error");
            if (len(err) > 0) {
                out.Reset();
                out.Append(err);
            }
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
            if (backend == AIChatBackend::Grok) {
                AppendGrokTranslationText(line, translationOut);
            } else if (backend == AIChatBackend::Claude) {
                AppendClaudeTranslationText(line, translationOut);
            } else if (backend == AIChatBackend::Codex) {
                AppendCodexTranslationText(line, translationOut);
            } else if (backend == AIChatBackend::AntiGravity) {
                AppendAntiGravityTranslationText(line, translationOut);
            }
        }
        while (off < output.len && (output.s[off] == '\n' || output.s[off] == '\r')) {
            off++;
        }
    }
    {
        Str s = ToStr(translationOut);
        str::TrimWSInPlace(s, str::TrimOpt::Both);
        translationOut.len = s.len;
    }
    if (len(translationOut) == 0 && output && !str::Contains(output, StrL("{\"type\":")) &&
        !str::Contains(output, StrL("{\"event\":"))) {
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
    Str permsFlag = gGlobalPrefs->grokBuild.alwaysApprove ? StrL("--always-approve") : Str{};
    // QuoteCmdLineArgTemp: full Windows argv quoting (not just " -> \") so
    // prompt text ending in \" cannot inject extra CLI flags (CWE-88 / GHSA).
    return fmt("%s -p %s --cwd %s --output-format streaming-json --model %s --effort low %s",
               QuoteCmdLineArgTemp(exePath), QuoteCmdLineArgTemp(prompt), QuoteCmdLineArgTemp(cwd),
               QuoteCmdLineArgTemp(model), permsFlag);
}

static TempStr BuildClaudeTranslateCmdLineTemp(Str exePath, Str prompt) {
    Str model = gGlobalPrefs->claudeCode.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = "claude-sonnet-4-20250514";
    }
    Str permsFlag = gGlobalPrefs->claudeCode.skipPermissions ? StrL("--dangerously-skip-permissions") : Str{};
    TempStr sessionId = AIChatGenerateSessionIdTemp();
    return fmt("%s -p --verbose --output-format stream-json --model %s %s --session-id %s %s",
               QuoteCmdLineArgTemp(exePath), QuoteCmdLineArgTemp(model), permsFlag, sessionId,
               QuoteCmdLineArgTemp(prompt));
}

static TempStr BuildCodexTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd) {
    Str model = gGlobalPrefs->codexBuild.model;
    bool hasModel = !str::IsEmptyOrWhiteSpace(model);
    Str skipFlag = gGlobalPrefs->codexBuild.skipSandbox ? StrL("--dangerously-bypass-approvals-and-sandbox") : Str{};
    if (skipFlag) {
        if (hasModel) {
            return fmt("%s exec --json -C %s --skip-git-repo-check -m %s -s read-only %s %s",
                       QuoteCmdLineArgTemp(exePath), QuoteCmdLineArgTemp(cwd), QuoteCmdLineArgTemp(model), skipFlag,
                       QuoteCmdLineArgTemp(prompt));
        }
        return fmt("%s exec --json -C %s --skip-git-repo-check -s read-only %s %s", QuoteCmdLineArgTemp(exePath),
                   QuoteCmdLineArgTemp(cwd), skipFlag, QuoteCmdLineArgTemp(prompt));
    }
    if (hasModel) {
        return fmt("%s exec --json -C %s --skip-git-repo-check -m %s -s read-only %s", QuoteCmdLineArgTemp(exePath),
                   QuoteCmdLineArgTemp(cwd), QuoteCmdLineArgTemp(model), QuoteCmdLineArgTemp(prompt));
    }
    return fmt("%s exec --json -C %s --skip-git-repo-check -s read-only %s", QuoteCmdLineArgTemp(exePath),
               QuoteCmdLineArgTemp(cwd), QuoteCmdLineArgTemp(prompt));
}

static TempStr BuildAntiGravityTranslateCmdLineTemp(Str exePath, Str prompt) {
    Str model = gGlobalPrefs->antiGravity.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = "gemini-3.6-flash";
    }
    // the antigravity CLI takes different flags than claude (see the chat
    // provider in AIAntiGravity.cpp): --effort instead of --session-id, and
    // --dangerously-skip-permissions instead of --auto-approve. A one-shot
    // translation needs no --conversation.
    Str permsFlag = gGlobalPrefs->antiGravity.autoApprove ? StrL("--dangerously-skip-permissions") : Str{};
    return fmt("%s -p --model %s --effort low --output-format stream-json %s %s", QuoteCmdLineArgTemp(exePath),
               QuoteCmdLineArgTemp(model), permsFlag, QuoteCmdLineArgTemp(prompt));
}

static TempStr FindBackendExecutableTemp(AIChatBackend backend) {
    if (backend == AIChatBackend::Grok) {
        return GrokBuildExecutablePathTemp();
    }
    if (backend == AIChatBackend::Claude) {
        return ClaudeCodeExecutablePathTemp();
    }
    if (backend == AIChatBackend::Codex) {
        return CodexBuildExecutablePathTemp();
    }
    if (backend == AIChatBackend::AntiGravity) {
        return AntiGravityExecutablePathTemp();
    }
    return {};
}

static bool IsBackendInstalled(AIChatBackend backend) {
    if (backend == AIChatBackend::Grok) {
        return IsGrokBuildInstalled();
    }
    if (backend == AIChatBackend::Claude) {
        return IsClaudeCodeInstalled();
    }
    if (backend == AIChatBackend::Codex) {
        return IsCodexBuildInstalled();
    }
    if (backend == AIChatBackend::AntiGravity) {
        return IsAntiGravityInstalled();
    }
    return false;
}

static Str BackendDisplayName(AIChatBackend backend) {
    if (backend == AIChatBackend::Grok) {
        return StrL("Grok Build");
    }
    if (backend == AIChatBackend::Claude) {
        return StrL("Claude Code");
    }
    if (backend == AIChatBackend::Codex) {
        return StrL("OpenAI Codex");
    }
    if (backend == AIChatBackend::AntiGravity) {
        return StrL("Antigravity");
    }
    return StrL("AI");
}

// in dropdown order
static const TranslateEngine gAllEngines[] = {
    TranslateEngine::Google, TranslateEngine::DeepL, TranslateEngine::Grok,
    TranslateEngine::Claude, TranslateEngine::Codex, TranslateEngine::AntiGravity,
};

static bool EngineIsAI(TranslateEngine engine) {
    return engine == TranslateEngine::Grok || engine == TranslateEngine::Claude || engine == TranslateEngine::Codex ||
           engine == TranslateEngine::AntiGravity;
}

static AIChatBackend BackendFromEngine(TranslateEngine engine) {
    if (engine == TranslateEngine::Claude) {
        return AIChatBackend::Claude;
    }
    if (engine == TranslateEngine::Codex) {
        return AIChatBackend::Codex;
    }
    if (engine == TranslateEngine::AntiGravity) {
        return AIChatBackend::AntiGravity;
    }
    return AIChatBackend::Grok;
}

static Str EngineDisplayName(TranslateEngine engine) {
    switch (engine) {
        case TranslateEngine::DeepL:
            return StrL("DeepL");
        case TranslateEngine::Grok:
        case TranslateEngine::Claude:
        case TranslateEngine::Codex:
        case TranslateEngine::AntiGravity:
            return BackendDisplayName(BackendFromEngine(engine));
        default:
            return StrL("Google");
    }
}

static bool IsEngineAvailable(TranslateEngine engine) {
    if (EngineIsAI(engine)) {
        return IsBackendInstalled(BackendFromEngine(engine));
    }
    // Google / DeepL translate by opening a browser
    return HasPermission(Perm::InternetAccess);
}

static TranslateEngine EngineFromName(Str name) {
    for (TranslateEngine engine : gAllEngines) {
        if (!str::IsEmptyOrWhiteSpace(name) && str::EqI(name, EngineDisplayName(engine))) {
            return engine;
        }
    }
    return TranslateEngine::Default;
}

// Default => the engine remembered in settings; anything unavailable falls
// back to the first available engine
static TranslateEngine ResolveEngine(TranslateEngine engine) {
    if (engine == TranslateEngine::Default && gGlobalPrefs) {
        engine = EngineFromName(gGlobalPrefs->translateEngine);
    }
    if (engine != TranslateEngine::Default && IsEngineAvailable(engine)) {
        return engine;
    }
    for (TranslateEngine cand : gAllEngines) {
        if (IsEngineAvailable(cand)) {
            return cand;
        }
    }
    return TranslateEngine::Google;
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
    dd->SetCurrentSelection(selIdx);
}

// build the Google / DeepL web-translator url for the given languages and text
static TempStr BuildTranslateUrlTemp(TranslateEngine engine, Str srcLang, Str dstLang, Str text) {
    TempStr enc = URLEncodeMayTruncateTemp(text);
    if (!enc) {
        return {};
    }
    TempStr src = LangCodeForUrlTemp(srcLang);
    if (!src) {
        src = str::DupTemp("auto");
    }
    TempStr dst = LangCodeForUrlTemp(dstLang);
    if (!dst) {
        dst = str::DupTemp("en");
    }
    if (engine == TranslateEngine::DeepL) {
        // DeepL uses plain "zh" for Chinese
        if (str::StartsWithI(src, StrL("zh"))) {
            src = str::DupTemp("zh");
        }
        if (str::StartsWithI(dst, StrL("zh"))) {
            dst = str::DupTemp("zh");
        }
        return fmt("https://www.deepl.com/translator#%s/%s/%s", src, dst, enc);
    }
    return fmt("https://translate.google.com/?op=translate&sl=%s&tl=%s&text=%s", src, dst, enc);
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
    if (backend == AIChatBackend::Grok) {
        cmdLine = BuildGrokTranslateCmdLineTemp(exePath, prompt, cwd);
    } else if (backend == AIChatBackend::Claude) {
        cmdLine = BuildClaudeTranslateCmdLineTemp(exePath, prompt);
    } else if (backend == AIChatBackend::Codex) {
        cmdLine = BuildCodexTranslateCmdLineTemp(exePath, prompt, cwd);
    } else if (backend == AIChatBackend::AntiGravity) {
        cmdLine = BuildAntiGravityTranslateCmdLineTemp(exePath, prompt);
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
        SetTranslateButtonText(_TRA("Translating..."));
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
        SetTranslateButtonText(_TRA("Translate"));
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
        result = str::Dup(_TRA("Translation failed."));
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
            .s = _TRA("Translation:"),
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
        staticPrompt = NewVirtText({.s = _TRA("Translate with"), .font = font, .isRtl = isRtl});
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

        staticFromLabel = NewVirtText({.s = _TRA("From:"), .font = font, .isRtl = isRtl});
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
            .s = _TRA("To:"),
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

        btnClose = NewButton(_TRA("Close"), false);
        btnClose->onClick =
            MkMethod1<SelectionTranslateWnd, VirtMouseEvent*, &SelectionTranslateWnd::OnCloseClicked>(this);
        btnRow->AddChild(btnClose);

        btnTranslate = NewButton(_TRA("Translate"), true);
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
    if (editSrcText) {
        HwndSetFocus(editSrcText->hwnd);
    }
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
    TempStr selText = GetSelectedTextTemp(tab, "\n", isTextOnlySelection);
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
    Str title = _TRA("Translate");
    if (!wnd->Create(hwndOwner, selText, title)) {
        EnableWindow(hwndOwner, TRUE);
        delete wnd;
        return;
    }

    gSelectionTranslateWnd = wnd;
    HwndToForeground(wnd->hwnd);
}
