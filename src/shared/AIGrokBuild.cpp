/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Grok Build provider for the AI chat sidebar (see AIChatPanel.cpp)
// Model discovery uses Win32 pipe inspection; the remaining provider is portable.

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/DirScan.h"
#include "base/File.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Translations.h"

#include "AIChatCommon.h"
#include "AIChatPanel.h"

#include "SumatraLog.h"

static bool gGrokExecutableSearched = false;
static Str gGrokExecutablePath;

static TempStr FindGrokExecutableTemp() {
    if (gGrokExecutableSearched) {
        return gGrokExecutablePath;
    }
    gGrokExecutableSearched = true;

    StrVec candidates;
    TempStr userProfile = AIChatHomeDirTemp();
    if (len(userProfile) > 0) {
#if OS_WIN
        candidates.Append(fmt("%s\\.grok\\bin\\grok.exe", userProfile));
        candidates.Append(fmt("%s\\.local\\bin\\grok.exe", userProfile));
#else
        candidates.Append(path::JoinTemp(userProfile, StrL(".grok/bin/grok")));
        candidates.Append(path::JoinTemp(userProfile, StrL(".local/bin/grok")));
#endif
    }
    gGrokExecutablePath = str::Dup(AIChatFindExecutableTemp(candidates, StrL("grok.exe"), StrL("grok")));
    return gGrokExecutablePath;
}

static Mutex gGrokBuildLogMutex;
static AIChatLogger gGrokBuildLogger = {&gGrokBuildLogMutex, StrL("grok-build-log.txt"), StrL("grok-build")};
static bool gTriedGrokModels = false;
static StrVec gGrokModels;

static void GrokBuildLog(Str direction, Str text) {
    AIChatLog(&gGrokBuildLogger, direction, text);
}

static bool ParseGrokModelsOutput(Str output, StrVec& models) {
    bool inModels = false;
    Str rest = output;
    Str line;
    while (str::NextLine(rest, line, rest)) {
        TempStr trimmed = str::DupTemp(line);
        str::TrimWSInPlace(trimmed, str::TrimOpt::Both);
        if (str::Eq(trimmed, StrL("Available models:"))) {
            inModels = true;
            continue;
        }
        if (!inModels || !str::TrimPrefix(trimmed, StrL("* "))) {
            continue;
        }
        Str rest2 = trimmed;
        Str model = str::NextWord(rest2);
        AIChatAppendModelUnique(models, model);
    }
    return len(models) > 0;
}

// `grok models` reports the catalog available to the current Grok login.
// Cache it for this app session; old CLIs and all other failures use the built-in model.
static bool QueryGrokModels(Str exePath, StrVec& models) {
    TempStr cmdLine = fmt("%s models", QuoteCmdLineArgTemp(exePath));
    str::Builder output;
    if (!AIChatRunCapture(cmdLine, 3000, output)) {
        return false;
    }
    return ParseGrokModelsOutput(ToStr(output), models);
}

// --- Session history ---

// URL-encode a path the way Grok stores session dirs (e.g. C:\foo -> C%3A%5Cfoo)
static TempStr EncodeGrokDirTemp(Str dir) {
    str::Builder buf;
    for (int i = 0; i < dir.len; i++) {
        unsigned char c = (unsigned char)dir.s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            buf.AppendChar((char)c);
        } else {
            buf.Append(fmt("%%%02X", c));
        }
    }
    return ToStrTemp(buf);
}

static bool IsGrokSessionDirName(Str name) {
    if (len(name) == 0) {
        return false;
    }
    int n = len(name);
    if (n != 36) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        char c = name.s[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') {
                return false;
            }
        } else if ((c < '0' || c > '9') && (c < 'a' || c > 'f')) {
            return false;
        }
    }
    return true;
}

static TempStr GrokSessionsProjectDirTemp(Str dir) {
    TempStr userProfile = AIChatHomeDirTemp();
    if (len(userProfile) == 0) {
        return {};
    }
    TempStr encodedDir = EncodeGrokDirTemp(dir);
    return fmt("%s\\.grok\\sessions\\%s", userProfile, encodedDir);
}

static Str GetGrokSessionDescription(Str projectDir, Str sessionId) {
    TempStr historyPath = fmt("%s\\prompt_history.jsonl", projectDir);
    Str data = file::ReadFile(historyPath);
    if (len(data) == 0) {
        return str::Dup(StrL("(no description)"));
    }
    Str result = AIChatFindHistoryPrompt(data, sessionId, StrL("prompt"));
    str::Free(data);
    return result ? result : str::Dup(StrL("(no description)"));
}

// Scan ~/.grok/sessions/<url-encoded-dir>/ for session subdirectories
static void CollectGrokSessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr projectDir = GrokSessionsProjectDirTemp(dir);
    if (len(projectDir) == 0 || !dir::Exists(projectDir)) {
        return;
    }

    DirIter di(projectDir);
    di.includeFiles = false;
    di.includeDirs = true;
    for (DirIterEntry* de : di) {
        if (!IsGrokSessionDirName(de->name)) {
            continue;
        }

        Str desc = GetGrokSessionDescription(projectDir, de->name);
        AIChatSessionInfo si;
        si.sessionId = str::Dup(de->name);
        si.display = desc;
        si.project = str::Dup(dir);
        si.timestamp = AIChatFileTimeToMs(de->modificationTime);
        VecAppend(sessions, si);
    }

    AIChatSortSessionsByTimestampDesc(sessions);
}

static TempStr StripGrokUserQueryWrapperTemp(Str text) {
    if (len(text) == 0) {
        return {};
    }
    Str contentStart;
    if (!str::Cut(text, StrL("<user_query>"), nullptr, &contentStart)) {
        return {}; // skip injected context (user_info, rules, skills, etc.)
    }
    Str content;
    if (!str::Cut(contentStart, StrL("</user_query>"), &content, nullptr)) {
        return {};
    }
    TempStr result = str::DupTemp(content);
    str::TrimWSInPlace(result, str::TrimOpt::Both);
    return len(result) > 0 ? result : TempStr();
}

static TempStr ExtractGrokChatUserTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"type\":\"user\""))) {
        return {};
    }
    if (str::Contains(line, StrL("\"type\":\"text\",\"text\":\""))) {
        TempStr text = AIChatJsonStrTemp(line, StrL("text"));
        return StripGrokUserQueryWrapperTemp(text);
    }
    TempStr content = AIChatJsonStrTemp(line, StrL("content"));
    return StripGrokUserQueryWrapperTemp(content);
}

static void AppendGrokHistoryTools(MainWindow* win, Str line) {
    Str searchFrom;
    if (!str::Cut(line, StrL("\"tool_calls\":["), nullptr, &searchFrom)) {
        return;
    }
    while (true) {
        Str rest;
        if (!str::Cut(searchFrom, StrL("\"name\":\""), nullptr, &rest)) {
            break;
        }
        str::Builder nameBuf;
        int j = 0;
        while (j < rest.len && rest.s[j] != '"') {
            if (rest.s[j] == '\\' && j + 1 < rest.len) {
                j++;
                char c = rest.s[j];
                if (c == 'n') {
                    nameBuf.AppendChar('\n');
                } else if (c == 't') {
                    nameBuf.AppendChar('\t');
                } else if (c == '\\') {
                    nameBuf.AppendChar('\\');
                } else if (c == '"') {
                    nameBuf.AppendChar('"');
                } else {
                    nameBuf.AppendChar(c);
                }
            } else {
                nameBuf.AppendChar(rest.s[j]);
            }
            j++;
        }
        if (len(nameBuf) > 0) {
            str::Builder desc;
            desc.Append(fmt("Tool: %s", ToStr(nameBuf)));
            AIChatHistoryAddTool(win, ToStr(desc));
        }
        if (j + 1 >= rest.len) {
            break;
        }
        searchFrom = Str(rest.s + j + 1, rest.len - j - 1);
    }
}

// Load conversation history from Grok's chat_history.jsonl
static void LoadGrokSessionHistory(MainWindow* win, Str sessionId, Str dir) {
    TempStr projectDir = GrokSessionsProjectDirTemp(dir);
    if (len(projectDir) == 0) {
        return;
    }
    TempStr sessionPath = fmt("%s\\%s\\chat_history.jsonl", projectDir, sessionId);
    AIChatJsonlReader jsonl(sessionPath);
    Str line;
    while (jsonl.Next(line)) {
        TempStr userText = ExtractGrokChatUserTextTemp(line);
        if (userText) {
            AIChatHistoryAddUser(win, userText);
        } else if (str::Contains(line, StrL("\"type\":\"assistant\""))) {
            TempStr text = AIChatJsonStrTemp(line, StrL("content"));
            if (len(text) > 0) {
                AIChatHistoryAppendText(win, text);
            }
            AppendGrokHistoryTools(win, line);
            AIChatHistoryFlushBlock(win);
        }
    }
}

// --- The provider ---

struct GrokBuildProvider : AIChatProvider {
    GrokBuildProvider() {
        backend = AIChatBackend::Grok;
        logger = &gGrokBuildLogger;
        name = StrL("Grok Build");
        exeName = StrL("grok");
        virtualHost = StrL("https://sumatrapdf.grok/");
        webViewDataDirPrefix = StrL("GrokWebView");
        docUri = StrL("/AI-Chat-with-document#grok-build");
        defaultModel = StrL("grok-4.5");
        optionItems = "Low\0Medium\0High\0XHigh\0Max\0";
        optionCount = 5;
        optionDefault = 1;
        checkboxLabel = StrL("Always Approve");
    }

    TempStr TitleTemp() override { return str::DupTemp(Tr("Grok chat")); }

    TempStr NotInstalledInstructionTemp() override {
        return str::DupTemp(Tr("Install Grok Build to use this feature."));
    }

    TempStr FindExecutableTemp() override { return FindGrokExecutableTemp(); }

    void BuildModelsList(StrVec& models) override {
        models.Reset();
        if (!gTriedGrokModels) {
            gTriedGrokModels = true;
            TempStr exePath = FindGrokExecutableTemp();
            if (exePath && QueryGrokModels(exePath, gGrokModels)) {
                defaultModel = gGrokModels[0];
            }
        }
        if (len(gGrokModels) > 0) {
            for (int i = 0; i < len(gGrokModels); i++) {
                AIChatAppendModelUnique(models, gGrokModels[i]);
            }
        } else {
            AIChatAppendModelUnique(models, StrL("grok-4.5"));
        }
        AIChatAppendModels(models, gSettings->grokBuild.models);
    }

    Str GetModel() override { return gSettings->grokBuild.model; }
    void SetModel(Str model) override { str::ReplaceWithCopy(&gSettings->grokBuild.model, model); }
    int GetOption() override { return gSettings->grokBuild.effort; }
    void SetOption(int option) override { gSettings->grokBuild.effort = option; }
    bool GetFlag() override { return gSettings->grokBuild.alwaysApprove; }
    void SetFlag(bool flag) override { gSettings->grokBuild.alwaysApprove = flag; }
    Str GetBgColor() override { return gSettings->grokBuild.bgColor.s; }

    void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) override { CollectGrokSessions(dir, sessions); }

    void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) override {
        LoadGrokSessionHistory(win, sessionId, dir);
    }

    TempStr BuildCmdLineTemp(const AIChatCmdArgs& args) override {
        Str efforts[] = {StrL("low"), StrL("medium"), StrL("high"), StrL("xhigh"), StrL("max")};
        Str permsFlag = args.flag ? StrL("--always-approve") : Str{};
        TempStr rules = fmt("The user is currently reading the file: %s", args.filePath);
        if (args.isNewSession) {
            return fmt("%s -p %s --cwd %s --output-format streaming-json --model %s --effort %s %s --rules %s",
                       QuoteCmdLineArgTemp(args.exePath), QuoteCmdLineArgTemp(args.escapedInput),
                       QuoteCmdLineArgTemp(args.dir), QuoteCmdLineArgTemp(args.model), efforts[args.option], permsFlag,
                       QuoteCmdLineArgTemp(rules));
        }
        return fmt("%s -p %s --cwd %s --output-format streaming-json --model %s --effort %s %s -r %s --rules %s",
                   QuoteCmdLineArgTemp(args.exePath), QuoteCmdLineArgTemp(args.escapedInput),
                   QuoteCmdLineArgTemp(args.dir), QuoteCmdLineArgTemp(args.model), efforts[args.option], permsFlag,
                   QuoteCmdLineArgTemp(args.sessionId), QuoteCmdLineArgTemp(rules));
    }

    void ParseStreamLine(Str line, AIChatStreamCtx* ctx) override {
        TempStr eventType = AIChatJsonStrTemp(line, StrL("type"));

        if (eventType && str::Eq(eventType, StrL("thought"))) {
            TempStr thought = AIChatJsonStrTemp(line, StrL("data"));
            if (len(thought) > 0) {
                GrokBuildLog(StrL("<<< thought"), thought);
            }
        } else if (eventType && str::Eq(eventType, StrL("text"))) {
            TempStr text = AIChatJsonStrTemp(line, StrL("data"));
            if (len(text) > 0) {
                AIChatPostUpdate(ctx, AIChatUpdateType::Text, text);
            }
        } else if (eventType && str::Eq(eventType, StrL("error"))) {
            TempStr err = AIChatJsonStrTemp(line, StrL("data"));
            if (len(err) == 0) {
                err = AIChatJsonStrTemp(line, StrL("message"));
            }
            if (err) {
                AIChatPostUpdate(ctx, AIChatUpdateType::Error, err);
            }
        } else if (eventType && str::Eq(eventType, StrL("end"))) {
            GrokBuildLog(StrL("<<< end"), line);
            TempStr newSessionId = AIChatJsonStrTemp(line, StrL("sessionId"));
            if (newSessionId) {
                AIChatStreamSetSessionId(ctx, newSessionId);
            }
            AIChatPostUpdate(ctx, AIChatUpdateType::Flush, {});
            AIChatPostUpdate(ctx, AIChatUpdateType::Finished, {});
        }
    }
};

static GrokBuildProvider gGrokBuildProvider;

AIChatProvider* GetGrokBuildProvider() {
    return &gGrokBuildProvider;
}
