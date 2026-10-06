/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PdfDate.h"

TempStr FormatPdfDateLocalTimeTemp(time_t secs) {
    if (secs == 0) {
        return {};
    }
    struct tm tm;
#if OS_WIN
    if (localtime_s(&tm, &secs) != 0) {
        return {};
    }
#else
    if (!localtime_r(&secs, &tm)) {
        return {};
    }
#endif
    char buf[64];
    size_t n = strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return str::DupTemp(Str(buf, (int)n));
}
