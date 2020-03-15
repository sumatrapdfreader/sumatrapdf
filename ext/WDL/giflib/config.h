#ifndef _CONFIG_H_
#define _CONFIG_H_
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define _OPEN_BINARY
#else
typedef unsigned int UINT32;
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#endif
#include <fcntl.h>
#endif
