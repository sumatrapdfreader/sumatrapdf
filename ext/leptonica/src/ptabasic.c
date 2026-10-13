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
 * \file  ptabasic.c
 * <pre>
 *
 *      Pta creation, destruction, copy, clone, empty
 *           PTA            *ptaCreate()
 *           PTA            *ptaCreateFromNuma()
 *           void            ptaDestroy()
 *           PTA            *ptaCopy()
 *           PTA            *ptaCopyRange()
 *           PTA            *ptaClone()
 *           l_int32         ptaEmpty()
 *
 *      Pta array extension
 *           l_int32         ptaAddPt()
 *           static l_int32  ptaExtendArrays()
 *
 *      Pta insertion and removal
 *           l_int32         ptaInsertPt()
 *           l_int32         ptaRemovePt()
 *
 *      Pta accessors
 *           l_int32         ptaGetCount()
 *           l_int32         ptaGetPt()
 *           l_int32         ptaGetIPt()
 *           l_int32         ptaSetPt()
 *           l_int32         ptaGetArrays()
 *
 *      Pta serialized for I/O
 *           PTA            *ptaRead()
 *           PTA            *ptaReadStream()
 *           PTA            *ptaReadMem()
 *           l_int32         ptaWriteDebug()
 *           l_int32         ptaWrite()
 *           l_int32         ptaWriteStream()
 *           l_int32         ptaWriteMem()
 *
 *      Ptaa creation, destruction
 *           PTAA           *ptaaCreate()
 *           void            ptaaDestroy()
 *
 *      Ptaa array extension
 *           l_int32         ptaaAddPta()
 *           static l_int32  ptaaExtendArray()
 *
 *      Ptaa accessors
 *           l_int32         ptaaGetCount()
 *           l_int32         ptaaGetPta()
 *           l_int32         ptaaGetPt()
 *
 *      Ptaa array modifiers
 *           l_int32         ptaaInitFull()
 *           l_int32         ptaaReplacePta()
 *           l_int32         ptaaAddPt()
 *           l_int32         ptaaTruncate()
 *
 *      Ptaa serialized for I/O
 *           PTAA           *ptaaRead()
 *           PTAA           *ptaaReadStream()
 *           PTAA           *ptaaReadMem()
 *           l_int32         ptaaWrite()
 *           l_int32         ptaaWriteStream()
 *           l_int32         ptaaWriteMem()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "array_internal.h"
#include "pix_internal.h"

static const l_uint32  MaxArraySize = 100000000;  /* 100 million */
static const l_uint32  MaxPtrArraySize = 10000000;  /* 10 million */
static const l_int32 InitialArraySize = 50;      /*!< n'importe quoi */

    /* Static functions */
static l_int32 ptaExtendArrays(PTA *pta);
static l_int32 ptaaExtendArray(PTAA *ptaa);

/*---------------------------------------------------------------------*
 *                Pta creation, destruction, copy, clone               *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaCreate()
 *
 * \param[in]    n    initial array sizes
 * \return  pta, or NULL on error.
 */
PTA *
ptaCreate(l_int32  n)
{
PTA  *pta;

    if (n <= 0 || n > (l_int32)MaxArraySize)
        n = InitialArraySize;

    pta = (PTA *)LEPT_CALLOC(1, sizeof(PTA));
    pta->n = 0;
    pta->nalloc = n;
    pta->refcount = 1;
    pta->x = (l_float32 *)LEPT_CALLOC(n, sizeof(l_float32));
    pta->y = (l_float32 *)LEPT_CALLOC(n, sizeof(l_float32));
    if (!pta->x || !pta->y) {
        ptaDestroy(&pta);
        return (PTA *)ERROR_PTR("x and y arrays not both made", __func__, NULL);
    }

    return pta;
}


/*!
 * \brief   ptaCreateFromNuma()
 *
 * \param[in]    nax   [optional] can be null
 * \param[in]    nay
 * \return  pta, or NULL on error.
 */
PTA *
ptaCreateFromNuma(NUMA  *nax,
                  NUMA  *nay)
{
l_int32    i, n;
l_float32  startx, delx, xval, yval;
PTA       *pta;

    if (!nay)
        return (PTA *)ERROR_PTR("nay not defined", __func__, NULL);
    n = numaGetCount(nay);
    if (nax && numaGetCount(nax) != n)
        return (PTA *)ERROR_PTR("nax and nay sizes differ", __func__, NULL);

    pta = ptaCreate(n);
    numaGetParameters(nay, &startx, &delx);
    for (i = 0; i < n; i++) {
        if (nax)
            numaGetFValue(nax, i, &xval);
        else  /* use implicit x values from nay */
            xval = startx + i * delx;
        numaGetFValue(nay, i, &yval);
        ptaAddPt(pta, xval, yval);
    }

    return pta;
}


/*!
 * \brief   ptaDestroy()
 *
 * \param[in,out]   ppta   will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the pta.
 *      (2) Always nulls the input ptr.
 * </pre>
 */
void
ptaDestroy(PTA  **ppta)
{
PTA  *pta;

    if (ppta == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((pta = *ppta) == NULL)
        return;

    if (--pta->refcount == 0) {
        LEPT_FREE(pta->x);
        LEPT_FREE(pta->y);
        LEPT_FREE(pta);
    }
    *ppta = NULL;
}


/*!
 * \brief   ptaCopy()
 *
 * \param[in]    pta
 * \return  copy of pta, or NULL on error
 */
PTA *
ptaCopy(PTA  *pta)
{
l_int32    i;
l_float32  x, y;
PTA       *npta;

    if (!pta)
        return (PTA *)ERROR_PTR("pta not defined", __func__, NULL);

    if ((npta = ptaCreate(pta->nalloc)) == NULL)
        return (PTA *)ERROR_PTR("npta not made", __func__, NULL);

    for (i = 0; i < pta->n; i++) {
        ptaGetPt(pta, i, &x, &y);
        ptaAddPt(npta, x, y);
    }

    return npta;
}


/*!
 * \brief   ptaCopyRange()
 *
 * \param[in]    ptas
 * \param[in]    istart    starting index in ptas
 * \param[in]    iend      ending index in ptas; use 0 to copy to end
 * \return  0 if OK, 1 on error
 */
PTA *
ptaCopyRange(PTA     *ptas,
             l_int32  istart,
             l_int32  iend)
{
l_int32  n, i, x, y;
PTA     *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    n = ptaGetCount(ptas);
    if (istart < 0)
        istart = 0;
    if (istart >= n)
        return (PTA *)ERROR_PTR("istart out of bounds", __func__, NULL);
    if (iend <= 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return (PTA *)ERROR_PTR("istart > iend; no pts", __func__, NULL);

    if ((ptad = ptaCreate(iend - istart + 1)) == NULL)
        return (PTA *)ERROR_PTR("ptad not made", __func__, NULL);
    for (i = istart; i <= iend; i++) {
        ptaGetIPt(ptas, i, &x, &y);
        ptaAddPt(ptad, x, y);
    }

    return ptad;
}


/*!
 * \brief   ptaClone()
 *
 * \param[in]    pta
 * \return  ptr to same pta, or NULL on error
 */
PTA *
ptaClone(PTA  *pta)
{
    if (!pta)
        return (PTA *)ERROR_PTR("pta not defined", __func__, NULL);

    ++pta->refcount;
    return pta;
}


/*!
 * \brief   ptaEmpty()
 *
 * \param[in]    pta
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      This only resets the Pta::n field, for reuse
 * </pre>
 */
l_ok
ptaEmpty(PTA  *pta)
{
    if (!pta)
        return ERROR_INT("ptad not defined", __func__, 1);
    pta->n = 0;
    return 0;
}


/*---------------------------------------------------------------------*
 *                         Pta array extension                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaAddPt()
 *
 * \param[in]    pta
 * \param[in]    x, y
 * \return  0 if OK, 1 on error
 */
l_ok
ptaAddPt(PTA       *pta,
         l_float32  x,
         l_float32  y)
{
l_int32  n;

    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);

    n = pta->n;
    if (n >= pta->nalloc) {
        if (ptaExtendArrays(pta))
            return ERROR_INT("extension failed", __func__, 1);
    }

    pta->x[n] = x;
    pta->y[n] = y;
    pta->n++;
    return 0;
}


/*!
 * \brief   ptaExtendArrays()
 *
 * \param[in]    pta
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Doubles the size of the array.
 *      (2) The max number of points is 100M.
 * </pre>
 */
static l_int32
ptaExtendArrays(PTA  *pta)
{
size_t  oldsize, newsize;

    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if (pta->nalloc > (l_int32)MaxArraySize)  /* belt & suspenders */
        return ERROR_INT("pta at maximum size; can't extend", __func__, 1);
    oldsize = 4 * pta->nalloc;
    if (pta->nalloc > MaxArraySize / 2) {
        newsize = 4 * MaxArraySize;
        pta->nalloc = MaxArraySize;
    } else {
        newsize = 2 * oldsize;
        pta->nalloc *= 2;
    }
    if ((pta->x = (l_float32 *)reallocNew((void **)&pta->x,
                                          oldsize, newsize)) == NULL)
        return ERROR_INT("new x array not returned", __func__, 1);
    if ((pta->y = (l_float32 *)reallocNew((void **)&pta->y,
                                          oldsize, newsize)) == NULL)
        return ERROR_INT("new y array not returned", __func__, 1);

    return 0;
}


/*---------------------------------------------------------------------*
 *                     Pta insertion and removal                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaInsertPt()
 *
 * \param[in]    pta
 * \param[in]    index   at which pt is to be inserted
 * \param[in]    x, y    point values
 * \return  0 if OK; 1 on error
 */
l_ok
ptaInsertPt(PTA     *pta,
            l_int32  index,
            l_int32  x,
            l_int32  y)
{
l_int32  i, n;

    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    n = ptaGetCount(pta);
    if (index < 0 || index > n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n);
        return 1;
    }

    if (n > pta->nalloc) {
        if (ptaExtendArrays(pta))
            return ERROR_INT("extension failed", __func__, 1);
    }
    pta->n++;
    for (i = n; i > index; i--) {
        pta->x[i] = pta->x[i - 1];
        pta->y[i] = pta->y[i - 1];
    }
    pta->x[index] = x;
    pta->y[index] = y;
    return 0;
}


/*!
 * \brief   ptaRemovePt()
 *
 * \param[in]    pta
 * \param[in]    index    of point to be removed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts pta[i] --> pta[i - 1] for all i > index.
 *      (2) It should not be used repeatedly on large arrays,
 *          because the function is O(n).
 * </pre>
 */
l_ok
ptaRemovePt(PTA     *pta,
            l_int32  index)
{
l_int32  i, n;

    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    n = ptaGetCount(pta);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

        /* Remove the point */
    for (i = index + 1; i < n; i++) {
        pta->x[i - 1] = pta->x[i];
        pta->y[i - 1] = pta->y[i];
    }
    pta->n--;
    return 0;
}


/*---------------------------------------------------------------------*
 *                           Pta accessors                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaGetCount()
 *
 * \param[in]    pta
 * \return  count, or 0 if no pta
 */
l_int32
ptaGetCount(PTA  *pta)
{
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 0);

    return pta->n;
}


/*!
 * \brief   ptaGetPt()
 *
 * \param[in]    pta
 * \param[in]    index    into arrays
 * \param[out]   px       [optional] float x value
 * \param[out]   py       [optional] float y value
 * \return  0 if OK; 1 on error
 */
l_ok
ptaGetPt(PTA        *pta,
         l_int32     index,
         l_float32  *px,
         l_float32  *py)
{
    if (px) *px = 0;
    if (py) *py = 0;
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if (index < 0 || index >= pta->n)
        return ERROR_INT("invalid index", __func__, 1);

    if (px) *px = pta->x[index];
    if (py) *py = pta->y[index];
    return 0;
}


/*!
 * \brief   ptaGetIPt()
 *
 * \param[in]    pta
 * \param[in]    index    into arrays
 * \param[out]   px       [optional] integer x value
 * \param[out]   py       [optional] integer y value
 * \return  0 if OK; 1 on error
 */
l_ok
ptaGetIPt(PTA      *pta,
          l_int32   index,
          l_int32  *px,
          l_int32  *py)
{
    if (px) *px = 0;
    if (py) *py = 0;
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if (index < 0 || index >= pta->n)
        return ERROR_INT("invalid index", __func__, 1);

    if (px) *px = (l_int32)(pta->x[index] + 0.5);
    if (py) *py = (l_int32)(pta->y[index] + 0.5);
    return 0;
}


/*!
 * \brief   ptaSetPt()
 *
 * \param[in]    pta
 * \param[in]    index    into arrays
 * \param[in]    x, y
 * \return  0 if OK; 1 on error
 */
l_ok
ptaSetPt(PTA       *pta,
         l_int32    index,
         l_float32  x,
         l_float32  y)
{
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if (index < 0 || index >= pta->n)
        return ERROR_INT("invalid index", __func__, 1);

    pta->x[index] = x;
    pta->y[index] = y;
    return 0;
}


/*!
 * \brief   ptaGetArrays()
 *
 * \param[in]    pta
 * \param[out]   pnax    [optional] numa of x array
 * \param[out]   pnay    [optional] numa of y array
 * \return  0 if OK; 1 on error or if pta is empty
 *
 * <pre>
 * Notes:
 *      (1) This copies the internal arrays into new Numas.
 * </pre>
 */
l_ok
ptaGetArrays(PTA    *pta,
             NUMA  **pnax,
             NUMA  **pnay)
{
l_int32  i, n;
NUMA    *nax, *nay;

    if (!pnax && !pnay)
        return ERROR_INT("no output requested", __func__, 1);
    if (pnax) *pnax = NULL;
    if (pnay) *pnay = NULL;
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if ((n = ptaGetCount(pta)) == 0)
        return ERROR_INT("pta is empty", __func__, 1);

    if (pnax) {
        if ((nax = numaCreate(n)) == NULL)
            return ERROR_INT("nax not made", __func__, 1);
        *pnax = nax;
        for (i = 0; i < n; i++)
            nax->array[i] = pta->x[i];
        nax->n = n;
    }
    if (pnay) {
        if ((nay = numaCreate(n)) == NULL)
            return ERROR_INT("nay not made", __func__, 1);
        *pnay = nay;
        for (i = 0; i < n; i++)
            nay->array[i] = pta->y[i];
        nay->n = n;
    }
    return 0;
}


/*---------------------------------------------------------------------*
 *                       Pta serialized for I/O                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaRead()
 *
 * \param[in]    filename
 * \return  pta, or NULL on error
 */
PTA *
ptaRead(const char  *filename)
{
FILE  *fp;
PTA   *pta;

    if (!filename)
        return (PTA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (PTA *)ERROR_PTR_1("stream not opened",
                                  filename, __func__, NULL);
    pta = ptaReadStream(fp);
    fclose(fp);
    if (!pta)
        return (PTA *)ERROR_PTR_1("pta not read", filename, __func__, NULL);
    return pta;
}


/*!
 * \brief   ptaReadStream()
 *
 * \param[in]    fp    file stream
 * \return  pta, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) It is OK for the pta to be empty (n == 0).
 * </pre>

 */
PTA *
ptaReadStream(FILE  *fp)
{
char       typestr[128];  /* hardcoded below in fscanf */
l_int32    i, n, ix, iy, type, version;
l_float32  x, y;
PTA       *pta;

    if (!fp)
        return (PTA *)ERROR_PTR("stream not defined", __func__, NULL);

    if (fscanf(fp, "\n Pta Version %d\n", &version) != 1)
        return (PTA *)ERROR_PTR("not a pta file", __func__, NULL);
    if (version != PTA_VERSION_NUMBER)
        return (PTA *)ERROR_PTR("invalid pta version", __func__, NULL);
    if (fscanf(fp, " Number of pts = %d; format = %127s\n", &n, typestr) != 2)
        return (PTA *)ERROR_PTR("not a pta file", __func__, NULL);
    if (n < 0)
        return (PTA *)ERROR_PTR("num pts <= 0", __func__, NULL);
    if (n > (l_int32)MaxArraySize)
        return (PTA *)ERROR_PTR("too many pts", __func__, NULL);
    if (n == 0) L_INFO("the pta is empty\n", __func__);

    if (!strcmp(typestr, "float"))
        type = 0;
    else  /* typestr is "integer" */
        type = 1;
    if ((pta = ptaCreate(n)) == NULL)
        return (PTA *)ERROR_PTR("pta not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        if (type == 0) {  /* data is float */
            if (fscanf(fp, "   (%f, %f)\n", &x, &y) != 2) {
                ptaDestroy(&pta);
                return (PTA *)ERROR_PTR("error reading floats", __func__, NULL);
            }
            ptaAddPt(pta, x, y);
        } else {   /* data is integer */
            if (fscanf(fp, "   (%d, %d)\n", &ix, &iy) != 2) {
                ptaDestroy(&pta);
                return (PTA *)ERROR_PTR("error reading ints", __func__, NULL);
            }
            ptaAddPt(pta, ix, iy);
        }
    }

    return pta;
}


/*!
 * \brief   ptaReadMem()
 *
 * \param[in]    data    serialization in ascii
 * \param[in]    size    of data in bytes; can use strlen to get it
 * \return  pta, or NULL on error
 */
PTA *
ptaReadMem(const l_uint8  *data,
           size_t          size)
{
FILE  *fp;
PTA   *pta;

    if (!data)
        return (PTA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (PTA *)ERROR_PTR("stream not opened", __func__, NULL);

    pta = ptaReadStream(fp);
    fclose(fp);
    if (!pta) L_ERROR("pta not read\n", __func__);
    return pta;
}


/*!
 * \brief   ptaWriteDebug()
 *
 * \param[in]    filename
 * \param[in]    pta
 * \param[in]    type       0 for float values; 1 for integer values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Debug version, intended for use in the library when writing
 *          to files in a temp directory with names that are compiled in.
 *          This is used instead of ptaWrite() for all such library calls.
 *      (2) The global variable LeptDebugOK defaults to 0, and can be set
 *          or cleared by the function setLeptDebugOK().
 * </pre>
 */
l_ok
ptaWriteDebug(const char  *filename,
              PTA         *pta,
              l_int32      type)
{
    if (LeptDebugOK) {
        return ptaWrite(filename, pta, type);
    } else {
        L_INFO("write to named temp file %s is disabled\n", __func__, filename);
        return 0;
    }
}


/*!
 * \brief   ptaWrite()
 *
 * \param[in]    filename
 * \param[in]    pta
 * \param[in]    type       0 for float values; 1 for integer values
 * \return  0 if OK, 1 on error
 */
l_ok
ptaWrite(const char  *filename,
         PTA         *pta,
         l_int32      type)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = ptaWriteStream(fp, pta, type);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("pta not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   ptaWriteStream()
 *
 * \param[in]    fp      file stream
 * \param[in]    pta
 * \param[in]    type    0 for float values; 1 for integer values
 * \return  0 if OK; 1 on error
 */
l_ok
ptaWriteStream(FILE    *fp,
               PTA     *pta,
               l_int32  type)
{
l_int32    i, n, ix, iy;
l_float32  x, y;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);

    n = ptaGetCount(pta);
    fprintf(fp, "\n Pta Version %d\n", PTA_VERSION_NUMBER);
    if (type == 0)
        fprintf(fp, " Number of pts = %d; format = float\n", n);
    else  /* type == 1 */
        fprintf(fp, " Number of pts = %d; format = integer\n", n);
    for (i = 0; i < n; i++) {
        if (type == 0) {  /* data is float */
            ptaGetPt(pta, i, &x, &y);
            fprintf(fp, "   (%f, %f)\n", x, y);
        } else {   /* data is integer */
            ptaGetIPt(pta, i, &ix, &iy);
            fprintf(fp, "   (%d, %d)\n", ix, iy);
        }
    }

    return 0;
}


/*!
 * \brief   ptaWriteMem()
 *
 * \param[out]   pdata    data of serialized pta; ascii
 * \param[out]   psize    size of returned data
 * \param[in]    pta
 * \param[in]    type     0 for float values; 1 for integer values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a pta in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
ptaWriteMem(l_uint8  **pdata,
            size_t    *psize,
            PTA       *pta,
            l_int32    type)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = ptaWriteStream(fp, pta, type);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = ptaWriteStream(fp, pta, type);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*---------------------------------------------------------------------*
 *                     PTAA creation, destruction                      *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaaCreate()
 *
 * \param[in]    n    initial number of ptrs
 * \return  ptaa, or NULL on error
 */
PTAA *
ptaaCreate(l_int32  n)
{
PTAA  *ptaa;

    if (n <= 0 || n > (l_int32)MaxPtrArraySize)
        n = InitialArraySize;

    ptaa = (PTAA *)LEPT_CALLOC(1, sizeof(PTAA));
    ptaa->n = 0;
    ptaa->nalloc = n;
    if ((ptaa->pta = (PTA **)LEPT_CALLOC(n, sizeof(PTA *))) == NULL) {
        ptaaDestroy(&ptaa);
        return (PTAA *)ERROR_PTR("pta ptrs not made", __func__, NULL);
    }
    return ptaa;
}


/*!
 * \brief   ptaaDestroy()
 *
 * \param[in,out]   pptaa   will be set to null before returning
 * \return  void
 */
void
ptaaDestroy(PTAA  **pptaa)
{
l_int32  i;
PTAA    *ptaa;

    if (pptaa == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((ptaa = *pptaa) == NULL)
        return;

    for (i = 0; i < ptaa->n; i++)
        ptaDestroy(&ptaa->pta[i]);
    LEPT_FREE(ptaa->pta);
    LEPT_FREE(ptaa);
    *pptaa = NULL;
}


/*---------------------------------------------------------------------*
 *                          PTAA array extension                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaaAddPta()
 *
 * \param[in]    ptaa
 * \param[in]    pta         to be added
 * \param[in]    copyflag    L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK, 1 on error
 */
l_ok
ptaaAddPta(PTAA    *ptaa,
           PTA     *pta,
           l_int32  copyflag)
{
l_int32  n;
PTA     *ptac;

    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);

    if (copyflag == L_INSERT) {
        ptac = pta;
    } else if (copyflag == L_COPY) {
        if ((ptac = ptaCopy(pta)) == NULL)
            return ERROR_INT("ptac not made", __func__, 1);
    } else if (copyflag == L_CLONE) {
        if ((ptac = ptaClone(pta)) == NULL)
            return ERROR_INT("pta clone not made", __func__, 1);
    } else {
        return ERROR_INT("invalid copyflag", __func__, 1);
    }

    n = ptaaGetCount(ptaa);
    if (n >= ptaa->nalloc) {
        if (ptaaExtendArray(ptaa)) {
            if (copyflag != L_INSERT)
                ptaDestroy(&ptac);
            return ERROR_INT("extension failed", __func__, 1);
        }
    }

    ptaa->pta[n] = ptac;
    ptaa->n++;
    return 0;
}


/*!
 * \brief   ptaaExtendArray()
 *
 * \param[in]    ptaa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This doubles the pta ptr array size.
 *      (2) The max number of pta ptrs is 10M.
 * </pre>
 *
 */
static l_int32
ptaaExtendArray(PTAA  *ptaa)
{
size_t  oldsize, newsize;

    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);
    oldsize = ptaa->nalloc * sizeof(PTA *);
    newsize = 2 * oldsize;
    if (newsize > 8 * MaxPtrArraySize)
        return ERROR_INT("newsize > 80 MB; too large", __func__, 1);

    if ((ptaa->pta = (PTA **)reallocNew((void **)&ptaa->pta,
                                        oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    ptaa->nalloc *= 2;
    return 0;
}


/*---------------------------------------------------------------------*
 *                          Ptaa accessors                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaaGetCount()
 *
 * \param[in]    ptaa
 * \return  count, or 0 if no ptaa
 */
l_int32
ptaaGetCount(PTAA  *ptaa)
{
    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 0);

    return ptaa->n;
}


/*!
 * \brief   ptaaGetPta()
 *
 * \param[in]    ptaa
 * \param[in]    index         to the i-th pta
 * \param[in]    accessflag    L_COPY or L_CLONE
 * \return  pta, or NULL on error
 */
PTA *
ptaaGetPta(PTAA    *ptaa,
           l_int32  index,
           l_int32  accessflag)
{
    if (!ptaa)
        return (PTA *)ERROR_PTR("ptaa not defined", __func__, NULL);
    if (index < 0 || index >= ptaa->n)
        return (PTA *)ERROR_PTR("index not valid", __func__, NULL);

    if (accessflag == L_COPY)
        return ptaCopy(ptaa->pta[index]);
    else if (accessflag == L_CLONE)
        return ptaClone(ptaa->pta[index]);
    else
        return (PTA *)ERROR_PTR("invalid accessflag", __func__, NULL);
}


/*!
 * \brief   ptaaGetPt()
 *
 * \param[in]    ptaa
 * \param[in]    ipta   to the i-th pta
 * \param[in]    jpt    index to the j-th pt in the pta
 * \param[out]   px     [optional] float x value
 * \param[out]   py     [optional] float y value
 * \return  0 if OK; 1 on error
 */
l_ok
ptaaGetPt(PTAA       *ptaa,
           l_int32     ipta,
           l_int32     jpt,
           l_float32  *px,
           l_float32  *py)
{
PTA  *pta;

    if (px) *px = 0;
    if (py) *py = 0;
    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);
    if (ipta < 0 || ipta >= ptaa->n)
        return ERROR_INT("index ipta not valid", __func__, 1);

    pta = ptaaGetPta(ptaa, ipta, L_CLONE);
    if (jpt < 0 || jpt >= pta->n) {
        ptaDestroy(&pta);
        return ERROR_INT("index jpt not valid", __func__, 1);
    }

    ptaGetPt(pta, jpt, px, py);
    ptaDestroy(&pta);
    return 0;
}


/*---------------------------------------------------------------------*
 *                        Ptaa array modifiers                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaaInitFull()
 *
 * \param[in]    ptaa    can have non-null ptrs in the ptr array
 * \param[in]    pta     to be replicated into the entire ptr array
 * \return  0 if OK; 1 on error
 */
l_ok
ptaaInitFull(PTAA  *ptaa,
             PTA   *pta)
{
l_int32  n, i;
PTA     *ptat;

    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);

    n = ptaa->nalloc;
    ptaa->n = n;
    for (i = 0; i < n; i++) {
        ptat = ptaCopy(pta);
        ptaaReplacePta(ptaa, i, ptat);
    }
    return 0;
}


/*!
 * \brief   ptaaReplacePta()
 *
 * \param[in]    ptaa
 * \param[in]    index   to the index-th pta
 * \param[in]    pta     insert and replace any existing one
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Any existing pta is destroyed, and the input one
 *          is inserted in its place.
 *      (2) If %index is invalid, return 1 (error)
 * </pre>
 */
l_ok
ptaaReplacePta(PTAA    *ptaa,
               l_int32  index,
               PTA     *pta)
{
l_int32  n;

    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    n = ptaaGetCount(ptaa);
    if (index < 0 || index >= n)
        return ERROR_INT("index not valid", __func__, 1);

    ptaDestroy(&ptaa->pta[index]);
    ptaa->pta[index] = pta;
    return 0;
}


/*!
 * \brief   ptaaAddPt()
 *
 * \param[in]    ptaa
 * \param[in]    ipta   to the i-th pta
 * \param[in]    x,y    point coordinates
 * \return  0 if OK; 1 on error
 */
l_ok
ptaaAddPt(PTAA      *ptaa,
          l_int32    ipta,
          l_float32  x,
          l_float32  y)
{
PTA  *pta;

    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);
    if (ipta < 0 || ipta >= ptaa->n)
        return ERROR_INT("index ipta not valid", __func__, 1);

    pta = ptaaGetPta(ptaa, ipta, L_CLONE);
    ptaAddPt(pta, x, y);
    ptaDestroy(&pta);
    return 0;
}


/*!
 * \brief   ptaaTruncate()
 *
 * \param[in]    ptaa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This identifies the largest index containing a pta that
 *          has any points within it, destroys all pta above that index,
 *          and resets the count.
 * </pre>
 */
l_ok
ptaaTruncate(PTAA  *ptaa)
{
l_int32  i, n, np;
PTA     *pta;

    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);

    n = ptaaGetCount(ptaa);
    for (i = n - 1; i >= 0; i--) {
        pta = ptaaGetPta(ptaa, i, L_CLONE);
        if (!pta) {
            ptaa->n--;
            continue;
        }
        np = ptaGetCount(pta);
        ptaDestroy(&pta);
        if (np == 0) {
            ptaDestroy(&ptaa->pta[i]);
            ptaa->n--;
        } else {
            break;
        }
    }
    return 0;
}


/*---------------------------------------------------------------------*
 *                       Ptaa serialized for I/O                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaaRead()
 *
 * \param[in]    filename
 * \return  ptaa, or NULL on error
 */
PTAA *
ptaaRead(const char  *filename)
{
FILE  *fp;
PTAA  *ptaa;

    if (!filename)
        return (PTAA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (PTAA *)ERROR_PTR_1("stream not opened",
                                   filename, __func__, NULL);
    ptaa = ptaaReadStream(fp);
    fclose(fp);
    if (!ptaa)
        return (PTAA *)ERROR_PTR_1("ptaa not read", filename, __func__, NULL);
    return ptaa;
}


/*!
 * \brief   ptaaReadStream()
 *
 * \param[in]    fp    file stream
 * \return  ptaa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) It is OK for the ptaa to be empty (n == 0).
 * </pre>
 */
PTAA *
ptaaReadStream(FILE  *fp)
{
l_int32  i, n, version;
PTA     *pta;
PTAA    *ptaa;

    if (!fp)
        return (PTAA *)ERROR_PTR("stream not defined", __func__, NULL);

    if (fscanf(fp, "\nPtaa Version %d\n", &version) != 1)
        return (PTAA *)ERROR_PTR("not a ptaa file", __func__, NULL);
    if (version != PTA_VERSION_NUMBER)
        return (PTAA *)ERROR_PTR("invalid ptaa version", __func__, NULL);
    if (fscanf(fp, "Number of Pta = %d\n", &n) != 1)
        return (PTAA *)ERROR_PTR("not a ptaa file", __func__, NULL);
    if (n < 0)
        return (PTAA *)ERROR_PTR("num pta ptrs <= 0", __func__, NULL);
    if (n > (l_int32)MaxPtrArraySize)
        return (PTAA *)ERROR_PTR("too many pta ptrs", __func__, NULL);
    if (n == 0) L_INFO("the ptaa is empty\n", __func__);

    if ((ptaa = ptaaCreate(n)) == NULL)
        return (PTAA *)ERROR_PTR("ptaa not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        if ((pta = ptaReadStream(fp)) == NULL) {
            ptaaDestroy(&ptaa);
            return (PTAA *)ERROR_PTR("error reading pta", __func__, NULL);
        }
        ptaaAddPta(ptaa, pta, L_INSERT);
    }

    return ptaa;
}


/*!
 * \brief   ptaaReadMem()
 *
 * \param[in]    data    serialization in ascii
 * \param[in]    size    of data in bytes; can use strlen to get it
 * \return  ptaa, or NULL on error
 */
PTAA *
ptaaReadMem(const l_uint8  *data,
            size_t          size)
{
FILE  *fp;
PTAA  *ptaa;

    if (!data)
        return (PTAA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (PTAA *)ERROR_PTR("stream not opened", __func__, NULL);

    ptaa = ptaaReadStream(fp);
    fclose(fp);
    if (!ptaa) L_ERROR("ptaa not read\n", __func__);
    return ptaa;
}


/*!
 * \brief   ptaaWriteDebug()
 *
 * \param[in]    filename
 * \param[in]    ptaa
 * \param[in]    type      0 for float values; 1 for integer values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Debug version, intended for use in the library when writing
 *          to files in a temp directory with names that are compiled in.
 *          This is used instead of ptaaWrite() for all such library calls.
 *      (2) The global variable LeptDebugOK defaults to 0, and can be set
 *          or cleared by the function setLeptDebugOK().
 * </pre>
 */
l_ok
ptaaWriteDebug(const char  *filename,
               PTAA        *ptaa,
               l_int32      type)
{
    if (LeptDebugOK) {
        return ptaaWrite(filename, ptaa, type);
    } else {
        L_INFO("write to named temp file %s is disabled\n", __func__, filename);
        return 0;
    }
}


/*!
 * \brief   ptaaWrite()
 *
 * \param[in]    filename
 * \param[in]    ptaa
 * \param[in]    type      0 for float values; 1 for integer values
 * \return  0 if OK, 1 on error
 */
l_ok
ptaaWrite(const char  *filename,
          PTAA        *ptaa,
          l_int32      type)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = ptaaWriteStream(fp, ptaa, type);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("ptaa not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   ptaaWriteStream()
 *
 * \param[in]    fp      file stream
 * \param[in]    ptaa
 * \param[in]    type    0 for float values; 1 for integer values
 * \return  0 if OK; 1 on error
 */
l_ok
ptaaWriteStream(FILE    *fp,
                PTAA    *ptaa,
                l_int32  type)
{
l_int32  i, n;
PTA     *pta;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);

    n = ptaaGetCount(ptaa);
    fprintf(fp, "\nPtaa Version %d\n", PTA_VERSION_NUMBER);
    fprintf(fp, "Number of Pta = %d\n", n);
    for (i = 0; i < n; i++) {
        pta = ptaaGetPta(ptaa, i, L_CLONE);
        ptaWriteStream(fp, pta, type);
        ptaDestroy(&pta);
    }

    return 0;
}


/*!
 * \brief   ptaaWriteMem()
 *
 * \param[out]   pdata    data of serialized ptaa; ascii
 * \param[out]   psize    size of returned data
 * \param[in]    ptaa
 * \param[in]    type     0 for float values; 1 for integer values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes %ptaa in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
ptaaWriteMem(l_uint8  **pdata,
             size_t    *psize,
             PTAA      *ptaa,
             l_int32    type)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = ptaaWriteStream(fp, ptaa, type);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = ptaaWriteStream(fp, ptaa, type);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}
