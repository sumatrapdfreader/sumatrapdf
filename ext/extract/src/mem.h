#ifndef EXTRACT_MEM_H
#define EXTRACT_MEM_H

#include "extract/alloc.h"

#include <stdarg.h>
#include <string.h>

void extract_bzero(void *b, size_t len);

int extract_vasprintf(extract_alloc_t* alloc, char** out, const char* format, va_list va)
        #ifdef __GNUC__
        __attribute__ ((format (printf, 3, 0)))
        #endif
        ;

int extract_asprintf(extract_alloc_t* alloc, char** out, const char* format, ...)
        #ifdef __GNUC__
        __attribute__ ((format (printf, 3, 4)))
        #endif
        ;

int extract_strdup(extract_alloc_t* alloc, const char* s, char** o_out);

#endif
