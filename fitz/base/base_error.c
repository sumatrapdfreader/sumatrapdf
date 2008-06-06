#include "fitz-base.h"

fz_error *
fz_keeperror(fz_error *eo)
{
    eo->refs++;
    return eo;
}

void
fz_droperror(fz_error *eo)
{
    if (eo->refs > 0)
        eo->refs--;
    if (eo->refs == 0)
    {
        if (eo->cause)
            fz_droperror(eo->cause);
        fz_free(eo);
    }
}

void
fz_printerror(fz_error *eo)
{
#if 1
    if (eo->cause)
    {
        fz_printerror(eo->cause);
        fprintf(stderr, "| %s:%d: %s(): %s\n", eo->file, eo->line, eo->func, eo->msg);
    }
    else
    {
        fprintf(stderr, "+ %s:%d: %s(): %s\n", eo->file, eo->line, eo->func, eo->msg);
    }
#else
    fprintf(stderr, "+ %s:%d: %s(): %s\n", eo->file, eo->line, eo->func, eo->msg);
    eo = eo->cause;

    while (eo)
    {
        fprintf(stderr, "| %s:%d: %s(): %s\n", eo->file, eo->line, eo->func, eo->msg);
        eo = eo->cause;
    }
#endif
}


void
fz_warn(char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "warning: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

fz_error *
fz_throwimp(fz_error *cause, const char *func, const char *file, int line, char *fmt, ...)
{
    va_list ap;
    fz_error *eo;

    eo = fz_malloc(sizeof(fz_error));
    if (!eo)
        return fz_outofmem; /* oops. we're *really* out of memory here. */

    eo->refs = 1;

    va_start(ap, fmt);
    vsnprintf(eo->msg, sizeof eo->msg, fmt, ap);
    eo->msg[sizeof(eo->msg) - 1] = '\0';
    va_end(ap);

    strlcpy(eo->func, func, sizeof eo->func);
    strlcpy(eo->file, file, sizeof eo->file);
    eo->line = line;

    if (cause)
        eo->cause = fz_keeperror(cause);
    else
        eo->cause = nil;

    return eo;
}

