#include "mupdf/pdf.h"

#include "pdf-encodings.h"
#include "pdf-glyphlist.h"

void
pdf_load_encoding(char **estrings, char *encoding)
{
	char **bstrings = NULL;
	int i;

	if (!strcmp(encoding, "StandardEncoding"))
		bstrings = (char**) pdf_standard;
	if (!strcmp(encoding, "MacRomanEncoding"))
		bstrings = (char**) pdf_mac_roman;
	if (!strcmp(encoding, "MacExpertEncoding"))
		bstrings = (char**) pdf_mac_expert;
	if (!strcmp(encoding, "WinAnsiEncoding"))
		bstrings = (char**) pdf_win_ansi;

	if (bstrings)
		for (i = 0; i < 256; i++)
			estrings[i] = bstrings[i];
}

int
pdf_lookup_agl(char *name)
{
	char buf[64];
	char *p;
	int l = 0;
	int r = nelem(agl_name_list) - 1;

	fz_strlcpy(buf, name, sizeof buf);

	/* kill anything after first period and underscore */
	p = strchr(buf, '.');
	if (p) p[0] = 0;
	p = strchr(buf, '_');
	if (p) p[0] = 0;

	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = strcmp(buf, agl_name_list[m]);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return agl_code_list[m];
	}

	if (strstr(buf, "uni") == buf)
		return strtol(buf + 3, NULL, 16);
	else if (strstr(buf, "u") == buf)
		return strtol(buf + 1, NULL, 16);
	else if (strstr(buf, "a") == buf && strlen(buf) >= 3)
		return strtol(buf + 1, NULL, 10);

	return 0;
}

static const char *empty_dup_list[] = { 0 };

const char **
pdf_lookup_agl_duplicates(int ucs)
{
	int l = 0;
	int r = nelem(agl_dup_offsets) / 2 - 1;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		if (ucs < agl_dup_offsets[m << 1])
			r = m - 1;
		else if (ucs > agl_dup_offsets[m << 1])
			l = m + 1;
		else
			return agl_dup_names + agl_dup_offsets[(m << 1) + 1];
	}
	return empty_dup_list;
}
