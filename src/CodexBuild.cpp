/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// OpenAI Codex provider for the AI chat sidebar (see AIChatPanel.cpp)

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

static TempStr FindCodexExecutableTemp() {
    StrVec candidates;
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (userProfile) {
        candidates.Append(fmt("%s\\.codex\\bin\\codex.exe", userProfile));
        candidates.Append(fmt("%s\\.local\\bin\\codex.exe", userProfile));
    }
    return AIChatFindExecutableTemp(candidates, WStr(L"codex.exe"), WStr(L"codex"));
}

bool IsCodexBuildInstalled() {
    return len(FindCodexExecutableTemp()) > 0;
}

TempStr CodexBuildExecutablePathTemp() {
    return FindCodexExecutableTemp();
}

static Mutex gCodexBuildLogMutex;
static AIChatLogger gCodexBuildLogger = {&gCodexBuildLogMutex, "gpt-5.5-log.txt", "gpt-5.5"};

// --- Session history ---

static TempStr CodexSessionsRootTemp() {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return {};
    }
    return fmt("%s\\.codex\\sessions", userProfile);
}

static TempStr NormalizeCodexPathTemp(Str path) {
    if (!path) {
        return {};
    }
    if (str::StartsWith(path, "\\\\?\\")) {
        return str::DupTemp(Str(path.s + 4, path.len - 4));
    }
    return str::DupTemp(path);
}

static bool CodexPathsEqual(Str a, Str b) {
    TempStr na = NormalizeCodexPathTemp(a);
    TempStr nb = NormalizeCodexPathTemp(b);
    if (!na || !nb) {
        return false;
    }
    return path::IsSame(na, nb);
}

static bool IsCodexRolloutFileName(Str name) {
    return name && str::StartsWith(name, "rollout-") && str::EndsWithI(name, ".jsonl");
}

static TempStr ExtractCodexPromptFromHistoryLineTemp(Str line, Str sessionId) {
    TempStr sid = AIChatJsonStrTemp(line, "session_id");
    if (!sid || !str::Eq(sid, sessionId)) {
        return {};
    }
    return AIChatJsonStrTemp(line, "text");
}

static Str GetCodexSessionDescription(Str sessionId) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    TempStr historyPath = userProfile ? fmt("%s\\.codex\\history.jsonl", userProfile) : nullptr;
    if (!historyPath) {
        return StrL("(no description)");
    }
    Str data = file::ReadFile(historyPath);
    if (len(data) == 0) {
        return StrL("(no description)");
    }
    Str content = data;
    Str rest = content;
    Str result;
    Str line;

    while (!result && str::NextLine(rest, line, rest)) {
        if (len(line) == 0) {
            continue;
        }
        TempStr prompt = ExtractCodexPromptFromHistoryLineTemp(str::DupTemp(line), sessionId);
        if (len(prompt) > 0) {
            result = str::Dup(prompt);
        }
    }
    str::Free(data);
    return result ? result : StrL("(no description)");
}

static bool ParseCodexRolloutMetaLine(Str line, Str matchDir, Str* sessionIdOut) {
    if (!str::Contains(line, StrL("\"type\":\"session_meta\""))) {
        return false;
    }
    Str payload;
    str::Cut(line, StrL("\"payload\":"), nullptr, &payload);
    TempStr cwd = payload ? AIChatJsonStrTemp(payload, "cwd") : nullptr;
    TempStr id = payload ? AIChatJsonStrTemp(payload, "id") : nullptr;
    if (!cwd || !id || !CodexPathsEqual(cwd, matchDir)) {
        return false;
    }
    *sessionIdOut = str::Dup(id);
    return true;
}

static void TryAddCodexSession(Str rolloutPath, const FILETIME& ft, Str matchDir, Vec<AIChatSessionInfo>& sessions) {
    Str data = file::ReadFile(rolloutPath);
    if (len(data) == 0) {
        return;
    }
    Str content = data;
    Str firstLine, rest;
    if (!str::NextLine(content, firstLine, rest) || len(firstLine) == 0) {
        str::Free(data);
        return;
    }
    TempStr line = str::DupTemp(firstLine);
    Str sessionId;
    if (!ParseCodexRolloutMetaLine(line, matchDir, &sessionId)) {
        str::Free(data);
        return;
    }
    i64 ts = AIChatFileTimeToMs(ft);
    for (int i = 0; i < len(sessions); i++) {
        if (str::Eq(sessions[i].sessionId, sessionId)) {
            if (ts > sessions[i].timestamp) {
                sessions[i].timestamp = ts;
            }
            str::Free(sessionId);
            str::Free(data);
            return;
        }
    }
    AIChatSessionInfo si;
    si.sessionId = sessionId;
    si.display = GetCodexSessionDescription(sessionId);
    si.project = str::Dup(matchDir);
    si.timestamp = ts;
    sessions.Append(si);
    str::Free(data);
}

static TempStr FindCodexRolloutPathTemp(Str sessionId) {
    TempStr root = CodexSessionsRootTemp();
    if (!root || !sessionId) {
        return {};
    }
    TempStr suffix = fmt("%s.jsonl", sessionId);
    TempStr result = nullptr;
    TempStr yearPat = fmt("%s\\*", root);
    WIN32_FIND_DATAW fdY;
    HANDLE hY = FindFirstFileW(CWStrTemp(yearPat), &fdY);
    if (hY == INVALID_HANDLE_VALUE) {
        return {};
    }
    do {
        if ((fdY.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        TempStr year = ToUtf8Temp(fdY.cFileName);
        if (str::Eq(year, ".") || str::Eq(year, "..")) {
            continue;
        }
        TempStr monthPat = fmt("%s\\%s\\*", root, year);
        WIN32_FIND_DATAW fdM;
        HANDLE hM = FindFirstFileW(CWStrTemp(monthPat), &fdM);
        if (hM == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if ((fdM.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                continue;
            }
            TempStr month = ToUtf8Temp(fdM.cFileName);
            if (str::Eq(month, ".") || str::Eq(month, "..")) {
                continue;
            }
            TempStr dayPat = fmt("%s\\%s\\%s\\*", root, year, month);
            WIN32_FIND_DATAW fdD;
            HANDLE hD = FindFirstFileW(CWStrTemp(dayPat), &fdD);
            if (hD == INVALID_HANDLE_VALUE) {
                continue;
            }
            do {
                if (fdD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }
                TempStr name = ToUtf8Temp(fdD.cFileName);
                if (str::EndsWithI(name, suffix)) {
                    result = fmt("%s\\%s\\%s\\%s", root, year, month, name);
                    FindClose(hD);
                    FindClose(hM);
                    FindClose(hY);
                    return result;
                }
            } while (FindNextFileW(hD, &fdD));
            FindClose(hD);
        } while (FindNextFileW(hM, &fdM));
        FindClose(hM);
    } while (FindNextFileW(hY, &fdY));
    FindClose(hY);
    return result;
}

// Scan ~/.codex/sessions/YYYY/MM/DD/rollout-*.jsonl for sessions with matching cwd
static void CollectCodexSessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr root = CodexSessionsRootTemp();
    if (!root || !dir::Exists(root)) {
        return;
    }

    TempStr yearPat = fmt("%s\\*", root);
    WIN32_FIND_DATAW fdY;
    HANDLE hY = FindFirstFileW(CWStrTemp(yearPat), &fdY);
    if (hY == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if ((fdY.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        TempStr year = ToUtf8Temp(fdY.cFileName);
        if (str::Eq(year, ".") || str::Eq(year, "..")) {
            continue;
        }
        TempStr monthPat = fmt("%s\\%s\\*", root, year);
        WIN32_FIND_DATAW fdM;
        HANDLE hM = FindFirstFileW(CWStrTemp(monthPat), &fdM);
        if (hM == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if ((fdM.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                continue;
            }
            TempStr month = ToUtf8Temp(fdM.cFileName);
            if (str::Eq(month, ".") || str::Eq(month, "..")) {
                continue;
            }
            TempStr dayPat = fmt("%s\\%s\\%s\\*", root, year, month);
            WIN32_FIND_DATAW fdD;
            HANDLE hD = FindFirstFileW(CWStrTemp(dayPat), &fdD);
            if (hD == INVALID_HANDLE_VALUE) {
                continue;
            }
            do {
                if (fdD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    continue;
                }
                TempStr name = ToUtf8Temp(fdD.cFileName);
                if (!IsCodexRolloutFileName(name)) {
                    continue;
                }
                TempStr fullPath = fmt("%s\\%s\\%s\\%s", root, year, month, name);
                TryAddCodexSession(fullPath, fdD.ftLastWriteTime, dir, sessions);
            } while (FindNextFileW(hD, &fdD));
            FindClose(hD);
        } while (FindNextFileW(hM, &fdM));
        FindClose(hM);
    } while (FindNextFileW(hY, &fdY));
    FindClose(hY);

    AIChatSortSessionsByTimestampDesc(sessions);
}

static bool IsCodexInjectedUserText(Str text) {
    if (!text) {
        return true;
    }
    if (str::Contains(text, StrL("# AGENTS.md"))) {
        return true;
    }
    if (str::Contains(text, StrL("<environment_context>"))) {
        return true;
    }
    if (str::Contains(text, StrL("<turn_aborted>"))) {
        return true;
    }
    if (str::Contains(text, StrL("<INSTRUCTIONS>"))) {
        return true;
    }
    return false;
}

static TempStr ExtractCodexRolloutUserTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"type\":\"response_item\""))) {
        return {};
    }
    if (!str::Contains(line, StrL("\"role\":\"user\""))) {
        return {};
    }
    Str inputText;
    if (!str::Cut(line, StrL("\"input_text\""), nullptr, &inputText)) {
        return {};
    }
    TempStr text = AIChatJsonStrTemp(inputText, "text");
    if (!text || IsCodexInjectedUserText(text)) {
        return {};
    }
    text.len -= str::TrimWSInPlace(text, str::TrimOpt::Both);
    return len(text) > 0 ? text : nullptr;
}

static TempStr ExtractCodexRolloutAssistantTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"type\":\"response_item\""))) {
        return {};
    }
    if (!str::Contains(line, StrL("\"role\":\"assistant\""))) {
        return {};
    }
    Str outputText;
    if (!str::Cut(line, StrL("\"output_text\""), nullptr, &outputText)) {
        return {};
    }
    return AIChatJsonStrTemp(outputText, "text");
}

static void AppendCodexRolloutTools(MainWindow* win, Str line) {
    if (!str::Contains(line, StrL("\"type\":\"response_item\""))) {
        return;
    }
    TempStr name = nullptr;
    if (str::Contains(line, StrL("\"type\":\"function_call\""))) {
        name = AIChatJsonStrTemp(line, "name");
    } else if (str::Contains(line, StrL("\"type\":\"custom_tool_call\""))) {
        name = AIChatJsonStrTemp(line, "name");
    }
    if (len(name) > 0) {
        str::Builder desc;
        desc.Append(fmt("Tool: %s", name));
        AIChatHistoryAddTool(win, ToStr(desc));
    }
}

// Load conversation history from Codex rollout JSONL
static void LoadCodexSessionHistory(MainWindow* win, Str sessionId, Str) {
    TempStr sessionPath = FindCodexRolloutPathTemp(sessionId);
    if (!sessionPath || !file::Exists(sessionPath)) {
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
            TempStr userText = ExtractCodexRolloutUserTextTemp(line);
            if (userText) {
                AIChatHistoryAddUser(win, userText);
            } else {
                TempStr assistantText = ExtractCodexRolloutAssistantTextTemp(line);
                if (len(assistantText) > 0) {
                    AIChatHistoryAppendText(win, assistantText);
                    AIChatHistoryFlushBlock(win);
                } else {
                    AppendCodexRolloutTools(win, line);
                }
            }
        }
    }

    str::Free(data);
}

// --- The provider ---

struct CodexBuildProvider : AIChatProvider {
    CodexBuildProvider() {
        backend = AIChatBackend::Codex;
        logger = &gCodexBuildLogger;
        name = "OpenAI Codex";
        exeName = "codex";
        virtualHost = "https://sumatrapdf.codex/";
        virtualHostW = L"https://sumatrapdf.codex/";
        webViewDataDirPrefix = "CodexWebView";
        docUri = "/AI-Chat-with-document#openai-codex";
        defaultModel = "gpt-5.5";
        optionItems = "Read-only\0Workspace write\0Full access\0";
        optionCount = 3;
        optionDefault = 1;
        checkboxLabel = "Skip Sandbox";
    }

    TempStr TitleTemp() override {
        return str::DupTemp(_TRA("Codex chat"));
    }

    TempStr NotInstalledInstructionTemp() override {
        return str::DupTemp(_TRA("OpenAI Codex CLI must be installed for this functionality"));
    }

    TempStr FindExecutableTemp() override {
        return FindCodexExecutableTemp();
    }

    void BuildModelsList(StrVec& models) override {
        models.Reset();
        AIChatAppendModelUnique(models, "gpt-5.5");
        AIChatAppendModelUnique(models, "gpt-5.4");
        AIChatAppendModelUnique(models, "o3");
        Str extra = gGlobalPrefs->codexBuild.models;
        if (len(extra) > 0) {
            StrVec parts;
            Split(&parts, extra, ",", true);
            for (int i = 0; i < len(parts); i++) {
                AIChatAppendModelUnique(models, parts[i]);
            }
        }
    }

    Str GetModel() override {
        return gGlobalPrefs->codexBuild.model;
    }
    void SetModel(Str model) override {
        str::ReplaceWithCopy(&gGlobalPrefs->codexBuild.model, model);
    }
    int GetOption() override {
        return gGlobalPrefs->codexBuild.sandbox;
    }
    void SetOption(int option) override {
        gGlobalPrefs->codexBuild.sandbox = option;
    }
    bool GetFlag() override {
        return gGlobalPrefs->codexBuild.skipSandbox;
    }
    void SetFlag(bool flag) override {
        gGlobalPrefs->codexBuild.skipSandbox = flag;
    }
    Str GetBgColor() override {
        return gGlobalPrefs->codexBuild.bgColor;
    }

    void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) override {
        CollectCodexSessions(dir, sessions);
    }

    void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) override {
        LoadCodexSessionHistory(win, sessionId, dir);
    }

    TempStr BuildCmdLineTemp(const AIChatCmdArgs& args) override {
        Str sandboxes[] = {StrL("read-only"), StrL("workspace-write"), StrL("danger-full-access")};
        Str skipFlag = args.flag ? StrL("--dangerously-bypass-approvals-and-sandbox") : Str{};
        // the input already contains the escaped user text; codex has no
        // system-prompt flag so the file context is folded into the prompt
        TempStr prompt = fmt("The user is currently reading the file: %s\n\n%s", args.filePath, args.escapedInput);
        if (args.isNewSession) {
            if (skipFlag) {
                return fmt("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s %s %s \"%s\"", args.exePath,
                           args.dir, args.model, sandboxes[args.option], skipFlag, prompt);
            }
            return fmt("\"%s\" exec --json -C \"%s\" --skip-git-repo-check -m %s -s %s \"%s\"", args.exePath, args.dir,
                       args.model, sandboxes[args.option], prompt);
        }
        if (skipFlag) {
            return fmt("\"%s\" exec resume --json --skip-git-repo-check -m %s %s %s \"%s\"", args.exePath, args.model,
                       skipFlag, args.sessionId, prompt);
        }
        return fmt("\"%s\" exec resume --json --skip-git-repo-check -m %s %s \"%s\"", args.exePath, args.model,
                   args.sessionId, prompt);
    }

    void ParseStreamLine(Str line, AIChatStreamCtx* ctx) override {
        if (len(line) == 0 || line.s[0] != '{') {
            return;
        }
        TempStr eventType = AIChatJsonStrTemp(line, "type");

        if (eventType && str::Eq(eventType, "thread.started")) {
            TempStr threadId = AIChatJsonStrTemp(line, "thread_id");
            if (threadId) {
                AIChatStreamSetSessionId(ctx, threadId);
            }
        } else if (eventType && str::Eq(eventType, "item.completed")) {
            Str p;
            if (str::Cut(line, StrL("\"type\":\"agent_message\""), nullptr, &p)) {
                TempStr text = AIChatJsonStrTemp(p, "text");
                if (len(text) > 0) {
                    AIChatPostUpdate(ctx, AIChatUpdateType::Text, text);
                }
            } else if (str::Cut(line, StrL("\"type\":\"command_execution\""), nullptr, &p)) {
                TempStr cmd = AIChatJsonStrTemp(p, "command");
                if (len(cmd) > 0) {
                    TempStr shortCmd = ShortenStringUtf8Temp(cmd, 80);
                    str::Builder desc;
                    desc.Append(fmt("Tool: %s", shortCmd));
                    AIChatPostUpdate(ctx, AIChatUpdateType::Tool, ToStr(desc));
                    AIChatPostUpdate(ctx, AIChatUpdateType::Flush, {});
                }
            }
        } else if (eventType && str::Eq(eventType, "turn.completed")) {
            AIChatPostUpdate(ctx, AIChatUpdateType::Flush, {});
        }
    }
};

static CodexBuildProvider gCodexBuildProvider;

AIChatProvider* GetCodexBuildProvider() {
    return &gCodexBuildProvider;
}
