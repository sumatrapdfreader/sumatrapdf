/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef SimpleLog_h
#define SimpleLog_h

namespace slog {

class Logger {
public:
    virtual ~Logger() { }
    virtual void Log(WCHAR *s) = 0;

    void LogFmt(WCHAR *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        ScopedMem<WCHAR> s(str::FmtV(fmt, args));
        Log(s);
        va_end(args);
    }

    void LogAndFree(WCHAR *s)
    {
        Log(s);
        free(s);
    }
};

class FileLogger : public Logger {
    ScopedHandle fh;

public:
    FileLogger(const WCHAR *fileName) :
        fh(CreateFile(fileName, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) { }
    FileLogger(HANDLE fh) : fh(fh) { }

    virtual void Log(WCHAR *s)
    {
        ScopedMem<char> utf8s(str::conv::ToUtf8(s));
        if (utf8s && INVALID_HANDLE_VALUE != fh) {
            DWORD len;
            BOOL ok = WriteFile(fh, utf8s.Get(), (DWORD)str::Len(utf8s), &len, NULL);
            if (ok)
                WriteFile(fh, "\r\n", 2, &len, NULL);
        }
    }
};

class MemoryLogger : public Logger {
    str::Str<WCHAR> log;

public:
    virtual void Log(WCHAR *s)
    {
        if (s) {
            log.Append(s);
            log.Append(L"\r\n");
        }
    }

    // caller MUST NOT free the result
    // (str::Dup data, if the logger is in use)
    WCHAR *GetData() { return log.LendData(); }
};

class DebugLogger : public Logger {
public:
    virtual void Log(WCHAR *s)
    {
        if (s) {
            // DbgView displays one line per OutputDebugString call
            OutputDebugString(ScopedMem<WCHAR>(str::Format(L"%s\n", s)));
        }
    }
};

class StderrLogger : public Logger {
public:
    virtual ~StderrLogger()
    {
        fflush(stderr);
    }

    virtual void Log(WCHAR *s)
    {
        if (s)
            fwprintf(stderr, L"%s\n", s);
    }
};

// allows to log into several logs at the same time (thread safe)
class MultiLogger : public Logger {
    Vec<Logger *>    loggers;
    CRITICAL_SECTION cs;

public:
    MultiLogger() { InitializeCriticalSection(&cs); }
    ~MultiLogger()
    {
        EnterCriticalSection(&cs);
        DeleteVecMembers(loggers);
        LeaveCriticalSection(&cs);
        DeleteCriticalSection(&cs);
    }

    virtual void Log(WCHAR *s)
    {
        ScopedCritSec scope(&cs);
        for (size_t i = 0; i < loggers.Count(); i++) {
            loggers.At(i)->Log(s);
        }
    }

    void AddLogger(Logger *logger)
    {
        ScopedCritSec scope(&cs);
        loggers.Append(logger);
    }
    void RemoveLogger(Logger *logger)
    {
        ScopedCritSec scope(&cs);
        loggers.Remove(logger);
    }
    size_t CountLoggers()
    {
        ScopedCritSec scope(&cs);
        return loggers.Count();
    }
};

} // namespace Log

#endif
