/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef SimpleLog_h
#define SimpleLog_h

#include "Vec.h"

namespace Log {

class Logger {
public:
    virtual ~Logger() { }
    virtual void Log(TCHAR *s, bool takeOwnership=false) = 0;
};

class FileLogger : public Logger {
    HANDLE fh;

public:
    FileLogger(const TCHAR *fileName)
    {
        fh = CreateFile(fileName, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    FileLogger(HANDLE fh) : fh(fh) { }
    virtual ~FileLogger() { CloseHandle(fh); }

    virtual void Log(TCHAR *s, bool takeOwnership=false)
    {
        ScopedMem<char> utf8s(Str::Conv::ToUtf8(s));
        if (utf8s && INVALID_HANDLE_VALUE != fh) {
            DWORD len;
            BOOL ok = WriteFile(fh, utf8s.Get(), Str::Len(utf8s), &len, NULL);
            if (ok)
                WriteFile(fh, "\r\n", 2, &len, NULL);
        }
        if (takeOwnership)
            free(s);
    }
};

class MemoryLogger : public Logger {
    StrVec  lines;

public:
    virtual void Log(TCHAR *s, bool takeOwnership=false)
    {
        TCHAR *tmp = s;
        if (!takeOwnership)
            tmp = Str::Dup(s);
        if (tmp)
            lines.Append(tmp);
    }

    // caller has to free() the result
    TCHAR *GetLines() { return lines.Join(_T("\r\n")); }
};

void Initialize();
void Destroy();

void AddLogger(Logger *);
void RemoveLogger(Logger *);

void Log(TCHAR *s, bool takeOwnership=false);
void LogFmt(TCHAR *fmt, ...);

} // namespace Log

#endif
