/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#if OS_WIN
#include "base/Win.h"
#endif
#include "base/Http.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/JsonParser.h"
#include "base/UITask.h"

#include "Settings.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "Translations.h"
#include "ContentHash.h"
#include "SumatraPDFWebCommon.h"

// The non-UI half of "Link SumatraPDF Web": the key check, the file list,
// uploads and downloads. Each runs on a worker thread and posts its result
// to the UI thread, where the app's dialog (SumatraPDFWebDialog.cpp) shows it.
//
//   dialog --> StartOnline*() --> thread: HTTP --> uitask --> onDone(result)

// Win32 error codes Http.cpp reports; the values are fixed, so posix can use them too
constexpr DWORD kErrInvalidParam = 87;
constexpr DWORD kErrFileTooLarge = 223;
constexpr DWORD kErrReadFault = 30;
#if OS_WIN
static_assert(kErrInvalidParam == ERROR_INVALID_PARAMETER);
static_assert(kErrFileTooLarge == ERROR_FILE_TOO_LARGE);
static_assert(kErrReadFault == ERROR_READ_FAULT);
#endif

constexpr Str kOnlineProdOrigin = StrL("https://web.sumatrapdfreader.org");
constexpr Str kOnlineLocalOrigin = StrL("http://localhost:9332");
constexpr Str kOnlineLoopbackOrigin = StrL("http://127.0.0.1:9332");

// 0: nothing in flight. A finished request whose id does not match is dropped.
static u32 gOnlineReqSeq = 0;

// The server's validUploadName: empty, "." and "..", / \ and control characters.
// The server stores files by content, so the name is only metadata; older servers
// also refused : " ; (": and " can't be in a Windows file name anyway).
bool ValidUploadName(Str name) {
    if (len(name) == 0 || str::Eq(name, StrL(".")) || str::Eq(name, StrL(".."))) {
        return false;
    }
    for (int i = 0; i < len(name); i++) {
        u8 c = (u8)name.s[i];
        if (c == '/' || c == '\\' || c < 0x20 || c == 0x7f) {
            return false;
        }
    }
    return true;
}

struct OnlineNameAcc {
    int idx = -1;
    Str name;
    i64 size = 0;
    u8 sha1[20]{};
    bool hasSha1 = false;
    i64 uploadedAt = 0;
    bool haveName = false;
    bool haveSize = false;
};

void FreeOnlineFiles(Vec<OnlineListedFile>& v) {
    for (OnlineListedFile& f : v) {
        str::Free(f.name);
        f.name = {};
    }
    VecReset(v);
}

static void PushOnlineName(Vec<OnlineListedFile>& out, OnlineNameAcc& cur) {
    if (cur.haveName && len(cur.name) > 0) {
        OnlineListedFile f;
        f.name = cur.name;
        f.size = cur.haveSize ? cur.size : 0;
        memcpy(f.sha1, cur.sha1, sizeof(f.sha1));
        f.hasSha1 = cur.hasSha1;
        f.uploadedAt = cur.uploadedAt;
        cur.name = {};
        VecAppend(out, f);
    } else {
        str::Free(cur.name);
    }
    cur = {};
}

static bool ParseOnlineSize(Str s, i64& out) {
    if (len(s) == 0) {
        return false;
    }
    i64 n = 0;
    for (int i = 0; i < len(s); i++) {
        int digit = s.s[i] - '0';
        if (digit < 0 || digit > 9 || n > (INT64_MAX - digit) / 10) {
            return false;
        }
        n = n * 10 + digit;
    }
    out = n;
    return true;
}

// {"files":[{"name":"...","size":N,"sha1":"<hex>","uploadedAt":ms}]} - name bytes
// are copied; the parser reuses its buffer.
bool ParseOnlineFiles(Str body, Vec<OnlineListedFile>& out) {
    json::Reader r(body);
    OnlineNameAcc cur;
    for (json::Value& v : r) {
        bool isName = json::PathMatch(v.path, StrL("/files"), StrL("*"), StrL("/name"));
        bool isSize = json::PathMatch(v.path, StrL("/files"), StrL("*"), StrL("/size"));
        bool isSha1 = json::PathMatch(v.path, StrL("/files"), StrL("*"), StrL("/sha1"));
        bool isUploadedAt = json::PathMatch(v.path, StrL("/files"), StrL("*"), StrL("/uploadedAt"));
        if (!isName && !isSize && !isSha1 && !isUploadedAt) {
            continue;
        }
        int idx = json::PathSegIndex(json::PathNth(v.path, 1));
        if (idx < 0) {
            continue;
        }
        if (cur.idx != idx) {
            PushOnlineName(out, cur);
            cur.idx = idx;
        }
        if (isName && v.type == json::Type::String) {
            str::Free(cur.name);
            cur.name = str::Dup(v.value);
            cur.haveName = true;
        } else if (isSize) {
            if (v.type != json::Type::Number || !ParseOnlineSize(v.value, cur.size)) {
                str::Free(cur.name);
                FreeOnlineFiles(out);
                return false;
            }
            cur.haveSize = true;
        } else if (isSha1 && v.type == json::Type::String) {
            cur.hasSha1 = len(v.value) == 40 && str::HexToMem(v.value, Str((char*)cur.sha1, sizeof(cur.sha1)));
        } else if (isUploadedAt && v.type == json::Type::Number) {
            cur.uploadedAt = ParseInt64(v.value);
        }
    }
    PushOnlineName(out, cur);
    if (!r.Ok()) {
        FreeOnlineFiles(out);
        return false;
    }
    return true;
}

static bool IsAlnum(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// ^so_[0-9A-Za-z]{10}_[0-9A-Za-z]{32}$
bool IsOnlineApiKey(Str s) {
    if (len(s) != kOnlineApiKeyLen || !s.s || s.s[0] != 's' || s.s[1] != 'o' || s.s[2] != '_' || s.s[13] != '_') {
        return false;
    }
    for (int i = 3; i < kOnlineApiKeyLen; i++) {
        if (i != 13 && !IsAlnum(s.s[i])) {
            return false;
        }
    }
    return true;
}

// Empty onlineServer is production. Anything that is not one of the three
// origins is not used, even when it would pass a prefix check.
// wasm: the site hosting the app, so URLs are relative.
static Str OnlineOrigin() {
    if (OS_WASM) {
        return {};
    }
    Str server = (gSettings && len(gSettings->onlineServer) > 0) ? gSettings->onlineServer : Str{};
    bool known = str::Eq(server, kOnlineProdOrigin) || str::Eq(server, kOnlineLocalOrigin) ||
                 str::Eq(server, kOnlineLoopbackOrigin);
    return known ? server : kOnlineProdOrigin;
}

// "<origin> <user id>" of the saved key, which a cached file list belongs to;
// empty without a key (and in wasm, which uses its host site's session)
TempStr OnlineAccountTemp() {
    if (OS_WASM || !gSettings || !IsOnlineApiKey(gSettings->onlineApiKey)) {
        return {};
    }
    // so_<10 char user id>_<secret>
    Str userId = Str(gSettings->onlineApiKey.s + 3, 10);
    return fmt("%s %s", OnlineOrigin(), userId);
}

// The origins above and nothing else: no user info, no other host or port.
static bool UrlAllowed(Str url) {
    return OnlineURLAllowed(url);
}

TempStr OnlineLoginUrlTemp() {
    TempStr url = fmt("%s/desktop-link", OnlineOrigin());
    return UrlAllowed(url) ? url : TempStr{};
}

static TempStr OnlineUrlTemp(Str pathAndQuery) {
    TempStr url = fmt("%s%s", OnlineOrigin(), pathAndQuery);
    return UrlAllowed(url) ? url : TempStr{};
}

static Str NotAllowedMsg() {
    return Tr("The server address is not allowed.");
}

// the message of the transport failure the platform reports, if any
static void NoteHttpError(Str* msgOut, DWORD err) {
#if OS_WIN
    TempStr msg = GetLastErrorStrTemp(err);
    str::TrimWSInPlace(msg, str::TrimOpt::Both);
    *msgOut = str::Dup(msg);
#else
    *msgOut = str::Dup(HttpErrorMsgTemp(err));
#endif
}

TempStr OnlineVerifyMsgTemp(const OnlineVerifyResult* res) {
    if (res->status == kHttpStatusUnauthorized) {
        return str::DupTemp(Tr("That key was rejected. Create a new key on the website and paste it again."));
    }
    if (res->status >= 100 && res->status < 600) {
        return fmt("HTTP %d", (int)res->status);
    }
    if (res->urlRejected) {
        return str::DupTemp(NotAllowedMsg());
    }
    if (len(res->message) > 0) {
        return str::DupTemp(res->message);
    }
    return str::DupTemp(Tr("Could not reach SumatraPDF Web."));
}

// true when the caller should stop. A 401 keeps the saved key.
// UI thread: an upload or download read the file; remember its content hash
void RememberOnlineJobHash(const OnlineJobResult* res) {
    if (!res->hashed || len(res->filePath) == 0) {
        return;
    }
    ContentHash h;
    memcpy(h.sha1, res->sha1, sizeof(h.sha1));
    h.size = res->hashedSize;
    h.mtime = res->hashedMtime;
    RememberContentHash(res->filePath, h);
}

bool OnlineJobFailed(const OnlineJobResult* res, DWORD* lastHttpStatus, TempStr* msg) {
    if (res->tooBig || res->error == kErrFileTooLarge) {
        if (res->status >= 100 && res->status < 600) {
            *lastHttpStatus = res->status;
        }
        if (res->job == OnlineJob::Download) {
            *msg = str::DupTemp(Tr("This file is over 50 MiB. SumatraPDF will not download it."));
        } else {
            *msg = str::DupTemp(
                Tr("This file is over 50 MiB. SumatraPDF will not upload it. The website still accepts files up to 200 "
                   "MiB."));
        }
        return true;
    }
    if (res->status == kHttpStatusUnauthorized) {
        *lastHttpStatus = kHttpStatusUnauthorized;
        *msg = str::DupTemp(Tr("The server rejected the saved API key. It may have been revoked."));
        return true;
    }
    if (res->status == kHttpStatusOk && res->error != 0) {
        *lastHttpStatus = kHttpStatusOk;
        *msg = str::DupTemp(res->job == OnlineJob::Download ? Tr("Could not save the download.")
                                                            : Tr("Could not reach SumatraPDF Web."));
        return true;
    }
    if (res->status == kHttpStatusOk) {
        *lastHttpStatus = kHttpStatusOk;
        return false;
    }
    if (res->status >= 100 && res->status < 600) {
        *lastHttpStatus = res->status;
        *msg = fmt("HTTP %d", (int)res->status);
        return true;
    }
    if (res->urlRejected) {
        *msg = str::DupTemp(NotAllowedMsg());
        return true;
    }
    if (res->error == kErrReadFault) {
        *msg = str::DupTemp(Tr("Could not read the file."));
        return true;
    }
    if (len(res->message) > 0) {
        *msg = str::DupTemp(res->message);
        return true;
    }
    *msg = str::DupTemp(Tr("Could not reach SumatraPDF Web."));
    return true;
}

// {} when the file can be uploaded, else the reason
TempStr CheckOnlineUploadTemp(Str path) {
    if (!ValidUploadName(path::GetBaseNameTemp(path))) {
        return str::DupTemp(Tr("That file name cannot be uploaded."));
    }
    i64 sz = file::GetSize(path);
    if (sz < 0) {
        return str::DupTemp(Tr("Could not read the file."));
    }
    if (sz > kOnlineMaxBytes) {
        return str::DupTemp(Tr(
            "This file is over 50 MiB. SumatraPDF will not upload it. The website still accepts files up to 200 MiB."));
    }
    return {};
}

struct OnlineVerifyReq {
    u32 id = 0;
    Str url;
    Str token;
    Func1<OnlineVerifyResult*> onDone;
    ~OnlineVerifyReq() {
        str::Free(url);
        str::Free(token);
    }
};

static void VerifyDone(OnlineVerifyResult* res) {
    AutoDelete del(res);
    res->onDone.Call(res);
}

// no key (wasm): the site's session cookie authenticates
static TempStr AuthHeaderTemp(Str token) {
    return len(token) > 0 ? fmt("Authorization: Bearer %s", token) : TempStr{};
}

static void VerifyThread(OnlineVerifyReq* req) {
    AutoDelete del(req);
    auto* res = new OnlineVerifyResult();
    res->id = req->id;
    res->token = str::Dup(req->token);
    res->onDone = req->onDone;
    res->urlRejected = !UrlAllowed(req->url);
    if (res->urlRejected) {
        uitask::Post(MkFunc0(VerifyDone, res), "OnlineKeyVerified");
        return;
    }
    HttpRsp rsp;
    HttpGetWithHeaders(req->url, AuthHeaderTemp(req->token), &rsp);
    res->status = rsp.httpStatusCode;
    res->error = rsp.error;
    // ERROR_INVALID_PARAMETER is also a WinHTTP failure after the session opens
    if (!res->urlRejected && rsp.httpStatusCode == (DWORD)-1 && rsp.error != 0) {
        NoteHttpError(&res->message, rsp.error);
    }
    uitask::Post(MkFunc0(VerifyDone, res), "OnlineKeyVerified");
}

bool StartOnlineVerify(Str token, u32* idOut, Func1<OnlineVerifyResult*> onDone, TempStr* err) {
    TempStr url = OnlineUrlTemp(StrL("/api/files"));
    if (len(url) == 0) {
        *err = str::DupTemp(NotAllowedMsg());
        return false;
    }
    auto* req = new OnlineVerifyReq();
    req->id = *idOut = ++gOnlineReqSeq;
    req->url = str::Dup(url);
    req->token = str::Dup(token);
    req->onDone = onDone;
    RunAsync(MkFunc0(VerifyThread, req), StrL("SumatraPDFWebKey"));
    return true;
}

static void NoteHttp(OnlineJobResult* res, const HttpRsp& rsp) {
    res->status = rsp.httpStatusCode;
    res->error = rsp.error;
    if (res->status != (DWORD)-1 || rsp.error == 0 || rsp.error == kErrInvalidParam || rsp.error == kErrFileTooLarge ||
        rsp.error == kErrReadFault) {
        return;
    }
    NoteHttpError(&res->message, rsp.error);
}

static void JobDone(OnlineJobResult* res) {
    AutoDelete del(res);
    res->onDone.Call(res);
}

static Str ReadOnlineFile(Str path, file::ReadStamp& stamp) {
    FILE* f = file::OpenFILE(path);
    if (!f) {
        return {};
    }
    AutoCall closeFile(fclose, f);
    if (!file::GetReadStamp(f, stamp) || stamp.size > kOnlineMaxBytes) {
        return {};
    }
    int n = (int)stamp.size;
    char* data = AllocArray<char>(n + 1);
    if (!data) {
        return {};
    }
    file::ReadStamp after;
    if (fread(data, 1, n, f) != (size_t)n || !file::GetReadStamp(f, after) || after != stamp ||
        !file::ReadStampMatchesPath(path, stamp)) {
        free(data);
        return {};
    }
    return Str(data, n);
}

// Content the server already has (another name, another device) isn't sent
// again: ask it to name that content. 404: it doesn't have it. A server from
// before sha1 links refuses the empty body. Either way, upload; the server
// checks the content against sha1.
static void UploadJob(OnlineJobReq* req, OnlineJobResult* res, Str hdr) {
    file::ReadStamp stamp;
    Str body = ReadOnlineFile(req->filePath, stamp);
    if (!body.s) {
        res->tooBig = stamp.size > kOnlineMaxBytes;
        res->error = res->tooBig ? kErrFileTooLarge : kErrReadFault;
        return;
    }
    defer {
        str::Free(body);
        res->hashed = file::ReadStampMatchesPath(req->filePath, stamp);
    };
    u8 digest[20];
    CalcSHA1Digest(body, digest);
    memcpy(res->sha1, digest, sizeof(digest));
    res->hashedSize = stamp.size;
    res->hashedMtime = stamp.mtime;
    TempStr url = fmt("%s&sha1=%s", req->url, str::MemToHexTemp(Str((const char*)digest, dimofi(digest))));
    HttpRsp linkRsp;
    HttpPostUrlNoRedirect(url, StrL("application/octet-stream"), hdr, Str{}, &linkRsp);
    if (linkRsp.httpStatusCode == kHttpStatusOk || linkRsp.httpStatusCode == kHttpStatusUnauthorized) {
        NoteHttp(res, linkRsp);
        return;
    }
    HttpRsp rsp;
    HttpPostUrlNoRedirect(url, StrL("application/octet-stream"), hdr, body, &rsp);
    NoteHttp(res, rsp);
}

// the downloaded file's hash, so it matches its server file by content
static void HashDownload(Str path, OnlineJobResult* res) {
    if (OS_WASM) {
        return;
    }
    file::ReadStamp stamp;
    Str data = ReadOnlineFile(path, stamp);
    if (!data.s) {
        return;
    }
    CalcSHA1Digest(data, res->sha1);
    res->hashed = file::ReadStampMatchesPath(path, stamp);
    res->hashedSize = stamp.size;
    res->hashedMtime = stamp.mtime;
    str::Free(data);
}

static void JobThread(OnlineJobReq* req) {
    AutoDelete del(req);
    auto* res = new OnlineJobResult();
    res->id = req->id;
    res->job = req->job;
    res->onDone = req->onDone;
    res->urlRejected = !UrlAllowed(req->url);
    if (res->urlRejected) {
        res->error = kErrInvalidParam;
        uitask::Post(MkFunc0(JobDone, res), "OnlineJobDone");
        return;
    }
    if (req->job == OnlineJob::Download) {
        res->cachePath = str::Dup(req->destPath);
        res->displayName = str::Dup(req->displayName);
        res->filePath = str::Dup(req->destPath);
    }
    if (req->job == OnlineJob::Upload) {
        res->filePath = str::Dup(req->filePath);
    }
    TempStr hdr = AuthHeaderTemp(req->token);
    if (req->job == OnlineJob::Upload) {
        UploadJob(req, res, hdr);
    } else if (req->job == OnlineJob::Delete) {
        HttpRsp rsp;
        HttpDeleteWithHeaders(req->url, hdr, &rsp);
        NoteHttp(res, rsp);
    } else if (req->job == OnlineJob::Download) {
        HttpRsp rsp;
        HttpDownloadWithHeaders(req->url, hdr, req->destPath, &rsp);
        NoteHttp(res, rsp);
        res->tooBig = rsp.error == kErrFileTooLarge;
        if (rsp.httpStatusCode == kHttpStatusOk && rsp.error == 0) {
            HashDownload(req->destPath, res);
        }
    } else {
        HttpRsp rsp;
        HttpGetWithHeaders(req->url, hdr, &rsp);
        NoteHttp(res, rsp);
        if (rsp.httpStatusCode == kHttpStatusOk) {
            res->body = str::Dup(ToStr(rsp.data));
        }
    }
    uitask::Post(MkFunc0(JobDone, res), "OnlineJobDone");
}

static void StartJob(OnlineJobReq* req, u32* idOut, Func1<OnlineJobResult*> onDone) {
    req->id = *idOut = ++gOnlineReqSeq;
    req->onDone = onDone;
    RunAsync(MkFunc0(JobThread, req), StrL("SumatraPDFWebFile"));
}

bool StartOnlineList(Str token, u32* idOut, Func1<OnlineJobResult*> onDone, TempStr* err) {
    TempStr url = OnlineUrlTemp(StrL("/api/files"));
    if (len(url) == 0) {
        *err = str::DupTemp(NotAllowedMsg());
        return false;
    }
    auto* req = new OnlineJobReq();
    req->job = OnlineJob::List;
    req->url = str::Dup(url);
    req->token = str::Dup(token);
    StartJob(req, idOut, onDone);
    return true;
}

bool StartOnlineUpload(Str token, Str path, u32* idOut, Func1<OnlineJobResult*> onDone, TempStr* err) {
    TempStr name = url::EncodeTemp(path::GetBaseNameTemp(path));
    TempStr url = OnlineUrlTemp(fmt("/api/files?name=%s", name));
    if (len(url) == 0) {
        *err = str::DupTemp(NotAllowedMsg());
        return false;
    }
    auto* req = new OnlineJobReq();
    req->job = OnlineJob::Upload;
    req->url = str::Dup(url);
    req->token = str::Dup(token);
    req->filePath = str::Dup(path);
    StartJob(req, idOut, onDone);
    return true;
}

// <appdata>/SumatraPDF-web/<name>; wasm: /uploads/<name>, where the hosting
// site keeps documents
TempStr OnlineDownloadPathTemp(Str name) {
    TempStr dir = OS_WASM ? str::DupTemp(StrL("/uploads")) : GetPathInAppDataDirTemp(StrL("SumatraPDF-web"));
    return len(dir) > 0 ? path::JoinTemp(dir, name) : TempStr{};
}

// OnlineDownloadPathTemp(name), only after the name check. The same name overwrites.
bool StartOnlineDownload(Str token, Str name, i64 size, u32* idOut, Func1<OnlineJobResult*> onDone, TempStr* err) {
    if (size > kOnlineMaxBytes) {
        *err = str::DupTemp(Tr("This file is over 50 MiB. SumatraPDF will not download it."));
        return false;
    }
    if (!ValidUploadName(name)) {
        *err = str::DupTemp(Tr("That file name cannot be downloaded."));
        return false;
    }
    TempStr cachePath = OnlineDownloadPathTemp(name);
    if (len(cachePath) == 0 || !dir::CreateAll(path::GetDirTemp(cachePath))) {
        *err = str::DupTemp(Tr("Could not save the download."));
        return false;
    }
    TempStr url = OnlineUrlTemp(fmt("/api/file?name=%s", url::EncodeTemp(name)));
    if (len(url) == 0) {
        *err = str::DupTemp(NotAllowedMsg());
        return false;
    }
    auto* req = new OnlineJobReq();
    req->job = OnlineJob::Download;
    req->url = str::Dup(url);
    req->token = str::Dup(token);
    req->destPath = str::Dup(cachePath);
    req->displayName = str::Dup(name);
    StartJob(req, idOut, onDone);
    return true;
}

bool StartOnlineDelete(Str token, Str name, u32* idOut, Func1<OnlineJobResult*> onDone, TempStr* err) {
    TempStr url = OnlineUrlTemp(fmt("/api/file?name=%s", url::EncodeTemp(name)));
    if (len(url) == 0) {
        *err = str::DupTemp(NotAllowedMsg());
        return false;
    }
    auto* req = new OnlineJobReq();
    req->job = OnlineJob::Delete;
    req->url = str::Dup(url);
    req->token = str::Dup(token);
    StartJob(req, idOut, onDone);
    return true;
}
