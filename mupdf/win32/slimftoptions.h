/* custom ftoption.h which selects the minimum features needed by mupdf */

#include <freetype/config/ftoption.h>
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
