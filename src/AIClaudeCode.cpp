/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Claude Code provider for the AI chat sidebar (see AIChatPanel.cpp)

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/DirScan.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Translations.h"

#include "AIChatCommon.h"
#include "AIChatPanel.h"

static bool gClaudeExecutableSearched = false;
static Str gClaudeExecutablePath;

static TempStr FindClaudeExecutableTemp() {
    if (gClaudeExecutableSearched) {
        return gClaudeExecutablePath;
    }
    gClaudeExecutableSearched = true;

    StrVec candidates;
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (userProfile) {
        candidates.Append(fmt("%s\\.local\\bin\\claude.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Local\\Programs\\claude-code\\claude.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Roaming\\npm\\claude.cmd", userProfile));
    }
    gClaudeExecutablePath = str::Dup(AIChatFindExecutableTemp(candidates, WStr(L"claude.exe"), WStr(L"claude")));
    return gClaudeExecutablePath;
}

// the providers (implemented in AIClaudeCode.cpp, AIGrokBuild.cpp, AICodexBuild.cpp)
bool IsClaudeCodeInstalled() {
    return len(FindClaudeExecutableTemp()) > 0;
}

TempStr ClaudeCodeExecutablePathTemp() {
    return FindClaudeExecutableTemp();
}

static Mutex gClaudeCodeLogMutex;
static AIChatLogger gClaudeCodeLogger = {&gClaudeCodeLogMutex, StrL("claude-code-log.txt"), StrL("claude-code")};

// --- Session history ---

// Compute the encoded project dir path that Claude uses:
// E:\foo_bar -> E--foo-bar (: removed, \ -> -, _ -> -)
static TempStr EncodeClaudeDirTemp(Str dir) {
    str::Builder buf;
    for (int i = 0; i < dir.len; i++) {
        char c = dir.s[i];
        if (c == ':' || c == '\\' || c == '/' || c == '_' || c == ' ') {
            buf.AppendChar('-');
        } else {
            buf.AppendChar(c);
        }
    }
    // remove trailing dash if present
    if (len(buf) > 0 && buf.LastChar() == '-') {
        buf.RemoveLast();
    }
    return ToStrTemp(buf);
}

// Extract user message text from a JSON line.
// Handles both "content":"string" and "content":[{"type":"text","text":"..."}] formats.
static TempStr ExtractUserTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"role\":\"user\""))) {
        return {};
    }
    // skip tool_result messages (they have role:user but contain tool output, not user text)
    if (str::Contains(line, StrL("\"tool_result\""))) {
        return {};
    }
    // try string format: "content":"text"
    if (str::Contains(line, StrL("\"content\":\""))) {
        TempStr content = AIChatJsonStrTemp(line, StrL("content"));
        if (!str::Contains(content, StrL("<command-"))) {
            return content;
        }
    }
    // try array format: "content":[{"type":"text","text":"..."}]
    if (str::Contains(line, StrL("\"content\":["))) {
        // find the text field inside the first text block
        bool hasText =
            str::Contains(line, StrL("\"type\":\"text\",\"text\":\"")) || str::Contains(line, StrL("\"text\":\""));
        if (hasText) {
            TempStr text = AIChatJsonStrTemp(line, StrL("text"));
            if (!str::Contains(text, StrL("<command-"))) {
                return text;
            }
        }
    }
    return {};
}

// Read the first user message from a session JSONL as description
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

// Scan ~/.claude/projects/<encoded-dir>/ for .jsonl session files
static void CollectClaudeSessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeClaudeDirTemp(dir);
    TempStr projectDir = path::JoinTemp(userProfile, StrL(".claude"), StrL("projects"));
    projectDir = path::JoinTemp(projectDir, encodedDir);
    if (!dir::Exists(projectDir)) {
        return;
    }

    DirIter di(projectDir);
    di.includeFiles = true;
    di.includeDirs = false;
    for (DirIterEntry* de : di) {
        // session files are named <uuid>.jsonl
        if (!str::EndsWithI(de->name, StrL(".jsonl"))) {
            continue;
        }
        int nameLen = len(de->name);
        if (nameLen < 42) { // uuid (36) + .jsonl (6) = 42
            continue;
        }
        // extract session ID (remove .jsonl extension)
        TempStr sessionId = str::DupTemp(Str(de->name.s, nameLen - 6));
        Str desc = GetSessionDescription(de->filePath);

        AIChatSessionInfo si;
        si.sessionId = str::Dup(sessionId);
        si.display = desc;
        si.project = str::Dup(dir);
        si.timestamp = AIChatFileTimeToMs(de->modificationTime);
        VecAppend(sessions, si);
    }

    AIChatSortSessionsByTimestampDesc(sessions);
}

// Load conversation history from a session's JSONL file
static void LoadClaudeSessionHistory(MainWindow* win, Str sessionId, Str dir) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeClaudeDirTemp(dir);
    TempStr sessionPath = fmt("%s\\.claude\\projects\\%s\\%s.jsonl", userProfile, encodedDir, sessionId);

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

        // Session JSONL format:
        // User messages: content can be string or array
        // Assistant messages: content is always array

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
            TempStr text = AIChatJsonStrTemp(line, StrL("text"));
            if (len(text) > 0) {
                AIChatHistoryAppendText(win, text);
                AIChatHistoryFlushBlock(win);
            }
            continue;
        }
        if (!str::Contains(line, StrL("\"type\":\"tool_use\""))) {
            continue;
        }
        TempStr toolName = AIChatJsonStrTemp(line, StrL("name"));
        if (!toolName) {
            continue;
        }
        TempStr fp = AIChatJsonStrTemp(line, StrL("file_path"));
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

struct ClaudeCodeProvider : AIChatProvider {
    ClaudeCodeProvider() {
        backend = AIChatBackend::Claude;
        logger = &gClaudeCodeLogger;
        name = StrL("Claude Code");
        exeName = StrL("claude");
        virtualHost = StrL("https://sumatrapdf.claude/");
        virtualHostW = L"https://sumatrapdf.claude/";
        webViewDataDirPrefix = StrL("ClaudeWebView");
        docUri = StrL("/AI-Chat-with-document#claude-code");
        defaultModel = StrL("opus");
        optionItems = "Low\0Medium\0High\0Max\0";
        optionCount = 4;
        optionDefault = 1;
        checkboxLabel = StrL("Skip Permissions");
        generatesSessionId = true;
        // claude emits result before the process exits; don't wait for EOF
        terminateOnFinish = true;
    }

    TempStr TitleTemp() override { return str::DupTemp(_TRA("Claude chat")); }

    TempStr NotInstalledInstructionTemp() override {
        return str::DupTemp(_TRA("Install Claude Code to use this feature."));
    }

    TempStr FindExecutableTemp() override { return FindClaudeExecutableTemp(); }

    void BuildModelsList(StrVec& models) override {
        models.Reset();
        AIChatAppendModelUnique(models, StrL("default"));
        AIChatAppendModelUnique(models, StrL("best"));
        AIChatAppendModelUnique(models, StrL("sonnet"));
        AIChatAppendModelUnique(models, StrL("opus"));
        AIChatAppendModelUnique(models, StrL("haiku"));
        AIChatAppendModelUnique(models, StrL("sonnet[1m]"));
        AIChatAppendModelUnique(models, StrL("opus[1m]"));
        AIChatAppendModelUnique(models, StrL("opusplan"));
        Str extra = gSettings->claudeCode.models;
        if (len(extra) > 0) {
            StrVec parts;
            Split(&parts, extra, StrL(","), true);
            for (int i = 0; i < len(parts); i++) {
                AIChatAppendModelUnique(models, parts[i]);
            }
        }
    }

    Str GetModel() override { return gSettings->claudeCode.model; }
    void SetModel(Str model) override { str::ReplaceWithCopy(&gSettings->claudeCode.model, model); }
    int GetOption() override { return gSettings->claudeCode.effort; }
    void SetOption(int option) override { gSettings->claudeCode.effort = option; }
    bool GetFlag() override { return gSettings->claudeCode.skipPermissions; }
    void SetFlag(bool flag) override { gSettings->claudeCode.skipPermissions = flag; }
    Str GetBgColor() override { return gSettings->claudeCode.bgColor.s; }

    void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) override { CollectClaudeSessions(dir, sessions); }

    void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) override {
        LoadClaudeSessionHistory(win, sessionId, dir);
    }

    TempStr BuildCmdLineTemp(const AIChatCmdArgs& args) override {
        Str efforts[] = {StrL("low"), StrL("medium"), StrL("high"), StrL("max")};
        Str permsFlag = args.flag ? StrL("--dangerously-skip-permissions") : StrL("");
        TempStr sessionName = path::GetBaseNameTemp(args.filePath);
        TempStr sysPrompt = fmt("The user is currently reading the file: %s", args.filePath);
        if (args.isNewSession) {
            return fmt(
                "%s -p --verbose --model %s --effort %s --output-format stream-json %s --session-id %s "
                "--name %s --append-system-prompt %s %s",
                QuoteCmdLineArgTemp(args.exePath), QuoteCmdLineArgTemp(args.model), efforts[args.option], permsFlag,
                args.sessionId, QuoteCmdLineArgTemp(sessionName), QuoteCmdLineArgTemp(sysPrompt),
                QuoteCmdLineArgTemp(args.escapedInput));
        }
        return fmt(
            "%s -p --verbose --model %s --effort %s --output-format stream-json %s --resume %s "
            "--append-system-prompt %s %s",
            QuoteCmdLineArgTemp(args.exePath), QuoteCmdLineArgTemp(args.model), efforts[args.option], permsFlag,
            args.sessionId, QuoteCmdLineArgTemp(sysPrompt), QuoteCmdLineArgTemp(args.escapedInput));
    }

    void ParseStreamLine(Str line, AIChatStreamCtx* ctx) override {
        TempStr eventType = AIChatJsonStrTemp(line, StrL("type"));
        if (!eventType) {
            return;
        }
        if (str::Eq(eventType, StrL("assistant"))) {
            if (str::Contains(line, StrL("\"type\":\"text\""))) {
                TempStr text = AIChatJsonStrTemp(line, StrL("text"));
                if (len(text) > 0) {
                    AIChatPostUpdate(ctx, AIChatUpdateType::Text, text);
                }
            }
            if (!str::Contains(line, StrL("\"type\":\"tool_use\""))) {
                return;
            }
            TempStr toolName = AIChatJsonStrTemp(line, StrL("name"));
            if (!toolName) {
                return;
            }
            TempStr fp = AIChatJsonStrTemp(line, StrL("file_path"));
            TempStr cmd = AIChatJsonStrTemp(line, StrL("command"));
            TempStr pat = AIChatJsonStrTemp(line, StrL("pattern"));
            str::Builder desc;
            desc.Append(fmt("Tool: %s", toolName));
            if (fp) {
                desc.Append(fmt(" (%s)", fp));
            } else if (cmd) {
                if (len(cmd) > 60) {
                    desc.Append(fmt(" $ %.60s...", cmd));
                } else {
                    desc.Append(fmt(" $ %s", cmd));
                }
            } else if (pat) {
                desc.Append(fmt(" /%s/", pat));
            }
            AIChatPostUpdate(ctx, AIChatUpdateType::Tool, ToStr(desc));
            return;
        }
        if (str::Eq(eventType, StrL("user"))) {
            if (str::Contains(line, StrL("\"tool_use_result\""))) {
                TempStr fp = AIChatJsonStrTemp(line, StrL("filePath"));
                if (fp) {
                    str::Builder desc;
                    desc.Append(fmt("Result: %s", fp));
                    AIChatPostUpdate(ctx, AIChatUpdateType::Tool, ToStr(desc));
                }
            }
            return;
        }
        if (str::Eq(eventType, StrL("result"))) {
            TempStr sub = AIChatJsonStrTemp(line, StrL("subtype"));
            if (sub && str::Eq(sub, StrL("error"))) {
                TempStr err = AIChatJsonStrTemp(line, StrL("error"));
                if (err) {
                    AIChatPostUpdate(ctx, AIChatUpdateType::Error, err);
                }
            }
            // claude emits result before the process exits; don't wait for EOF
            if (sub &&
                (str::Eq(sub, StrL("success")) || str::Eq(sub, StrL("completion")) || str::Eq(sub, StrL("error")))) {
                AIChatPostUpdate(ctx, AIChatUpdateType::Flush, {});
                AIChatPostUpdate(ctx, AIChatUpdateType::Finished, {});
            }
        }
    }
};

static ClaudeCodeProvider gClaudeCodeProvider;

AIChatProvider* GetClaudeCodeProvider() {
    return &gClaudeCodeProvider;
}
