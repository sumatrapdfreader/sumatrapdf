/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#if defined(GPUI_HAVE_CURL) && GPUI_HAVE_CURL
#include <curl/curl.h>
#else
#include <sys/wait.h>
#endif

#include "base/File.h"
#include "base/Http.h"

// ng: was Http_mac.cpp. Linux and macOS link libcurl; wasm keeps the command
// fallback (a no-op in practice). The portable half of the API is in Http.cpp,
// outside its OS_WIN block.

static constexpr DWORD kHttpErrorSuccess = 0;
static constexpr DWORD kHttpErrorFailure = 1;

#if defined(GPUI_HAVE_CURL) && GPUI_HAVE_CURL

static constexpr long kHttpTimeoutSecs = 30;

static size_t AppendResponse(char* data, size_t size, size_t count, void* userData) {
    size_t n = size * count;
    if (n > INT_MAX) {
        return 0;
    }
    auto* out = (str::Builder*)userData;
    return out->Append(Str(data, (int)n)) ? n : 0;
}

struct DownloadState {
    FILE* file = nullptr;
    const Func1<HttpProgress*>* cbProgress = nullptr;
    i64 maxSize = -1;
    HttpProgress progress{};
};

static size_t WriteDownload(char* data, size_t size, size_t count, void* userData) {
    size_t n = size * count;
    auto* state = (DownloadState*)userData;
    if (n > INT64_MAX || (state->maxSize >= 0 && state->progress.nDownloaded > state->maxSize - (i64)n)) {
        return 0;
    }
    size_t written = fwrite(data, 1, n, state->file);
    if (written != n) {
        return written;
    }
    state->progress.nDownloaded += (i64)n;
    state->cbProgress->Call(&state->progress);
    return written;
}

static void SetCurlDefaults(CURL* curl, Str url) {
    curl_easy_setopt(curl, CURLOPT_URL, CStrTemp(url));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kHttpTimeoutSecs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SumatraPdfHTTP");
}

static curl_slist* AppendHeaders(curl_slist* list, Str headers) {
    int start = 0;
    for (int i = 0; i <= len(headers); i++) {
        if (i < len(headers) && headers.s[i] != '\n' && headers.s[i] != '\r') {
            continue;
        }
        if (i > start) {
            list = curl_slist_append(list, CStrTemp(Str(headers.s + start, i - start)));
        }
        if (i < len(headers) && headers.s[i] == '\r' && i + 1 < len(headers) && headers.s[i + 1] == '\n') {
            i++;
        }
        start = i + 1;
    }
    return list;
}

static bool PerformRequest(CURL* curl, HttpRsp* rspOut) {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rspOut->data);
    CURLcode code = curl_easy_perform(curl);
    rspOut->error = (DWORD)code;
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    rspOut->httpStatusCode = (DWORD)status;
    return code == CURLE_OK;
}

static bool PostUrl(Str url, Str headers, Str body, HttpRsp* rspOut) {
    rspOut->data.Reset();
    rspOut->error = kHttpErrorSuccess;
    rspOut->httpStatusCode = 0;
    CURL* curl = curl_easy_init();
    if (!curl) {
        rspOut->error = kHttpErrorFailure;
        return false;
    }

    SetCurlDefaults(curl, url);
    curl_slist* headerList = AppendHeaders(nullptr, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.s ? body.s : "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)len(body));
    bool ok = PerformRequest(curl, rspOut);
    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return ok && IsHttpRspOk(rspOut);
}

static TempStr BuildPostUrlTemp(Str server, int port, Str url) {
    Str scheme = port == 443 ? StrL("https://") : StrL("http://");
    TempStr host = str::JoinTemp(scheme, server);
    if (port != 443 && port != 80) {
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
    CURL* curl = curl_easy_init();
    if (!curl) {
        rspOut->error = kHttpErrorFailure;
        return false;
    }
    SetCurlDefaults(curl, urlA);
    bool ok = PerformRequest(curl, rspOut);
    curl_easy_cleanup(curl);
    return ok && IsHttpRspOk(rspOut);
}

bool HttpGetToFile(Str urlA, Str destFilePath, const Func1<HttpProgress*>& cbProgress, i64 maxSize) {
    logf("HttpGetToFile: url: '%s', file: '%s'\n", urlA, destFilePath);
    FILE* file = fopen(CStrTemp(destFilePath), "wb");
    if (!file) {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(file);
        file::Delete(destFilePath);
        return false;
    }

    DownloadState state{file, &cbProgress, maxSize};
    SetCurlDefaults(curl, urlA);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteDownload);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    bool ok = fclose(file) == 0 && code == CURLE_OK && status == 200;
    if (!ok) {
        file::Delete(destFilePath);
    }
    return ok;
}

bool HttpPost(Str serverA, int port, Str urlA, str::Builder* headers, str::Builder* data) {
    HttpRsp rsp;
    Str headerData = headers ? ToStr(*headers) : Str();
    Str body = data ? ToStr(*data) : Str();
    return PostUrl(BuildPostUrlTemp(serverA, port, urlA), headerData, body, &rsp) && rsp.httpStatusCode == 200;
}

bool HttpPostUrl(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut) {
    str::Builder headers;
    if (len(contentType) > 0) {
        headers.Append(fmt("Content-Type: %s", contentType));
    }
    TempStr extra = HttpNormalizeHeadersTemp(extraHeaders);
    if (len(extra) > 0) {
        if (len(headers) > 0) {
            headers.Append(StrL("\r\n"));
        }
        headers.Append(extra);
    }
    return PostUrl(url, ToStr(headers), body ? body : Str(), rspOut);
}

#else

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
    if (len(status) == 0) {
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
    if (len(bodyPath) == 0 || len(statusPath) == 0) {
        rspOut->error = kHttpErrorFailure;
        return false;
    }

    str::Builder cmd;
    str::BuilderReserve(cmd, 1024);
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

bool HttpGetToFile(Str urlA, Str destFilePath, const Func1<HttpProgress*>& cbProgress, i64 maxSize) {
    logf("HttpGetToFile: url: '%s', file: '%s'\n", urlA, destFilePath);
    TempStr statusPath = NewTempFilePathTemp(StrL("sumatra-http-status-"));
    if (len(statusPath) == 0) {
        return false;
    }

    str::Builder cmd;
    str::BuilderReserve(cmd, 1024);
    bool ok = AppendCurlBase(&cmd, destFilePath, statusPath);
    if (maxSize >= 0) {
        cmd.Append(fmt(" --max-filesize %lld ", maxSize));
    }
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
    if (len(bodyPath) == 0 || len(outPath) == 0 || len(statusPath) == 0) {
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
        str::Builder cmd;
        str::BuilderReserve(cmd, 1024);
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

// POST body to url with an explicit Content-Type and optional extra headers.
// Blocking; rspOut carries the status code and the response body.
bool HttpPostUrl(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut) {
    rspOut->data.Reset();
    rspOut->error = kHttpErrorSuccess;
    rspOut->httpStatusCode = 0;

    TempStr bodyPath = NewTempFilePathTemp(StrL("sumatra-http-post-body-"));
    TempStr outPath = NewTempFilePathTemp(StrL("sumatra-http-post-out-"));
    TempStr statusPath = NewTempFilePathTemp(StrL("sumatra-http-status-"));
    if (len(bodyPath) == 0 || len(outPath) == 0 || len(statusPath) == 0) {
        rspOut->error = kHttpErrorFailure;
        return false;
    }

    bool ok = file::WriteFile(bodyPath, body ? body : Str());
    if (ok) {
        str::Builder cmd;
        str::BuilderReserve(cmd, 1024);
        ok = AppendCurlBase(&cmd, outPath, statusPath);
        cmd.Append(StrL(" --request POST "));
        if (len(contentType) > 0) {
            cmd.Append(StrL(" --header "));
            AppendShellQuoted(&cmd, str::JoinTemp(StrL("Content-Type: "), contentType));
        }
        str::Builder headers;
        headers.Append(HttpNormalizeHeadersTemp(extraHeaders));
        AppendHeaders(&cmd, &headers);
        cmd.Append(StrL(" --data-binary @"));
        AppendShellQuoted(&cmd, bodyPath);
        cmd.Append(StrL(" "));
        AppendShellQuoted(&cmd, url);
        AppendStatusRedirect(&cmd, statusPath);
        ok = ok && RunShellCommand(ToStr(cmd));
    }
    rspOut->httpStatusCode = (DWORD)ReadHttpStatusCode(statusPath);
    if (ok) {
        Str out = file::ReadFile(outPath);
        if (out) {
            ok = rspOut->data.Append(out);
        }
    }
    if (!ok) {
        rspOut->error = kHttpErrorFailure;
    }
    file::Delete(bodyPath);
    file::Delete(outPath);
    file::Delete(statusPath);
    return IsHttpRspOk(rspOut);
}

#endif
