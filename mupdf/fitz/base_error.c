#include "fitz_base.h"

void fz_warn(char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "warning: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

fz_error fz_throwimp(const char *func, const char *file, int line, char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "+ %s:%d: %s(): ", file, line, func);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    return ((fz_error)-1);
}

fz_error fz_rethrowimp(fz_error cause, const char *func, const char *file, int line, char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "| %s:%d: %s(): ", file, line, func);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    return cause;
}

fz_error fz_catchimp(fz_error cause, const char *func, const char *file, int line, char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "\\ %s:%d: %s(): ", file, line, func);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    return cause;
}

