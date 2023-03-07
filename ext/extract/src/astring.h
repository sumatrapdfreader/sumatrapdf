#ifndef ARTIFEX_EXTRACT_AUTOSTRING_XML
#define ARTIFEX_EXTRACT_AUTOSTRING_XML

#include "extract/alloc.h"

/* Only for internal use by extract code.  */

/* A simple string struct that reallocs as required. */
typedef struct
{
	char   *chars;      /* NULL or zero-terminated. */
	size_t  chars_num;  /* Length of string pointed to by .chars. */
} extract_astring_t;

/* Initialises <string> so it is ready for use. */
void extract_astring_init(extract_astring_t *string);

/* Frees any existing data and returns with <string> ready for use as if by
extract_astring_init(). */
void extract_astring_free(extract_alloc_t *alloc, extract_astring_t *string);

int extract_astring_catl(extract_alloc_t *alloc, extract_astring_t *string, const char *s, size_t s_len);

int extract_astring_catc(extract_alloc_t *alloc, extract_astring_t *string, char c);

int extract_astring_cat(extract_alloc_t *alloc, extract_astring_t *string, const char *s);
int extract_astring_catf(extract_alloc_t *alloc, extract_astring_t *string, const char *format, ...);

/* Removes last <len> chars. */
int extract_astring_truncate(extract_astring_t *content, int len);

/* Removes last char if it is <c>. */
int extract_astring_char_truncate_if(extract_astring_t *content, char c);

/* Appends specified character using XML escapes as necessary. */
int extract_astring_cat_xmlc(extract_alloc_t *alloc, extract_astring_t *string, int c);

/* Appends unicode character <c> to <string>.
	xml:
		If true, we use XML escape sequences for special characters
		such as '<' and unicode values above 127. Otherwise we encode
		as utf8.
	ascii_ligatures: if true we expand ligatures to "fl", "fi" etc.
	ascii_dash:
		If true we replace unicode dash characters with '-'.
	ascii_apostrophe:
		If true we replace unicode apostrophe with ascii single-quote "'".
*/
int extract_astring_catc_unicode(extract_alloc_t   *alloc,
				extract_astring_t *string,
				int c,
				int xml,
				int ascii_ligatures,
				int ascii_dash,
				int ascii_apostrophe);

/* Appends specific unicode character, using XML escape sequences as required. */
int extract_astring_catc_unicode_xml(extract_alloc_t *alloc, extract_astring_t *string, int c);

#endif
