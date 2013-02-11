// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#pragma once
#include <vector>
#include <cstring>
#include <string>
#include <cassert>

#pragma warning(disable:4018)
#pragma warning(disable:4267)
#pragma warning(disable:4244)

typedef unsigned int u32;

inline char* sCopyString( char* a, const char* b, int len )
{
    return strncpy( a, b, len );
}
