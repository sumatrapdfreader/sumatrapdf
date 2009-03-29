/*
SPARC specific render optims live here
*/
#include "fitz_base.h"
#include "fitz_tree.h"
#include "fitz_draw.h"

#ifdef HAVE_VIS

#endif

#if defined (ARCH_SPARC)
void
fz_accelerate(void)
{
#  ifdef HAVE_VIS
	if (fz_cpuflags & HAVE_VIS)
	{
	}
#  endif
}
#endif

