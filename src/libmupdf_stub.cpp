/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 (see COPYING) */

#include "BaseUtil.h"

/* Note: I don't quite know why I need to provide this for ucrt builds. 
Is it some kind of weak linking where DllMain() symbol is defined
in some .lib file that we exclude in ucrt build (like libc.lib)
but can be over-written by DllMain() in .obj file?
*/

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	return TRUE;
}