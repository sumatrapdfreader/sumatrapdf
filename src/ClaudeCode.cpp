/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Claude Code provider for the AI chat sidebar (see AIChatPanel.cpp)

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "Translations.h"

#include "AIChatCommon.h"
#include "AIChatPanel.h"

static TempStr FindClaudeExecutableTemp() {
    StrVec candidates;
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (userProfile) {
        candidates.Append(fmt("%s\\.local\\bin\\claude.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Local\\Programs\\claude-code\\claude.exe", userProfile));
        candidates.Append(fmt("%s\\AppData\\Roaming\\npm\\claude.cmd", userProfile));
    }
    return AIChatFindExecutableTemp(candidates, WStr(L"claude.exe"), WStr(L"claude"));
}

bool IsClaudeCodeInstalled() {
    return len(FindClaudeExecutableTemp()) > 0;
}

TempStr ClaudeCodeExecutablePathTemp() {
    return FindClaudeExecutableTemp();
}

static Mutex gClaudeCodeLogMutex;
static AIChatLogger gClaudeCodeLogger = {&gClaudeCodeLogMutex, "claude-code-log.txt", "claude-code"};

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
        TempStr content = AIChatJsonStrTemp(line, "content");
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
            TempStr text = AIChatJsonStrTemp(line, "text");
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
        return StrL("(empty)");
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
    return result ? result : StrL("(no description)");
}

// Scan ~/.claude/projects/<encoded-dir>/ for .jsonl session files
static void CollectClaudeSessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return;
    }
    TempStr encodedDir = EncodeClaudeDirTemp(dir);
    TempStr projectDir = fmt("%s\\.claude\\projects\\%s", userProfile, encodedDir);

    // scan for *.jsonl files in the project directory
    TempStr pattern = fmt("%s\\*.jsonl", projectDir);
    WIN32_FIND_DATAW fd;
    WCHAR* patternW = CWStrTemp(pattern);
    HANDLE hFind = FindFirstFileW(patternW, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        TempStr fileName = ToUtf8Temp(fd.cFileName);
        // session files are named <uuid>.jsonl
        int nameLen = len(fileName);
        if (nameLen < 42) { // uuid (36) + .jsonl (6) = 42
            continue;
        }
        // extract session ID (remove .jsonl extension)
        TempStr sessionId = str::DupTemp(Str(fileName.s, nameLen - 6));

        TempStr fullPath = fmt("%s\\%s", projectDir, fileName);
        Str desc = GetSessionDescription(fullPath);

        AIChatSessionInfo si;
        si.sessionId = str::Dup(sessionId);
        si.display = desc;
        si.project = str::Dup(dir);
        si.timestamp = AIChatFileTimeToMs(fd.ftLastWriteTime);
        sessions.Append(si);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

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
        if (len(lineRaw) > 0) {
            TempStr line = str::DupTemp(lineRaw);

            // Session JSONL format:
            // User messages: content can be string or array
            // Assistant messages: content is always array

            TempStr userText = ExtractUserTextTemp(line);
            if (userText) {
                AIChatHistoryAddUser(win, userText);
            } else if (str::Contains(line, StrL("\"role\":\"assistant\""))) {
                // assistant message
                if (str::Contains(line, StrL("\"type\":\"thinking\""))) {
                    // skip thinking blocks
                } else if (str::Contains(line, StrL("\"type\":\"text\""))) {
                    TempStr text = AIChatJsonStrTemp(line, "text");
                    if (len(text) > 0) {
                        AIChatHistoryAppendText(win, text);
                        AIChatHistoryFlushBlock(win);
                    }
                } else if (str::Contains(line, StrL("\"type\":\"tool_use\""))) {
                    TempStr toolName = AIChatJsonStrTemp(line, "name");
                    if (toolName) {
                        TempStr fp = AIChatJsonStrTemp(line, "file_path");
                        str::Builder desc;
                        desc.Append(fmt("Tool: %s", toolName));
                        if (fp) {
                            desc.Append(fmt(" (%s)", fp));
                        }
                        AIChatHistoryAddTool(win, ToStr(desc));
                    }
                }
            }
        }
    }

    str::Free(data);
}

// --- The provider ---

struct ClaudeCodeProvider : AIChatProvider {
    ClaudeCodeProvider() {
        backend = AIChatBackend::Claude;
        logger = &gClaudeCodeLogger;
        name = "Claude Code";
        exeName = "claude";
        virtualHost = "https://sumatrapdf.claude/";
        virtualHostW = L"https://sumatrapdf.claude/";
        webViewDataDirPrefix = "ClaudeWebView";
        docUri = "/AI-Chat-with-document#claude-code";
        defaultModel = "opus";
        optionItems = "Low\0Medium\0High\0Max\0";
        optionCount = 4;
        optionDefault = 1;
        checkboxLabel = "Skip Permissions";
        generatesSessionId = true;
        // claude emits result before the process exits; don't wait for EOF
        terminateOnFinish = true;
    }

    TempStr TitleTemp() override {
        return str::DupTemp(_TRA("Claude chat"));
    }

    TempStr NotInstalledInstructionTemp() override {
        return str::DupTemp(_TRA("Claude Code cli must be installed for this functionality"));
    }

    TempStr FindExecutableTemp() override {
        return FindClaudeExecutableTemp();
    }

    void BuildModelsList(StrVec& models) override {
        models.Reset();
        AIChatAppendModelUnique(models, "sonnet");
        AIChatAppendModelUnique(models, "opus");
        AIChatAppendModelUnique(models, "haiku");
        Str extra = gGlobalPrefs->claudeCode.models;
        if (len(extra) > 0) {
            StrVec parts;
            Split(&parts, extra, ",", true);
            for (int i = 0; i < len(parts); i++) {
                AIChatAppendModelUnique(models, parts[i]);
            }
        }
    }

    Str GetModel() override {
        return gGlobalPrefs->claudeCode.model;
    }
    void SetModel(Str model) override {
        str::ReplaceWithCopy(&gGlobalPrefs->claudeCode.model, model);
    }
    int GetOption() override {
        return gGlobalPrefs->claudeCode.effort;
    }
    void SetOption(int option) override {
        gGlobalPrefs->claudeCode.effort = option;
    }
    bool GetFlag() override {
        return gGlobalPrefs->claudeCode.skipPermissions;
    }
    void SetFlag(bool flag) override {
        gGlobalPrefs->claudeCode.skipPermissions = flag;
    }
    Str GetBgColor() override {
        return gGlobalPrefs->claudeCode.bgColor;
    }

    void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) override {
        CollectClaudeSessions(dir, sessions);
    }

    void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) override {
        LoadClaudeSessionHistory(win, sessionId, dir);
    }

    TempStr BuildCmdLineTemp(const AIChatCmdArgs& args) override {
        Str efforts[] = {"low", "medium", "high", "max"};
        Str permsFlag = args.flag ? "--dangerously-skip-permissions" : "";
        TempStr sessionName = path::GetBaseNameTemp(args.filePath);
        if (args.isNewSession) {
            return fmt(
                "\"%s\" -p --verbose --model %s --effort %s --output-format stream-json %s --session-id %s "
                "--name \"%s\" "
                "--append-system-prompt \"The user is currently reading the file: %s\" "
                "\"%s\"",
                args.exePath, args.model, efforts[args.option], permsFlag, args.sessionId, sessionName, args.filePath,
                args.escapedInput);
        }
        return fmt(
            "\"%s\" -p --verbose --model %s --effort %s --output-format stream-json %s --resume %s "
            "--append-system-prompt \"The user is currently reading the file: %s\" "
            "\"%s\"",
            args.exePath, args.model, efforts[args.option], permsFlag, args.sessionId, args.filePath,
            args.escapedInput);
    }

    void ParseStreamLine(Str line, AIChatStreamCtx* ctx) override {
        TempStr eventType = AIChatJsonStrTemp(line, "type");

        if (eventType && str::Eq(eventType, "assistant")) {
            if (str::Contains(line, StrL("\"type\":\"text\""))) {
                TempStr text = AIChatJsonStrTemp(line, "text");
                if (len(text) > 0) {
                    AIChatPostUpdate(ctx, AIChatUpdateType::Text, text);
                }
            }
            if (str::Contains(line, StrL("\"type\":\"tool_use\""))) {
                TempStr toolName = AIChatJsonStrTemp(line, "name");
                if (toolName) {
                    TempStr fp = AIChatJsonStrTemp(line, "file_path");
                    TempStr cmd = AIChatJsonStrTemp(line, "command");
                    TempStr pat = AIChatJsonStrTemp(line, "pattern");
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
                }
            }
        } else if (eventType && str::Eq(eventType, "user")) {
            if (str::Contains(line, StrL("\"tool_use_result\""))) {
                TempStr fp = AIChatJsonStrTemp(line, "filePath");
                if (fp) {
                    str::Builder desc;
                    desc.Append(fmt("Result: %s", fp));
                    AIChatPostUpdate(ctx, AIChatUpdateType::Tool, ToStr(desc));
                }
            }
        } else if (eventType && str::Eq(eventType, "result")) {
            TempStr sub = AIChatJsonStrTemp(line, "subtype");
            if (sub && str::Eq(sub, "error")) {
                TempStr err = AIChatJsonStrTemp(line, "error");
                if (err) {
                    AIChatPostUpdate(ctx, AIChatUpdateType::Error, err);
                }
            }
            // claude emits result before the process exits; don't wait for EOF
            if (sub && (str::Eq(sub, "success") || str::Eq(sub, "completion") || str::Eq(sub, "error"))) {
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
