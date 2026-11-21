/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SumatraPDFWebCommon.cpp and each app's SumatraPDFWebDialog.cpp ---

// so_ + 10 char user id + _ + 32 char secret
constexpr int kOnlineApiKeyLen = 46;
constexpr DWORD kHttpStatusOk = 200;
constexpr DWORD kHttpStatusUnauthorized = 401;

constexpr i64 kOnlineMaxBytes = (i64)50 << 20;
constexpr int kOnlineFileLines = 6;

enum class OnlineJob : int {
    List = 1,
    Upload,
    Download,
    Delete,
};

struct OnlineListedFile {
    Str name;
    i64 size = 0;
};

struct OnlineJobResult {
    u32 id = 0;
    OnlineJob job = OnlineJob::List;
    DWORD status = (DWORD)-1;
    DWORD error = 0;
    Str message;
    Str body;
    Str cachePath;
    Str displayName;
    bool urlRejected = false;
    bool tooBig = false;
    Func1<OnlineJobResult*> onDone;
    ~OnlineJobResult() {
        str::Free(message);
        str::Free(body);
        str::Free(cachePath);
        str::Free(displayName);
    }
};

struct OnlineJobReq {
    u32 id = 0;
    OnlineJob job = OnlineJob::List;
    Str url;
    Str token;
    Str filePath;
    Str destPath;
    Str displayName;
    Func1<OnlineJobResult*> onDone;
    ~OnlineJobReq() {
        str::Free(url);
        str::Free(token);
        str::Free(filePath);
        str::Free(destPath);
        str::Free(displayName);
    }
};

struct OnlineVerifyResult {
    u32 id = 0;
    DWORD status = (DWORD)-1;
    DWORD error = 0;
    Str token;
    Str message;
    bool urlRejected = false;
    Func1<OnlineVerifyResult*> onDone;
    ~OnlineVerifyResult() {
        str::Free(token);
        str::Free(message);
    }
};

bool ValidUploadName(Str name);
bool IsOnlineApiKey(Str s);
void FreeOnlineFiles(Vec<OnlineListedFile>& v);
bool ParseOnlineFiles(Str body, Vec<OnlineListedFile>& out);
TempStr OnlineLoginUrlTemp();
TempStr OnlineVerifyMsgTemp(const OnlineVerifyResult* res);
bool OnlineJobFailed(const OnlineJobResult* res, DWORD* lastHttpStatus, TempStr* msg);
TempStr CheckOnlineUploadTemp(Str path);
bool StartOnlineVerify(Str token, u32* idOut, Func1<OnlineVerifyResult*> onDone, TempStr* err);
bool StartOnlineList(Str token, u32* idOut, Func1<OnlineJobResult*> onDone, TempStr* err);
bool StartOnlineUpload(Str token, Str path, u32* idOut, Func1<OnlineJobResult*> onDone, TempStr* err);
TempStr OnlineDownloadPathTemp(Str name);
bool StartOnlineDownload(Str token, Str name, i64 size, u32* idOut, Func1<OnlineJobResult*> onDone, TempStr* err);
bool StartOnlineDelete(Str token, Str name, u32* idOut, Func1<OnlineJobResult*> onDone, TempStr* err);
