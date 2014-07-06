/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "zip.h"

bool zip_uncompress_deflate(ar_archive_zip *zip, void *buffer, size_t count)
{
#ifdef HAVE_ZLIB
    struct ar_archive_zip_uncomp *uncomp = &zip->uncomp;

    if (!uncomp->method) {
        uncomp->method = METHOD_DEFLATE;
        memset(&uncomp->state.zstream, 0, sizeof(uncomp->state.zstream));
    }

    warn("TODO: Support Deflate");
    return false;
#else
    warn("Deflate support requires ZLIB");
    return false;
#endif
}

bool zip_uncompress_deflate64(ar_archive_zip *zip, void *buffer, size_t count)
{
#ifdef HAVE_ZLIB
    warn("TODO: Support Deflate64");
    return false;
#else
    warn("Deflate64 support requires a modified ZLIB");
    return false;
#endif
}

bool zip_uncompress_bzip2(ar_archive_zip *zip, void *buffer, size_t count)
{
#ifdef HAVE_BZIP2
    warn("TODO: Support BZIP2");
    return false;
#else
    warn("BZIP2 support requires BZIP2");
    return false;
#endif
}

bool zip_uncompress_lzma(ar_archive_zip *zip, void *buffer, size_t count)
{
#ifdef HAVE_LZMA
    warn("TODO: Support LZMA");
    return false;
#else
    warn("LZMA support requires LZMA SDK");
    return false;
#endif
}

void zip_clear_uncompress(struct ar_archive_zip_uncomp *uncomp)
{
#ifdef HAVE_ZLIB
    if (uncomp->method == METHOD_DEFLATE) {
        inflateEnd(&uncomp->state.zstream);
    }
#endif
    uncomp->method = 0;
}
