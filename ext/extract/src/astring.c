#include "../include/extract_alloc.h"

#include "astring.h"
#include "mem.h"
#include "memento.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void extract_astring_init(extract_astring_t* string)
{
    string->chars = NULL;
    string->chars_num = 0;
}

void extract_astring_free(extract_alloc_t* alloc, extract_astring_t* string)
{
    extract_free(alloc, &string->chars);
    extract_astring_init(string);
}


int extract_astring_catl(extract_alloc_t* alloc, extract_astring_t* string, const char* s, size_t s_len)
{
    if (extract_realloc2(alloc, &string->chars, string->chars_num+1, string->chars_num + s_len + 1)) return -1;
    memcpy(string->chars + string->chars_num, s, s_len);
    string->chars[string->chars_num + s_len] = 0;
    string->chars_num += s_len;
    return 0;
}

int extract_astring_catc(extract_alloc_t* alloc, extract_astring_t* string, char c)
{
    return extract_astring_catl(alloc, string, &c, 1);
}

int extract_astring_cat(extract_alloc_t* alloc, extract_astring_t* string, const char* s)
{
    return extract_astring_catl(alloc, string, s, strlen(s));
}

int extract_astring_catf(extract_alloc_t* alloc, extract_astring_t* string, const char* format, ...)
{
    char* buffer = NULL;
    int e;
    va_list va;
    va_start(va, format);
    e = extract_vasprintf(alloc, &buffer, format, va);
    va_end(va);
    if (e < 0) return e;
    e = extract_astring_cat(alloc, string, buffer);
    extract_free(alloc, &buffer);
    return e;
}

int extract_astring_truncate(extract_astring_t* content, int len)
{
    assert((size_t) len <= content->chars_num);
    content->chars_num -= len;
    content->chars[content->chars_num] = 0;
    return 0;
}

int astring_char_truncate_if(extract_astring_t* content, char c)
{
    if (content->chars_num && content->chars[content->chars_num-1] == c) {
        extract_astring_truncate(content, 1);
    }
    return 0;
}

int extract_astring_cat_xmlc(extract_alloc_t* alloc, extract_astring_t* string, int c)
{
    int ret = -1;
    
    if (0) {}

    /* Escape XML special characters. */
    else if (c == '<')  extract_astring_cat(alloc, string, "&lt;");
    else if (c == '>')  extract_astring_cat(alloc, string, "&gt;");
    else if (c == '&')  extract_astring_cat(alloc, string, "&amp;");
    else if (c == '"')  extract_astring_cat(alloc, string, "&quot;");
    else if (c == '\'') extract_astring_cat(alloc, string, "&apos;");

    /* Expand ligatures. */
    else if (c == 0xFB00)
    {
        if (extract_astring_cat(alloc, string, "ff")) goto end;
    }
    else if (c == 0xFB01)
    {
        if (extract_astring_cat(alloc, string, "fi")) goto end;
    }
    else if (c == 0xFB02)
    {
        if (extract_astring_cat(alloc, string, "fl")) goto end;
    }
    else if (c == 0xFB03)
    {
        if (extract_astring_cat(alloc, string, "ffi")) goto end;
    }
    else if (c == 0xFB04)
    {
        if (extract_astring_cat(alloc, string, "ffl")) goto end;
    }

    /* Output ASCII verbatim. */
    else if (c >= 32 && c <= 127)
    {
        if (extract_astring_catc(alloc, string, (char) c)) goto end;
    }

    /* Escape all other characters. */
    else
    {
        char    buffer[32];
        if (c < 32
                && (c != 0x9 && c != 0xa && c != 0xd)
                )
        {
            /* Illegal xml character; see
            https://www.w3.org/TR/xml/#charsets. We replace with
            0xfffd, the unicode replacement character. */
            c = 0xfffd;
        }
        snprintf(buffer, sizeof(buffer), "&#x%x;", c);
        if (extract_astring_cat(alloc, string, buffer)) goto end;
    }
    
    ret = 0;
    
    end:
    return ret;
}
