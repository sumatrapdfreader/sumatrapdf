#include "utils/BaseUtil.h"

str::Str<char> logBuf;
bool logToStderr;

void log(std::string_view s) {
    logBuf.Append(s.data(), s.size());
    if (logToStderr) {
        fwrite(s.data(), 1, s.size(), stderr);
        fflush(stderr);
    }
}


void log(const char* s) {
    auto sv = std::string_view(s);
    log(sv);
}

void log(const WCHAR* s) {
    if (!s) {
        return;
    }
    OwnedData tmp = str::conv::ToUtf8(s);
    auto sv = tmp.AsView();
    log(sv);
}

void logf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt); 
    AutoFreeStr s(str::FmtV(fmt, args));
    log(s.AsView());
    va_end(args);
}

void dbglogf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt); 
    AutoFreeStr s(str::FmtV(fmt, args));
    OutputDebugStringA(s.Get());
    va_end(args);
}

void logf(const WCHAR* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFreeW s(str::FmtV(fmt, args));
    log(s);
    va_end(args);
}
