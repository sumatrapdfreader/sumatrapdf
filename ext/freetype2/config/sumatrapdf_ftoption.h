#ifndef __SUMATRAPDF__FTOPTION_H__
#define __SUMATRAPDF__FTOPTION_H__

#ifdef __FTOPTION_H__
#error "This header must be included before the default configuration header!"
#endif

#include <freetype/config/ftconfig.h>

FT_BEGIN_HEADER

#undef FT_CONFIG_OPTION_USE_LZW
#undef FT_CONFIG_OPTION_USE_ZLIB
#undef FT_CONFIG_OPTION_SYSTEM_ZLIB

#undef FT_CONFIG_OPTION_INCREMENTAL

#define TT_CONFIG_OPTION_BYTECODE_INTERPRETER
#undef TT_CONFIG_OPTION_UNPATENTED_HINTING

#undef FT_CONFIG_OPTION_OLD_INTERNALS

FT_END_HEADER

#endif
