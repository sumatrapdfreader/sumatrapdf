/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/CmdLineArgs.h"
#include "base/UITask.h"
#include "EmbeddedResources.h"
#if OS_POSIX && !OS_WASM
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
extern char** environ;
#endif
#include "PDFScript.h"

static StrVec gScriptNames;
static StrVec gScriptSources;

static void LoadPDFScripts() {
    static bool loaded = false;
    if (loaded) return;
    loaded = true;
    int size = 0;
    u8* data = GetEmbeddedFileData(StrL("scripts.js"), &size);
    if (!data) return;
    StrVec sources;
    Split(&sources, Str((char*)data, size), StrL("// ---"));
    free(data);

    const Str prefix = StrL("// Info:");
    for (Str source : sources) {
        Str line;
        Str rest = source;
        while (str::NextLine(rest, line, rest) && str::IsEmptyOrWhiteSpace(line)) {
            source = rest;
        }
        gScriptSources.Append(source);

        Str name;
        rest = source;
        while (str::NextLine(rest, line, rest)) {
            line = str::DupTemp(line);
            str::TrimWSInPlace(line, str::TrimOpt::Both);
            if (!str::StartsWith(line, prefix)) continue;
            name = Str(line.s + len(prefix), len(line) - len(prefix));
            str::TrimWSInPlace(name, str::TrimOpt::Both);
            if (str::EndsWith(name, StrL("."))) name.len--;
            break;
        }
        if (len(name) == 0) name = fmt("Script %d", len(gScriptNames) + 1);
        gScriptNames.Append(name);
    }
}

int PDFScriptCount() {
    LoadPDFScripts();
    return len(gScriptSources);
}

Str PDFScriptName(int index) {
    return index >= 0 && index < PDFScriptCount() ? gScriptNames[index] : Str();
}

Str PDFScriptSource(int index) {
    return index >= 0 && index < PDFScriptCount() ? gScriptSources[index] : Str();
}

PDFScriptResult::~PDFScriptResult() {
    str::Free(output);
}

TempStr PDFScriptOutputPath(Str input) {
    if (len(input) == 0) return {};
    Str base = path::GetPathNoExtTemp(input);
    Str ext = path::GetExtTemp(input);
    for (int i = 1;; i++) {
        TempStr output = fmt("%s.%d%s", base, i, ext);
        if (!file::Exists(output)) return output;
    }
}

static TempStr ExpandScriptArg(Str value, Str input, Str output) {
    TempStr expanded = str::ReplaceTemp(value, StrL("<INPUT>"), input);
    expanded = str::ReplaceTemp(expanded, StrL("<OUTPUT>"), output);
    expanded = str::ReplaceTemp(expanded, StrL("<DIR>"), path::GetDirTemp(input));
    return str::ReplaceTemp(expanded, StrL("<BASE>"), path::GetBaseNameTemp(path::GetPathNoExtTemp(input)));
}

void GetPDFScriptArgs(Str script, Str input, StrVec& values, StrVec& hints) {
    values.Reset();
    hints.Reset();
    Str output = PDFScriptOutputPath(input);
    const Str prefix = StrL("// Arg:");
    Str line;
    Str rest = script;
    while (str::NextLine(rest, line, rest)) {
        line = str::DupTemp(line);
        str::TrimWSInPlace(line, str::TrimOpt::Both);
        if (!str::StartsWith(line, prefix)) continue;
        Str spec(line.s + len(prefix), len(line) - len(prefix));
        Str value;
        Str hint;
        if (!str::Cut(spec, StrL(" : "), &value, &hint)) hint = spec;
        str::TrimWSInPlace(value, str::TrimOpt::Both);
        str::TrimWSInPlace(hint, str::TrimOpt::Both);
        values.Append(ExpandScriptArg(value, input, output));
        hints.Append(hint);
    }
}

// Keep path quoting in JavaScript: "<INPUT>" also works for Windows backslashes and quotes.
static TempStr ScriptPathTemp(Str path) {
    str::Builder out;
    for (int i = 0; i < len(path); i++) {
        char c = path.s[i];
        switch (c) {
            case '\\':
                out.Append(StrL("\\\\"));
                break;
            case '"':
                out.Append(StrL("\\\""));
                break;
            case '\'':
                out.Append(StrL("\\'"));
                break;
            case '\n':
                out.Append(StrL("\\n"));
                break;
            case '\r':
                out.Append(StrL("\\r"));
                break;
            default:
                out.AppendChar(c);
                break;
        }
    }
    return str::DupTemp(ToStr(out));
}

TempStr ExpandPDFScript(Str script, const StrVec& args) {
    Str input = len(args) > 0 ? args[0] : Str();
    Str output = len(args) > 1 ? args[1] : Str();
    TempStr expanded = str::ReplaceTemp(script, StrL("<INPUT>"), ScriptPathTemp(input));
    return str::ReplaceTemp(expanded, StrL("<OUTPUT>"), ScriptPathTemp(output));
}

struct PDFScriptTask {
    Str script;
    Str exe;
    StrVec args;
    Func1<PDFScriptResult*> onDone;
    PDFScriptResult* result = new PDFScriptResult();
    ~PDFScriptTask() {
        str::Free(script);
        str::Free(exe);
        delete result;
    }
};

// A child process isolates MuJS and its process-wide stdout/stderr from the viewer.
static int RunScriptChild(Str exe, Str scriptPath, Str outputPath, const StrVec& args) {
#if OS_WIN
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE output = CreateFileW(CWStrTemp(outputPath), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input =
        CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    if (output == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE) {
        if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        return -1;
    }
    str::Builder cmd;
    cmd.Append(fmt("%s run %s", QuoteCmdLineArgTemp(exe), QuoteCmdLineArgTemp(scriptPath)));
    for (Str arg : args) {
        cmd.Append(fmt(" %s", QuoteCmdLineArgTemp(arg.s ? arg : StrL(""))));
    }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = input;
    si.hStdOutput = si.hStdError = output;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(CWStrTemp(exe), CWStrTemp(ToStr(cmd)), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                             nullptr, &si, &pi);
    CloseHandle(input);
    CloseHandle(output);
    if (!ok) return -1;
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return (int)code;
#elif OS_POSIX && !OS_WASM
    StrVec argvStrings;
    argvStrings.Append(exe);
    argvStrings.Append(StrL("run"));
    argvStrings.Append(scriptPath);
    for (Str arg : args) argvStrings.Append(arg);
    Vec<char*> argv;
    for (Str arg : argvStrings) VecAppend(argv, CStrTemp(arg.s ? arg : StrL("")));
    VecAppend(argv, (char*)nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, CStrTemp(outputPath), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    pid_t pid = 0;
    int err = posix_spawn(&pid, CStrTemp(exe), &actions, nullptr, argv.els, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (err) return -1;
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#else
    return -1;
#endif
}

static void FinishPDFScript(PDFScriptTask* task) {
    PDFScriptResult* result = task->result;
    task->result = nullptr;
    task->onDone.Call(result);
    delete task;
}

static void RunPDFScriptTask(PDFScriptTask* task) {
    TempStr scriptPath = GetTempFilePathTemp(StrL("pdf-script"));
    TempStr outputPath = GetTempFilePathTemp(StrL("pdf-output"));
    if (scriptPath && outputPath && file::WriteFile(scriptPath, task->script)) {
        task->result->exitCode = RunScriptChild(task->exe, scriptPath, outputPath, task->args);
        task->result->output = file::ReadFile(outputPath);
    }
    if (scriptPath) file::Delete(scriptPath);
    if (outputPath) file::Delete(outputPath);
    if (task->result->exitCode != 0) {
        str::Builder out;
        out.Append(task->result->output);
        out.Append(fmt("\nScript exited with code %d.\n", task->result->exitCode));
        str::ReplaceWithCopy(&task->result->output, ToStr(out));
    }
    uitask::Post(MkFunc0(FinishPDFScript, task));
}

void StartPDFScript(Str script, const StrVec& args, const Func1<PDFScriptResult*>& onDone) {
    auto* task = new PDFScriptTask();
    task->script = str::Dup(ExpandPDFScript(script, args));
    task->exe = str::Dup(GetSelfExePathTemp());
    for (Str arg : args) task->args.Append(arg);
    task->onDone = onDone;
    ThreadHandle thread = StartThread(MkFunc0(RunPDFScriptTask, task), StrL("PDFScript"));
    if (!thread) {
        task->result->output = str::Dup(StrL("Could not start script runner."));
        uitask::Post(MkFunc0(FinishPDFScript, task));
        return;
    }
    SafeCloseThreadHandle(&thread);
}
