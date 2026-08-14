/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Antigravity CLI provider for the AI chat sidebar (see AIChatPanel.cpp)

#include "base/Base.h"
#include "base/CmdLineArgsIter.h"
#include "base/DirScan.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "Translations.h"

#include "AIChatCommon.h"
#include "AIChatPanel.h"

static TempStr FindAntiGravityExecutableTemp() {
    StrVec candidates;
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (userProfile) {
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
    }
    TempStr res = AIChatFindExecutableTemp(candidates, WStr(L"antigravity.exe"), WStr(L"antigravity"));
    if (res) {
        logf("FindAntiGravityExecutableTemp: found %s\n", res);
        return res;
    }
    res = AIChatFindExecutableTemp(candidates, WStr(L"agy.exe"), WStr(L"agy"));
    if (res) {
        logf("FindAntiGravityExecutableTemp: found agy %s\n", res);
    } else {
        logf("FindAntiGravityExecutableTemp: not found\n");
    }
    return res;
}

bool IsAntiGravityInstalled() {
    return len(FindAntiGravityExecutableTemp()) > 0;
}

TempStr AntiGravityExecutablePathTemp() {
    return FindAntiGravityExecutableTemp();
}

static Mutex gAntiGravityLogMutex;
static AIChatLogger gAntiGravityLogger = {&gAntiGravityLogMutex, "antigravity-log.txt", "antigravity"};

// --- Session history ---

static TempStr EncodeAntiGravityDirTemp(Str dir) {
    str::Builder buf;
    for (int i = 0; i < dir.len; i++) {
        char c = dir.s[i];
        if (c == ':' || c == '\\' || c == '/' || c == '_' || c == ' ') {
            buf.AppendChar('-');
        } else {
            buf.AppendChar(c);
        }
    }
    if (len(buf) > 0 && buf.LastChar() == '-') {
        buf.RemoveLast();
    }
    return ToStrTemp(buf);
}

static TempStr ExtractUserTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"role\":\"user\""))) {
        return {};
    }
    if (str::Contains(line, StrL("\"tool_result\""))) {
        return {};
    }
    if (str::Contains(line, StrL("\"content\":\""))) {
        TempStr content = AIChatJsonStrTemp(line, "content");
        if (!str::Contains(content, StrL("<command-"))) {
            return content;
        }
    }
    if (str::Contains(line, StrL("\"content\":["))) {
        bool hasText =
            str::Contains(line, StrL("\"type\":\"text\",\"text\":\"")) || str::Contains(line, StrL("\"text\":\""));
        if (hasText) {
            TempStr text = AIChatJsonStrTemp(line, "text");
            if (!str::Contains(text, StrL("<command-"))) {
                return text;
            }
        }
    }
    return {};
}

static Str GetSessionDescription(Str sessionPath) {
    Str data = file::ReadFile(sessionPath);
    if (len(data) == 0) {
        return str::Dup(StrL("(empty)"));
    }
    Str content = data;
    Str rest = content;
    Str result;
    Str line;

    while (!result && str::NextLine(rest, line, rest)) {
        if (len(line) == 0) {
            continue;
        }
        TempStr userText = ExtractUserTextTemp(str::DupTemp(line));
        if (userText) {
            result = str::Dup(userText);
        }
    }
    str::Free(data);
    return result ? result : str::Dup(StrL("(no description)"));
}

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

        Str desc = GetSessionDescription(de->filePath);
        AIChatSessionInfo si;
        si.sessionId = str::Dup(sessionId);
        si.display = desc;
        si.project = str::Dup(dir);
        si.timestamp = AIChatFileTimeToMs(de->modificationTime);
        sessions.Append(si);
    }
}

static void CollectAntiGravitySessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeAntiGravityDirTemp(dir);

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
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeAntiGravityDirTemp(dir);

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

    Str data = file::ReadFile(sessionPath);
    if (len(data) == 0) {
        return;
    }

    Str content = data;
    Str rest = content;
    Str lineRaw;

    while (str::NextLine(rest, lineRaw, rest)) {
        if (len(lineRaw) == 0) {
            continue;
        }
        TempStr line = str::DupTemp(lineRaw);

        TempStr userText = ExtractUserTextTemp(line);
        if (userText) {
            AIChatHistoryAddUser(win, userText);
            continue;
        }
        if (!str::Contains(line, StrL("\"role\":\"assistant\"")) ||
            str::Contains(line, StrL("\"type\":\"thinking\""))) {
            continue;
        }
        if (str::Contains(line, StrL("\"type\":\"text\""))) {
            TempStr text = AIChatJsonStrTemp(line, "text");
            if (len(text) > 0) {
                AIChatHistoryAppendText(win, text);
                AIChatHistoryFlushBlock(win);
            }
            continue;
        }
        if (!str::Contains(line, StrL("\"type\":\"tool_use\""))) {
            continue;
        }
        TempStr toolName = AIChatJsonStrTemp(line, "name");
        if (!toolName) {
            continue;
        }
        TempStr fp = AIChatJsonStrTemp(line, "file_path");
        str::Builder desc;
        desc.Append(fmt("Tool: %s", toolName));
        if (fp) {
            desc.Append(fmt(" (%s)", fp));
        }
        AIChatHistoryAddTool(win, ToStr(desc));
    }

    str::Free(data);
}

// --- The provider ---

struct AntiGravityProvider : AIChatProvider {
    AntiGravityProvider() {
        backend = AIChatBackend::AntiGravity;
        logger = &gAntiGravityLogger;
        name = "Antigravity";
        exeName = "antigravity";
        virtualHost = "https://sumatrapdf.antigravity/";
        virtualHostW = L"https://sumatrapdf.antigravity/";
        webViewDataDirPrefix = "AntiGravityWebView";
        docUri = "/AI-Chat-with-document#antigravity";
        defaultModel = "gemini-3.6-flash";
        optionItems = "Low\0Medium\0High\0Max\0";
        optionCount = 4;
        optionDefault = 1;
        checkboxLabel = "Auto Approve";
        generatesSessionId = true;
        terminateOnFinish = true;
    }

    TempStr TitleTemp() override { return str::DupTemp(_TRA("Antigravity chat")); }

    TempStr NotInstalledInstructionTemp() override {
        return str::DupTemp(_TRA("Install Antigravity CLI to use this feature."));
    }

    TempStr FindExecutableTemp() override { return FindAntiGravityExecutableTemp(); }

    void BuildModelsList(StrVec& models) override {
        models.Reset();
        AIChatAppendModelUnique(models, "gemini-3.6-flash");
        AIChatAppendModelUnique(models, "gemini-3.6-pro");
        AIChatAppendModelUnique(models, "claude-3-5-sonnet");
        AIChatAppendModelUnique(models, "gpt-4o");
        Str extra = gGlobalPrefs->antiGravity.models;
        if (len(extra) > 0) {
            StrVec parts;
            Split(&parts, extra, ",", true);
            for (int i = 0; i < len(parts); i++) {
                AIChatAppendModelUnique(models, parts[i]);
            }
        }
    }

    Str GetModel() override { return gGlobalPrefs->antiGravity.model; }
    void SetModel(Str model) override { str::ReplaceWithCopy(&gGlobalPrefs->antiGravity.model, model); }
    int GetOption() override { return gGlobalPrefs->antiGravity.effort; }
    void SetOption(int option) override { gGlobalPrefs->antiGravity.effort = option; }
    bool GetFlag() override { return gGlobalPrefs->antiGravity.autoApprove; }
    void SetFlag(bool flag) override { gGlobalPrefs->antiGravity.autoApprove = flag; }
    Str GetBgColor() override { return gGlobalPrefs->antiGravity.bgColor.s; }

    void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) override {
        CollectAntiGravitySessions(dir, sessions);
    }

    void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) override {
        LoadAntiGravitySessionHistory(win, sessionId, dir);
    }

    TempStr BuildCmdLineTemp(const AIChatCmdArgs& args) override {
        // must match the option list ("Low\0Medium\0High\0Max\0")
        Str efforts[] = {"low", "medium", "high", "max"};
        int effortIdx = args.option;
        if (effortIdx < 0 || effortIdx >= 4) {
            effortIdx = 1;
        }
        Str autoApproveFlag = args.flag ? "--dangerously-skip-permissions" : "";
        // agy has no --append-system-prompt (or any system-prompt) flag, so fold
        // the file context into the user prompt itself.
        TempStr prompt = fmt("The user is currently reading the file: %s\n\n%s", args.filePath, args.escapedInput);
        // agy ignores every flag that appears after -p/--print, so -p and the
        // prompt must come last, after --model/--effort/--output-format/--conversation.
        // Otherwise --output-format stream-json is dropped and agy prints plain
        // text the stream parser can't read (response comes back empty).
        TempStr conversationArg = str::DupTemp("");
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
        AIChatLog(&gAntiGravityLogger, "<<< stream", line);
        TempStr eventName = AIChatJsonStrTemp(line, "event");
        if (!eventName) {
            return;
        }
        if (str::Eq(eventName, StrL("init"))) {
            TempStr convId = AIChatJsonStrTemp(line, "conversation_id");
            if (len(convId) > 0) {
                AIChatPostUpdate(ctx, AIChatUpdateType::SessionId, convId);
            }
            return;
        }
        if (str::Eq(eventName, StrL("step_update"))) {
            if (str::Contains(line, StrL("\"step_type\":\"agent_response\""))) {
                TempStr delta = AIChatJsonStrTemp(line, "text_delta");
                if (len(delta) > 0) {
                    AIChatPostUpdate(ctx, AIChatUpdateType::Text, delta);
                }
            } else if (str::Contains(line, StrL("\"step_type\":\"tool_use\""))) {
                TempStr toolName = AIChatJsonStrTemp(line, "tool_name");
                if (toolName) {
                    str::Builder desc;
                    desc.Append(fmt("Tool: %s", toolName));
                    AIChatPostUpdate(ctx, AIChatUpdateType::Tool, ToStr(desc));
                }
            }
            return;
        }
        if (str::Eq(eventName, StrL("result"))) {
            TempStr status = AIChatJsonStrTemp(line, "status");
            if (status && str::Eq(status, StrL("ERROR"))) {
                TempStr err = AIChatJsonStrTemp(line, "error");
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
