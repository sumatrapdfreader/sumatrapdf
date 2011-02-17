/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */

/* The most basic things, including string handling functions */
#include "base_util.h"
#include "str_util.h"

char * str_cat_s(char * dst, size_t dst_cch_size, const char * src)
{
    return str_catn_s(dst, dst_cch_size, src, strlen(src));
}

char * str_catn_s(char *dst, size_t dst_cch_size, const char *src, size_t src_cch_size)
{
    char *dstEnd = dst + strlen(dst);
    size_t len = min(src_cch_size + 1, dst_cch_size - (dstEnd - dst));
    if (dst_cch_size <= (size_t)(dstEnd - dst))
        return NULL;
    
    strncpy(dstEnd, src, len);
    dstEnd[len - 1] = '\0';
    
    if (src_cch_size >= len)
        return NULL;
    return dst;
}

int char_is_dir_sep(char c)
{
    if ('/' == c || '\\' == c)
        return TRUE;
    return FALSE;
}

/* Concatenate 3 strings. Any string can be NULL.
   Caller needs to free() memory. */
char *str_cat3(const char *str1, const char *str2, const char *str3)
{
    if (!str1)
        str1 = "";
    if (!str2)
        str2 = "";
    if (!str3)
        str3 = "";

    return str_printf("%s%s%s", str1, str2, str3);
}

/* Concatenate 2 strings. Any string can be NULL.
   Caller needs to free() memory. */
char *str_cat(const char *str1, const char *str2)
{
    return str_cat3(str1, str2, NULL);
}

char *str_dupn(const char *str, size_t str_len_cch)
{
    char *copy;

    if (!str)
        return NULL;
    copy = memdup((void *)str, str_len_cch + 1);
    if (copy)
        copy[str_len_cch] = 0;
    return copy;
}

int str_copyn(char *dst, size_t dst_cch_size, const char *src, size_t src_cch_size)
{
    size_t len = min(src_cch_size + 1, dst_cch_size);
    
    strncpy(dst, src, len);
    dst[len - 1] = '\0';
    
    if (src_cch_size >= dst_cch_size)
        return FALSE;
    return TRUE;
}

int str_copy(char *dst, size_t dst_cch_size, const char *src)
{
    return str_copyn(dst, dst_cch_size, src, strlen(src));
}

int str_eq(const char *str1, const char *str2)
{
    if (!str1 && !str2)
        return TRUE;
    if (!str1 || !str2)
        return FALSE;
    if (0 == strcmp(str1, str2))
        return TRUE;
    return FALSE;
}

int str_ieq(const char *str1, const char *str2)
{
    if (!str1 && !str2)
        return TRUE;
    if (!str1 || !str2)
        return FALSE;
    if (0 == _stricmp(str1, str2))
        return TRUE;
    return FALSE;
}

int str_eqn(const char *str1, const char *str2, size_t len)
{
    if (!str1 && !str2)
        return TRUE;
    if (!str1 || !str2)
        return FALSE;
    if (0 == strncmp(str1, str2, len))
        return TRUE;
    return FALSE;
}

/* return true if 'str' starts with 'txt', case-sensitive */
int str_startswith(const char *str, const char *txt)
{
    return str_eqn(str, txt, strlen(txt));
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
int str_startswithi(const char *str, const char *txt)
{
    if (!str && !txt)
        return TRUE;
    if (!str || !txt)
        return FALSE;

    if (0 == _strnicmp(str, txt, strlen(txt)))
        return TRUE;
    return FALSE;
}

int str_endswith(const char *txt, const char *end)
{
    size_t end_len;
    size_t txt_len;

    if (!txt || !end)
        return FALSE;

    txt_len = strlen(txt);
    end_len = strlen(end);
    if (end_len > txt_len)
        return FALSE;
    if (str_eq(txt+txt_len-end_len, end))
        return TRUE;
    return FALSE;
}

int str_endswithi(const char *txt, const char *end)
{
    size_t end_len;
    size_t txt_len;

    if (!txt || !end)
        return FALSE;

    txt_len = strlen(txt);
    end_len = strlen(end);
    if (end_len > txt_len)
        return FALSE;
    if (str_ieq(txt+txt_len-end_len, end))
        return TRUE;
    return FALSE;
}

int str_empty(const char *str)
{
    if (!str)
        return TRUE;
    if (0 == *str)
        return TRUE;
    return FALSE;
}

/* split a string '*txt' at the border character 'c'. Something like python's
   string.split() except called iteratively.
   Returns a copy of the string (must be free()d by the caller).
   Returns NULL to indicate there's no more items. */
char *str_split_iter(char **txt, char c)
{
    const char *tmp;
    const char *pos;
    char *result;

    tmp = (const char*)*txt;
    if (!tmp)
        return NULL;

    pos = str_find_char(tmp, c);
    if (pos) {
         result = str_dupn(tmp, (int)(pos-tmp));
         *txt = (char*)pos+1;
    } else {
        result = str_dup(tmp);
        *txt = NULL; /* next iteration will return NULL */
    }
    return result;
}

/* Replace all posible versions (Unix, Windows, Mac) of newline character
   with 'replace'. Returns newly allocated string with normalized newlines
   or NULL if error.
   Caller needs to free() the result */
char *str_normalize_newline(const char *txt, const char *replace)
{
    size_t          replace_len;
    char            c;
    char *          result;
    const char *    tmp;
    char *          tmp_out;
    size_t          result_len = 0;

    replace_len = strlen(replace);
    tmp = txt;
    for (;;) {
        c = *tmp++;
        if (!c)
            break;
        if (0xa == c) {
            /* a single 0xa => Unix */
            result_len += replace_len;
        } else if (0xd == c) {
            if (0xa == *tmp) {
                /* 0xd 0xa => dos */
                result_len += replace_len;
                ++tmp;
            }
            else {
                /* just 0xd => Mac */
                result_len += replace_len;
            }
        } else
            ++result_len;
    }

    if (0 == result_len || (size_t)-1 == result_len)
        return NULL;

    result = (char*)malloc(result_len+1);
    if (!result)
        return NULL;
    tmp_out = result;
    for (;;) {
        c = *txt++;
        if (!c)
            break;
        if (0xa == c) {
            /* a single 0xa => Unix */
            memcpy(tmp_out, replace, replace_len);
            tmp_out += replace_len;
        } else if (0xd == c) {
            if (0xa == *txt) {
                /* 0xd 0xa => dos */
                memcpy(tmp_out, replace, replace_len);
                tmp_out += replace_len;
                ++txt;
            }
            else {
                /* just 0xd => Mac */
                memcpy(tmp_out, replace, replace_len);
                tmp_out += replace_len;
            }
        } else
            *tmp_out++ = c;
    }

    *tmp_out = 0;
    return result;
}

#define WHITE_SPACE_CHARS " \n\t\r"

/* Strip all 'to_strip' characters from the beginning of the string.
   Does stripping in-place */
void str_strip_left(char *txt, const char *to_strip)
{
    char *new_start = txt;
    char c;
    if (!txt || !to_strip)
        return;
    for (;;) {
        c = *new_start;
        if (0 == c)
            break;
        if (!str_contains(to_strip, c))
            break;
        ++new_start;
    }

    if (new_start != txt) {
        memmove(txt, new_start, strlen(new_start)+1);
    }
}

/* Strip white-space characters from the beginning of the string.
   Does stripping in-place */
void str_strip_ws_left(char *txt)
{
    str_strip_left(txt, WHITE_SPACE_CHARS);
}

void str_strip_right(char *txt, const char *to_strip)
{
    char * new_end;
    char   c;
    if (!txt || !to_strip)
        return;
    if (0 == *txt)
        return;
    /* point at the last character in the string */
    new_end = txt + strlen(txt) - 1;
    for (;;) {
        c = *new_end;
        if (!str_contains(to_strip, c))
            break;
        if (txt == new_end)
            break;
        --new_end;
    }
    if (str_contains(to_strip, *new_end))
        new_end[0] = 0;
    else
        new_end[1] = 0;
}

void str_strip_ws_right(char *txt)
{
    str_strip_right(txt, WHITE_SPACE_CHARS);
}

void str_strip_both(char *txt, const char *to_strip)
{
    str_strip_left(txt, to_strip);
    str_strip_right(txt, to_strip);
}

void str_strip_ws_both(char *txt)
{
    str_strip_ws_left(txt);
    str_strip_ws_right(txt);
}

/* Convert binary data in <buf> of size <len> to a hex-encoded string */
char *mem_to_hexstr(const unsigned char *buf, int len)
{
    int i;
    /* 2 hex chars per byte, +1 for terminating 0 */
    char *ret = calloc((size_t)len + 1, 2);
    if (!ret)
        return NULL;
    for (i = 0; i < len; i++) {
        sprintf(ret + 2 * i, "%02x", *buf++);
    }
    ret[2 * len] = '\0';
    return ret;
}

/* replace a string pointed by <dst> with a copy of <src>
   (i.e. free existing <dst>).
   Returns FALSE if failed to replace (due to out of memory) */
BOOL str_dup_replace(char **dst, const char *src)
{
    char *dup = (char*)strdup(src);
    if (!dup)
        return FALSE;
    free(*dst);
    *dst = dup;
    return TRUE;
}

/* Caller needs to free() the result */
static char *multibyte_to_multibyte(const char *src, UINT CodePage1, UINT CodePage2)
{
    char *res = NULL;
    WCHAR *tmp;
    int requiredBufSize = MultiByteToWideChar(CodePage1, 0, src, -1, NULL, 0);
    tmp = calloc(requiredBufSize, sizeof(WCHAR));
    if (!tmp)
        return NULL;
    MultiByteToWideChar(CodePage1, 0, src, -1, tmp, requiredBufSize);

    requiredBufSize = WideCharToMultiByte(CodePage2, 0, tmp, -1, NULL, 0, NULL, NULL);
    res = malloc(requiredBufSize);
    if (res)
        WideCharToMultiByte(CodePage2, 0, tmp, -1, res, requiredBufSize, NULL, NULL);
    free(tmp);

    return res;
}

/* Caller needs to free() the result */
char *str_to_multibyte(const char *src, UINT CodePage)
{
    if (CP_ACP == CodePage)
        return str_dup(src);

    return multibyte_to_multibyte(src, CP_ACP, CodePage);
}

/* Caller needs to free() the result */
char *multibyte_to_str(const char *src, UINT CodePage)
{
    if (CP_ACP == CodePage)
        return str_dup(src);

    return multibyte_to_multibyte(src, CodePage, CP_ACP);
}

/* Reverse of mem_to_hexstr. Convert a 0-terminatd hex-encoded string <s> to
   binary data pointed by <buf> of max sisze bufLen.
   Returns FALSE if size of <s> doesn't match <bufLen>. */
BOOL hexstr_to_mem(const char *s, unsigned char *buf, int bufLen)
{
    for (; bufLen > 0; bufLen--) {
        int c;
        if (1 != sscanf(s, "%02x", &c))
            return FALSE;
        s += 2;
        *buf++ = (unsigned char)c;
    }
    return *s == '\0';
}

int str_contains(const char *str, char c)
{
    const char *pos = str_find_char(str, c);
    if (!pos)
        return FALSE;
    return TRUE;
}

char *str_printf(const char *format, ...)
{
    char *result;
    va_list     args;
    va_start(args, format);
    result = str_printf_args(format, args);
    va_end(args);
    return result;
}

char *str_printf_args(const char *format, va_list args)
{
    char   message[256];
    size_t bufCchSize = dimof(message);
    char * buf = message;

    for (;;)
    {
        int count = vsnprintf(buf, bufCchSize, format, args);
        if (0 <= count && (size_t)count < bufCchSize)
            break;
        /* we have to make the buffer bigger. The algorithm used to calculate
           the new size is arbitrary (aka. educated guess) */
        if (buf != message)
            free(buf);
        if (bufCchSize < 4*1024)
            bufCchSize += bufCchSize;
        else
            bufCchSize += 1024;
        buf = (char *)malloc(bufCchSize);
        if (!buf)
            break;
    }

    if (buf == message)
        buf = str_dup(message);

    return buf;
}

int str_printf_s(char *out, size_t out_cch_size, const char *format, ...)
{
    va_list args;
    int count;

    va_start(args, format);
    count = vsnprintf(out, out_cch_size, format, args);
    if (count < 0 || (size_t)count >= out_cch_size)
        out[out_cch_size - 1] = '\0';
    va_end(args);

    return count;
}

void win32_dbg_out(const char *format, ...)
{
    char        buf[4096];
    char *      p = buf;
    va_list     args;

    va_start(args, format);
    _vsnprintf(p, sizeof(buf), format, args);
    OutputDebugStringA(buf);
    va_end(args);
}

void win32_dbg_out_hex(const char *dsc, const unsigned char *data, int dataLen)
{
    char *hexStr;
    if (!data) dataLen = 0;

    hexStr = mem_to_hexstr(data, dataLen);
    win32_dbg_out("%s%s\n", dsc ? dsc : "", hexStr);
    free(hexStr);
}

/* Return the number of digits needed to represents a given number in base 10
   string representation.
*/
size_t digits_for_number(int64_t num)
{
    size_t digits = 1;
    /* negative numbers need '-' in front of them */
    if (num < 0) {
        ++digits;
        num = -num;
    }

    while (num >= 10)
    {
        ++digits;
        num = num / 10;
    }
    return digits;
}
