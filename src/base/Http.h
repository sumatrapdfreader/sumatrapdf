/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct HttpRsp {
    Str url;
    str::Builder data;
    DWORD error = (DWORD)-1;
    DWORD httpStatusCode = (DWORD)-1;

    HttpRsp() = default;
    ~HttpRsp();
};

struct HttpProgress {
    i64 nDownloaded;
};

bool IsHttpRspOk(const HttpRsp*);

bool HttpGet(Str url, HttpRsp* rspOut);
bool HttpGetToFile(Str url, Str destFilePath, const Func1<HttpProgress*>& cbProgress, i64 maxSize = -1);

bool HttpPostUrl(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut);
TempStr HttpNormalizeHeadersTemp(Str headers);

bool OnlineURLAllowed(Str url);
bool HttpGetWithHeaders(Str url, Str extraHeaders, HttpRsp* rspOut);
bool HttpDeleteWithHeaders(Str url, Str extraHeaders, HttpRsp* rspOut);
bool HttpPostUrlNoRedirect(Str url, Str contentType, Str extraHeaders, Str body, HttpRsp* rspOut);
bool HttpDownloadWithHeaders(Str url, Str extraHeaders, Str destPath, HttpRsp* rspOut);
#if !OS_WIN
TempStr HttpErrorMsgTemp(DWORD err);
#endif

// How much URL-encoded text we're willing to put in a URL. ShellExecuteW hands
// the URL to the browser as a command line, so the hard ceiling is
// CreateProcess's 32767 WCHARs; browsers give up well before that and each has
// its own limit, so stay comfortably below.
constexpr int kMaxUrlEncodedLen = 8192;

TempStr URLEncodeMayTruncateTemp(Str s, int maxEncodedLen = 0, bool* didTruncateOut = nullptr);

struct HttpStreamCallbacks {
    void* ctx;
    void (*onStatus)(void* ctx, int status);
    bool (*onData)(void* ctx, Str bytes);
};

struct HttpStreamCancel {
    HttpStreamCancel();
    ~HttpStreamCancel();
    HttpStreamCancel(const HttpStreamCancel&) = delete;
    HttpStreamCancel& operator=(const HttpStreamCancel&) = delete;
    void Cancel();
    bool IsCanceled();

  private:
    AtomicBool canceled = 0;
#if OS_WIN
    // signaled by Cancel(); HttpPostStream waits on it next to its request
    void* event = nullptr;
    friend bool HttpPostStream(Str, Str, Str, const HttpStreamCallbacks&, HttpStreamCancel&);
#endif
};

// Both callbacks run on the calling thread. Status precedes data, and returning
// false from onData stops the stream. The bytes are valid until onData returns.
bool HttpPostStream(Str url, Str headers, Str body, const HttpStreamCallbacks&, HttpStreamCancel&);
