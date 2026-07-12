/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Grok Build provider for the AI chat sidebar (see AIChatPanel.cpp)

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

static TempStr FindGrokExecutableTemp() {
    StrVec candidates;
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (userProfile) {
        candidates.Append(fmt("%s\\.grok\\bin\\grok.exe", userProfile));
        candidates.Append(fmt("%s\\.local\\bin\\grok.exe", userProfile));
    }
    return AIChatFindExecutableTemp(candidates, WStr(L"grok.exe"), WStr(L"grok"));
}

bool IsGrokBuildInstalled() {
    return len(FindGrokExecutableTemp()) > 0;
}

TempStr GrokBuildExecutablePathTemp() {
    return FindGrokExecutableTemp();
}

static Mutex gGrokBuildLogMutex;
static AIChatLogger gGrokBuildLogger = {&gGrokBuildLogMutex, "grok-build-log.txt", "grok-build"};

static void GrokBuildLog(Str direction, Str text) {
    AIChatLog(&gGrokBuildLogger, direction, text);
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
    if (!name) {
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
        } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

static TempStr GrokSessionsProjectDirTemp(Str dir) {
    TempStr userProfile = GetSpecialFolderTemp(CSIDL_PROFILE);
    if (!userProfile) {
        return {};
    }
    TempStr encodedDir = EncodeGrokDirTemp(dir);
    return fmt("%s\\.grok\\sessions\\%s", userProfile, encodedDir);
}

static TempStr ExtractGrokPromptFromHistoryLineTemp(Str line, Str sessionId) {
    TempStr sid = AIChatJsonStrTemp(line, "session_id");
    if (!sid || !str::Eq(sid, sessionId)) {
        return {};
    }
    return AIChatJsonStrTemp(line, "prompt");
}

static Str GetGrokSessionDescription(Str projectDir, Str sessionId) {
    TempStr historyPath = fmt("%s\\prompt_history.jsonl", projectDir);
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
        TempStr prompt = ExtractGrokPromptFromHistoryLineTemp(str::DupTemp(line), sessionId);
        if (len(prompt) > 0) {
            result = str::Dup(prompt);
        }
    }
    str::Free(data);
    return result ? result : StrL("(no description)");
}

// Scan ~/.grok/sessions/<url-encoded-dir>/ for session subdirectories
static void CollectGrokSessions(Str dir, Vec<AIChatSessionInfo>& sessions) {
    TempStr projectDir = GrokSessionsProjectDirTemp(dir);
    if (!projectDir || !dir::Exists(projectDir)) {
        return;
    }

    TempStr pattern = fmt("%s\\*", projectDir);
    WIN32_FIND_DATAW fd;
    WCHAR* patternW = CWStrTemp(pattern);
    HANDLE hFind = FindFirstFileW(patternW, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        TempStr fileName = ToUtf8Temp(fd.cFileName);
        if (str::Eq(fileName, ".") || str::Eq(fileName, "..")) {
            continue;
        }
        if (!IsGrokSessionDirName(fileName)) {
            continue;
        }

        Str desc = GetGrokSessionDescription(projectDir, fileName);
        i64 ts = AIChatFileTimeToMs(fd.ftLastWriteTime);

        AIChatSessionInfo si;
        si.sessionId = str::Dup(fileName);
        si.display = desc;
        si.project = str::Dup(dir);
        si.timestamp = ts;
        sessions.Append(si);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    AIChatSortSessionsByTimestampDesc(sessions);
}

static TempStr StripGrokUserQueryWrapperTemp(Str text) {
    if (!text) {
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
    return len(result) > 0 ? result : nullptr;
}

static TempStr ExtractGrokChatUserTextTemp(Str line) {
    if (!str::Contains(line, StrL("\"type\":\"user\""))) {
        return {};
    }
    if (str::Contains(line, StrL("\"type\":\"text\",\"text\":\""))) {
        TempStr text = AIChatJsonStrTemp(line, "text");
        return StripGrokUserQueryWrapperTemp(text);
    }
    TempStr content = AIChatJsonStrTemp(line, "content");
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
    if (!projectDir) {
        return;
    }
    TempStr sessionPath = fmt("%s\\%s\\chat_history.jsonl", projectDir, sessionId);
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
            TempStr userText = ExtractGrokChatUserTextTemp(line);
            if (userText) {
                AIChatHistoryAddUser(win, userText);
            } else if (str::Contains(line, StrL("\"type\":\"assistant\""))) {
                TempStr text = AIChatJsonStrTemp(line, "content");
                if (len(text) > 0) {
                    AIChatHistoryAppendText(win, text);
                }
                AppendGrokHistoryTools(win, line);
                AIChatHistoryFlushBlock(win);
            }
        }
    }

    str::Free(data);
}

// --- The provider ---

struct GrokBuildProvider : AIChatProvider {
    GrokBuildProvider() {
        backend = AIChatBackend::Grok;
        logger = &gGrokBuildLogger;
        name = "Grok Build";
        exeName = "grok";
        virtualHost = "https://sumatrapdf.grok/";
        virtualHostW = L"https://sumatrapdf.grok/";
        webViewDataDirPrefix = "GrokWebView";
        docUri = "/AI-Chat-with-document#grok-build";
        defaultModel = "grok-composer-2.5-fast";
        optionItems = "Low\0Medium\0High\0XHigh\0Max\0";
        optionCount = 5;
        optionDefault = 1;
        checkboxLabel = "Always Approve";
    }

    TempStr TitleTemp() override {
        return str::DupTemp(_TRA("Grok chat"));
    }

    TempStr NotInstalledInstructionTemp() override {
        return str::DupTemp(_TRA("Grok Build cli must be installed for this functionality"));
    }

    TempStr FindExecutableTemp() override {
        return FindGrokExecutableTemp();
    }

    void BuildModelsList(StrVec& models) override {
        models.Reset();
        AIChatAppendModelUnique(models, "grok-composer-2.5-fast");
        AIChatAppendModelUnique(models, "grok-build");
        Str extra = gGlobalPrefs->grokBuild.models;
        if (len(extra) > 0) {
            StrVec parts;
            Split(&parts, extra, ",", true);
            for (int i = 0; i < len(parts); i++) {
                AIChatAppendModelUnique(models, parts[i]);
            }
        }
    }

    Str GetModel() override {
        return gGlobalPrefs->grokBuild.model;
    }
    void SetModel(Str model) override {
        str::ReplaceWithCopy(&gGlobalPrefs->grokBuild.model, model);
    }
    int GetOption() override {
        return gGlobalPrefs->grokBuild.effort;
    }
    void SetOption(int option) override {
        gGlobalPrefs->grokBuild.effort = option;
    }
    bool GetFlag() override {
        return gGlobalPrefs->grokBuild.alwaysApprove;
    }
    void SetFlag(bool flag) override {
        gGlobalPrefs->grokBuild.alwaysApprove = flag;
    }
    Str GetBgColor() override {
        return gGlobalPrefs->grokBuild.bgColor;
    }

    void CollectSessions(Str dir, Vec<AIChatSessionInfo>& sessions) override {
        CollectGrokSessions(dir, sessions);
    }

    void LoadSessionHistory(MainWindow* win, Str sessionId, Str dir) override {
        LoadGrokSessionHistory(win, sessionId, dir);
    }

    TempStr BuildCmdLineTemp(const AIChatCmdArgs& args) override {
        Str efforts[] = {StrL("low"), StrL("medium"), StrL("high"), StrL("xhigh"), StrL("max")};
        Str permsFlag = args.flag ? StrL("--always-approve") : Str{};
        TempStr rules = fmt("The user is currently reading the file: %s", args.filePath);
        if (args.isNewSession) {
            return fmt(
                "\"%s\" -p \"%s\" --cwd \"%s\" --output-format streaming-json --model %s --effort %s %s --rules "
                "\"%s\"",
                args.exePath, args.escapedInput, args.dir, args.model, efforts[args.option], permsFlag, rules);
        }
        return fmt(
            "\"%s\" -p \"%s\" --cwd \"%s\" --output-format streaming-json --model %s --effort %s %s -r %s --rules "
            "\"%s\"",
            args.exePath, args.escapedInput, args.dir, args.model, efforts[args.option], permsFlag, args.sessionId,
            rules);
    }

    void ParseStreamLine(Str line, AIChatStreamCtx* ctx) override {
        TempStr eventType = AIChatJsonStrTemp(line, "type");

        if (eventType && str::Eq(eventType, "thought")) {
            TempStr thought = AIChatJsonStrTemp(line, "data");
            if (len(thought) > 0) {
                GrokBuildLog("<<< thought", thought);
            }
        } else if (eventType && str::Eq(eventType, "text")) {
            TempStr text = AIChatJsonStrTemp(line, "data");
            if (len(text) > 0) {
                AIChatPostUpdate(ctx, AIChatUpdateType::Text, text);
            }
        } else if (eventType && str::Eq(eventType, "error")) {
            TempStr err = AIChatJsonStrTemp(line, "data");
            if (!err) {
                err = AIChatJsonStrTemp(line, "message");
            }
            if (err) {
                AIChatPostUpdate(ctx, AIChatUpdateType::Error, err);
            }
        } else if (eventType && str::Eq(eventType, "end")) {
            GrokBuildLog("<<< end", line);
            TempStr newSessionId = AIChatJsonStrTemp(line, "sessionId");
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
