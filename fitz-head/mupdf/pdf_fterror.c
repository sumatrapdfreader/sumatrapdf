#include "fitz.h"
#include "mupdf.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#undef FT_ERRORDEF
#undef FT_NOERRORDEF
#undef FT_ERRORDEF_
#undef FT_NOERRORDEF_

#define FT_ERR_PREFIX               FT_Err_
#define FT_ERRORDEF(e, v, s)        s,
#define FT_ERRORDEF_(e, v, s)       FT_ERRORDEF(FT_ERR_CAT(FT_ERR_PREFIX, e), v + FT_ERR_BASE, s)
#define FT_NOERRORDEF(e, v, s)      s,
#define FT_NOERRORDEF_(e, v, s)     FT_ERRORDEF(FT_ERR_CAT(FT_ERR_PREFIX, e), v + FT_ERR_BASE, s)

static char *fterrorlist[] = {
#include FT_ERROR_DEFINITIONS_H
};

char *pdf_fterrorstring(int code)
{
    if (code < 0 || code >= sizeof (fterrorlist) / sizeof (char *))
        return "unknown error";
    return fterrorlist[code];
};

