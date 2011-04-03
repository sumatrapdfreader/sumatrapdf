/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#include "SimpleLog.h"
#include "Vec.h"

namespace Log {

// TODO: implement this as a MultiLogger instead, so that we can test
//       just the MultiLogger instead?
static Vec<Logger*> *g_loggers;
static CRITICAL_SECTION g_logCs;

void Initialize()
{
    g_loggers = new Vec<Logger*>();
    InitializeCriticalSection(&g_logCs);
}

void Destroy()
{
    DeleteVecMembers(*g_loggers);
    DeleteCriticalSection(&g_logCs);
    delete g_loggers;
    g_loggers = NULL;
}

// Note: unless you remove the logger with RemoveLogger(), we own the Logger
// and it'll be deleted when Log::Destroy() is called.
void AddLogger(Logger *logger)
{        
    ScopedCritSec cs(&g_logCs);
    g_loggers->Append(logger);
}

void RemoveLogger(Logger *logger)
{
    ScopedCritSec cs(&g_logCs);
    g_loggers->Remove(logger);
}

void Log(TCHAR *s)
{
    ScopedCritSec cs(&g_logCs);
    if (0 == g_loggers->Count())
        return;

    for (size_t i = 0; i < g_loggers->Count(); i++)
        g_loggers->At(i)->Log(s);
}

void LogFmt(TCHAR *fmt, ...)
{
    if (0 == g_loggers->Count())
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

