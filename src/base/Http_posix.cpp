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

// Capped at 1 MiB like the Windows version; the rest of the body is dropped.
constexpr i64 kHttpMemBodyCap = 1024 * 1024;
// Desktop cap. The website still stores up to 200 MiB.
constexpr i64 kHttpDownloadMaxBytes = (i64)50 << 20;
// Win32 ERROR_INVALID_PARAMETER / ERROR_FILE_TOO_LARGE / ERROR_GEN_FAILURE
constexpr DWORD kHttpErrInvalidParam = 87;
constexpr DWORD kHttpErrFileTooLarge = 223;
constexpr DWORD kHttpErrGenFailure = 31;
constexpr DWORD kHttpNoStatus = (DWORD)-1;
constexpr DWORD kHttpOk200 = 200;

TempStr HttpErrorMsgTemp(DWORD err) {
    if (err == kHttpErrFileTooLarge) {
        return str::DupTemp(StrL("The file is too large."));
    }
    if (err == kHttpErrGenFailure) {
        return str::DupTemp(StrL("Could not write the file."));
    }
    return str::DupTemp(Str(curl_easy_strerror((CURLcode)err)));
}

struct CappedBody {
    str::Builder* out;
    i64 cap;
};

static size_t AppendCapped(char* data, size_t size, size_t count, void* userData) {
    size_t n = size * count;
    auto* body = (CappedBody*)userData;
    i64 room = body->cap - (i64)len(*body->out);
    if (room > 0) {
        i64 take = (i64)n < room ? (i64)n : room;
        body->out->Append(Str(data, (int)take));
    }
    return n;
}

struct PartFile {
    FILE* file = nullptr;
    i64 got = 0;
    bool tooBig = false;
};

static size_t WritePart(char* data, size_t size, size_t count, void* userData) {
    size_t n = size * count;
    auto* pf = (PartFile*)userData;
    if (pf->got + (i64)n > kHttpDownloadMaxBytes) {
        pf->tooBig = true;
        return 0;
    }
    size_t written = fwrite(data, 1, n, pf->file);
    pf->got += (i64)written;
    return written;
}

// Shared by the "WithHeaders" calls: no redirects, extra headers in *listOut.
static CURL* NewNoRedirectCurl(Str url, Str extraHeaders, curl_slist** listOut, HttpRsp* rspOut) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        rspOut->error = kHttpErrorFailure;
        return nullptr;
    }
    SetCurlDefaults(curl, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kHttpTimeoutSecs);
    // no total limit: a transfer lives while data flows
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, kHttpTimeoutSecs);
    *listOut = AppendHeaders(nullptr, HttpNormalizeHeadersTemp(extraHeaders));
    return curl;
}

static void NoteStatus(CURL* curl, HttpRsp* rspOut) {
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    rspOut->httpStatusCode = status > 0 ? (DWORD)status : kHttpNoStatus;
}

enum class HttpMethod {
    Get,
    Post,
    Delete,
};

// Body into rspOut->data, capped. Ok: 200 for a GET, any 2xx for a POST.
static bool InMemoryRequest(const char* what, HttpMethod method, Str url, Str contentType, Str extraHeaders, Str body,
                            HttpRsp* rspOut) {
    rspOut->data.Reset();
    rspOut->error = kHttpErrorSuccess;
    rspOut->httpStatusCode = kHttpNoStatus;
    if (!OnlineURLAllowed(url)) {
        rspOut->error = kHttpErrInvalidParam;
        logf("%s: url: '%s' status: %d\n", Str(what), url, -1);
        return false;
    }

    curl_slist* list = nullptr;
    CURL* curl = NewNoRedirectCurl(url, extraHeaders, &list, rspOut);
    if (!curl) {
        return false;
    }
    bool isPost = method == HttpMethod::Post;
    if (isPost) {
        if (!str::IsEmptyOrWhiteSpace(contentType)) {
            list = curl_slist_append(list, CStrTemp(fmt("Content-Type: %s", contentType)));
        }
        // no 100-continue round trip
        list = curl_slist_append(list, "Expect:");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.s ? body.s : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)len(body));
    }
    if (method == HttpMethod::Delete) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    CappedBody cb{&rspOut->data, kHttpMemBodyCap};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendCapped);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &cb);

    CURLcode code = curl_easy_perform(curl);
    NoteStatus(curl, rspOut);
    DWORD sc = rspOut->httpStatusCode;
    // a received response is not a transport error
    rspOut->error = sc != kHttpNoStatus ? kHttpErrorSuccess : (DWORD)code;
    bool ok = isPost ? (sc >= 200 && sc < 300) : sc == kHttpOk200;
    logf("%s: url: '%s' status: %d\n", Str(what), url, (int)sc);
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);
    return ok;
}

bool HttpGetWithHeaders(Str url, Str extraHeaders, HttpRsp* rspOut) {
    return InMemoryRequest("HttpGetWithHeaders", HttpMethod::Get, url, Str(), extraHeaders, Str(), rspOut);
}

bool HttpDeleteWithHeaders(Str url, Str extraHeaders, HttpRsp* rspOut) {
    return InMemoryRequest("HttpDeleteWithHeaders", HttpMethod::Delete, url, Str(), extraHeaders, Str(), rspOut);
}

// A 3xx is an error; redirects are not followed.
bool HttpPostUrlNoRedirect(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut) {
    return InMemoryRequest("HttpPostUrlNoRedirect", HttpMethod::Post, url, contentType, extraHeaders, body, rspOut);
}

// Does not follow redirects. A body over 50 MiB deletes the partial file.
// Writes destPath.part and renames it, so a failure keeps the last good copy.
bool HttpDownloadWithHeaders(Str url, Str extraHeaders, Str destPath, HttpRsp* rspOut) {
    rspOut->data.Reset();
    rspOut->error = kHttpErrorSuccess;
    rspOut->httpStatusCode = kHttpNoStatus;
    if (!OnlineURLAllowed(url)) {
        rspOut->error = kHttpErrInvalidParam;
        logf("HttpDownloadWithHeaders: url: '%s' status: %d\n", url, -1);
        return false;
    }

    curl_slist* list = nullptr;
    CURL* curl = NewNoRedirectCurl(url, extraHeaders, &list, rspOut);
    if (!curl) {
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)kHttpDownloadMaxBytes);

    // a non-200 body lands in the part file too and is deleted below
    TempStr partPath = str::JoinTemp(destPath, StrL(".part"));
    PartFile pf;
    pf.file = fopen(CStrTemp(partPath), "wb");
    if (!pf.file) {
        rspOut->error = kHttpErrGenFailure;
        logf("HttpDownloadWithHeaders: url: '%s' status: %d\n", url, -1);
        curl_slist_free_all(list);
        curl_easy_cleanup(curl);
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &pf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WritePart);

    CURLcode code = curl_easy_perform(curl);
    NoteStatus(curl, rspOut);
    DWORD sc = rspOut->httpStatusCode;
    bool ok = code == CURLE_OK && sc == kHttpOk200;
    if (code != CURLE_OK && sc == kHttpNoStatus) {
        rspOut->error = (DWORD)code;
    }
    if (pf.tooBig || code == CURLE_FILESIZE_EXCEEDED) {
        rspOut->error = kHttpErrFileTooLarge;
        ok = false;
    }
    if (fclose(pf.file) != 0 && ok) {
        rspOut->error = kHttpErrGenFailure;
        ok = false;
    }
    if (ok && !file::RenameReplace(destPath, partPath)) {
        rspOut->error = kHttpErrGenFailure;
        ok = false;
    }
    if (!ok) {
        file::Delete(partPath);
    }
    logf("HttpDownloadWithHeaders: url: '%s' status: %d\n", url, (int)sc);
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);
    return ok;
}

#else

// No libcurl (wasm): the web transfer calls are not supported.
TempStr HttpErrorMsgTemp(DWORD) {
    return str::DupTemp(StrL("HTTP transfers are not supported on this platform."));
}

static bool NotSupported(HttpRsp* rspOut) {
    rspOut->error = kHttpErrorFailure;
    rspOut->httpStatusCode = (DWORD)-1;
    return false;
}

#if OS_WASM
#include <emscripten/emscripten.h>

// Sync XHR to the page's own origin (the site's session cookie authenticates).
// Returns the status (0: no response); the response text waits in
// Module.sumatraXhrText for WasmXhrTake.
// NOLINTNEXTLINE
EM_JS(int, WasmXhr,
      (const char* method, int methodLen, const char* url, int urlLen, const char* ctype, int ctypeLen,
       const char* body, int bodyLen),
      {
          Module.sumatraXhrText = "";
          if (typeof XMLHttpRequest == "undefined") return 0;
          try {
              var xhr = new XMLHttpRequest();
              xhr.open(UTF8ToString(method, methodLen), UTF8ToString(url, urlLen), false);
              if (ctypeLen > 0) xhr.setRequestHeader("Content-Type", UTF8ToString(ctype, ctypeLen));
              xhr.send(bodyLen > 0 ? HEAPU8.slice(body, body + bodyLen) : null);
              Module.sumatraXhrText = xhr.responseText || "";
              return xhr.status;
          } catch (e) {
              console.warn("XHR failed", e);
              return 0;
          }
      });

// NOLINTNEXTLINE
EM_JS(int, WasmXhrTextLen, (), { return lengthBytesUTF8(Module.sumatraXhrText || ""); });

// Sync GET of a binary body into MEMFS. Written whole, only on a 200 within
// maxBytes. Returns the status (0: no response); -1: over maxBytes.
// NOLINTNEXTLINE
EM_JS(int, WasmXhrDownload, (const char* url, int urlLen, const char* path, int pathLen, double maxBytes), {
    if (typeof XMLHttpRequest == "undefined") return 0;
    try {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", UTF8ToString(url, urlLen), false);
        xhr.overrideMimeType("text/plain; charset=x-user-defined");
        xhr.send(null);
        if (xhr.status != 200) return xhr.status;
        var text = xhr.responseText;
        if (text.length > maxBytes) return -1;
        var bytes = new Uint8Array(text.length);
        for (var i = 0; i < text.length; i++) {
            bytes[i] = text.charCodeAt(i) & 255;
        }
        FS.writeFile(UTF8ToString(path, pathLen), bytes);
        return 200;
    } catch (e) {
        console.warn("download failed", e);
        return 0;
    }
});

// NOLINTNEXTLINE
EM_JS(void, WasmXhrTake, (char* out, int cap), {
    stringToUTF8(Module.sumatraXhrText || "", out, cap);
    Module.sumatraXhrText = "";
});

constexpr int kHttpMemBodyCap = 1024 * 1024;
// ERROR_INVALID_PARAMETER, as Http.cpp reports a rejected URL
constexpr DWORD kWasmErrInvalidParam = 87;

// extraHeaders is ignored: no bearer key in the browser, the cookie is sent
static bool WasmRequest(const char* what, Str method, Str url, Str contentType, Str body, HttpRsp* rspOut) {
    rspOut->data.Reset();
    rspOut->error = kHttpErrorSuccess;
    rspOut->httpStatusCode = (DWORD)-1;
    if (!OnlineURLAllowed(url)) {
        rspOut->error = kWasmErrInvalidParam;
        logf("%s: url: '%s' status: %d\n", Str(what), url, -1);
        return false;
    }
    int sc = WasmXhr(method.s, len(method), url.s, len(url), contentType.s, len(contentType), body.s, len(body));
    int n = WasmXhrTextLen();
    if (n > 0 && n <= kHttpMemBodyCap) {
        char* buf = AllocArrayTemp<char>(n + 1);
        WasmXhrTake(buf, n + 1);
        rspOut->data.Append(Str(buf, n));
    }
    logf("%s: url: '%s' status: %d\n", Str(what), url, sc);
    if (sc <= 0) {
        rspOut->error = kHttpErrorFailure;
        return false;
    }
    rspOut->httpStatusCode = (DWORD)sc;
    bool isPost = str::Eq(method, StrL("POST"));
    return isPost ? (sc >= 200 && sc < 300) : sc == 200;
}

bool HttpGetWithHeaders(Str url, Str, HttpRsp* rspOut) {
    return WasmRequest("HttpGetWithHeaders", StrL("GET"), url, Str(), Str(), rspOut);
}

bool HttpDeleteWithHeaders(Str url, Str, HttpRsp* rspOut) {
    return WasmRequest("HttpDeleteWithHeaders", StrL("DELETE"), url, Str(), Str(), rspOut);
}

bool HttpPostUrlNoRedirect(Str url, Str contentType, Str, Str body, HttpRsp* rspOut) {
    return WasmRequest("HttpPostUrlNoRedirect", StrL("POST"), url, contentType, body, rspOut);
}

// ERROR_FILE_TOO_LARGE, as Http.cpp reports a body over the cap
constexpr DWORD kWasmErrFileTooLarge = 223;
constexpr i64 kWasmDownloadMaxBytes = (i64)50 << 20;

bool HttpDownloadWithHeaders(Str url, Str, Str destPath, HttpRsp* rspOut) {
    rspOut->data.Reset();
    rspOut->error = kHttpErrorSuccess;
    rspOut->httpStatusCode = (DWORD)-1;
    if (!OnlineURLAllowed(url)) {
        rspOut->error = kWasmErrInvalidParam;
        logf("HttpDownloadWithHeaders: url: '%s' status: %d\n", url, -1);
        return false;
    }
    int sc = WasmXhrDownload(url.s, len(url), destPath.s, len(destPath), (double)kWasmDownloadMaxBytes);
    logf("HttpDownloadWithHeaders: url: '%s' status: %d\n", url, sc);
    if (sc == -1) {
        rspOut->httpStatusCode = 200;
        rspOut->error = kWasmErrFileTooLarge;
        return false;
    }
    if (sc <= 0) {
        rspOut->error = kHttpErrorFailure;
        return false;
    }
    rspOut->httpStatusCode = (DWORD)sc;
    return sc == 200;
}
#else
bool HttpGetWithHeaders(Str, Str, HttpRsp* rspOut) {
    return NotSupported(rspOut);
}

bool HttpDeleteWithHeaders(Str, Str, HttpRsp* rspOut) {
    return NotSupported(rspOut);
}

bool HttpPostUrlNoRedirect(Str, Str, Str, Str, HttpRsp* rspOut) {
    return NotSupported(rspOut);
}

bool HttpDownloadWithHeaders(Str, Str, Str, HttpRsp* rspOut) {
    return NotSupported(rspOut);
}
#endif

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

// Scheme, host, and port. A prefix of the production host is a different host.
// wasm: only the page's own /api/ (the site hosting the app).
bool OnlineURLAllowed(Str url) {
#if OS_WASM
    return str::StartsWith(url, StrL("/api/"));
#endif
    struct Origin {
        Str prefix;
        Str host;
    };
    constexpr Origin kOrigins[] = {
        {StrL("https://"), StrL("web.sumatrapdfreader.org")},
        {StrL("https://"), StrL("web.sumatrapdfreader.org:443")},
        {StrL("http://"), StrL("localhost:9332")},
        {StrL("http://"), StrL("127.0.0.1:9332")},
    };
    for (const Origin& o : kOrigins) {
        if (!str::StartsWithI(url, o.prefix)) {
            continue;
        }
        Str rest = Str(url.s + len(o.prefix), len(url) - len(o.prefix));
        if (len(rest) < len(o.host) || !str::EqI(Str(rest.s, len(o.host)), o.host)) {
            continue;
        }
        // the authority ends here: nothing, a path, a query or a fragment
        if (len(rest) == len(o.host)) {
            return true;
        }
        char c = rest.s[len(o.host)];
        if (c == '/' || c == '?' || c == '#') {
            return true;
        }
    }
    return false;
}

#if defined(GPUI_HAVE_CURL) && GPUI_HAVE_CURL
struct HttpStreamState {
    CURL* curl;
    HttpStreamSink* sink;
    bool gotStatus = false;
};

static size_t WriteStream(char* bytes, size_t size, size_t count, void* context) {
    auto* state = (HttpStreamState*)context;
    size_t n = size * count;
    if (n > INT_MAX || state->sink->IsCanceled()) return 0;
    if (!state->gotStatus) {
        long status = 0;
        curl_easy_getinfo(state->curl, CURLINFO_RESPONSE_CODE, &status);
        state->sink->OnStatus((int)status);
        state->gotStatus = true;
    }
    return state->sink->OnData(Str(bytes, (int)n)) ? n : 0;
}

static int CancelStream(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return ((HttpStreamSink*)context)->IsCanceled() ? 1 : 0;
}

bool HttpPostStream(Str url, Str headers, Str body, HttpStreamSink* sink, HttpStreamCancel*) {
    if (sink->IsCanceled()) return false;
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    SetCurlDefaults(curl, url);
    constexpr long kStreamTimeoutSecs = 180;
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kStreamTimeoutSecs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kHttpTimeoutSecs);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_slist* list = AppendHeaders(nullptr, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.s);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)len(body));
    HttpStreamState state{curl, sink};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteStream);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CancelStream);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, sink);
    CURLcode code = curl_easy_perform(curl);
    if (!state.gotStatus) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        sink->OnStatus((int)status);
    }
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);
    return code == CURLE_OK && !sink->IsCanceled();
}
#else
bool HttpPostStream(Str, Str, Str, HttpStreamSink*, HttpStreamCancel*) {
    return false;
}
#endif
