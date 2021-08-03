#ifndef ARTIFEX_EXTRACT_AUTOSTRING_XML
#define ARTIFEX_EXTRACT_AUTOSTRING_XML

/* Only for internal use by extract code.  */

/* A simple string struct that reallocs as required. */
typedef struct
{
    char*   chars;      /* NULL or zero-terminated. */
    size_t  chars_num;  /* Length of string pointed to by .chars. */
} extract_astring_t;

void extract_astring_init(extract_astring_t* string);

void extract_astring_free(extract_alloc_t* alloc, extract_astring_t* string);

int extract_astring_catl(extract_alloc_t* alloc, extract_astring_t* string, const char* s, size_t s_len);

int extract_astring_catc(extract_alloc_t* alloc, extract_astring_t* string, char c);

int extract_astring_cat(extract_alloc_t* alloc, extract_astring_t* string, const char* s);
int extract_astring_catf(extract_alloc_t* alloc, extract_astring_t* string, const char* format, ...);

int extract_astring_truncate(extract_astring_t* content, int len);
/* Removes last <len> chars. */

int astring_char_truncate_if(extract_astring_t* content, char c);
/* Removes last char if it is <c>. */

int extract_astring_cat_xmlc(extract_alloc_t* alloc, extract_astring_t* string, int c);
/* Appends specified character using XML escapes as necessary. */

#endif
