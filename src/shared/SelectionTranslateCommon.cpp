/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/UITask.h"
#include "base/Http.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/PlatformFont.h"
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
#include "SelectionTranslate.h"
#include "SelectionTranslateCommon.h"

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

static TempStr NormalizeLangNameTemp(Str lang) {
    if (str::IsEmptyOrWhiteSpace(lang)) {
        return {};
    }
    TempStr normalized = str::DupTemp(lang);
    str::TrimWSInPlace(normalized, str::TrimOpt::Both);
    return normalized;
}

TempStr DefaultSourceLanguageTemp() {
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

// remember engine / from / to across launches of the dialog
void MaybeSaveTranslatePrefs(TranslateEngine engine, Str srcLang, Str dstLang) {
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

bool LanguagesAreSameTemp(Str a, Str b) {
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

Str BackendLogName(AIChatBackend backend) {
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

bool TranslationLooksLikeError(Str text) {
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

TempStr FormatTranslationErrorForDisplayTemp(AIChatBackend backend, Str raw) {
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

TempStr StripTrailingSlashTemp(TempStr path) {
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

TempStr BuildTranslationPromptTemp(Str srcLang, Str dstLang, Str text) {
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

void ParseTranslationOutput(AIChatBackend backend, Str output, str::Builder& translationOut) {
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

TempStr BuildGrokTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd) {
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

TempStr BuildClaudeTranslateCmdLineTemp(Str exePath, Str prompt) {
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

TempStr BuildCodexTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd) {
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

TempStr BuildAntiGravityTranslateCmdLineTemp(Str exePath, Str prompt) {
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

TempStr FindBackendExecutableTemp(AIChatBackend backend) {
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

Str BackendDisplayName(AIChatBackend backend) {
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

bool EngineIsAI(TranslateEngine engine) {
    return engine == TranslateEngine::Grok || engine == TranslateEngine::Claude || engine == TranslateEngine::Codex ||
           engine == TranslateEngine::AntiGravity;
}

AIChatBackend BackendFromEngine(TranslateEngine engine) {
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

Str EngineDisplayName(TranslateEngine engine) {
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

bool IsEngineAvailable(TranslateEngine engine) {
    if (EngineIsAI(engine)) {
        return IsBackendInstalled(BackendFromEngine(engine));
    }
    // Google / DeepL translate by opening a browser
    return HasPermission(Perm::InternetAccess);
}

TranslateEngine EngineFromName(Str name) {
    for (TranslateEngine engine : gAllEngines) {
        if (!str::IsEmptyOrWhiteSpace(name) && str::EqI(name, EngineDisplayName(engine))) {
            return engine;
        }
    }
    return TranslateEngine::Default;
}

// Default => the engine remembered in settings; anything unavailable falls
// back to the first available engine
TranslateEngine ResolveEngine(TranslateEngine engine) {
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
TempStr BuildTranslateUrlTemp(TranslateEngine engine, Str srcLang, Str dstLang, Str text) {
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
