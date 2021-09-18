#ifndef ARTIFEX_EXTRACT_OUTF_H
#define ARTIFEX_EXTRACT_OUTF_H

/* Simple printf-style debug output. */

#define outf(format, ...) \
        (1 > extract_outf_verbose) ? (void) 0 : (extract_outf)(1, __FILE__, __LINE__, __FUNCTION__, 1 /*ln*/, format, ##__VA_ARGS__)

#define outf0(format, ...) \
        (0 > extract_outf_verbose) ? (void) 0 : (extract_outf)(0, __FILE__, __LINE__, __FUNCTION__, 1 /*ln*/, format, ##__VA_ARGS__)

#define outfx(format, ...)

/* Only for internal use by extract code.  */

extern int extract_outf_verbose;

void (extract_outf)(
        int level,
        const char* file, int line,
        const char* fn,
        int ln,
        const char* format,
        ...
        )
        #ifdef __GNUC__
        __attribute__ ((format (printf, 6, 7)))
        #endif
        ;
/* Outputs text if <level> is less than or equal to verbose value set by
outf_level_set(). */

void extract_outf_verbose_set(int verbose);
/* Set verbose value. Higher values are more verbose. Initial value is 0. */

#endif
