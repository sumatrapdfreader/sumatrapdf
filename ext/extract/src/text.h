#ifndef EXTRACT_TEXT_H
#define EXTRACT_TEXT_H

#include "extract/alloc.h"

#include "astring.h"


int extract_content_insert(
        extract_alloc_t*    alloc,
        const char*         original,
        const char*         single_name,
        const char*         mid_begin_name,
        const char*         mid_end_name,
        extract_astring_t*  contentss,
        int                 contentss_num,
        char**              o_out
        );
/* Creates a new string by inserting sequence of strings into a template
string.

If <single_name> is in <original>, it is replaced by <contentss>.

Otherwise the text between the end of <mid_begin_name> and beginning of
<mid_end_name> is replaced by <contentss>.

If <mid_begin_name> is NULL, we insert into the zero-length region before
<mid_end_name>.

If <mid_end_name> is NULL, we insert into the zero-length region after
<mid_begin_name>.

At least one of <single_name>, <mid_begin_name> and <mid_end_name> must be
non-NULL.
*/

#endif
