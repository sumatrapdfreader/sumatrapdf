/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include <sys/wait.h>

#include "base/File.h"
#include "base/Http.h"

static constexpr DWORD kHttpErrorSuccess = 0;
static constexpr DWORD kHttpErrorFailure = 1;

static void AppendShellQuoted(str::Builder* cmd, Str s) {
    cmd->AppendChar('\'');
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        if (c == '\'') {
            cmd->Append(StrL("'\\''"));
        } else {
            cmd->AppendChar(c);
        }
    }
    cmd->AppendChar('\'');
}

static bool RunShellCommand(Str command) {
    int res = system(CStrTemp(command));
    if (res < 0) {
        return false;
    }
    return WIFEXITED(res) && WEXITSTATUS(res) == 0;
}

static TempStr NewTempFilePathTemp(Str prefix) {
    TempStr path = GetTempFilePathTemp(prefix);
    if (path) {
        file::Delete(path);
    }
    return path;
}

static int ReadHttpStatusCode(Str statusPath) {
    Str status = file::ReadFile(statusPath);
    if (!status) {
        return 0;
    }
    return ParseInt(status);
}

static bool AppendCurlBase(str::Builder* cmd, Str outPath, Str statusPath) {
    (void)statusPath;
    bool ok = cmd->Append(StrL("curl --location --silent --show-error --user-agent 'SumatraPdfHTTP' --output "));
    AppendShellQuoted(cmd, outPath);
    ok = ok && cmd->Append(StrL(" --write-out '%{http_code}' "));
    ok = ok && cmd->Append(StrL(" --max-time 30 "));
    ok = ok && cmd->Append(StrL(" "));
    return ok;
}

static void AppendStatusRedirect(str::Builder* cmd, Str statusPath) {
    cmd->Append(StrL(" > "));
    AppendShellQuoted(cmd, statusPath);
}

static void AppendHeaders(str::Builder* cmd, str::Builder* headers) {
    if (!headers || len(*headers) == 0) {
        return;
    }

    Str h = ToStr(*headers);
    int start = 0;
    for (int i = 0; i <= h.len; i++) {
        if (i < h.len && h.s[i] != '\n' && h.s[i] != '\r') {
            continue;
        }
        if (i > start) {
            cmd->Append(StrL(" --header "));
            AppendShellQuoted(cmd, Str(h.s + start, i - start));
        }
        if (i < h.len && h.s[i] == '\r' && i + 1 < h.len && h.s[i + 1] == '\n') {
            i++;
        }
        start = i + 1;
    }
}

static TempStr BuildPostUrlTemp(Str server, int port, Str url) {
    Str scheme = port == 443 ? StrL("https://") : StrL("http://");
    TempStr host = str::JoinTemp(scheme, server);
    if (!((port == 443) || (port == 80))) {
        host = fmt("%s:%d", host, port);
    }
    if (url && url.s[0] == '/') {
        return str::JoinTemp(host, url);
    }
    return str::JoinTemp(host, StrL("/"), url);
}

bool HttpGet(Str urlA, HttpRsp* rspOut) {
    logf("HttpGet: url: '%s'\n", urlA);
    rspOut->data.Reset();
    rspOut->error = kHttpErrorSuccess;
    rspOut->httpStatusCode = 0;

    TempStr bodyPath = NewTempFilePathTemp(StrL("sumatra-http-body-"));
    TempStr statusPath = NewTempFilePathTemp(StrL("sumatra-http-status-"));
    if (!bodyPath || !statusPath) {
        rspOut->error = kHttpErrorFailure;
        return false;
    }

    str::Builder cmd(1024);
    bool ok = AppendCurlBase(&cmd, bodyPath, statusPath);
    AppendShellQuoted(&cmd, urlA);
    AppendStatusRedirect(&cmd, statusPath);
    ok = ok && RunShellCommand(ToStr(cmd));
    rspOut->httpStatusCode = (DWORD)ReadHttpStatusCode(statusPath);
    if (ok) {
        Str body = file::ReadFile(bodyPath);
        if (body) {
            ok = rspOut->data.Append(body);
        }
    }
    if (!ok) {
        rspOut->error = kHttpErrorFailure;
    }
    file::Delete(bodyPath);
    file::Delete(statusPath);
    return IsHttpRspOk(rspOut);
}

bool HttpGetToFile(Str urlA, Str destFilePath, const Func1<HttpProgress*>& cbProgress) {
    logf("HttpGetToFile: url: '%s', file: '%s'\n", urlA, destFilePath);
    TempStr statusPath = NewTempFilePathTemp(StrL("sumatra-http-status-"));
    if (!statusPath) {
        return false;
    }

    str::Builder cmd(1024);
    bool ok = AppendCurlBase(&cmd, destFilePath, statusPath);
    AppendShellQuoted(&cmd, urlA);
    AppendStatusRedirect(&cmd, statusPath);
    ok = ok && RunShellCommand(ToStr(cmd));
    int statusCode = ReadHttpStatusCode(statusPath);
    ok = ok && statusCode == 200;
    if (ok) {
        HttpProgress progress{};
        progress.nDownloaded = file::GetSize(destFilePath);
        cbProgress.Call(&progress);
    } else {
        file::Delete(destFilePath);
    }
    file::Delete(statusPath);
    return ok;
}

bool HttpPost(Str serverA, int port, Str urlA, str::Builder* headers, str::Builder* data) {
    TempStr bodyPath = NewTempFilePathTemp(StrL("sumatra-http-post-body-"));
    TempStr outPath = NewTempFilePathTemp(StrL("sumatra-http-post-out-"));
    TempStr statusPath = NewTempFilePathTemp(StrL("sumatra-http-status-"));
    if (!bodyPath || !outPath || !statusPath) {
        return false;
    }

    bool ok = true;
    if (data && len(*data) > 0) {
        ok = file::WriteFile(bodyPath, ToStr(*data));
    } else {
        ok = file::WriteFile(bodyPath, Str());
    }
    if (!ok) {
        goto Exit;
    }

    {
        TempStr url = BuildPostUrlTemp(serverA, port, urlA);
        str::Builder cmd(1024);
        ok = AppendCurlBase(&cmd, outPath, statusPath);
        cmd.Append(StrL(" --request POST "));
        AppendHeaders(&cmd, headers);
        cmd.Append(StrL(" --data-binary @"));
        AppendShellQuoted(&cmd, bodyPath);
        cmd.Append(StrL(" "));
        AppendShellQuoted(&cmd, url);
        AppendStatusRedirect(&cmd, statusPath);
        ok = ok && RunShellCommand(ToStr(cmd));
    }

    ok = ok && ReadHttpStatusCode(statusPath) == 200;

Exit:
    file::Delete(bodyPath);
    file::Delete(outPath);
    file::Delete(statusPath);
    return ok;
}
