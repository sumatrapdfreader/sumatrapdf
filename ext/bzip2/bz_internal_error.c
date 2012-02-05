/* Use when compiling with BZ_NO_STDIO */

#include <assert.h>

void bz_internal_error(int errcode)
{
	assert(0);
}
