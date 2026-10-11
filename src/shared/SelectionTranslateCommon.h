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

TempStr DefaultDestinationLanguageTemp();

// implemented by each app
TempStr OsDefaultDestinationLanguageTemp();

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

#if OS_WIN
// Parallel LANG_* ids and English names; keep in the same order.
static const WORD gPrimaryLangIds[] = {
    LANG_ENGLISH,    LANG_CHINESE,    LANG_GERMAN,    LANG_FRENCH,    LANG_SPANISH, LANG_ITALIAN,
    LANG_PORTUGUESE, LANG_RUSSIAN,    LANG_JAPANESE,  LANG_KOREAN,    LANG_ARABIC,  LANG_HINDI,
    LANG_TURKISH,    LANG_VIETNAMESE, LANG_POLISH,    LANG_UKRAINIAN, LANG_DUTCH,   LANG_THAI,
    LANG_INDONESIAN, LANG_CZECH,      LANG_SWEDISH,   LANG_ROMANIAN,  LANG_GREEK,   LANG_HEBREW,
    LANG_DANISH,     LANG_FINNISH,    LANG_NORWEGIAN, LANG_HUNGARIAN, LANG_SLOVAK,  LANG_BENGALI,
};
#endif

Str PrimaryLangIdToEnglishName(WORD primary);
