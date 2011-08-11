#ifndef __SUMATRAPDF__FTOPTION_H__
#define __SUMATRAPDF__FTOPTION_H__

#ifdef __FTOPTION_H__
#error "This header must be included before the default configuration header!"
#endif

#include <freetype/config/ftconfig.h>

FT_BEGIN_HEADER

#undef FT_CONFIG_OPTION_USE_LZW
#undef FT_CONFIG_OPTION_USE_ZLIB
#undef FT_CONFIG_OPTION_MAC_FONTS
#undef FT_CONFIG_OPTION_INCREMENTAL
#undef TT_CONFIG_OPTION_EMBEDDED_BITMAPS
#undef TT_CONFIG_OPTION_GX_VAR_SUPPORT
#undef TT_CONFIG_OPTION_BDF
#undef T1_CONFIG_OPTION_NO_AFM
#undef T1_CONFIG_OPTION_NO_MM_SUPPORT

#undef FT_CONFIG_OPTION_OLD_INTERNALS

#ifdef _DEBUG
#define FT_DEBUG_LEVEL_ERROR
#endif

FT_END_HEADER

#endif
