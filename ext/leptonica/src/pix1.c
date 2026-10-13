/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*!
 * \file pix1.c
 * <pre>
 *
 *    The pixN.c {N = 1,2,3,4,5} files are sorted by the type of operation.
 *    The primary functions in these files are:
 *
 *        pix1.c: constructors, destructors and field accessors
 *        pix2.c: pixel poking of image, pad and border pixels
 *        pix3.c: masking and logical ops, counting, mirrored tiling
 *        pix4.c: histograms, statistics, fg/bg estimation
 *        pix5.c: property measurements, rectangle extraction
 *
 *
 *    This file has the basic constructors, destructors and field accessors
 *
 *    Pix memory management (allows custom allocator and deallocator)
 *          static void  *pixdata_malloc()
 *          static void   pixdata_free()
 *          void          setPixMemoryManager()
 *
 *    Pix creation
 *          PIX          *pixCreate()
 *          PIX          *pixCreateNoInit()
 *          PIX          *pixCreateTemplate()
 *          PIX          *pixCreateTemplateNoInit()
 *          PIX          *pixCreateWithCmap()
 *          PIX          *pixCreateHeader()
 *          PIX          *pixClone()
 *
 *    Pix destruction
 *          void          pixDestroy()
 *          static void   pixFree()
 *
 *    Pix copy
 *          PIX          *pixCopy()
 *          l_int32       pixResizeImageData()
 *          l_int32       pixCopyColormap()
 *          l_int32       pixTransferAllData()
 *          l_int32       pixSwapAndDestroy()
 *
 *    Pix accessors
 *          l_int32       pixGetWidth()
 *          l_int32       pixSetWidth()
 *          l_int32       pixGetHeight()
 *          l_int32       pixSetHeight()
 *          l_int32       pixGetDepth()
 *          l_int32       pixSetDepth()
 *          l_int32       pixGetDimensions()
 *          l_int32       pixSetDimensions()
 *          l_int32       pixCopyDimensions()
 *          l_int32       pixGetSpp()
 *          l_int32       pixSetSpp()
 *          l_int32       pixCopySpp()
 *          l_int32       pixGetWpl()
 *          l_int32       pixSetWpl()
 *          l_uint32      pixGetXRes()
 *          l_int32       pixSetXRes()
 *          l_uint32      pixGetYRes()
 *          l_int32       pixSetYRes()
 *          l_int32       pixGetResolution()
 *          l_int32       pixSetResolution()
 *          l_int32       pixCopyResolution()
 *          l_int32       pixScaleResolution()
 *          l_int32       pixGetInputFormat()
 *          l_int32       pixSetInputFormat()
 *          l_int32       pixCopyInputFormat()
 *          l_int32       pixSetSpecial()
 *          char         *pixGetText()
 *          l_int32       pixSetText()
 *          l_int32       pixAddText()
 *          l_int32       pixCopyText()
 *          l_uint8      *pixGetTextCompNew()
 *          l_int32      *pixSetTextCompNew()
 *          PIXCMAP      *pixGetColormap()
 *          l_int32       pixSetColormap()
 *          l_int32       pixDestroyColormap()
 *          l_uint32     *pixGetData()
 *          l_int32       pixFreeAndSetData()
 *          l_int32       pixSetData()
 *          l_int32       pixFreeData()
 *          l_uint32     *pixExtractData()
 *
 *    Pix line ptrs
 *          void        **pixGetLinePtrs()
 *
 *    Pix size comparisons
 *          l_int32       pixSizesEqual()
 *          l_int32       pixMaxAspectRatio()
 *
 *    Pix debug
 *          l_int32       pixPrintStreamInfo()
 *
 *
 *  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *      Important notes on direct management of pix image data
 *  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *  Custom allocator and deallocator
 *  --------------------------------
 *
 *  At the lowest level, you can specify the function that does the
 *  allocation and deallocation of the data field in the pix.
 *  By default, this is malloc and free.  However, by calling
 *  setPixMemoryManager(), custom functions can be substituted.
 *  When using this, keep two things in mind:
 *
 *   (1) Call setPixMemoryManager() before any pix have been allocated
 *   (2) Destroy all pix as usual, in order to prevent leaks.
 *
 *  In pixalloc.c, we provide an example custom allocator and deallocator.
 *  To use it, you must call pmsCreate() before any pix have been allocated
 *  and pmsDestroy() at the end after all pix have been destroyed.
 *
 *
 *  Direct manipulation of the pix data field
 *  -----------------------------------------
 *
 *  Memory management of the (image) data field in the pix is
 *  handled differently from that in the colormap and text fields.
 *  For colormap and text, the functions pixSetColormap() and
 *  pixSetText() remove the existing heap data and insert the
 *  new data.  For the image data, pixSetData() just reassigns the
 *  data field; any existing data will be lost if there isn't
 *  another handle for it.
 *
 *  Why is pixSetData() limited in this way?  Because the image
 *  data can be very large, we need flexible ways to handle it,
 *  particularly when you want to re-use the data in a different
 *  context without making a copy.  Here are some different
 *  things you might want to do:
 *
 *  (1) Use pixCopy(pixd, pixs) where pixd is not the same size
 *      as pixs.  This will remove the data in pixd, allocate a
 *      new data field in pixd, and copy the data from pixs, leaving
 *      pixs unchanged.
 *
 *  (2) Use pixTransferAllData(pixd, &pixs, ...) to transfer the
 *      data from pixs to pixd without making a copy of it.  If
 *      pixs is not cloned, this will do the transfer and destroy pixs.
 *      But if the refcount of pixs is greater than 1, it just copies
 *      the data and decrements the ref count.
 *
 *  (3) Use pixSwapAndDestroy(pixd, &pixs) to replace pixs by an
 *      existing pixd.  This is similar to pixTransferAllData(), but
 *      simpler, in that it never makes any copies and if pixs is
 *      cloned, the other references are not changed by this operation.
 *
 *  (4) Use pixExtractData() to extract the image data from the pix
 *      without copying if possible.  This could be used, for example,
 *      to convert from a pix to some other data structure with minimal
 *      heap allocation.  After the data is extracated, the pixels can
 *      be munged and used in another context.  However, the danger
 *      here is that the pix might have a refcount > 1, in which case
 *      a copy of the data must be made and the input pix left unchanged.
 *      If there are no clones, the image data can be extracted without
 *      a copy, and the data ptr in the pix must be nulled before
 *      destroying it because the pix will no longer 'own' the data.
 *
 *  We have provided accessors and functions here that should be
 *  sufficient so that you can do anything you want without
 *  explicitly referencing any of the pix member fields.
 *
 *  However, to avoid memory smashes and leaks when doing special operations
 *  on the pix data field, look carefully at the behavior of the image
 *  data accessors and keep in mind that when you invoke pixDestroy(),
 *  the pix considers itself the owner of all its heap data.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"

static void pixFree(PIX *pix);

/*-------------------------------------------------------------------------*
 *                        Pix Memory Management                            *
 *                                                                         *
 *  These functions give you the freedom to specify at compile or run      *
 *  time the allocator and deallocator to be used for the pix raster       *
 *  image data.  They have no effect on any other heap allocation,         *
 *  including the pix struct itself, which is controlled by the            *
 *  #defines in environ.h.                                                 *
 *                                                                         *
 *  The default functions for allocating pix raster data are malloc and    *
 *  free (or leptonica_* custom allocators if LEPTONICA_INTERCEPT_ALLOC    *
 *  is defined).  Use setPixMemoryManager() to specify other functions     *
 *  to use specifically for pix raster image data.                         *
 *-------------------------------------------------------------------------*/
/*! Pix memory manager */
    /*
     * <pre>
     * Notes:
     *      (1) The allocator and deallocator function types,
     *          alloc_fn and dealloc_fn, are defined in pix.h.
     * </pre>
     */
struct PixMemoryManager
{
    alloc_fn    allocator;
    dealloc_fn  deallocator;
};

/*! Default Pix memory manager */
static struct PixMemoryManager  pix_mem_manager = {
#ifdef LEPTONICA_INTERCEPT_ALLOC
    &leptonica_malloc,
    &leptonica_free
#else
    &malloc,
    &free
#endif  /* LEPTONICA_INTERCEPT_ALLOC */
};

static void *
pixdata_malloc(size_t  size)
{
#ifndef _MSC_VER
    return (*pix_mem_manager.allocator)(size);
#else  /* _MSC_VER */
    /* Under MSVC++, pix_mem_manager is initialized after a call to
     * pixdata_malloc.  Just ignore the custom allocator feature. */
    return LEPT_MALLOC(size);
#endif  /* _MSC_VER */
}

static void
pixdata_free(void  *ptr)
{
#ifndef _MSC_VER
    (*pix_mem_manager.deallocator)(ptr);
#else  /* _MSC_VER */
    /* Under MSVC++, pix_mem_manager is initialized after a call to
     * pixdata_malloc.  Just ignore the custom allocator feature. */
    LEPT_FREE(ptr);
#endif  /* _MSC_VER */
}

/*!
 * \brief   setPixMemoryManager()
 *
 * \param[in]   allocator    [optional] use NULL to skip
 * \param[in]   deallocator  [optional] use NULL to skip
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Use this to change the alloc and/or dealloc functions;
 *          e.g., setPixMemoryManager(my_malloc, my_free).
 *      (2) The C99 standard (section 6.7.5.3, par. 8) says:
 *            A declaration of a parameter as "function returning type"
 *            shall be adjusted to "pointer to function returning type"
 *          so that it can be in either of these two forms:
 *            (a) type (function-ptr(type, ...))
 *            (b) type ((*function-ptr)(type, ...))
 *          because form (a) is implicitly converted to form (b), as in the
 *          definition of struct PixMemoryManager above.  So, for example,
 *          we should be able to declare either of these:
 *            (a) void *(allocator(size_t))
 *            (b) void *((*allocator)(size_t))
 *          However, MSVC++ only accepts the second version.
 * </pre>
 */
void
setPixMemoryManager(alloc_fn   allocator,
                    dealloc_fn deallocator)
{
    if (allocator) pix_mem_manager.allocator = allocator;
    if (deallocator) pix_mem_manager.deallocator = deallocator;
}


/*--------------------------------------------------------------------*
 *                             Pix Creation                           *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixCreate()
 *
 * \param[in]    width, height, depth
 * \return  pixd with data allocated and initialized to 0,
 *                    or NULL on error
 */
PIX *
pixCreate(l_int32  width,
          l_int32  height,
          l_int32  depth)
{
PIX  *pixd;

    if ((pixd = pixCreateNoInit(width, height, depth)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    memset(pixd->data, 0, 4LL * pixd->wpl * pixd->h);
    return pixd;
}


/*!
 * \brief   pixCreateNoInit()
 *
 * \param[in]    width, height, depth
 * \return  pixd with data allocated but not initialized,
 *                    or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Pad bits are set to avoid reading uninitialized data, because
 *          some optimized routines read from pad bits.
 *      (2) Initializing memory is very fast, so this optimization is
 *          not used in the library.
 * </pre>
 */
PIX *
pixCreateNoInit(l_int32  width,
                l_int32  height,
                l_int32  depth)
{
l_int32    wpl;
PIX       *pixd;
l_uint32  *data;

    if ((pixd = pixCreateHeader(width, height, depth)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    wpl = pixGetWpl(pixd);
    if ((data = (l_uint32 *)pixdata_malloc(4LL * wpl * height)) == NULL) {
        pixDestroy(&pixd);
        return (PIX *)ERROR_PTR("pixdata_malloc fail for data",
                                __func__, NULL);
    }
    pixSetData(pixd, data);
    pixSetPadBits(pixd, 0);
    return pixd;
}


/*!
 * \brief   pixCreateTemplate()
 *
 * \param[in]    pixs
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Makes a Pix of the same size as the input Pix, with the
 *          data array allocated and initialized to 0.
 *      (2) Copies the other fields, including colormap if it exists.
 * </pre>
 */
PIX *
pixCreateTemplate(const PIX  *pixs)
{
PIX  *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    if ((pixd = pixCreateTemplateNoInit(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    memset(pixd->data, 0, 4LL * pixd->wpl * pixd->h);
    return pixd;
}


/*!
 * \brief   pixCreateTemplateNoInit()
 *
 * \param[in]    pixs
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Makes a Pix of the same size as the input Pix, with
 *          the data array allocated but not initialized to 0.
 *      (2) Copies the other fields, including colormap if it exists.
 *      (3) Pad bits are set to avoid reading uninitialized data, because
 *          some optimized routines read from pad bits.
 *      (4) Initializing memory is very fast, so this optimization is
 *          not used in the library.
 * </pre>
 */
PIX *
pixCreateTemplateNoInit(const PIX  *pixs)
{
l_int32  w, h, d;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, &d);
    if ((pixd = pixCreateNoInit(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopySpp(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixCopyColormap(pixd, pixs);
    pixCopyText(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixSetPadBits(pixd, 0);
    return pixd;
}


/*!
 * \brief   pixCreateWithCmap()
 *
 * \param[in]    width
 * \param[in]    height
 * \param[in]    depth        2, 4 or 8 bpp
 * \param[in]    initcolor    L_SET_BLACK, L_SET_WHITE
 * \return  pixd   with the initialization color assigned to all pixels,
 *                 or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) Creates a pix with a cmap, initialized to value 0.
 *      (2) Initializes the pix black or white by adding that color
 *          to the cmap at index 0.
 * </pre>
 */
PIX *
pixCreateWithCmap(l_int32  width,
                 l_int32  height,
                 l_int32  depth,
                 l_int32  initcolor)
{
PIX       *pix;
PIXCMAP   *cmap;

    if (depth != 2 && depth != 4 && depth != 8)
        return (PIX *)ERROR_PTR("depth not 2, 4 or 8 bpp", __func__, NULL);

    if ((pix = pixCreate(width, height, depth)) == NULL)
        return (PIX *)ERROR_PTR("pix not made", __func__, NULL);
    cmap = pixcmapCreate(depth);
    pixSetColormap(pix, cmap);
    if (initcolor == L_SET_BLACK)
         pixcmapAddColor(cmap, 0, 0, 0);
    else  /* L_SET_WHITE */
         pixcmapAddColor(cmap, 255, 255, 255);
    return pix;
}


/*!
 * \brief   pixCreateHeader()
 *
 * \param[in]    width, height, depth
 * \return  pixd with no data allocated, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) It is assumed that all 32 bit pix have 3 spp.  If there is
 *          a valid alpha channel, this will be set to 4 spp later.
 *      (2) All pixCreate*() functions call pixCreateHeader().
            If the number of bytes to be allocated is larger than the
 *          maximum value in an int32, we can get overflow, resulting
 *          in a smaller amount of memory actually being allocated.
 *          Later, an attempt to access memory that wasn't allocated will
 *          cause a crash.  So to avoid crashing a program (or worse)
 *          with bad (or malicious) input, we limit the requested
 *          allocation of image data in a typesafe way.
 * </pre>
 */
PIX *
pixCreateHeader(l_int32  width,
                l_int32  height,
                l_int32  depth)
{
l_int32   wpl;
l_uint64  wpl64, bignum;
PIX      *pixd;

    if ((depth != 1) && (depth != 2) && (depth != 4) && (depth != 8)
         && (depth != 16) && (depth != 24) && (depth != 32))
        return (PIX *)ERROR_PTR("depth must be {1, 2, 4, 8, 16, 24, 32}",
                                __func__, NULL);
    if (width <= 0)
        return (PIX *)ERROR_PTR("width must be > 0", __func__, NULL);
    if (height <= 0)
        return (PIX *)ERROR_PTR("height must be > 0", __func__, NULL);

        /* Avoid overflow in malloc, malicious or otherwise */
    wpl64 = ((l_uint64)width * (l_uint64)depth + 31) / 32;
    if (wpl64 > ((1LL << 24) - 1)) {
        L_ERROR("requested w = %d, h = %d, d = %d\n",
                __func__, width, height, depth);
        return (PIX *)ERROR_PTR("wpl >= 2^24", __func__, NULL);
    }
    wpl = (l_int32)wpl64;
    bignum = 4LL * wpl * height;   /* number of bytes to be requested */
    if (bignum > ((1LL << 31) - 1)) {
        L_ERROR("requested w = %d, h = %d, d = %d\n",
                __func__, width, height, depth);
        return (PIX *)ERROR_PTR("requested bytes >= 2^31", __func__, NULL);
    }

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (bignum > (1LL << 26)) {
        L_ERROR("fuzzer requested > 64 MB; refused\n", __func__);
        return NULL;
    }
    if (width > 20000) {
        L_ERROR("fuzzer requested width > 20K; refused\n", __func__);
        return NULL;
    }
    if (height > 20000) {
        L_ERROR("fuzzer requested height > 20K; refused\n", __func__);
        return NULL;
    }
#endif   /* FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION */

    if ((pixd = (PIX *)LEPT_CALLOC(1, sizeof(PIX))) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixSetWidth(pixd, width);
    pixSetHeight(pixd, height);
    pixSetDepth(pixd, depth);
    pixSetWpl(pixd, wpl);
    if (depth == 24 || depth == 32)
        pixSetSpp(pixd, 3);
    else
        pixSetSpp(pixd, 1);
    pixd->refcount = 1;
    pixd->informat = IFF_UNKNOWN;
    return pixd;
}


/*!
 * \brief   pixClone()
 *
 * \param[in]    pixs
 * \return  same pix ptr, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) A "clone" is simply a handle (ptr) to an existing pix.
 *          It is implemented because (a) images can be large and
 *          hence expensive to copy, and (b) extra handles to a data
 *          structure need to be made with a simple policy to avoid
 *          both double frees and memory leaks.  Pix are reference
 *          counted.  The side effect of pixClone() is an increase
 *          by 1 in the ref count.
 *      (2) The protocol to be used is:
 *          (a) Whenever you want a new handle to an existing image,
 *              call pixClone(), which just bumps a ref count.
 *          (b) Always call pixDestroy() on all handles.  This
 *              decrements the ref count, nulls the handle, and
 *              only destroys the pix when pixDestroy() has been
 *              called on all handles.
 * </pre>
 */
PIX *
pixClone(PIX  *pixs)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    ++pixs->refcount;

    return pixs;
}


/*--------------------------------------------------------------------*
 *                           Pix Destruction                          *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixDestroy()
 *
 * \param[in,out]   ppix     will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the pix.
 *      (2) Always nulls the input ptr.
 * </pre>
 */
void
pixDestroy(PIX  **ppix)
{
PIX  *pix;

    if (!ppix) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((pix = *ppix) == NULL)
        return;
    pixFree(pix);
    *ppix = NULL;
}


/*!
 * \brief   pixFree()
 *
 * \param[in]    pix
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the pix.
 * </pre>
 */
static void
pixFree(PIX  *pix)
{
l_uint32  *data;
char      *text;

    if (!pix) return;

    if (--pix->refcount == 0) {
        if ((data = pixGetData(pix)) != NULL)
            pixdata_free(data);
        if ((text = pixGetText(pix)) != NULL)
            LEPT_FREE(text);
        pixDestroyColormap(pix);
        LEPT_FREE(pix);
    }
    return;
}


/*-------------------------------------------------------------------------*
 *                                 Pix Copy                                *
 *-------------------------------------------------------------------------*/
/*!
 * \brief   pixCopy()
 *
 * \param[in]    pixd   [optional] can be null, equal to pixs,
 *                      different from pixs
 * \param[in]    pixs
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) There are three cases:
 *            (a) pixd == null  (makes a new pix; refcount = 1)
 *            (b) pixd == pixs  (no-op)
 *            (c) pixd != pixs  (data copy; no change in refcount)
 *          If the refcount of pixd > 1, case (c) will side-effect
 *          these handles.
 *      (2) The general pattern of use is:
 *             pixd = pixCopy(pixd, pixs);
 *          This will work for all three cases.
 *          For clarity when the case is known, you can use:
 *            (a) pixd = pixCopy(NULL, pixs);
 *            (c) pixCopy(pixd, pixs);
 *      (3) For case (c), we check if pixs and pixd are the same
 *          size (w,h,d).  If so, the data is copied directly.
 *          Otherwise, the data is reallocated to the correct size
 *          and the copy proceeds.  The refcount of pixd is unchanged.
 *      (4) This operation, like all others that may involve a pre-existing
 *          pixd, will side-effect any existing clones of pixd.
 * </pre>
 */
PIX *
pixCopy(PIX        *pixd,   /* can be null */
        const PIX  *pixs)
{
l_int32  bytes;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixs == pixd)
        return pixd;

        /* Total bytes in image data */
    bytes = 4 * pixGetWpl(pixs) * pixGetHeight(pixs);

        /* If we're making a new pix ... */
    if (!pixd) {
        if ((pixd = pixCreateTemplate(pixs)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
        memcpy(pixd->data, pixs->data, bytes);
        return pixd;
    }

        /* Reallocate image data if sizes are different.  If this fails,
         * pixd hasn't been changed.  But we want to signal that the copy
         * failed, so return NULL.  This will cause a memory leak if the
         * return ptr is assigned to pixd, but that is preferred to proceeding
         * with an incorrect pixd, and in any event this use case of
         * pixCopy() -- reallocating into an existing pix -- is infrequent.  */
    if (pixResizeImageData(pixd, pixs) == 1)
        return (PIX *)ERROR_PTR("reallocation of data failed", __func__, NULL);

        /* Copy non-image data fields */
    pixCopyColormap(pixd, pixs);
    pixCopySpp(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixCopyText(pixd, pixs);

        /* Copy image data */
    memcpy(pixd->data, pixs->data, bytes);
    return pixd;
}


/*!
 * \brief   pixResizeImageData()
 *
 * \param[in]   pixd   gets new uninitialized buffer for image data
 * \param[in]   pixs   determines the size of the buffer; not changed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If the sizes of data in pixs and pixd are unequal, this
 *          frees the existing image data in pixd and allocates
 *          an uninitialized buffer that will hold the required amount
 *          of image data in pixs.  The image data from pixs is not
 *          copied into the new buffer.
 *      (2) On failure to allocate, pixd is unchanged.
 * </pre>
 */
l_ok
pixResizeImageData(PIX        *pixd,
                   const PIX  *pixs)
{
l_int32    w, h, d, wpl, bytes;
l_uint32  *data;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);

    if (pixSizesEqual(pixs, pixd))  /* nothing to do */
        return 0;

        /* Make sure we can copy the data */
    pixGetDimensions(pixs, &w, &h, &d);
    wpl = pixGetWpl(pixs);
    bytes = 4 * wpl * h;
    if ((data = (l_uint32 *)pixdata_malloc(bytes)) == NULL)
        return ERROR_INT("pixdata_malloc fail for data", __func__, 1);

        /* OK, do it */
    pixSetWidth(pixd, w);
    pixSetHeight(pixd, h);
    pixSetDepth(pixd, d);
    pixSetWpl(pixd, wpl);
    pixFreeAndSetData(pixd, data);  /* free old data and assign new data */
    pixCopyResolution(pixd, pixs);
    return 0;
}


/*!
 * \brief   pixCopyColormap()
 *
 * \param[in]   pixd
 * \param[in]   pixs   copies the colormap to %pixd
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This destroys the colormap in pixd, unless the operation is a no-op
 * </pre>
 */
l_ok
pixCopyColormap(PIX        *pixd,
                const PIX  *pixs)
{
l_int32         valid;
const PIXCMAP  *cmaps;
PIXCMAP        *cmapd;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (pixs == pixd)
        return 0;   /* no-op */
    if (pixGetDepth(pixs) != pixGetDepth(pixd))
        return ERROR_INT("depths of pixs and pixd differ", __func__, 1);

    pixDestroyColormap(pixd);
    if ((cmaps = pixs->colormap) == NULL)  /* not an error */
        return 0;
    pixcmapIsValid(cmaps, NULL, &valid);
    if (!valid)
        return ERROR_INT("cmap not valid", __func__, 1);

    if ((cmapd = pixcmapCopy(cmaps)) == NULL)
        return ERROR_INT("cmapd not made", __func__, 1);
    pixSetColormap(pixd, cmapd);
    return 0;
}


/*!
 * \brief   pixTransferAllData()
 *
 * \param[in]      pixd        must be different from pixs
 * \param[in,out]  ppixs       will be nulled if refcount goes to 0
 * \param[in]      copytext    1 to copy the text field; 0 to skip
 * \param[in]      copyformat  1 to copy the informat field; 0 to skip
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a complete data transfer from pixs to pixd,
 *          followed by the destruction of pixs (refcount permitting).
 *      (2) If the refcount of pixs is 1, pixs is destroyed.  Otherwise,
 *          the data in pixs is copied (rather than transferred) to pixd.
 *      (3) This operation, like all others with a pre-existing pixd,
 *          will side-effect any existing clones of pixd.  The pixd
 *          refcount does not change.
 *      (4) When might you use this?  Suppose you have an in-place Pix
 *          function (returning void) with the typical signature:
 *              void function-inplace(PIX *pix, ...)
 *          where "..." are non-pointer input parameters, and suppose
 *          further that you sometimes want to return an arbitrary Pix
 *          in place of the input Pix.  There are two ways you can do this:
 *          (a) The straightforward way is to change the function
 *              signature to take the address of the Pix ptr:
 * \code
 *                  void function-inplace(PIX **ppix, ...) {
 *                      PIX *pixt = function-makenew(*ppix);
 *                      pixDestroy(ppix);
 *                      *ppix = pixt;
 *                      return;
 *                  }
 * \endcode
 *              Here, the input and returned pix are different, as viewed
 *              by the calling function, and the inplace function is
 *              expected to destroy the input pix to avoid a memory leak.
 *          (b) Keep the signature the same and use pixTransferAllData()
 *              to return the new Pix in the input Pix struct:
 * \code
 *                  void function-inplace(PIX *pix, ...) {
 *                      PIX *pixt = function-makenew(pix);
 *                      pixTransferAllData(pix, &pixt, 0, 0);
 *                               // pixDestroy() is called on pixt
 *                      return;
 *                  }
 * \endcode
 *              Here, the input and returned pix are the same, as viewed
 *              by the calling function, and the inplace function must
 *              never destroy the input pix, because the calling function
 *              maintains an unchanged handle to it.
 * </pre>
 */
l_ok
pixTransferAllData(PIX     *pixd,
                   PIX    **ppixs,
                   l_int32  copytext,
                   l_int32  copyformat)
{
l_int32  nbytes;
PIX     *pixs;

    if (!ppixs)
        return ERROR_INT("&pixs not defined", __func__, 1);
    if ((pixs = *ppixs) == NULL)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (pixs == pixd)  /* no-op */
        return ERROR_INT("pixd == pixs", __func__, 1);

    if (pixs->refcount == 1) {  /* transfer the data, cmap, text */
        pixFreeData(pixd);  /* dealloc any existing data */
        pixSetData(pixd, pixGetData(pixs));  /* transfer new data from pixs */
        pixs->data = NULL;  /* pixs no longer owns data */
        pixDestroyColormap(pixd);  /* free the old one, if it exists */
        pixd->colormap = pixGetColormap(pixs);  /* transfer to pixd */
        pixs->colormap = NULL;  /* pixs no longer owns colormap */
        if (copytext) {
            pixSetText(pixd, pixGetText(pixs));
            pixSetText(pixs, NULL);
        }
    } else {  /* preserve pixs by making a copy of the data, cmap, text */
        pixResizeImageData(pixd, pixs);
        nbytes = 4 * pixGetWpl(pixs) * pixGetHeight(pixs);
        memcpy(pixGetData(pixd), pixGetData(pixs), nbytes);
        pixCopyColormap(pixd, pixs);
        if (copytext)
            pixCopyText(pixd, pixs);
    }

    pixCopySpp(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixCopyDimensions(pixd, pixs);
    if (copyformat)
        pixCopyInputFormat(pixd, pixs);

        /* This will destroy pixs if data was transferred;
         * otherwise, it just decrements its refcount. */
    pixDestroy(ppixs);
    return 0;
}


/*!
 * \brief   pixSwapAndDestroy()
 *
 * \param[out]     ppixd   [optional] input pixd can be null,
 *                         and it must be different from pixs
 * \param[in,out]  ppixs   will be nulled after the swap
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Simple operation to change the handle name safely.
 *          After this operation, the original image in pixd has
 *          been destroyed, pixd points to what was pixs, and
 *          the input pixs ptr has been nulled.
 *      (2) This works safely whether or not pixs and pixd are cloned.
 *          If pixs is cloned, the other handles still point to
 *          the original image, with the ref count reduced by 1.
 *      (3) Usage example:
 * \code
 *            Pix *pix1 = pixRead("...");
 *            Pix *pix2 = function(pix1, ...);
 *            pixSwapAndDestroy(&pix1, &pix2);
 *            pixDestroy(&pix1);  // holds what was in pix2
 * \endcode
 *          Example with clones ([] shows ref count of image generated
 *                               by the function):
 * \code
 *            Pix *pixs = pixRead("...");
 *            Pix *pix1 = pixClone(pixs);
 *            Pix *pix2 = function(pix1, ...);   [1]
 *            Pix *pix3 = pixClone(pix2);   [1] --> [2]
 *            pixSwapAndDestroy(&pix1, &pix2);
 *            pixDestroy(&pixs);  // still holds read image
 *            pixDestroy(&pix1);  // holds what was in pix2  [2] --> [1]
 *            pixDestroy(&pix3);  // holds what was in pix2  [1] --> [0]
 * \endcode
 * </pre>
 */
l_ok
pixSwapAndDestroy(PIX  **ppixd,
                  PIX  **ppixs)
{
    if (!ppixd)
        return ERROR_INT("&pixd not defined", __func__, 1);
    if (!ppixs)
        return ERROR_INT("&pixs not defined", __func__, 1);
    if (*ppixs == NULL)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (ppixs == ppixd)  /* no-op */
        return ERROR_INT("&pixd == &pixs", __func__, 1);

    pixDestroy(ppixd);
    *ppixd = pixClone(*ppixs);
    pixDestroy(ppixs);
    return 0;
}


/*--------------------------------------------------------------------*
 *                              Pix Accessors                         *
 *--------------------------------------------------------------------*/
l_int32
pixGetWidth(const PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);

    return pix->w;
}


l_int32
pixSetWidth(PIX     *pix,
            l_int32  width)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (width < 0) {
        pix->w = 0;
        return ERROR_INT("width must be >= 0", __func__, 1);
    }

    pix->w = width;
    return 0;
}


l_int32
pixGetHeight(const PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);

    return pix->h;
}


l_int32
pixSetHeight(PIX     *pix,
             l_int32  height)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (height < 0) {
        pix->h = 0;
        return ERROR_INT("h must be >= 0", __func__, 1);
    }

    pix->h = height;
    return 0;
}


l_int32
pixGetDepth(const PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);

    return pix->d;
}


l_int32
pixSetDepth(PIX     *pix,
            l_int32  depth)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (depth < 1)
        return ERROR_INT("d must be >= 1", __func__, 1);

    pix->d = depth;
    return 0;
}


/*!
 * \brief   pixGetDimensions()
 *
 * \param[in]    pix
 * \param[out]   pw, ph, pd    [optional] each can be null
 * \return  0 if OK, 1 on error
 */
l_ok
pixGetDimensions(const PIX  *pix,
                 l_int32    *pw,
                 l_int32    *ph,
                 l_int32    *pd)
{
    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (pd) *pd = 0;
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (pw) *pw = pix->w;
    if (ph) *ph = pix->h;
    if (pd) *pd = pix->d;
    return 0;
}


/*!
 * \brief   pixSetDimensions()
 *
 * \param[in]    pix
 * \param[in]    w, h, d   use 0 to skip the setting for any of these
 * \return  0 if OK, 1 on error
 */
l_ok
pixSetDimensions(PIX     *pix,
                 l_int32  w,
                 l_int32  h,
                 l_int32  d)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (w > 0) pixSetWidth(pix, w);
    if (h > 0) pixSetHeight(pix, h);
    if (d > 0) pixSetDepth(pix, d);
    return 0;
}


/*!
 * \brief   pixCopyDimensions()
 *
 * \param[in]   pixd
 * \param[in]   pixs
 * \return  0 if OK, 1 on error
 */
l_ok
pixCopyDimensions(PIX        *pixd,
                  const PIX  *pixs)
{
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixs == pixd)
        return 0;   /* no-op */

    pixSetWidth(pixd, pixGetWidth(pixs));
    pixSetHeight(pixd, pixGetHeight(pixs));
    pixSetDepth(pixd, pixGetDepth(pixs));
    pixSetWpl(pixd, pixGetWpl(pixs));
    return 0;
}


l_int32
pixGetSpp(const PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);

    return pix->spp;
}


/*
 * \brief   pixSetSpp()
 *
 * \param[in]   pix
 * \param[in]   spp   1, 3 or 4 samples
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For a 32 bpp pix, this can be used to ignore the
 *          alpha sample (spp == 3) or to use it (spp == 4).
 *          For example, to write a spp == 4 image without the alpha
 *          sample (as an rgb pix), call pixSetSpp(pix, 3) and
 *          then write it out as a png.
 * </pre>
 */
l_int32
pixSetSpp(PIX     *pix,
          l_int32  spp)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (spp < 1)
        return ERROR_INT("spp must be >= 1", __func__, 1);

    pix->spp = spp;
    return 0;
}


/*!
 * \brief   pixCopySpp()
 *
 * \param[in]   pixd
 * \param[in]   pixs
 * \return  0 if OK, 1 on error
 */
l_ok
pixCopySpp(PIX        *pixd,
           const PIX  *pixs)
{
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixs == pixd)
        return 0;   /* no-op */

    pixSetSpp(pixd, pixGetSpp(pixs));
    return 0;
}


l_int32
pixGetWpl(const PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);
    return pix->wpl;
}


l_int32
pixSetWpl(PIX     *pix,
          l_int32  wpl)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pix->wpl = wpl;
    return 0;
}


l_int32
pixGetXRes(const PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);
    return pix->xres;
}


l_int32
pixSetXRes(PIX     *pix,
           l_int32  res)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pix->xres = res;
    return 0;
}


l_int32
pixGetYRes(const PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);
    return pix->yres;
}


l_int32
pixSetYRes(PIX     *pix,
           l_int32  res)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pix->yres = res;
    return 0;
}


/*!
 * \brief   pixGetResolution()
 *
 * \param[in]    pix
 * \param[out]   pxres, pyres   [optional] each can be null
 * \return  0 if OK, 1 on error
 */
l_ok
pixGetResolution(const PIX  *pix,
                 l_int32    *pxres,
                 l_int32    *pyres)
{
    if (pxres) *pxres = 0;
    if (pyres) *pyres = 0;
    if (!pxres && !pyres)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (pxres) *pxres = pix->xres;
    if (pyres) *pyres = pix->yres;
    return 0;
}


/*!
 * \brief   pixSetResolution()
 *
 * \param[in]   pix
 * \param[in]   xres, yres   use 0 to skip setting a value for either of these
 * \return  0 if OK, 1 on error
 */
l_ok
pixSetResolution(PIX     *pix,
                 l_int32  xres,
                 l_int32  yres)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (xres > 0) pix->xres = xres;
    if (yres > 0) pix->yres = yres;
    return 0;
}


l_int32
pixCopyResolution(PIX        *pixd,
                  const PIX  *pixs)
{
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (pixs == pixd)
        return 0;   /* no-op */

    pixSetXRes(pixd, pixGetXRes(pixs));
    pixSetYRes(pixd, pixGetYRes(pixs));
    return 0;
}


l_int32
pixScaleResolution(PIX       *pix,
                   l_float32  xscale,
                   l_float32  yscale)
{
l_float64  xres, yres;
l_float64  maxres = 100000000.0;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (xscale <= 0 || yscale <= 0)
        return ERROR_INT("invalid scaling ratio", __func__, 1);

    xres = (l_float64)xscale * (l_float32)(pix->xres) + 0.5;
    yres = (l_float64)yscale * (l_float32)(pix->yres) + 0.5;
    pix->xres = (l_uint32)L_MIN(xres, maxres);
    pix->yres = (l_uint32)L_MIN(yres, maxres);
    return 0;
}


l_int32
pixGetInputFormat(const PIX  *pix)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);
    return pix->informat;
}


l_int32
pixSetInputFormat(PIX     *pix,
                  l_int32  informat)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pix->informat = informat;
    return 0;
}


l_int32
pixCopyInputFormat(PIX        *pixd,
                   const PIX  *pixs)
{
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (pixs == pixd)
        return 0;   /* no-op */

    pixSetInputFormat(pixd, pixGetInputFormat(pixs));
    return 0;
}


l_int32
pixSetSpecial(PIX     *pix,
              l_int32  special)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pix->special = special;
    return 0;
}


/*!
 * \brief   pixGetText()
 *
 * \param[in]   pix
 * \return  ptr to existing text string
 *
 * <pre>
 * Notes:
 *      (1) The text string belongs to the pix:
 *          * the caller must NOT free it
 *          * it must not be used after the pix is destroyed
 * </pre>
 */
char *
pixGetText(PIX  *pix)
{
    if (!pix)
        return (char *)ERROR_PTR("pix not defined", __func__, NULL);
    return pix->text;
}


/*!
 * \brief   pixSetText()
 *
 * \param[in]   pix
 * \param[in]   textstring   can be null
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This removes any existing textstring and puts a copy of
 *          the input textstring there.
 * </pre>
 */
l_ok
pixSetText(PIX         *pix,
           const char  *textstring)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    stringReplace(&pix->text, textstring);
    return 0;
}


/*!
 * \brief   pixAddText()
 *
 * \param[in]    pix
 * \param[in]    textstring   can be null
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This adds the new textstring to any existing text.
 *      (2) Either or both the existing text and the new text
 *          string can be null.
 * </pre>
 */
l_ok
pixAddText(PIX         *pix,
           const char  *textstring)
{
char  *newstring;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    newstring = stringJoin(pixGetText(pix), textstring);
    stringReplace(&pix->text, newstring);
    LEPT_FREE(newstring);
    return 0;
}


l_int32
pixCopyText(PIX        *pixd,
            const PIX  *pixs)
{
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (pixs == pixd)
        return 0;   /* no-op */

    pixSetText(pixd, pixs->text);
    return 0;
}


/*!
 * \brief   pixGetTextCompNew()
 *
 * \param[in]   pix
 * \param[out]  psize    this number of bytes of returned binary data
 * \return  ptr to binary data derived from the text string in the pix,
 *          after decoding and uncompressing
 *
 * <pre>
 * Notes:
 *      (1) The ascii string in the text field of the input pix was
 *          previously stored there using pixSetTextCompNew().
 *      (2) This retrieves the string and performs ascii85 decoding
 *          followed by decompression on it.  The returned binary data
 *          is owned by the caller and must be freed.
 * </pre>
 */
l_uint8 *
pixGetTextCompNew(PIX     *pix,
                  size_t  *psize)
{
char  *str;

    if (!pix)
        return (l_uint8 *)ERROR_PTR("pix not defined", __func__, NULL);
    str = pixGetText(pix);
    return decodeAscii85WithComp(str, strlen(str), psize);
}


/*!
 * \brief   pixSetTextCompNew()
 *
 * \param[in]   pix
 * \param[in]   data    binary data
 * \param[in]   size    number of bytes of binary data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This receives binary data and performs compression and ascii85
 *          encoding on it.  The ascii result is stored in the input pix,
 *          replacing any string that may be there.
 *      (2) The input %data can be reconstructed using pixGetTextCompNew().
 * </pre>
 */
l_ok
pixSetTextCompNew(PIX            *pix,
                  const l_uint8  *data,
                  size_t          size)
{
size_t  encodesize;  /* ignored */

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    LEPT_FREE(pix->text);
    pix->text = encodeAscii85WithComp(data, size, &encodesize);
    return 0;
}


PIXCMAP *
pixGetColormap(PIX  *pix)
{
    if (!pix)
        return (PIXCMAP *)ERROR_PTR("pix not defined", __func__, NULL);
    return pix->colormap;
}


/*!
 * \brief   pixSetColormap()
 *
 * \param[in]   pix
 * \param[in]   colormap   optional; can be null.
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) If %colormap is not defined, this is a no-op.
 *      (2) This destroys any existing colormap before assigning the
 *          new %colormap to %pix.
 *      (3) If the colormap is not valid, this returns 1.  The caller
 *          should check if there is a possibility that the pix and
 *          colormap depths differ.
 *      (4) This does not do the work of checking pixs for a pixel value
 *          that is out of bounds for the colormap -- that only needs to
 *          be done when reading and writing with an I/O library like
 *          png and gif.
 *      (5) Because colormaps are not ref counted, the new colormap
 *          must not belong to any other pix.
 * </pre>
 */
l_ok
pixSetColormap(PIX      *pix,
               PIXCMAP  *colormap)
{
l_int32  valid;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!colormap) return 0;

        /* Make sure the colormap doesn't get lost */
    pixDestroyColormap(pix);
    pix->colormap = colormap;

    pixcmapIsValid(colormap, NULL, &valid);
    if (!valid)
        return ERROR_INT("colormap is not valid", __func__, 1);
    return 0;
}


/*!
 * \brief   pixDestroyColormap()
 *
 * \param[in]   pix
 * \return  0 if OK, 1 on error
 */
l_ok
pixDestroyColormap(PIX  *pix)
{
PIXCMAP  *cmap;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if ((cmap = pix->colormap) != NULL) {
        pixcmapDestroy(&cmap);
        pix->colormap = NULL;
    }
    return 0;
}


/*!
 * \brief   pixGetData()
 *
 * \param[in]    pix
 * \return  ptr to image data
 *
 * <pre>
 * Notes:
 *      (1) This gives a new handle for the data.  The data is still
 *          owned by the pix, so do not call LEPT_FREE() on it.
 *      (2) This cannot guarantee that the pix data returned will not
 *          be changed, so %pix cannot be declared const.  And because
 *          most imaging operations call this for access to the data,
 *          this prevents them from declaring %pix to be const, even if
 *          they only use the data for inspection.
 * </pre>
 */
l_uint32 *
pixGetData(PIX  *pix)
{
    if (!pix)
        return (l_uint32 *)ERROR_PTR("pix not defined", __func__, NULL);
    return pix->data;
}


/*!
 * \brief   pixFreeAndSetData()
 *
 * \param[in]   pix
 * \param[in]   data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This frees the existing raster data in the pix and assigns %data.
 * </pre>
 */
l_int32
pixFreeAndSetData(PIX       *pix,
                  l_uint32  *data)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixFreeData(pix);
    pix->data = data;
    return 0;
}


/*!
 * \brief   pixSetData()
 *
 * \param[in]   pix
 * \param[in]   data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does not free any existing data.  To free existing
 *          data, use pixFreeAndSetData() instead.
 * </pre>
 */
l_int32
pixSetData(PIX       *pix,
           l_uint32  *data)
{
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pix->data = data;
    return 0;
}


/*!
 * \brief   pixFreeData()
 *
 * \param[in]   pix
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This frees the data and sets the pix data ptr to null.
 *          It should be used before pixSetData() in the situation where
 *          you want to free any existing data before doing
 *          a subsequent assignment with pixSetData().
 * </pre>
 */
l_int32
pixFreeData(PIX  *pix)
{
l_uint32  *data;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if ((data = pixGetData(pix)) != NULL) {
        pixdata_free(data);
        pix->data = NULL;
    }
    return 0;
}


/*!
 * \brief   pixExtractData()
 *
 * \param[in]   pix
 * \return  ptr to data, or null on error
 *
 * <pre>
 * Notes:
 *      (1) This extracts the pix image data for use in another context.
 *          The caller still needs to use pixDestroy() on the input pix.
 *      (2) If refcount == 1, the data is extracted and the
 *          pix->data ptr is set to NULL.
 *      (3) If refcount > 1, this simply returns a copy of the data,
 *          using the pix allocator, and leaving the input pix unchanged.
 * </pre>
 */
l_uint32 *
pixExtractData(PIX  *pixs)
{
l_int32    bytes;
l_uint32  *data, *datas;

    if (!pixs)
        return (l_uint32 *)ERROR_PTR("pixs not defined", __func__, NULL);

    if (pixs->refcount == 1) {  /* extract */
        data = pixGetData(pixs);
        pixSetData(pixs, NULL);
    } else {  /* refcount > 1; copy */
        bytes = 4 * pixGetWpl(pixs) * pixGetHeight(pixs);
        datas = pixGetData(pixs);
        if ((data = (l_uint32 *)pixdata_malloc(bytes)) == NULL)
            return (l_uint32 *)ERROR_PTR("data not made", __func__, NULL);
        memcpy(data, datas, bytes);
    }

    return data;
}


/*--------------------------------------------------------------------*
 *                          Pix line ptrs                             *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixGetLinePtrs()
 *
 * \param[in]    pix
 * \param[out]   psize   [optional] array size, which is the pix height
 * \return  array of line ptrs, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is intended to be used for fast random pixel access.
 *          For example, for an 8 bpp image,
 *              val = GET_DATA_BYTE(lines8[i], j);
 *          is equivalent to, but much faster than,
 *              pixGetPixel(pix, j, i, &val);
 *      (2) How much faster?  For 1 bpp, it's from 6 to 10x faster.
 *          For 8 bpp, it's an amazing 30x faster.  So if you are
 *          doing random access over a substantial part of the image,
 *          use this line ptr array.
 *      (3) When random access is used in conjunction with a stack,
 *          queue or heap, the overall computation time depends on
 *          the operations performed on each struct that is popped
 *          or pushed, and whether we are using a priority queue (O(logn))
 *          or a queue or stack (O(1)).  For example, for maze search,
 *          the overall ratio of time for line ptrs vs. pixGet/Set* is
 *             Maze type     Type                   Time ratio
 *               binary      queue                     0.4
 *               gray        heap (priority queue)     0.6
 *      (4) Because this returns a void** and the accessors take void*,
 *          the compiler cannot check the pointer types.  It is
 *          strongly recommended that you adopt a naming scheme for
 *          the returned ptr arrays that indicates the pixel depth.
 *          (This follows the original intent of Simonyi's "Hungarian"
 *          application notation, where naming is used proactively
 *          to make errors visibly obvious.)  By doing this, you can
 *          tell by inspection if the correct accessor is used.
 *          For example, for an 8 bpp pixg:
 *              void **lineg8 = pixGetLinePtrs(pixg, NULL);
 *              val = GET_DATA_BYTE(lineg8[i], j);  // fast access; BYTE, 8
 *              ...
 *              LEPT_FREE(lineg8);  // don't forget this
 *      (5) These are convenient for accessing bytes sequentially in an
 *          8 bpp grayscale image.  People who write image processing code
 *          on 8 bpp images are accustomed to grabbing pixels directly out
 *          of the raster array.  Note that for little endians, you first
 *          need to reverse the byte order in each 32-bit word.
 *          Here's a typical usage pattern:
 *              pixEndianByteSwap(pix);   // always safe; no-op on big-endians
 *              l_uint8 **lineptrs = (l_uint8 **)pixGetLinePtrs(pix, NULL);
 *              pixGetDimensions(pix, &w, &h, NULL);
 *              for (i = 0; i < h; i++) {
 *                  l_uint8 *line = lineptrs[i];
 *                  for (j = 0; j < w; j++) {
 *                      val = line[j];
 *                      ...
 *                  }
 *              }
 *              pixEndianByteSwap(pix);  // restore big-endian order
 *              LEPT_FREE(lineptrs);
 *          This can be done even more simply as follows:
 *              l_uint8 **lineptrs = pixSetupByteProcessing(pix, &w, &h);
 *              for (i = 0; i < h; i++) {
 *                  l_uint8 *line = lineptrs[i];
 *                  for (j = 0; j < w; j++) {
 *                      val = line[j];
 *                      ...
 *                  }
 *              }
 *              pixCleanupByteProcessing(pix, lineptrs);
 * </pre>
 */
void **
pixGetLinePtrs(PIX      *pix,
               l_int32  *psize)
{
l_int32    i, h, wpl;
l_uint32  *data;
void     **lines;

    if (psize) *psize = 0;
    if (!pix)
        return (void **)ERROR_PTR("pix not defined", __func__, NULL);

    h = pixGetHeight(pix);
    if (psize) *psize = h;
    if ((lines = (void **)LEPT_CALLOC(h, sizeof(void *))) == NULL)
        return (void **)ERROR_PTR("lines not made", __func__, NULL);
    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    for (i = 0; i < h; i++)
        lines[i] = (void *)(data + i * wpl);

    return lines;
}


/*--------------------------------------------------------------------*
 *                         Pix Size Comparisons                       *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixSizesEqual()
 *
 * \param[in]    pix1, pix2
 * \return  1 if the two pix have same {h, w, d}; 0 otherwise.
 */
l_int32
pixSizesEqual(const PIX  *pix1,
              const PIX  *pix2)
{
    if (!pix1 || !pix2)
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 0);

    if (pix1 == pix2)
        return 1;

    if ((pixGetWidth(pix1) != pixGetWidth(pix2)) ||
        (pixGetHeight(pix1) != pixGetHeight(pix2)) ||
        (pixGetDepth(pix1) != pixGetDepth(pix2)))
        return 0;
    else
        return 1;
}


/*!
 * \brief   pixMaxAspectRatio()
 *
 * \param[in]    pixs      32 bpp rgb
 * \param[out]   pratio    max aspect ratio, >= 1.0; -1.0 on error
 * \return  0 if OK, 1 on error
 */
l_ok
pixMaxAspectRatio(PIX        *pixs,
                  l_float32  *pratio)
{
l_int32  w, h;

    if (!pratio)
        return ERROR_INT("&ratio not defined", __func__, 1);
    *pratio = -1.0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w == 0 || h == 0) {
        L_ERROR("invalid size: w = %d, h = %d\n", __func__, w, h);
        return 1;
    }

    *pratio = L_MAX((l_float32)h / (l_float32)w, (l_float32)w / (l_float32)h);
    return 0;
}


/*--------------------------------------------------------------------*
 *                    Print output for debugging                      *
 *--------------------------------------------------------------------*/
extern const char *ImageFileFormatExtensions[];

/*!
 * \brief   pixPrintStreamInfo()
 *
 * \param[in]    fp    file stream
 * \param[in]    pix
 * \param[in]    text  [optional] identifying string; can be null
 * \return  0 if OK, 1 on error
 */
l_ok
pixPrintStreamInfo(FILE        *fp,
                   const PIX   *pix,
                   const char  *text)
{
l_int32         informat;
const PIXCMAP  *cmap;

    if (!fp)
        return ERROR_INT("fp not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if (text)
        fprintf(fp, "  Pix Info for %s:\n", text);
    fprintf(fp, "    width = %d, height = %d, depth = %d, spp = %d\n",
            pixGetWidth(pix), pixGetHeight(pix), pixGetDepth(pix),
            pixGetSpp(pix));
    fprintf(fp, "    wpl = %d, data = %p, refcount = %d\n",
            pixGetWpl(pix), pix->data, pix->refcount);
    fprintf(fp, "    xres = %d, yres = %d\n", pixGetXRes(pix), pixGetYRes(pix));
    if ((cmap = pix->colormap) != NULL)
        pixcmapWriteStream(fp, cmap);
    else
        fprintf(fp, "    no colormap\n");
    informat = pixGetInputFormat(pix);
    fprintf(fp, "    input format: %d (%s)\n", informat,
            ImageFileFormatExtensions[informat]);
    if (pix->text != NULL)
        fprintf(fp, "    text: %s\n", pix->text);

    return 0;
}
