/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SelectionTranslateCommon.cpp and each app's SelectionTranslate.cpp ---

static const Str kSrcLangAuto = StrL("Auto");
TempStr DefaultSourceLanguageTemp();
void MaybeSaveTranslatePrefs(TranslateEngine engine, Str srcLang, Str dstLang);
bool LanguagesAreSameTemp(Str a, Str b);
Str BackendLogName(AIChatBackend backend);
bool TranslationLooksLikeError(Str text);
TempStr FormatTranslationErrorForDisplayTemp(AIChatBackend backend, Str raw);
TempStr StripTrailingSlashTemp(TempStr path);
TempStr BuildTranslationPromptTemp(Str srcLang, Str dstLang, Str text);
void ParseTranslationOutput(AIChatBackend backend, Str output, str::Builder& translationOut);
TempStr BuildGrokTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd);
TempStr BuildClaudeTranslateCmdLineTemp(Str exePath, Str prompt);
TempStr BuildCodexTranslateCmdLineTemp(Str exePath, Str prompt, Str cwd);
TempStr BuildAntiGravityTranslateCmdLineTemp(Str exePath, Str prompt);
TempStr FindBackendExecutableTemp(AIChatBackend backend);
Str BackendDisplayName(AIChatBackend backend);
// in dropdown order
static const TranslateEngine gAllEngines[] = {
    TranslateEngine::Google, TranslateEngine::DeepL, TranslateEngine::Grok,
    TranslateEngine::Claude, TranslateEngine::Codex, TranslateEngine::AntiGravity,
};
bool EngineIsAI(TranslateEngine engine);
AIChatBackend BackendFromEngine(TranslateEngine engine);
Str EngineDisplayName(TranslateEngine engine);
bool IsEngineAvailable(TranslateEngine engine);
TranslateEngine EngineFromName(Str name);
TranslateEngine ResolveEngine(TranslateEngine engine);
TempStr BuildTranslateUrlTemp(TranslateEngine engine, Str srcLang, Str dstLang, Str text);
