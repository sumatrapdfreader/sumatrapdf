/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace slog {

class Logger {
  public:
    virtual ~Logger() {}
    virtual void Log(const WCHAR* s) = 0;

    void LogFmt(const WCHAR* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        AutoFreeW s(str::FmtV(fmt, args));
        Log(s);
        va_end(args);
    }

    void LogAndFree(WCHAR* s) {
        Log(s);
        free(s);
    }
};

class MemoryLogger : public Logger {
    str::Str<WCHAR> log;

  public:
    virtual void Log(const WCHAR* s) {
        if (s) {
            log.Append(s);
            log.Append(L"\r\n");
        }
    }

    // caller MUST NOT free the result
    // (str::Dup data, if the logger is in use)
    WCHAR* GetData() { return log.LendData(); }
};

class DebugLogger : public Logger {
  public:
    virtual void Log(const WCHAR* s) {
        if (s) {
            // DbgView displays one line per OutputDebugString call
            OutputDebugString(AutoFreeW(str::Format(L"%s\n", s)));
        }
    }
};

class StderrLogger : public Logger {
  public:
    virtual ~StderrLogger() { fflush(stderr); }

    virtual void Log(const WCHAR* s) {
        if (s)
            fwprintf(stderr, L"%s\n", s);
    }
};

} // namespace slog
