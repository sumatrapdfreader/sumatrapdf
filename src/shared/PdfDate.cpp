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

// The current time as a PDF date, "D:YYYYMMDDHHmmSSOHH'mm'", where O is the
// relationship of local time to UTC. Stamps CreationDate/ModDate (issue #949).
TempStr FormatPdfDateNowTemp() {
    time_t t = time(nullptr);
    struct tm lt{};
    struct tm gt{};
#if OS_WIN
    localtime_s(&lt, &t);
    gmtime_s(&gt, &t);
#else
    localtime_r(&t, &lt);
    gmtime_r(&t, &gt);
#endif
    // local minus UTC, in minutes; the day can differ across midnight
    int off = (lt.tm_hour - gt.tm_hour) * 60 + (lt.tm_min - gt.tm_min);
    int dayDiff = lt.tm_yday - gt.tm_yday;
    if (dayDiff == 1 || dayDiff < -1) {
        off += 24 * 60;
    } else if (dayDiff == -1 || dayDiff > 1) {
        off -= 24 * 60;
    }
    char sign = '+';
    if (off < 0) {
        sign = '-';
        off = -off;
    }
    int offH = off / 60;
    int offM = off % 60;
    return fmt("D:%04d%02d%02d%02d%02d%02d%c%02d'%02d'", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
               lt.tm_min, lt.tm_sec, sign, offH, offM);
}
