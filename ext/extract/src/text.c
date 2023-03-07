#include "text.h"

#include "astring.h"
#include "outf.h"

#include <assert.h>
#include <errno.h>
#include <string.h>


int
extract_content_insert(
		extract_alloc_t    *alloc,
		const char         *original,
		const char         *single_name,
		const char         *mid_begin_name,
		const char         *mid_end_name,
		extract_astring_t  *contentss,
		int                 contentss_num,
		char              **o_out)
{
	int                e         = -1;
	const char        *mid_begin = NULL;
	const char        *mid_end   = NULL;
	const char        *single    = NULL;
	extract_astring_t  out;
	extract_astring_init(&out);

	assert(single_name || mid_begin_name || mid_end_name);

	if (single_name) single = strstr(original, single_name);

	if (single)
	{
		outf("Have found single_name='%s', using in preference to mid_begin_name=%s mid_end_name=%s",
			 single_name,
			 mid_begin_name,
			 mid_end_name);
		mid_begin = single;
		mid_end = single + strlen(single_name);
	}
	else
	{
		if (mid_begin_name) {
			mid_begin = strstr(original, mid_begin_name);
			if (!mid_begin) {
				outf("error: could not find '%s' in odt content", mid_begin_name);
				errno = ESRCH;
				goto end;
			}
			mid_begin += strlen(mid_begin_name);
		}
		if (mid_end_name) {
			mid_end = strstr(mid_begin ? mid_begin : original, mid_end_name);
			if (!mid_end) {
				outf("error: could not find '%s' in odt content", mid_end_name);
				e = -1;
				errno = ESRCH;
				goto end;
			}
		}
		if (!mid_begin) {
			mid_begin = mid_end;
		}
		if (!mid_end) {
			mid_end = mid_begin;
		}
	}

	if (extract_astring_catl(alloc, &out, original, mid_begin - original)) goto end;
	{
		int i;
		for (i=0; i<contentss_num; ++i) {
			if (extract_astring_catl(alloc, &out, contentss[i].chars, contentss[i].chars_num)) goto end;
		}
	}
	assert( mid_end);
	/* As per docs, at least one of <single_name>, <mid_begin_name> and
	<mid_end_name> is non-null, and this ensures that mid_end must not be null.
	*/
	/* coverity[var_deref_model] */
	if (extract_astring_cat(alloc, &out, mid_end)) goto end;

	*o_out = out.chars;
	out.chars = NULL;

	e = 0;
end:

	if (e) {
		extract_astring_free(alloc, &out);
		*o_out = NULL;
	}

	return e;
}
