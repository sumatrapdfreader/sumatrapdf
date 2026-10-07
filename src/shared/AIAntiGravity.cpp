/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Antigravity CLI provider for the AI chat sidebar (see AIChatPanel.cpp)

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/DirScan.h"
#include "base/File.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Translations.h"

#include "AIChatCommon.h"
#include "AIChatPanel.h"

#include "SumatraLog.h"

static bool gAntiGravityExecutableSearched = false;
static Str gAntiGravityExecutablePath;

static TempStr FindAntiGravityExecutableTemp() {
    if (gAntiGravityExecutableSearched) {
        return gAntiGravityExecutablePath;
    }
    gAntiGravityExecutableSearched = true;

    StrVec candidates;
    TempStr userProfile = AIChatHomeDirTemp();
    if (len(userProfile) > 0) {
#if OS_WIN
        candidates.Append(fmt("%s\\AppData\\Local\\agy\\bin\\agy.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Local\\agy\\bin\\antigravity.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Roaming\\Antigravity\\bin\\agy.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Roaming\\Antigravity\\bin\\antigravity.exe", userProfile));
        candidates.Append(fmt("%s\\.gemini\\antigravity-cli\\bin\\antigravity.exe", userProfile));
        candidates.Append(fmt("%s\\.gemini\\antigravity-cli\\bin\\agy.exe", userProfile));
        candidates.Append(fmt("%s\\.local\\bin\\antigravity.exe", userProfile));
        candidates.Append(fmt("%s\\.local\\bin\\agy.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Local\\Programs\\antigravity\\antigravity.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Local\\Programs\\agy\\agy.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Roaming\\npm\\antigravity.cmd", userProfile));
        candidates.Append(fmt("%s\\AppData\\Roaming\\npm\\agy.cmd", userProfile));
#else
        candidates.Append(path::JoinTemp(userProfile, StrL(".gemini/antigravity-cli/bin/antigravity")));
        candidates.Append(path::JoinTemp(userProfile, StrL(".gemini/antigravity-cli/bin/agy")));
        candidates.Append(path::JoinTemp(userProfile, StrL(".local/bin/antigravity")));
        candidates.Append(path::JoinTemp(userProfile, StrL(".local/bin/agy")));
#endif
    }
    TempStr res = AIChatFindExecutableTemp(candidates, StrL("antigravity.exe"), StrL("antigravity"));
    if (res) {
        logf("FindAntiGravityExecutableTemp: found %s\n", res);
        gAntiGravityExecutablePath = str::Dup(res);
        return gAntiGravityExecutablePath;
    }
    res = AIChatFindExecutableTemp(candidates, StrL("agy.exe"), StrL("agy"));
    if (res) {
        logf("FindAntiGravityExecutableTemp: found agy %s\n", res);
    } else {
        logf("FindAntiGravityExecutableTemp: not found\n");
    }
    gAntiGravityExecutablePath = str::Dup(res);
    return gAntiGravityExecutablePath;
}

bool IsAntiGravityInstalled() {
    return len(FindAntiGravityExecutableTemp()) > 0;
}

TempStr AntiGravityExecutablePathTemp() {
    return FindAntiGravityExecutableTemp();
}

static Mutex gAntiGravityLogMutex;
static AIChatLogger gAntiGravityLogger = {&gAntiGravityLogMutex, StrL("antigravity-log.txt"), StrL("antigravity")};
static bool gTriedAntiGravityModels = false;
static StrVec gAntiGravityModels;

// used when `agy models` fails
static const char* kAntiGravityModels[] = {
    "gemini-3.8-flash-high",    "gemini-3.8-flash-medium", "gemini-3.8-flash-low",  "gemini-3.7-flash-high",
    "gemini-3.7-flash-medium",  "gemini-3.7-flash-low",    "gemini-3.6-flash-high", "gemini-3.6-flash-medium",
    "gemini-3.6-flash-low",     "gemini-3.1-pro-high",     "gemini-3.1-pro-low",    "claude-sonnet-4-6",
    "claude-opus-4-6-thinking", "gpt-oss-120b-medium",
};

// `agy models` prints a status line, then one "<model id>\t<display name>" line per model
static bool ParseAntiGravityModelsOutput(Str output, StrVec& models) {
    Str rest = output;
    Str line;
    while (str::NextLine(rest, line, rest)) {
        int tabIdx = str::IndexOfChar(line, '\t');
        if (tabIdx <= 0) {
            continue;
        }
        AIChatAppendModelUnique(models, Str(line.s, tabIdx));
    }
    return len(models) > 0;
}

// `agy models` reports the models available to the current login.
// Cache it for this app session; on failure use the built-in list.
static bool QueryAntiGravityModels(Str exePath, StrVec& models) {
    TempStr cmdLine = fmt("%s models", QuoteCmdLineArgTemp(exePath));
    str::Builder output;
    if (!AIChatRunCapture(cmdLine, 5000, output)) {
        return false;
    }
    AIChatLog(&gAntiGravityLogger, StrL("<<< models"), ToStr(output));
    return ParseAntiGravityModelsOutput(ToStr(output), models);
}

// --- Session history ---

static void CollectAntiGravitySessionsFromDir(Str projectDir, Str dir, Vec<AIChatSessionInfo>& sessions) {
    if (!dir::Exists(projectDir)) {
        return;
    }

    DirIter di(projectDir);
    di.includeFiles = true;
    di.includeDirs = false;
    for (DirIterEntry* de : di) {
        if (!str::EndsWithI(de->name, StrL(".jsonl"))) {
            continue;
        }
        int nameLen = len(de->name);
        if (nameLen < 42) {
            continue;
        }
        TempStr sessionId = str::DupTemp(Str(de->name.s, nameLen - 6));

        bool duplicate = false;
        for (int i = 0; i < len(sessions); i++) {
            if (str::Eq(sessions[i].sessionId, sessionId)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        Str desc = AIChatSessionDescription(de->filePath);
        AIChatSessionInfo si;
        si.sessionId = str::Dup(sessionId);
        si.display = desc;
        si.project = str::Dup(dir);
        si.timestamp = AIChatFileTimeToMs(de->modificationTime);
        VecAppend(sessions, si);
    }
}

static void CollectAntiGravitySessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr userProfile = AIChatHomeDirTemp();
    if (len(userProfile) == 0) {
        return;
    }
    TempStr encodedDir = AIChatEncodeSessionDirTemp(dir);

    // Try ~/.gemini/antigravity/projects/<encoded-dir>/
    TempStr projectDir1 = fmt("%s\\.gemini\\antigravity\\projects\\%s", userProfile, encodedDir);
    CollectAntiGravitySessionsFromDir(projectDir1, dir, sessions);

    // Try ~/.gemini/antigravity-cli/projects/<encoded-dir>/
    TempStr projectDir2 = fmt("%s\\.gemini\\antigravity-cli\\projects\\%s", userProfile, encodedDir);
    CollectAntiGravitySessionsFromDir(projectDir2, dir, sessions);

    // Try ~/.gemini/projects/<encoded-dir>/
    TempStr projectDir3 = fmt("%s\\.gemini\\projects\\%s", userProfile, encodedDir);
    CollectAntiGravitySessionsFromDir(projectDir3, dir, sessions);

    AIChatSortSessionsByTimestampDesc(sessions);
}

static void LoadAntiGravitySessionHistory(MainWindow* win, Str sessionId, Str dir) {
    TempStr userProfile = AIChatHomeDirTemp();
    if (len(userProfile) == 0) {
        return;
    }
    TempStr encodedDir = AIChatEncodeSessionDirTemp(dir);

    TempStr sessionPath = fmt("%s\\.gemini\\antigravity\\projects\\%s\\%s.jsonl", userProfile, encodedDir, sessionId);
    if (!file::Exists(sessionPath)) {
        sessionPath = fmt("%s\\.gemini\\antigravity-cli\\projects\\%s\\%s.jsonl", userProfile, encodedDir, sessionId);
    }
    if (!file::Exists(sessionPath)) {
        sessionPath = fmt("%s\\.gemini\\projects\\%s\\%s.jsonl", userProfile, encodedDir, sessionId);
    }
    if (!file::Exists(sessionPath)) {
        return;
    }

    AIChatLoadSessionHistory(win, sessionPath);
}

// --- The provider ---

struct AntiGravityProvider : AIChatProvider {
    AntiGravityProvider() {
        backend = AIChatBackend::AntiGravity;
        logger = &gAntiGravityLogger;
        name = StrL("Antigravity");
        exeName = StrL("antigravity");
        virtualHost = StrL("https://sumatrapdf.antigravity/");
        webViewDataDirPrefix = StrL("AntiGravityWebView");
        docUri = StrL("/AI-Chat-with-document#antigravity");
        defaultModel = Str(kAntiGravityDefaultModel);
        optionItems = "Low\0Medium\0High\0Max\0";
        optionCount = 4;
        optionDefault = 1;
        checkboxLabel = StrL("Auto Approve");
        generatesSessionId = true;
        terminateOnFinish = true;
    }

    TempStr TitleTemp() override { return str::DupTemp(Tr("Antigravity chat")); }

    TempStr NotInstalledInstructionTemp() override {
        return str::DupTemp(Tr("Install Antigravity CLI to use this feature."));
    }

    TempStr FindExecutableTemp() override { return FindAntiGravityExecutableTemp(); }

    void BuildModelsList(StrVec& models) override {
        models.Reset();
        if (!gTriedAntiGravityModels) {
            gTriedAntiGravityModels = true;
            TempStr exePath = FindAntiGravityExecutableTemp();
            if (exePath) {
                QueryAntiGravityModels(exePath, gAntiGravityModels);
            }
        }
        if (len(gAntiGravityModels) > 0) {
            for (int i = 0; i < len(gAntiGravityModels); i++) {
                AIChatAppendModelUnique(models, gAntiGravityModels[i]);
            }
        } else {
            for (const char* model : kAntiGravityModels) {
                AIChatAppendModelUnique(models, Str(model));
            }
        }
        Str extra = gSettings->antiGravity.models;
        if (len(extra) > 0) {
            StrVec parts;
            Split(&parts, extra, StrL(","), true);
            for (int i = 0; i < len(parts); i++) {
                AIChatAppendModelUnique(models, parts[i]);
            }
        }
    }

    Str GetModel() override { return gSettings->antiGravity.model; }
    void SetModel(Str model) override { str::ReplaceWithCopy(&gSettings->antiGravity.model, model); }
    int GetOption() override { return gSettings->antiGravity.effort; }
    void SetOption(int option) override { gSettings->antiGravity.effort = option; }
    bool GetFlag() override { return gSettings->antiGravity.autoApprove; }
    void SetFlag(bool flag) override { gSettings->antiGravity.autoApprove = flag; }
    Str GetBgColor() override { return gSettings->antiGravity.bgColor.s; }

    void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) override {
        CollectAntiGravitySessions(dir, sessions);
    }

    void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) override {
        LoadAntiGravitySessionHistory(win, sessionId, dir);
    }

    TempStr BuildCmdLineTemp(const AIChatCmdArgs& args) override {
        // must match the option list ("Low\0Medium\0High\0Max\0")
        Str efforts[] = {StrL("low"), StrL("medium"), StrL("high"), StrL("max")};
        int effortIdx = args.option;
        if (effortIdx < 0 || effortIdx >= 4) {
            effortIdx = 1;
        }
        Str autoApproveFlag = args.flag ? StrL("--dangerously-skip-permissions") : StrL("");
        // agy has no --append-system-prompt (or any system-prompt) flag, so fold
        // the file context into the user prompt itself.
        TempStr prompt = fmt("The user is currently reading the file: %s\n\n%s", args.filePath, args.escapedInput);
        // agy ignores every flag that appears after -p/--print, so -p and the
        // prompt must come last, after --model/--effort/--output-format/--conversation.
        // Otherwise --output-format stream-json is dropped and agy prints plain
        // text the stream parser can't read (response comes back empty).
        TempStr conversationArg = str::DupTemp(StrL(""));
        if (len(args.sessionId) > 0 && !str::Eq(args.sessionId, StrL("pending"))) {
            conversationArg = fmt("--conversation %s", args.sessionId);
        }
        TempStr res = fmt("%s --model %s --effort %s --output-format stream-json %s %s -p %s",
                          QuoteCmdLineArgTemp(args.exePath), QuoteCmdLineArgTemp(args.model), efforts[effortIdx],
                          autoApproveFlag, conversationArg, QuoteCmdLineArgTemp(prompt));
        logf("AntiGravity BuildCmdLineTemp: %s\n", res);
        return res;
    }

    void ParseStreamLine(Str line, AIChatStreamCtx* ctx) override {
        AIChatLog(&gAntiGravityLogger, StrL("<<< stream"), line);
        TempStr eventName = AIChatJsonStrTemp(line, StrL("event"));
        if (len(eventName) == 0) {
            return;
        }
        if (str::Eq(eventName, StrL("init"))) {
            TempStr convId = AIChatJsonStrTemp(line, StrL("conversation_id"));
            if (len(convId) > 0) {
                AIChatPostUpdate(ctx, AIChatUpdateType::SessionId, convId);
            }
            return;
        }
        if (str::Eq(eventName, StrL("step_update"))) {
            if (str::Contains(line, StrL("\"step_type\":\"agent_response\""))) {
                TempStr delta = AIChatJsonStrTemp(line, StrL("text_delta"));
                if (len(delta) > 0) {
                    AIChatPostUpdate(ctx, AIChatUpdateType::Text, delta);
                }
            } else if (str::Contains(line, StrL("\"step_type\":\"tool_use\""))) {
                TempStr toolName = AIChatJsonStrTemp(line, StrL("tool_name"));
                if (toolName) {
                    str::Builder desc;
                    desc.Append(fmt("Tool: %s", toolName));
                    AIChatPostUpdate(ctx, AIChatUpdateType::Tool, ToStr(desc));
                }
            }
            return;
        }
        if (str::Eq(eventName, StrL("result"))) {
            TempStr status = AIChatJsonStrTemp(line, StrL("status"));
            if (status && str::Eq(status, StrL("ERROR"))) {
                TempStr err = AIChatJsonStrTemp(line, StrL("error"));
                if (err) {
                    AIChatPostUpdate(ctx, AIChatUpdateType::Error, err);
                }
            }
            AIChatPostUpdate(ctx, AIChatUpdateType::Flush, {});
            AIChatPostUpdate(ctx, AIChatUpdateType::Finished, {});
        }
    }
};

static AntiGravityProvider gAntiGravityProvider;

AIChatProvider* GetAntiGravityProvider() {
    return &gAntiGravityProvider;
}
