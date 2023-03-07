#include "astring.h"
#include "mem.h"
#include "memento.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void extract_astring_init(extract_astring_t *string)
{
	string->chars = NULL;
	string->chars_num = 0;
}

void extract_astring_free(extract_alloc_t *alloc, extract_astring_t *string)
{
	extract_free(alloc, &string->chars);
	extract_astring_init(string);
}


int extract_astring_catl(extract_alloc_t *alloc, extract_astring_t *string, const char *s, size_t s_len)
{
	if (extract_realloc2(alloc, &string->chars, string->chars_num+1, string->chars_num + s_len + 1))
		return -1;
	/* Coverity doesn't seem to realise that extract_realloc2() modifies
	string->chars. */
	/* coverity[deref_parm_field_in_call] */
	memcpy(string->chars + string->chars_num, s, s_len);
	string->chars[string->chars_num + s_len] = 0;
	string->chars_num += s_len;
	return 0;
}

int extract_astring_catc(extract_alloc_t *alloc, extract_astring_t *string, char c)
{
	return extract_astring_catl(alloc, string, &c, 1);
}

int extract_astring_cat(extract_alloc_t *alloc, extract_astring_t *string, const char *s)
{
	return extract_astring_catl(alloc, string, s, strlen(s));
}

int extract_astring_catf(extract_alloc_t *alloc, extract_astring_t *string, const char *format, ...)
{
	char    *buffer = NULL;
	int      e;
	va_list  va;

	va_start(va, format);
	e = extract_vasprintf(alloc, &buffer, format, va);
	va_end(va);
	if (e < 0) return e;
	e = extract_astring_cat(alloc, string, buffer);
	extract_free(alloc, &buffer);

	return e;
}

int extract_astring_truncate(extract_astring_t *content, int len)
{
	assert((size_t) len <= content->chars_num);

	content->chars_num -= len;
	content->chars[content->chars_num] = 0;

	return 0;
}

int extract_astring_char_truncate_if(extract_astring_t *content, char c)
{
	if (content->chars_num && content->chars[content->chars_num-1] == c)
		extract_astring_truncate(content, 1);

	return 0;
}

int extract_astring_catc_unicode(extract_alloc_t  *alloc,
				extract_astring_t *string,
				int                c,
				int                xml,
				int                ascii_ligatures,
				int                ascii_dash,
				int                ascii_apostrophe)
{
	int ret = -1;

	if (0) {}

	/* Escape XML special characters. */
	else if (xml && c == '<')  extract_astring_cat(alloc, string, "&lt;");
	else if (xml && c == '>')  extract_astring_cat(alloc, string, "&gt;");
	else if (xml && c == '&')  extract_astring_cat(alloc, string, "&amp;");
	else if (xml && c == '"')  extract_astring_cat(alloc, string, "&quot;");
	else if (xml && c == '\'') extract_astring_cat(alloc, string, "&apos;");

	/* Expand ligatures. */
	else if (ascii_ligatures && c == 0xFB00)
	{
		if (extract_astring_cat(alloc, string, "ff")) goto end;
	}
	else if (ascii_ligatures && c == 0xFB01)
	{
		if (extract_astring_cat(alloc, string, "fi")) goto end;
	}
	else if (ascii_ligatures && c == 0xFB02)
	{
		if (extract_astring_cat(alloc, string, "fl")) goto end;
	}
	else if (ascii_ligatures && c == 0xFB03)
	{
		if (extract_astring_cat(alloc, string, "ffi")) goto end;
	}
	else if (ascii_ligatures && c == 0xFB04)
	{
		if (extract_astring_cat(alloc, string, "ffl")) goto end;
	}

	/* Convert some special characters to ascii. */
	else if (ascii_dash && c == 0x2212)
	{
		if (extract_astring_catc(alloc, string, '-')) goto end;
	}
	else if (ascii_apostrophe && c == 0x2019)
	{
		if (extract_astring_catc(alloc, string, '\'')) goto end;
	}

	/* Output ASCII verbatim. */
	else if (c >= 32 && c <= 127)
	{
		if (extract_astring_catc(alloc, string, (char) c)) goto end;
	}

	/* Escape all other characters. */
	else if (xml)
	{
		char	buffer[32];
		if (c < 32 && (c != 0x9 && c != 0xa && c != 0xd))
		{
			/* Illegal xml character; see
			https://www.w3.org/TR/xml/#charsets. We replace with
			0xfffd, the unicode replacement character. */
			c = 0xfffd;
		}
		snprintf(buffer, sizeof(buffer), "&#x%x;", c);
		if (extract_astring_cat(alloc, string, buffer)) goto end;
	}
	else
	{
		/* Use utf8. */
		if (c < 0x80)
		{
			if (extract_astring_catc(alloc, string, (char) c)) return -1;
		}
		else if (c < 0x0800)
		{
			char cc[2] = {	(char) (((c >> 6) & 0x1f) | 0xc0),
					(char) (((c >> 0) & 0x3f) | 0x80) };
			if (extract_astring_catl(alloc, string, cc, sizeof(cc))) return -1;
		}
		else if (c < 0x10000)
		{
			char cc[3] = {	(char) (((c >> 12) & 0x0f) | 0xe0),
					(char) (((c >>  6) & 0x3f) | 0x80),
					(char) (((c >>  0) & 0x3f) | 0x80) };
			if (extract_astring_catl(alloc, string, cc, sizeof(cc))) return -1;
		}
		else if (c < 0x110000)
		{
			char cc[4] = {	(char) (((c >> 18) & 0x07) | 0xf0),
					(char) (((c >> 12) & 0x3f) | 0x80),
					(char) (((c >>  6) & 0x3f) | 0x80),
					(char) (((c >>  0) & 0x3f) | 0x80) };
			if (extract_astring_catl(alloc, string, cc, sizeof(cc))) return -1;
		}
		else
		{
			/* Use replacement character. */
			char cc[4] = { (char) 0xef, (char) 0xbf, (char) 0xbd, 0};
			if (extract_astring_catl(alloc, string, cc, sizeof(cc))) return -1;
		}
	}

	ret = 0;

end:
	return ret;
}

int extract_astring_catc_unicode_xml(extract_alloc_t *alloc, extract_astring_t *string, int c)
{
	/* FIXME: better to use ascii_ligatures=0, but that requires updates to
	expected output files. */
	return extract_astring_catc_unicode(
					alloc,
					string,
					c,
					1 /*xml*/,
					1 /*ascii_ligatures*/,
					0 /*ascii_dash*/,
					0 /*ascii_apostrophe*/
					);
}
