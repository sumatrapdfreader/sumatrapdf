/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: Simplified BSD (see ./COPYING) */

#include "SimpleLog.h"
#include "Vec.h"

namespace Log {

// TODO: implement this as a MultiLogger instead, so that we can test
//       just the MultiLogger instead?
static Vec<Logger*> *gLoggers;
static CRITICAL_SECTION gLogCs;

void Initialize()
{
    gLoggers = new Vec<Logger*>();
    InitializeCriticalSection(&gLogCs);
}

void Destroy()
{
    DeleteVecMembers(*gLoggers);
    DeleteCriticalSection(&gLogCs);
    delete gLoggers;
    gLoggers = NULL;
}

// Note: unless you remove the logger with RemoveLogger(), we own the Logger
// and it'll be deleted when Log::Destroy() is called.
void AddLogger(Logger *logger)
{        
    ScopedCritSec cs(&gLogCs);
    gLoggers->Append(logger);
}

void RemoveLogger(Logger *logger)
{
    ScopedCritSec cs(&gLogCs);
    gLoggers->Remove(logger);
}

void Log(TCHAR *s)
{
    ScopedCritSec cs(&gLogCs);
    for (size_t i = 0; i < gLoggers->Count(); i++)
        gLoggers->At(i)->Log(s);
}

void LogFmt(TCHAR *fmt, ...)
{
    if (0 == gLoggers->Count())
        return;

    va_list args;
    va_start(args, fmt);
    TCHAR *s = Str::FmtV(fmt, args);
    LogAndFree(s);
    va_end(args);
}

void LogAndFree(TCHAR *s)
{
    Log(s);
    free(s);
}

} // namespace Log

