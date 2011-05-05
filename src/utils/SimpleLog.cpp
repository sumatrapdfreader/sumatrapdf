/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "SimpleLog.h"

namespace Log {

static MultiLogger *gLogger;

void Initialize()
{
    gLogger = new MultiLogger();
}

void Destroy()
{
    delete gLogger;
    gLogger = NULL;
}

// Note: unless you remove the logger with RemoveLogger(), we own the Logger
// and it'll be deleted when Log::Destroy() is called.
void AddLogger(Logger *logger)
{
    gLogger->AddLogger(logger);
}

void RemoveLogger(Logger *logger)
{
    gLogger->RemoveLogger(logger);
}

void Log(TCHAR *s)
{
    gLogger->Log(s);
}

void LogFmt(TCHAR *fmt, ...)
{
    if (0 == gLogger->CountLoggers())
        return;

    va_list args;
    va_start(args, fmt);
    TCHAR *s = str::FmtV(fmt, args);
    gLogger->LogAndFree(s);
    va_end(args);
}

void LogAndFree(TCHAR *s)
{
    gLogger->LogAndFree(s);
}

} // namespace Log

