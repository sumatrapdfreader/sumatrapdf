#include "utils/BaseUtil.h"

static str::Str<char> logBuf;

std::string_view getLogView() {
    return logBuf.AsView();
}

void log(std::string_view s) {
    logBuf.Append(s.data(), s.size());
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
}

void logf(const WCHAR* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AutoFreeW s(str::FmtV(fmt, args));
    log(s);
}

void resetLog() {
    logBuf.Reset();
}
