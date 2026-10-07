/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Translating the selection: orig's SelectionTranslate.cpp. The engine list,
// the language lists, the prompts, the per-backend command lines and stream
// parsing, the "looks like an error" rules and the remembered From / To / engine
// preferences are orig's.
//
// ng: orig's dialog is a WS_THICKFRAME window with win32 edits and editable
// combos that disables the main window while it is up. Where the platform can
// have such a window (gui/ToolWindow.h) it is that, in orig's sizes; elsewhere
// it is one of the port's gpui Dialogs with the same rows in the same order,
// each editable combo an Input beside a Select. The worker thread reports back
// through uitask::Post exactly as orig's does.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "base/CmdLineArgs.h"
#include "base/Http.h"
#include "base/UITask.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "TextSelection.h"
#include "Selection.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "Translations.h"
#include "Theme.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/PlatformFont.h"
#include "gui/ToolWindow.h"
#include "SelectionTranslate.h"

#include "SumatraLog.h"

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

#if OS_WIN
// clang-format off
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

static const char* gPrimaryLangNames =
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
    for (int i = 0; i < dimofi(gPrimaryLangIds); i++) {
        if (gPrimaryLangIds[i] == primary) {
            return SeqStrByIndex(gPrimaryLangNames, i);
        }
    }
    return {};
}
#else
// Map the selected UI language when the OS has no Windows LANGID API.
static const char* gLangCodeToName =
    "en\0English\0zh\0Chinese (Simplified)\0tw\0Chinese (Traditional)\0cn\0Chinese (Simplified)\0de\0German\0"
    "fr\0French\0es\0Spanish\0it\0Italian\0pt\0Portuguese\0ru\0Russian\0ja\0Japanese\0kr\0Korean\0ko\0Korean\0"
    "ar\0Arabic\0hi\0Hindi\0tr\0Turkish\0vn\0Vietnamese\0vi\0Vietnamese\0pl\0Polish\0uk\0Ukrainian\0nl\0Dutch\0"
    "th\0Thai\0id\0Indonesian\0cz\0Czech\0cs\0Czech\0sv\0Swedish\0ro\0Romanian\0el\0Greek\0he\0Hebrew\0"
    "da\0Danish\0fi\0Finnish\0no\0Norwegian\0hu\0Hungarian\0sk\0Slovak\0bn\0Bengali\0";
#endif

static TempStr OsDefaultDestinationLanguageTemp() {
#if OS_WIN
    LANGID langId = GetUserDefaultUILanguage();
    Str name = PrimaryLangIdToEnglishName(PRIMARYLANGID(langId));
    if (name) {
        return name;
    }
    if (SUBLANGID(langId) == SUBLANG_CHINESE_TRADITIONAL) {
        return StrL("Chinese (Traditional)");
    }
#else
    Str code = trans::GetCurrentLangCode();
    int idx = SeqStrIndexIS(gLangCodeToName, code);
    if (idx >= 0 && idx % 2 == 0) {
        return SeqStrByIndex(gLangCodeToName, idx + 1);
    }
#endif
    return StrL("English");
}

static TempStr DefaultDestinationLanguageTemp() {
    if (gSettings && !str::IsEmptyOrWhiteSpace(gSettings->translateToLang)) {
        return gSettings->translateToLang;
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
    if (gSettings && !str::IsEmptyOrWhiteSpace(gSettings->translateFromLang)) {
        return gSettings->translateFromLang;
    }
    return kSrcLangAuto;
}

// update one remembered translate pref; returns true if it changed
static bool UpdateTranslatePref(Str* pref, Str value) {
    TempStr normalized = NormalizeLangNameTemp(value);
    if (len(normalized) == 0 || (*pref && str::EqI(*pref, normalized))) {
        return false;
    }
    str::ReplacePtr(pref, str::Dup(normalized));
    return true;
}

static Str EngineDisplayName(TranslateEngine engine);

// remember engine / from / to across launches of the dialog
static void MaybeSaveTranslatePrefs(TranslateEngine engine, Str srcLang, Str dstLang) {
    if (!gSettings) {
        return;
    }
    bool changed = UpdateTranslatePref(&gSettings->translateEngine, EngineDisplayName(engine));
    changed |= UpdateTranslatePref(&gSettings->translateFromLang, srcLang);
    changed |= UpdateTranslatePref(&gSettings->translateToLang, dstLang);
    if (changed) {
        ScheduleSaveSettings();
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
    logf("selection-translate %s %s: %s\n", BackendLogName(backend), direction, text);
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
        return str::DupTemp(Tr("Translation failed."));
    }
    if (str::ContainsI(raw, StrL("failed to authenticate")) || str::ContainsI(raw, StrL("authentication_failed")) ||
        str::ContainsI(raw, StrL("invalid authentication credentials"))) {
        if (backend == AIChatBackend::Claude) {
            return str::DupTemp(
                Tr("Claude Code is not signed in. Open a terminal, run \"claude auth login\", "
                   "then try again."));
        }
        if (backend == AIChatBackend::Grok) {
            return str::DupTemp(Tr("Grok Build is not signed in."));
        }
        if (backend == AIChatBackend::Codex) {
            return str::DupTemp(Tr("OpenAI Codex is not signed in."));
        }
        if (backend == AIChatBackend::AntiGravity) {
            return str::DupTemp(Tr(
                "Antigravity CLI is not signed in. Open a terminal, run \"antigravity auth login\", then try again."));
        }
    }
    if (str::ContainsI(raw, StrL("model is not supported"))) {
        return str::DupTemp(Tr("The configured AI model is not available for your account."));
    }
    if (str::ContainsI(raw, StrL("did not contain text"))) {
        return str::DupTemp(Tr("Translation response did not contain text."));
    }
    return str::DupTemp(raw);
}

static TempStr StripTrailingSlashTemp(TempStr path) {
    if (len(path) == 0) {
        return {};
    }
    TempStr p = str::DupTemp(path);
    while (len(p) > 0 && (p.s[len(p) - 1] == '\\' || p.s[len(p) - 1] == '/')) {
        p.len--;
        p.s[len(p)] = 0;
    }
    return p;
}

static TempStr NormalizeTextForPromptTemp(Str text) {
    if (len(text) == 0) {
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

static void AppendGrokTranslationText(Str line, str::Builder& out) {
    TempStr eventType = AIChatJsonStrTemp(line, StrL("type"));
    if (eventType && str::Eq(eventType, StrL("text"))) {
        TempStr text = AIChatJsonStrTemp(line, StrL("data"));
        out.AppendNonEmpty(text);
    }
}

static void AppendClaudeTranslationText(Str line, str::Builder& out) {
    TempStr eventType = AIChatJsonStrTemp(line, StrL("type"));
    if (len(eventType) == 0) {
        return;
    }
    if (str::Eq(eventType, StrL("result"))) {
        bool isError = str::Contains(line, StrL("\"is_error\":true"));
        TempStr text = AIChatJsonStrTemp(line, StrL("result"));
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
        TempStr text = AIChatJsonStrTemp(line, StrL("text"));
        if (len(text) > 0 && !TranslationLooksLikeError(text)) {
            out.Append(text);
        }
    } else if (str::Eq(eventType, StrL("content_block_delta"))) {
        TempStr text = AIChatJsonStrTemp(line, StrL("text"));
        out.AppendNonEmpty(text);
    }
}

static void AppendCodexTranslationText(Str line, str::Builder& out) {
    if (len(line) == 0 || line.s[0] != '{') {
        return;
    }
    TempStr eventType = AIChatJsonStrTemp(line, StrL("type"));
    if (len(eventType) == 0 || !str::Eq(eventType, StrL("item.completed"))) {
        return;
    }
    TempStr text = AIChatJsonStrTemp(line, StrL("text"));
    if (len(text) > 0) {
        out.Append(text);
        return;
    }
    Str agentMsg;
    if (str::Cut(line, StrL("\"type\":\"agent_message\""), nullptr, &agentMsg)) {
        text = AIChatJsonStrTemp(agentMsg, StrL("text"));
        out.AppendNonEmpty(text);
    }
}

// antigravity's stream-json isn't claude's: text arrives as `text_delta` in
// `event:step_update` lines with `step_type:agent_response`, and errors as an
// `event:result` with status ERROR (see AIAntiGravity.cpp::ParseStreamLine).
static void AppendAntiGravityTranslationText(Str line, str::Builder& out) {
    TempStr eventName = AIChatJsonStrTemp(line, StrL("event"));
    if (len(eventName) == 0) {
        return;
    }
    if (str::Eq(eventName, StrL("step_update"))) {
        if (str::Contains(line, StrL("\"step_type\":\"agent_response\""))) {
            TempStr delta = AIChatJsonStrTemp(line, StrL("text_delta"));
            out.AppendNonEmpty(delta);
        }
        return;
    }
    if (str::Eq(eventName, StrL("result"))) {
        TempStr status = AIChatJsonStrTemp(line, StrL("status"));
        if (status && str::Eq(status, StrL("ERROR"))) {
            TempStr err = AIChatJsonStrTemp(line, StrL("error"));
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
        TempStr trimmed = str::DupTemp(output);
        str::TrimWSInPlace(trimmed, str::TrimOpt::Both);
        if (!str::IsEmptyOrWhiteSpace(trimmed)) {
            translationOut.Append(trimmed);
        }
    }
}

static TempStr BuildGrokTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd) {
    Str model = gSettings->grokBuild.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = StrL("grok-composer-2.5-fast");
    }
    Str permsFlag = gSettings->grokBuild.alwaysApprove ? StrL("--always-approve") : Str{};
    // QuoteCmdLineArgTemp: full Windows argv quoting (not just " -> \") so
    // prompt text ending in \" cannot inject extra CLI flags (CWE-88 / GHSA).
    return fmt("%s -p %s --cwd %s --output-format streaming-json --model %s --effort low %s",
               QuoteCmdLineArgTemp(exePath), QuoteCmdLineArgTemp(prompt), QuoteCmdLineArgTemp(cwd),
               QuoteCmdLineArgTemp(model), permsFlag);
}

static TempStr BuildClaudeTranslateCmdLineTemp(Str exePath, Str prompt) {
    Str model = gSettings->claudeCode.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = StrL("claude-sonnet-4-20250514");
    }
    Str permsFlag = gSettings->claudeCode.skipPermissions ? StrL("--dangerously-skip-permissions") : Str{};
    TempStr sessionId = AIChatGenerateSessionIdTemp();
    return fmt("%s -p --verbose --output-format stream-json --model %s %s --session-id %s %s",
               QuoteCmdLineArgTemp(exePath), QuoteCmdLineArgTemp(model), permsFlag, sessionId,
               QuoteCmdLineArgTemp(prompt));
}

static TempStr BuildCodexTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd) {
    Str model = gSettings->codexBuild.model;
    bool hasModel = !str::IsEmptyOrWhiteSpace(model);
    Str skipFlag = gSettings->codexBuild.skipSandbox ? StrL("--dangerously-bypass-approvals-and-sandbox") : Str{};
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
    Str model = gSettings->antiGravity.model;
    if (str::IsEmptyOrWhiteSpace(model)) {
        model = Str(kAntiGravityDefaultModel);
    }
    // agy takes -p/--print's next argument as the prompt and ignores flags
    // after it (see AIAntiGravity.cpp). Putting -p first made the prompt
    // "--model", so the model answered with its own name instead of translating.
    Str permsFlag = gSettings->antiGravity.autoApprove ? StrL("--dangerously-skip-permissions") : Str{};
    return fmt("%s --model %s --effort low --output-format stream-json %s -p %s", QuoteCmdLineArgTemp(exePath),
               QuoteCmdLineArgTemp(model), permsFlag, QuoteCmdLineArgTemp(prompt));
}

static TempStr FindBackendExecutableTemp(AIChatBackend backend) {
    if (backend == AIChatBackend::Grok) {
        return GetGrokBuildProvider()->FindExecutableTemp();
    }
    if (backend == AIChatBackend::Claude) {
        return GetClaudeCodeProvider()->FindExecutableTemp();
    }
    if (backend == AIChatBackend::Codex) {
        return GetCodexBuildProvider()->FindExecutableTemp();
    }
    if (backend == AIChatBackend::AntiGravity) {
        return GetAntiGravityProvider()->FindExecutableTemp();
    }
    return {};
}

static bool IsBackendInstalled(AIChatBackend backend) {
    if (backend == AIChatBackend::Grok) {
        return GetGrokBuildProvider()->IsInstalled();
    }
    if (backend == AIChatBackend::Claude) {
        return GetClaudeCodeProvider()->IsInstalled();
    }
    if (backend == AIChatBackend::Codex) {
        return GetCodexBuildProvider()->IsInstalled();
    }
    if (backend == AIChatBackend::AntiGravity) {
        return GetAntiGravityProvider()->IsInstalled();
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
    if (engine == TranslateEngine::Default && gSettings) {
        engine = EngineFromName(gSettings->translateEngine);
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

// build the Google / DeepL web-translator url for the given languages and text
static TempStr BuildTranslateUrlTemp(TranslateEngine engine, Str srcLang, Str dstLang, Str text) {
    TempStr enc = URLEncodeMayTruncateTemp(text);
    if (len(enc) == 0) {
        return {};
    }
    TempStr src = LangCodeForUrlTemp(srcLang);
    if (len(src) == 0) {
        src = str::DupTemp(StrL("auto"));
    }
    TempStr dst = LangCodeForUrlTemp(dstLang);
    if (len(dst) == 0) {
        dst = str::DupTemp(StrL("en"));
    }
    if (engine == TranslateEngine::DeepL) {
        // DeepL uses plain "zh" for Chinese
        if (str::StartsWithI(src, StrL("zh"))) {
            src = str::DupTemp(StrL("zh"));
        }
        if (str::StartsWithI(dst, StrL("zh"))) {
            dst = str::DupTemp(StrL("zh"));
        }
        return fmt("https://www.deepl.com/translator#%s/%s/%s", src, dst, enc);
    }
    return fmt("https://translate.google.com/?op=translate&sl=%s&tl=%s&text=%s", src, dst, enc);
}

static bool RunTranslation(AIChatBackend backend, Str srcLang, Str dstLang, Str text, Str& msgOut) {
    TempStr exePath = FindBackendExecutableTemp(backend);
    if (len(exePath) == 0) {
        msgOut = str::Dup(Tr("The selected AI CLI is not installed."));
        return false;
    }

    TempStr prompt = BuildTranslationPromptTemp(srcLang, dstLang, text);
    TempStr cwd = StripTrailingSlashTemp(GetTempDirPathTemp());
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
    str::BuilderReserve(output, 4096);
    AIChatReadPipeToEnd(launch.hReadPipe, output);
    launch.hReadPipe = nullptr;

    if (!AIChatWaitForProcess(launch.hProcess, 5 * 60 * 1000)) {
        AIChatTerminateProcess(launch.hProcess);
        AIChatCloseProcess(&launch.hProcess, false);
        msgOut = str::Dup(Tr("Translation timed out."));
        LogTranslation(backend, StrL("<<< error"), msgOut);
        return false;
    }
    AIChatCloseProcess(&launch.hProcess, false);

    LogTranslation(backend, StrL("<<< raw"), ToStr(output));

    str::Builder translation;
    str::BuilderReserve(translation, 1024);
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

static bool TranslateDlgTestResult(bool ok, Str text);

// backend: 0=Claude, 1=Grok, 2=Codex, 3=AntiGravity; -1: no translation, the
// open dialog shows `text` as a result (srcLang "error": as a failure)
TempStr SelectionTranslateResultTemp(int backend, Str srcLang, Str dstLang, Str text, int* exitCode) {
    if (backend < 0) {
        bool shown = TranslateDlgTestResult(!str::Eq(srcLang, StrL("error")), text);
        if (exitCode) {
            *exitCode = shown ? 0 : 1;
        }
        return str::DupTemp(shown ? StrL("shown") : StrL("no dialog"));
    }
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

// --- the dialog -------------------------------------------------------------

struct TranslateDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    // engine pre-selected in the dropdown when the dialog opens
    TranslateEngine engine = TranslateEngine::Google;
    // AI backend of the in-flight translation (for error formatting)
    AIChatBackend backend = AIChatBackend::Grok;
    bool translating = false;
    bool resultVisible = false;
    bool resultIsError = false;
    // a generation counter: a result from a superseded dialog is dropped
    int generation = 0;

    gpui::InputState* editSrcText = nullptr;
    gpui::InputState* editResult = nullptr;
    gpui::InputState* editSrcLang = nullptr;
    gpui::InputState* editDstLang = nullptr;
    DialogSelect ddEngine;
    DialogSelect ddSrcLang;
    DialogSelect ddDstLang;
    // the engines offered, in dropdown order (parallel to ddEngine's items)
    Vec<TranslateEngine> engines;
    // orig's window, where the platform can have one; null: a dialog in the
    // frame
    ToolWindow* tw = nullptr;
    // its client size (dips) without the result rows
    Size toolClient;
    // orig's EditSetFocus(editSrcText) once the window is up
    bool toolWantsFocus = false;
};

static TranslateDlg gTranslate;

struct TranslateView {
    static void OnTranslate(TranslateView* self, gpui::Ctx* cx, const gpui::ClickEvent*);
    static void OnClose(TranslateView* self, gpui::Ctx* cx, const gpui::ClickEvent*);
};

static gp::Entity<TranslateView> gTranslateView;

// the dialog in the frame; a window of its own is not the frame's business
bool IsSelectionTranslateDialogVisible() {
    return gTranslate.visible && !gTranslate.tw;
}

static void TranslateOpenToolWindow(MainWindow* win, Str selText);
static void TranslateToolShowResult();

void CloseSelectionTranslateDialog() {
    if (!gTranslate.visible) {
        return;
    }
    gTranslate.visible = false;
    if (gTranslate.tw) {
        ToolWindowClose(gTranslate.tw);
        gTranslate.tw = nullptr;
    }
    gTranslate.generation++;
    gTranslate.ddEngine.Free();
    gTranslate.ddSrcLang.Free();
    gTranslate.ddDstLang.Free();
    delete gTranslate.editSrcText;
    gTranslate.editSrcText = nullptr;
    delete gTranslate.editResult;
    gTranslate.editResult = nullptr;
    delete gTranslate.editSrcLang;
    gTranslate.editSrcLang = nullptr;
    delete gTranslate.editDstLang;
    gTranslate.editDstLang = nullptr;
    VecReset(gTranslate.engines);
    AppShellInvalidate(gTranslate.win);
}

static TempStr DlgSrcLangTemp() {
    Str s = FromGpui(gp::InputValue(gTranslate.editSrcLang));
    return str::DupTemp(len(s) > 0 ? s : kSrcLangAuto);
}

static TempStr DlgDstLangTemp() {
    Str s = FromGpui(gp::InputValue(gTranslate.editDstLang));
    return str::DupTemp(len(s) > 0 ? s : StrL("English"));
}

static TranslateEngine DlgEngine() {
    int sel = gTranslate.ddEngine.sel;
    if (sel >= 0 && sel < len(gTranslate.engines)) {
        return gTranslate.engines[sel];
    }
    return gTranslate.engine;
}

// the worker thread and its hand-off back to the ui thread (orig's
// SelectionTranslateTaskData / SelectionTranslateDoneData)
struct TranslateTaskData {
    int generation = 0;
    AIChatBackend backend = AIChatBackend::Grok;
    Str srcLang;
    Str dstLang;
    Str text;
    bool ok = false;
    Str msg;
};

static void FreeTranslateTask(TranslateTaskData* d) {
    str::Free(d->srcLang);
    str::Free(d->dstLang);
    str::Free(d->text);
    str::Free(d->msg);
    delete d;
}

static void OnTranslateDone(TranslateTaskData* data) {
    if (!gTranslate.visible || gTranslate.generation != data->generation) {
        FreeTranslateTask(data);
        return;
    }
    gTranslate.translating = false;
    TempStr display =
        data->ok ? str::DupTemp(data->msg) : FormatTranslationErrorForDisplayTemp(gTranslate.backend, data->msg);
    gTranslate.resultVisible = true;
    gTranslate.resultIsError = !data->ok;
    TranslateToolShowResult();
    if (gTranslate.editResult) {
        gp::InputSetValue(gTranslate.editResult, ToGpui(display));
    }
    logf("SelectionTranslate: %s '%s'\n", Str(data->ok ? "ok" : "failed"), display);
    AppShellInvalidate(gTranslate.win);
    FreeTranslateTask(data);
}

static bool TranslateDlgTestResult(bool ok, Str text) {
    if (!gTranslate.visible) {
        return false;
    }
    auto* task = new TranslateTaskData();
    task->generation = gTranslate.generation;
    task->ok = ok;
    task->msg = str::Dup(text);
    OnTranslateDone(task);
    return true;
}

static void SelectionTranslateThread(TranslateTaskData* data) {
    Str result;
    bool ok = RunTranslation(data->backend, data->srcLang, data->dstLang, data->text, result);
    if (!ok && len(result) == 0) {
        result = str::Dup(Tr("Translation failed."));
    }
    data->ok = ok;
    data->msg = result;
    uitask::Post(MkFunc0(OnTranslateDone, data), "SelectionTranslateDone");
}

static void StartTranslation() {
    if (gTranslate.translating) {
        return;
    }
    TempStr srcLang = DlgSrcLangTemp();
    TempStr dstLang = DlgDstLangTemp();
    TempStr text = str::DupTemp(FromGpui(gp::InputValue(gTranslate.editSrcText)));
    logf("StartTranslation: '%s' -> '%s', %d bytes\n", srcLang, dstLang, len(text));
    if (str::IsEmptyOrWhiteSpace(text) || LanguagesAreSameTemp(srcLang, dstLang)) {
        return;
    }

    TranslateEngine curEngine = ResolveEngine(DlgEngine());
    MaybeSaveTranslatePrefs(curEngine, srcLang, dstLang);

    if (!EngineIsAI(curEngine)) {
        // Google / DeepL translate in the browser; keep the dialog open so the
        // user can tweak languages or pick a different engine
        TempStr url = BuildTranslateUrlTemp(curEngine, srcLang, dstLang, text);
        if (url) {
            logf("SelectionTranslate: opening '%s'\n", url);
            SumatraLaunchBrowser(url);
        }
        return;
    }

    gTranslate.backend = BackendFromEngine(curEngine);
    gTranslate.translating = true;
    gTranslate.resultVisible = false;

    auto* task = new TranslateTaskData();
    task->generation = gTranslate.generation;
    task->backend = gTranslate.backend;
    task->srcLang = str::Dup(srcLang);
    task->dstLang = str::Dup(dstLang);
    task->text = str::Dup(text);
    RunAsync(MkFunc0(SelectionTranslateThread, task), StrL("SelectionTranslate"));
}

bool SelectionTranslateOnEnter() {
    if (!IsSelectionTranslateDialogVisible()) {
        return false;
    }
    StartTranslation();
    AppShellInvalidate(gTranslate.win);
    return true;
}

void TranslateView::OnTranslate(TranslateView*, gp::Ctx* cx, const gp::ClickEvent*) {
    StartTranslation();
    gp::Notify(cx);
}

void TranslateView::OnClose(TranslateView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseSelectionTranslateDialog();
    gp::Notify(cx);
}

static void FillLanguageSelect(DialogSelect& dd, Str initial, bool includeAuto) {
    StrVec items;
    if (includeAuto) {
        items.Append(kSrcLangAuto);
    }
    for (Str lang : gPopularLanguages) {
        items.Append(lang);
    }
    int sel = 0;
    for (int i = 0; i < len(items); i++) {
        if (str::EqI(items[i], initial)) {
            sel = i;
            break;
        }
    }
    dd.SetItems(items, sel);
}

static void FillEngineSelect(TranslateEngine selected) {
    StrVec items;
    VecReset(gTranslate.engines);
    int selIdx = 0;
    for (TranslateEngine engine : gAllEngines) {
        if (!IsEngineAvailable(engine)) {
            continue;
        }
        if (engine == selected) {
            selIdx = len(items);
        }
        items.Append(EngineDisplayName(engine));
        VecAppend(gTranslate.engines, engine);
    }
    if (len(items) == 0) {
        items.Append(EngineDisplayName(TranslateEngine::Google));
        VecAppend(gTranslate.engines, TranslateEngine::Google);
    }
    gTranslate.ddEngine.SetItems(items, selIdx);
}

void ShowSelectionTranslateDialog(WindowTab* tab, TranslateEngine engineIn) {
    if (!tab || !tab->win || !tab->selectionOnPage) {
        return;
    }
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }
    bool isTextOnlySelection = false;
    TempStr selText = GetSelectedTextTemp(tab, StrL("\n"), isTextOnlySelection);
    if (str::IsEmptyOrWhiteSpace(selText)) {
        return;
    }
    // ng: a page's lines end in "\r"; a win32 edit wants "\r\n", a gpui one
    // draws a line break for each of the two
    selText = str::ReplaceTemp(selText, StrL("\r"), StrL(""));

    if (gTranslate.visible && gTranslate.tw) {
        // orig: HwndSetFocus() on the one that is open
        ToolWindowActivate(gTranslate.tw);
        return;
    }
    CloseSelectionTranslateDialog();
    MainWindow* win = tab->win;
    gTranslate.win = win;
    gTranslate.engine = ResolveEngine(engineIn);
    gTranslate.translating = false;
    gTranslate.resultVisible = false;
    gTranslate.resultIsError = false;

    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gTranslate.editSrcText = new gp::InputState();
    gTranslate.editSrcText->kind = gp::InputKind::Textarea;
    gTranslate.editSrcText->focus = gp::FocusHandleNew(app);
    gp::InputSetValue(gTranslate.editSrcText, ToGpui(selText));
    gTranslate.editResult = new gp::InputState();
    gTranslate.editResult->kind = gp::InputKind::Textarea;
    gTranslate.editResult->focus = gp::FocusHandleNew(app);
    gTranslate.editSrcLang = new gp::InputState();
    gTranslate.editSrcLang->focus = gp::FocusHandleNew(app);
    gp::InputSetValue(gTranslate.editSrcLang, ToGpui(DefaultSourceLanguageTemp()));
    gTranslate.editDstLang = new gp::InputState();
    gTranslate.editDstLang->focus = gp::FocusHandleNew(app);
    gp::InputSetValue(gTranslate.editDstLang, ToGpui(DefaultDestinationLanguageTemp()));

    gTranslate.ddEngine.Init(app);
    gTranslate.ddSrcLang.Init(app);
    gTranslate.ddDstLang.Init(app);
    FillEngineSelect(gTranslate.engine);
    FillLanguageSelect(gTranslate.ddSrcLang, DefaultSourceLanguageTemp(), true);
    FillLanguageSelect(gTranslate.ddDstLang, DefaultDestinationLanguageTemp(), false);

    gTranslate.visible = true;
    TranslateOpenToolWindow(win, selText);
    logf("ShowSelectionTranslateDialog: engine '%s', %d bytes selected\n", EngineDisplayName(gTranslate.engine),
         len(selText));
    AppShellInvalidate(win);
}

// --- a window of its own (Windows) ------------------------------------------

// orig's layout at 96 dpi: 12 around; the source edit (7 lines of the app
// font: 113); "Translation:" 8 under it and the result edit 4 under that (6
// lines: 98); the engine row, the language row and the button row, each 8
// under the one before; 23 high combos, 25 high buttons
constexpr float kTrPad = 12;
constexpr float kTrGap = 8;
constexpr float kTrSrcDy = 113;
constexpr float kTrLabelDy = 15;
constexpr float kTrResultGap = 4;
constexpr float kTrResultDy = 98;
constexpr float kTrComboDy = 23;
constexpr float kTrBtnDy = 25;
constexpr float kTrBtnPadDx = 12;
// orig gives the default button 4 more on each side
constexpr float kTrDefBtnPadDx = 15;
constexpr float kTrBtnGap = 7;
constexpr float kTrFontPx = 12;
// a language combo's ideal width, and what the source edit adds to its text
constexpr int kTrLangComboDx = 132;
constexpr int kTrEditExtraDx = 10;
constexpr int kTrEditIdealChars = 40;
constexpr int kTrEditMaxChars = 120;
// the result rows: label gap + label + edit gap + edit
constexpr int kTrResultRowsDy = (int)(kTrGap + kTrLabelDy + kTrResultGap + kTrResultDy);
constexpr int kTrBaseDy =
    (int)(kTrPad + kTrSrcDy + kTrGap + kTrComboDy + kTrGap + kTrComboDy + kTrGap + kTrBtnDy + kTrPad);

static Str TranslateToolTitle() {
    return Tr("Translate");
}

// orig's Relayout(): the window keeps its size, or grows to what the content
// needs once the result rows are shown
static void TranslateToolShowResult() {
    ToolWindow* tw = gTranslate.tw;
    if (!tw) {
        return;
    }
    Size want = gTranslate.toolClient;
    want.dy += kTrResultRowsDy;
    ToolWindowSetMinClient(tw, want);
    gp::Window* gw = ToolWindowGpui(tw);
    if (!gw) {
        return;
    }
    gp::WinSize ws = gp::WindowSize(gw);
    Size cur{(int)ws.dipW, (int)ws.dipH};
    if (cur.dx < want.dx || cur.dy < want.dy) {
        ToolWindowSetClientSize(tw, Size(std::max(cur.dx, want.dx), std::max(cur.dy, want.dy)));
    }
}

static gp::El* TranslateToolLabel(gp::Ctx* cx, Str s, float font) {
    return gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(font)->Fg(gp::ThemeNow(cx->app).foreground)->Shrink0();
}

static gp::El* TranslateToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gTranslate.visible || !gTranslate.tw) {
        return nullptr;
    }
    if (!gTranslateView.IsValid()) {
        gTranslateView = gp::EntityNewState<TranslateView>(cx->app);
    }
    gTranslate.ddEngine.PollChanged(cx->app);
    if (gTranslate.toolWantsFocus) {
        gTranslate.toolWantsFocus = false;
        gp::InputFocus(gTranslate.editSrcText, cx->app, cx->win);
    }
    float font = kTrFontPx * ToolWindowSetUiFontPx(cx, kTrFontPx);
    gp::WinSize ws = gp::WindowSize(cx->win);
    bool busy = gTranslate.translating;

    // the edits share what the window has beyond its fixed rows
    float editsDy = ws.dipH - ((float)kTrBaseDy - kTrSrcDy);
    float srcDy = editsDy;
    float resultDy = 0;
    if (gTranslate.resultVisible) {
        editsDy -= kTrGap + kTrLabelDy + kTrResultGap;
        float extra = std::max(editsDy - kTrSrcDy - kTrResultDy, 0.f);
        srcDy = kTrSrcDy + extra / 2;
        resultDy = kTrResultDy + extra / 2;
    }

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->Pad(kTrPad);
    col->Child(gpc::Textarea::New(cx, GStrL("translate-src"), gTranslate.editSrcText)
                   ->H(srcDy)
                   ->Disabled(busy)
                   ->IntoEl()
                   ->Shrink0());
    if (gTranslate.resultVisible) {
        Str label = gTranslate.resultIsError ? Str(Tr("Error:")) : Str(Tr("Translation:"));
        col->Child(gp::Div(cx->a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->H(kTrGap + kTrLabelDy)
                       ->PadT(kTrGap)
                       ->Shrink0()
                       ->Child(TranslateToolLabel(cx, label, font)));
        col->Child(gp::Div(cx->a)
                       ->PadT(kTrResultGap)
                       ->Shrink0()
                       ->Child(gpc::Textarea::New(cx, GStrL("translate-result"), gTranslate.editResult)
                                   ->H(resultDy)
                                   ->Readonly()
                                   ->IntoEl()));
    }

    auto row = [&](float dy) {
        return gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->H(kTrGap + dy)->PadT(kTrGap)->Shrink0();
    };
    gp::El* engineRow = row(kTrComboDy);
    engineRow->Child(TranslateToolLabel(cx, Tr("Translate with"), font));
    engineRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->PadL(4)->Child(
        gTranslate.ddEngine.Build(cx, StrL("translate-engine"), gp::kFill, busy)));
    col->Child(engineRow);

    gp::El* langRow = row(kTrComboDy);
    langRow->Child(TranslateToolLabel(cx, Tr("From:"), font));
    langRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->PadL(4)->Child(
        gTranslate.ddSrcLang.BuildCombo(cx, StrL("translate-from"), gTranslate.editSrcLang, gp::kFill, busy)));
    langRow->Child(gp::Div(cx->a)->PadL(4)->Shrink0()->Child(TranslateToolLabel(cx, Tr("To:"), font)));
    langRow->Child(gp::Div(cx->a)->Flex1()->MinW(0)->Child(
        gTranslate.ddDstLang.BuildCombo(cx, StrL("translate-to"), gTranslate.editDstLang, gp::kFill, busy)));
    col->Child(langRow);

    TempStr srcLang = DlgSrcLangTemp();
    TempStr dstLang = DlgDstLangTemp();
    TempStr srcText = str::DupTemp(FromGpui(gp::InputValue(gTranslate.editSrcText)));
    bool enable = !busy && !str::IsEmptyOrWhiteSpace(srcText) && !LanguagesAreSameTemp(srcLang, dstLang);
    Str okText = busy ? Str(Tr("Translating...")) : Str(Tr("Translate"));

    auto button = [&](gp::Str id, Str label, gp::Listener onClick, bool isDefault, bool disabled) {
        gpc::Button* b = gpc::Button::New(cx, id)->Label(GpuiDup(cx->a, label))->Disabled(disabled)->OnClick(onClick);
        if (isDefault) {
            b->Primary();
        }
        return b->IntoEl()->H(kTrBtnDy)->PadX(isDefault ? kTrDefBtnPadDx : kTrBtnPadDx)->Shrink0();
    };
    gp::El* buttons = row(kTrBtnDy)->JustifyEnd()->Gap(kTrBtnGap);
    buttons->Child(button(GStrL("translate-close"), Tr("Close"), gp::ListenTo(gTranslateView, &TranslateView::OnClose),
                          false, false));
    buttons->Child(button(GStrL("translate-go"), okText, gp::ListenTo(gTranslateView, &TranslateView::OnTranslate),
                          true, !enable));
    col->Child(buttons);
    return col;
}

// orig's closeOnEsc and closeOnCtrlW: the window closes before the focused
// control sees the key
static bool TranslateToolOnCaptureKey(MainWindow*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    if (!gTranslate.visible) {
        return false;
    }
    bool isEsc = ev->vk == VK_ESCAPE && !IsTrackedPopupOpen(cx) && !gTranslate.ddSrcLang.comboOpen &&
                 !gTranslate.ddDstLang.comboOpen;
    bool isCtrlW = ev->vk == 'W' && ev->ctrl && !ev->alt;
    if (!isEsc && !isCtrlW) {
        return false;
    }
    CloseSelectionTranslateDialog();
    return true;
}

// Enter is the default button's; a multi-line edit keeps it
// (WindowBase::ActivateOnEnter)
static bool TranslateToolOnKey(MainWindow*, gp::Ctx*, const gp::KeyEvent* ev) {
    if (!gTranslate.visible || ev->vk != VK_RETURN || ev->ctrl || ev->alt) {
        return false;
    }
    StartTranslation();
    AppShellInvalidate(gTranslate.win);
    return true;
}

static void TranslateToolOnClosed(MainWindow*) {
    gTranslate.tw = nullptr;
    CloseSelectionTranslateDialog();
}

#if OS_WIN
// orig's ideal size (which is its minimum too), from its controls' ideal
// sizes: the source edit is as wide as the selection's longest line, between
// 40 and 120 average characters; the language row needs its two combos
static Size TranslateToolClientSize(MainWindow* win, Str selText) {
    int dpi = std::max(AppShellWindowDpi(win), 96);
    PlatformFont* font = GetDefaultGuiFont();
    int textDx = 0;
    for (int off = 0; off < selText.len;) {
        Str rest = Str(selText.s + off, selText.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        textDx = std::max(PlatformFontMeasureText(font, Str(rest.s, lineLen)).dx, textDx);
        off += lineLen + (nl >= 0 ? 1 : 0);
    }
    int acw = font->averageCharWidth;
    int editDx = std::clamp(textDx, kTrEditIdealChars * acw, kTrEditMaxChars * acw) + MulDiv(kTrEditExtraDx, dpi, 96);
    int fromDx = PlatformFontMeasureText(font, Tr("From:")).dx;
    int toDx = PlatformFontMeasureText(font, Tr("To:")).dx;
    int langRowDx = fromDx + toDx + MulDiv(2 * 4 + 2 * kTrLangComboDx, dpi, 96);
    int dx = std::max(editDx, langRowDx);
    return Size(MulDiv(dx, 96, dpi) + 2 * (int)kTrPad, kTrBaseDy);
}
#endif

static void TranslateOpenToolWindow(MainWindow* win, Str selText) {
    if (gTranslate.tw || !ToolWindowsAvailable()) {
        return;
    }
#if OS_WIN
    // orig: WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, no owner,
    // the main window disabled while it is up, centered on it
    ToolWindowDesc desc;
    desc.name = "translate";
    desc.title = TranslateToolTitle;
    desc.frame = ToolWinFrame::Caption;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::TopLevel;
    desc.modal = ToolWinModal::Yes;
    desc.build = TranslateToolBuild;
    desc.onKey = TranslateToolOnKey;
    desc.onCaptureKey = TranslateToolOnCaptureKey;
    desc.onClosed = TranslateToolOnClosed;
    gTranslate.toolClient = TranslateToolClientSize(win, selText);
    desc.minClient = gTranslate.toolClient;
    gTranslate.toolWantsFocus = true;
    gTranslate.tw = ToolWindowOpen(desc, win, ToolWindowCenteredRect(desc, win, gTranslate.toolClient));
#else
    (void)win;
    (void)selText;
#endif
}

gp::El* SelectionTranslateDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gTranslate.visible || gTranslate.tw || gTranslate.win != win) {
        return nullptr;
    }
    if (!gTranslateView.IsValid()) {
        gTranslateView = gp::EntityNewState<TranslateView>(cx->app);
    }
    gTranslate.ddEngine.PollChanged(cx->app);
    if (gTranslate.ddSrcLang.PollChanged(cx->app)) {
        gp::InputSetValue(gTranslate.editSrcLang, ToGpui(gTranslate.ddSrcLang.SelText()));
    }
    if (gTranslate.ddDstLang.PollChanged(cx->app)) {
        gp::InputSetValue(gTranslate.editDstLang, ToGpui(gTranslate.ddDstLang.SelText()));
    }

    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8)->W(gp::kFill);

    // the two text fields come first and the controls that act on them
    // (engine, languages, buttons) follow, so the dialog reads top to bottom
    body->Child(gpc::Textarea::New(cx, GStrL("translate-src"), gTranslate.editSrcText)->Rows(7)->IntoEl());
    if (gTranslate.resultVisible) {
        Str label = gTranslate.resultIsError ? Str(Tr("Error:")) : Str(Tr("Translation:"));
        body->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(13)->Fg(th.foreground));
        body->Child(gpc::Textarea::New(cx, GStrL("translate-result"), gTranslate.editResult)->Rows(6)->IntoEl());
    }

    gp::El* engineRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(6);
    engineRow->Child(gp::TextEl(cx->a, ToGpui(Tr("Translate with")))->Font(13)->Fg(th.foreground));
    engineRow->Child(
        gp::Div(cx->a)->Flex1()->MinW(0)->Child(gTranslate.ddEngine.Build(cx, StrL("translate-engine"), gp::kFill)));
    body->Child(engineRow);

    gp::El* langRow = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(6);
    langRow->Child(gp::TextEl(cx->a, ToGpui(Tr("From:")))->Font(13)->Fg(th.foreground));
    gp::El* srcCombo = gp::Div(cx->a)->FlexRow()->Flex1()->MinW(0)->Gap(4);
    srcCombo->Child(
        gp::Div(cx->a)->Flex1()->MinW(0)->Child(gpc::Input::New(cx, GStrL("translate-from"), gTranslate.editSrcLang)
                                                    ->WithSize(gp::UiSize::Small)
                                                    ->W(gp::kFill)
                                                    ->IntoEl()));
    srcCombo->Child(gTranslate.ddSrcLang.Build(cx, StrL("translate-from-list"), 120));
    langRow->Child(srcCombo);
    langRow->Child(gp::TextEl(cx->a, ToGpui(Tr("To:")))->Font(13)->Fg(th.foreground));
    gp::El* dstCombo = gp::Div(cx->a)->FlexRow()->Flex1()->MinW(0)->Gap(4);
    dstCombo->Child(
        gp::Div(cx->a)->Flex1()->MinW(0)->Child(gpc::Input::New(cx, GStrL("translate-to"), gTranslate.editDstLang)
                                                    ->WithSize(gp::UiSize::Small)
                                                    ->W(gp::kFill)
                                                    ->IntoEl()));
    dstCombo->Child(gTranslate.ddDstLang.Build(cx, StrL("translate-to-list"), 120));
    langRow->Child(dstCombo);
    body->Child(langRow);

    TempStr srcLang = DlgSrcLangTemp();
    TempStr dstLang = DlgDstLangTemp();
    TempStr srcText = str::DupTemp(FromGpui(gp::InputValue(gTranslate.editSrcText)));
    bool enable =
        !gTranslate.translating && !str::IsEmptyOrWhiteSpace(srcText) && !LanguagesAreSameTemp(srcLang, dstLang);
    Str okText = gTranslate.translating ? Str(Tr("Translating...")) : Str(Tr("Translate"));

    gp::El* footer = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(8);
    footer->Child(gp::Div(cx->a)->Flex1());
    footer->Child(gpc::Button::New(cx, GStrL("translate-close"))
                      ->Label(ToGpui(Tr("Close")))
                      ->WithSize(gp::UiSize::Small)
                      ->OnClick(gp::ListenTo(gTranslateView, &TranslateView::OnClose))
                      ->IntoEl());
    footer->Child(gpc::Button::New(cx, GStrL("translate-go"))
                      ->Label(ToGpui(okText))
                      ->Primary()
                      ->WithSize(gp::UiSize::Small)
                      ->Disabled(!enable)
                      ->OnClick(gp::ListenTo(gTranslateView, &TranslateView::OnTranslate))
                      ->IntoEl());

    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(Tr("Translate")))
        ->Body(body)
        ->Footer(footer)
        ->W(560)
        ->OnClose(gp::ListenTo(gTranslateView, &TranslateView::OnClose))
        ->IntoEl(gp::WindowSize(cx->win));
}
