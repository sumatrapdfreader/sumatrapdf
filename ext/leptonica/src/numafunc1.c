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
 * \file  numafunc1.c
 * <pre>
 *
 *      --------------------------------------
 *      This file has these Numa utilities:
 *         - arithmetic operations
 *         - simple data analysis
 *         - generation of special sequences
 *         - permutations
 *         - interpolation
 *         - sorting
 *         - data analysis requiring sorting
 *         - joins and rearrangements
 *      --------------------------------------
 *
 *      Arithmetic and logic
 *          NUMA        *numaArithOp()
 *          NUMA        *numaLogicalOp()
 *          NUMA        *numaInvert()
 *          l_int32      numaSimilar()
 *          l_int32      numaAddToNumber()
 *
 *      Simple extractions
 *          l_int32      numaGetMin()
 *          l_int32      numaGetMax()
 *          l_int32      numaGetSum()
 *          NUMA        *numaGetPartialSums()
 *          l_int32      numaGetSumOnInterval()
 *          l_int32      numaHasOnlyIntegers()
 *          l_int32      numaGetMean()
 *          l_int32      numaGetMeanAbsval()
 *          NUMA        *numaSubsample()
 *          NUMA        *numaMakeDelta()
 *          NUMA        *numaMakeSequence()
 *          NUMA        *numaMakeConstant()
 *          NUMA        *numaMakeAbsval()
 *          NUMA        *numaAddBorder()
 *          NUMA        *numaAddSpecifiedBorder()
 *          NUMA        *numaRemoveBorder()
 *          l_int32      numaCountNonzeroRuns()
 *          l_int32      numaGetNonzeroRange()
 *          l_int32      numaGetCountRelativeToZero()
 *          NUMA        *numaClipToInterval()
 *          NUMA        *numaMakeThresholdIndicator()
 *          NUMA        *numaUniformSampling()
 *          NUMA        *numaReverse()
 *
 *      Signal feature extraction
 *          NUMA        *numaLowPassIntervals()
 *          NUMA        *numaThresholdEdges()
 *          NUMA        *numaGetSpanValues()
 *          NUMA        *numaGetEdgeValues()
 *
 *      Interpolation
 *          l_int32      numaInterpolateEqxVal()
 *          l_int32      numaInterpolateEqxInterval()
 *          l_int32      numaInterpolateArbxVal()
 *          l_int32      numaInterpolateArbxInterval()
 *
 *      Functions requiring interpolation
 *          l_int32      numaFitMax()
 *          l_int32      numaDifferentiateInterval()
 *          l_int32      numaIntegrateInterval()
 *
 *      Sorting
 *          NUMA        *numaSortGeneral()
 *          NUMA        *numaSortAutoSelect()
 *          NUMA        *numaSortIndexAutoSelect()
 *          l_int32      numaChooseSortType()
 *          NUMA        *numaSort()
 *          NUMA        *numaBinSort()
 *          NUMA        *numaGetSortIndex()
 *          NUMA        *numaGetBinSortIndex()
 *          NUMA        *numaSortByIndex()
 *          l_int32      numaIsSorted()
 *          l_int32      numaSortPair()
 *          NUMA        *numaInvertMap()
 *          l_int32      numaAddSorted()
 *          l_int32      numaFindSortedLoc()
 *
 *      Random permutation
 *          NUMA        *numaPseudorandomSequence()
 *          NUMA        *numaRandomPermutation()
 *
 *      Functions requiring sorting
 *          l_int32      numaGetRankValue()
 *          l_int32      numaGetMedian()
 *          l_int32      numaGetBinnedMedian()
 *          l_int32      numaGetMeanDevFromMedian()
 *          l_int32      numaGetMedianDevFromMedian()
 *          l_int32      numaGetMode()
 *
 *      Rearrangements
 *          l_int32      numaJoin()
 *          l_int32      numaaJoin()
 *          NUMA        *numaaFlattenToNuma()
 *
 *    Things to remember when using the Numa:
 *
 *    (1) The numa is a struct, not an array.  Always use accessors
 *        (see numabasic.c), never the fields directly.
 *
 *    (2) The number array holds l_float32 values.  It can also
 *        be used to store l_int32 values.  See numabasic.c for
 *        details on using the accessors.
 *
 *    (3) If you use numaCreate(), no numbers are stored and the size is 0.
 *        You have to add numbers to increase the size.
 *        If you want to start with a numa of a fixed size, with each
 *        entry initialized to the same value, use numaMakeConstant().
 *
 *    (4) Occasionally, in the comments we denote the i-th element of a
 *        numa by na[i].  This is conceptual only -- the numa is not an array!
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"
#include "array_internal.h"

/*----------------------------------------------------------------------*
 *                Arithmetic and logical ops on Numas                   *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaArithOp()
 *
 * \param[in]    nad     [optional] can be null or equal to na1 (in-place
 * \param[in]    na1
 * \param[in]    na2
 * \param[in]    op      L_ARITH_ADD, L_ARITH_SUBTRACT,
 *                       L_ARITH_MULTIPLY, L_ARITH_DIVIDE
 * \return  nad always: operation applied to na1 and na2
 *
 * <pre>
 * Notes:
 *      (1) The sizes of na1 and na2 must be equal.
 *      (2) nad can only null or equal to na1.
 *      (3) To add a constant to a numa, or to multiply a numa by
 *          a constant, use numaTransform().
 * </pre>
 */
NUMA *
numaArithOp(NUMA    *nad,
            NUMA    *na1,
            NUMA    *na2,
            l_int32  op)
{
l_int32    i, n;
l_float32  val1, val2;

    if (!na1 || !na2)
        return (NUMA *)ERROR_PTR("na1, na2 not both defined", __func__, nad);
    n = numaGetCount(na1);
    if (n != numaGetCount(na2))
        return (NUMA *)ERROR_PTR("na1, na2 sizes differ", __func__, nad);
    if (nad && nad != na1)
        return (NUMA *)ERROR_PTR("nad defined but not in-place", __func__, nad);
    if (op != L_ARITH_ADD && op != L_ARITH_SUBTRACT &&
        op != L_ARITH_MULTIPLY && op != L_ARITH_DIVIDE)
        return (NUMA *)ERROR_PTR("invalid op", __func__, nad);
    if (op == L_ARITH_DIVIDE) {
        for (i = 0; i < n; i++) {
            numaGetFValue(na2, i, &val2);
            if (val2 == 0.0)
                return (NUMA *)ERROR_PTR("na2 has 0 element", __func__, nad);
        }
    }

        /* If nad is not identical to na1, make it an identical copy */
    if (!nad)
        nad = numaCopy(na1);

    for (i = 0; i < n; i++) {
        numaGetFValue(nad, i, &val1);
        numaGetFValue(na2, i, &val2);
        switch (op) {
        case L_ARITH_ADD:
            numaSetValue(nad, i, val1 + val2);
            break;
        case L_ARITH_SUBTRACT:
            numaSetValue(nad, i, val1 - val2);
            break;
        case L_ARITH_MULTIPLY:
            numaSetValue(nad, i, val1 * val2);
            break;
        case L_ARITH_DIVIDE:
            numaSetValue(nad, i, val1 / val2);
            break;
        default:
            lept_stderr(" Unknown arith op: %d\n", op);
            return nad;
        }
    }

    return nad;
}


/*!
 * \brief   numaLogicalOp()
 *
 * \param[in]    nad     [optional] can be null or equal to na1 (in-place
 * \param[in]    na1
 * \param[in]    na2
 * \param[in]    op      L_UNION, L_INTERSECTION, L_SUBTRACTION, L_EXCLUSIVE_OR
 * \return  nad always: operation applied to na1 and na2
 *
 * <pre>
 * Notes:
 *      (1) The sizes of na1 and na2 must be equal.
 *      (2) nad can only be null or equal to na1.
 *      (3) This is intended for use with indicator arrays (0s and 1s).
 *          Input data is extracted as integers (0 == false, anything
 *          else == true); output results are 0 and 1.
 *      (4) L_SUBTRACTION is subtraction of val2 from val1.  For bit logical
 *          arithmetic this is (val1 & ~val2), but because these values
 *          are integers, we use (val1 && !val2).
 * </pre>
 */
NUMA *
numaLogicalOp(NUMA    *nad,
              NUMA    *na1,
              NUMA    *na2,
              l_int32  op)
{
l_int32  i, n, val1, val2, val;

    if (!na1 || !na2)
        return (NUMA *)ERROR_PTR("na1, na2 not both defined", __func__, nad);
    n = numaGetCount(na1);
    if (n != numaGetCount(na2))
        return (NUMA *)ERROR_PTR("na1, na2 sizes differ", __func__, nad);
    if (nad && nad != na1)
        return (NUMA *)ERROR_PTR("nad defined; not in-place", __func__, nad);
    if (op != L_UNION && op != L_INTERSECTION &&
        op != L_SUBTRACTION && op != L_EXCLUSIVE_OR)
        return (NUMA *)ERROR_PTR("invalid op", __func__, nad);

        /* If nad is not identical to na1, make it an identical copy */
    if (!nad)
        nad = numaCopy(na1);

    for (i = 0; i < n; i++) {
        numaGetIValue(nad, i, &val1);
        numaGetIValue(na2, i, &val2);
        val1 = (val1 == 0) ? 0 : 1;
        val2 = (val2 == 0) ? 0 : 1;
        switch (op) {
        case L_UNION:
            val = (val1 || val2) ? 1 : 0;
            numaSetValue(nad, i, val);
            break;
        case L_INTERSECTION:
            val = (val1 && val2) ? 1 : 0;
            numaSetValue(nad, i, val);
            break;
        case L_SUBTRACTION:
            val = (val1 && !val2) ? 1 : 0;
            numaSetValue(nad, i, val);
            break;
        case L_EXCLUSIVE_OR:
            val = (val1 != val2) ? 1 : 0;
            numaSetValue(nad, i, val);
            break;
        default:
            lept_stderr(" Unknown logical op: %d\n", op);
            return nad;
        }
    }

    return nad;
}


/*!
 * \brief   numaInvert()
 *
 * \param[in]    nad    [optional] can be null or equal to nas (in-place
 * \param[in]    nas
 * \return  nad always: 'inverts' nas
 *
 * <pre>
 * Notes:
 *      (1) This is intended for use with indicator arrays (0s and 1s).
 *          It gives a boolean-type output, taking the input as
 *          an integer and inverting it:
 *              0              -->  1
 *              anything else  -->   0
 * </pre>
 */
NUMA *
numaInvert(NUMA  *nad,
           NUMA  *nas)
{
l_int32  i, n, val;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, nad);
    if (nad && nad != nas)
        return (NUMA *)ERROR_PTR("nad defined; not in-place", __func__, nad);

    if (!nad)
        nad = numaCopy(nas);
    n = numaGetCount(nad);
    for (i = 0; i < n; i++) {
        numaGetIValue(nad, i, &val);
        if (!val)
            val = 1;
        else
            val = 0;
        numaSetValue(nad, i, val);
    }
    return nad;
}


/*!
 * \brief   numaSimilar()
 *
 * \param[in]    na1
 * \param[in]    na2
 * \param[in]    maxdiff    use 0.0 for exact equality
 * \param[out]   psimilar   1 if similar; 0 if different
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Float values can differ slightly due to roundoff and
 *          accumulated errors.  Using %maxdiff > 0.0 allows similar
 *          arrays to be identified.
 * </pre>
*/
l_int32
numaSimilar(NUMA      *na1,
            NUMA      *na2,
            l_float32  maxdiff,
            l_int32   *psimilar)
{
l_int32    i, n;
l_float32  val1, val2;

    if (!psimilar)
        return ERROR_INT("&similar not defined", __func__, 1);
    *psimilar = 0;
    if (!na1 || !na2)
        return ERROR_INT("na1 and na2 not both defined", __func__, 1);
    maxdiff = L_ABS(maxdiff);

    n = numaGetCount(na1);
    if (n != numaGetCount(na2)) return 0;

    for (i = 0; i < n; i++) {
        numaGetFValue(na1, i, &val1);
        numaGetFValue(na2, i, &val2);
        if (L_ABS(val1 - val2) > maxdiff) return 0;
    }

    *psimilar = 1;
    return 0;
}


/*!
 * \brief   numaAddToNumber()
 *
 * \param[in]    na       source numa
 * \param[in]    index    element to be changed
 * \param[in]    val      new value to be added
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is useful for accumulating sums, regardless of the index
 *          order in which the values are made available.
 *      (2) Before use, the numa has to be filled up to %index.  This would
 *          typically be used by creating the numa with the full sized
 *          array, initialized to 0.0, using numaMakeConstant().
 * </pre>
 */
l_ok
numaAddToNumber(NUMA      *na,
                l_int32    index,
                l_float32  val)
{
l_int32  n;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

    na->array[index] += val;
    return 0;
}


/*----------------------------------------------------------------------*
 *                         Simple extractions                           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaGetMin()
 *
 * \param[in]    na        source numa
 * \param[out]   pminval   [optional] min value
 * \param[out]   piminloc  [optional] index of min location
 * \return  0 if OK; 1 on error
 */
l_ok
numaGetMin(NUMA       *na,
           l_float32  *pminval,
           l_int32    *piminloc)
{
l_int32    i, n, iminloc;
l_float32  val, minval;

    if (!pminval && !piminloc)
        return ERROR_INT("nothing to do", __func__, 1);
    if (pminval) *pminval = 0.0;
    if (piminloc) *piminloc = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    minval = +1000000000.;
    iminloc = 0;
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        if (val < minval) {
            minval = val;
            iminloc = i;
        }
    }

    if (pminval) *pminval = minval;
    if (piminloc) *piminloc = iminloc;
    return 0;
}


/*!
 * \brief   numaGetMax()
 *
 * \param[in]    na        source numa
 * \param[out]   pmaxval   [optional] max value
 * \param[out]   pimaxloc  [optional] index of max location
 * \return  0 if OK; 1 on error
 */
l_ok
numaGetMax(NUMA       *na,
           l_float32  *pmaxval,
           l_int32    *pimaxloc)
{
l_int32    i, n, imaxloc;
l_float32  val, maxval;

    if (!pmaxval && !pimaxloc)
        return ERROR_INT("nothing to do", __func__, 1);
    if (pmaxval) *pmaxval = 0.0;
    if (pimaxloc) *pimaxloc = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    maxval = -1000000000.;
    imaxloc = 0;
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        if (val > maxval) {
            maxval = val;
            imaxloc = i;
        }
    }

    if (pmaxval) *pmaxval = maxval;
    if (pimaxloc) *pimaxloc = imaxloc;
    return 0;
}


/*!
 * \brief   numaGetSum()
 *
 * \param[in]    na     source numa
 * \param[out]   psum   sum of values
 * \return  0 if OK, 1 on error
 */
l_ok
numaGetSum(NUMA       *na,
           l_float32  *psum)
{
l_int32    i, n;
l_float32  val, sum;

    if (!psum)
        return ERROR_INT("&sum not defined", __func__, 1);
    *psum = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);
    sum = 0.0;
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        sum += val;
    }
    *psum = sum;
    return 0;
}


/*!
 * \brief   numaGetPartialSums()
 *
 * \param[in]    na    source numa
 * \return  nasum, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) nasum[i] is the sum for all j <= i of na[j].
 *          So nasum[0] = na[0].
 *      (2) If you want to generate a rank function, where rank[0] - 0.0,
 *          insert a 0.0 at the beginning of the nasum array.
 * </pre>
 */
NUMA *
numaGetPartialSums(NUMA  *na)
{
l_int32    i, n;
l_float32  val, sum;
NUMA      *nasum;

    if (!na)
        return (NUMA *)ERROR_PTR("na not defined", __func__, NULL);

    if ((n = numaGetCount(na)) == 0)
        L_WARNING("na is empty\n", __func__);
    nasum = numaCreate(n);
    sum = 0.0;
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        sum += val;
        numaAddNumber(nasum, sum);
    }
    return nasum;
}


/*!
 * \brief   numaGetSumOnInterval()
 *
 * \param[in]    na      source numa
 * \param[in]    first   beginning index
 * \param[in]    last    final index; use -1 to go to the end
 * \param[out]   psum    sum of values in the index interval range
 * \return  0 if OK, 1 on error
 */
l_ok
numaGetSumOnInterval(NUMA       *na,
                     l_int32     first,
                     l_int32     last,
                     l_float32  *psum)
{
l_int32    i, n;
l_float32  val, sum;

    if (!psum)
        return ERROR_INT("&sum not defined", __func__, 1);
    *psum = 0.0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    sum = 0.0;
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);
    if (first < 0) first = 0;
    if (first >= n || last < -1)  /* not an error */
        return 0;
    if (last == -1)
        last = n - 1;
    last = L_MIN(last, n - 1);

    for (i = first; i <= last; i++) {
        numaGetFValue(na, i, &val);
        sum += val;
    }
    *psum = sum;
    return 0;
}


/*!
 * \brief   numaHasOnlyIntegers()
 *
 * \param[in]    na           source numa
 * \param[out]   pallints     1 if all sampled values are ints; else 0
 * \return  0 if OK, 1 on error
 */
l_ok
numaHasOnlyIntegers(NUMA     *na,
                    l_int32  *pallints)
{
l_int32    i, n;
l_float32  val;

    if (!pallints)
        return ERROR_INT("&allints not defined", __func__, 1);
    *pallints = TRUE;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);
    for (i = 0; i < n; i ++) {
        numaGetFValue(na, i, &val);
        if (val != (l_int32)val) {
            *pallints = FALSE;
            return 0;
        }
    }
    return 0;
}


/*!
 * \brief   numaGetMean()
 *
 * \param[in]    na     source numa
 * \param[out]   pave   average of values
 * \return  0 if OK, 1 on error
 */
l_ok
numaGetMean(NUMA       *na,
            l_float32  *pave)
{
l_int32    n;
l_float32  sum;

    if (!pave)
        return ERROR_INT("&ave not defined", __func__, 1);
    *pave = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    numaGetSum(na, &sum);
    *pave = sum / n;
    return 0;
}

/*!
 * \brief   numaGetMeanAbsval()
 *
 * \param[in]    na         source numa
 * \param[out]   paveabs    average of absolute values
 * \return  0 if OK, 1 on error
 */
l_ok
numaGetMeanAbsval(NUMA       *na,
                  l_float32  *paveabs)
{
l_int32  n;
NUMA    *na1;

    if (!paveabs)
        return ERROR_INT("&aveabs not defined", __func__, 1);
    *paveabs = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    na1 = numaMakeAbsval(NULL, na);
    numaGetMean(na1, paveabs);
    numaDestroy(&na1);
    return 0;
}


/*!
 * \brief   numaSubsample()
 *
 * \param[in]    nas
 * \param[in]    subfactor    subsample factor, >= 1
 * \return  nad evenly sampled values from nas, or NULL on error
 */
NUMA *
numaSubsample(NUMA    *nas,
              l_int32  subfactor)
{
l_int32    i, n;
l_float32  val;
NUMA      *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (subfactor < 1)
        return (NUMA *)ERROR_PTR("subfactor < 1", __func__, NULL);

    nad = numaCreate(0);
    if ((n = numaGetCount(nas)) == 0)
        L_WARNING("nas is empty\n", __func__);
    for (i = 0; i < n; i++) {
        if (i % subfactor != 0) continue;
        numaGetFValue(nas, i, &val);
        numaAddNumber(nad, val);
    }

    return nad;
}


/*!
 * \brief   numaMakeDelta()
 *
 * \param[in]    nas    input numa
 * \return  numa of difference values val[i+1] - val[i],
 *                    or NULL on error
 */
NUMA *
numaMakeDelta(NUMA  *nas)
{
l_int32    i, n;
l_float32  prev, cur;
NUMA      *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if ((n = numaGetCount(nas)) < 2) {
        L_WARNING("n < 2; returning empty numa\n", __func__);
        return numaCreate(1);
    }

    nad = numaCreate(n - 1);
    numaGetFValue(nas, 0, &prev);
    for (i = 1; i < n; i++) {
        numaGetFValue(nas, i, &cur);
        numaAddNumber(nad, cur - prev);
        prev = cur;
    }
    return nad;
}


/*!
 * \brief   numaMakeSequence()
 *
 * \param[in]    startval
 * \param[in]    increment
 * \param[in]    size        of sequence
 * \return  numa of sequence of evenly spaced values, or NULL on error
 */
NUMA *
numaMakeSequence(l_float32  startval,
                 l_float32  increment,
                 l_int32    size)
{
l_int32    i;
l_float32  val;
NUMA      *na;

    if ((na = numaCreate(size)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);

    for (i = 0; i < size; i++) {
        val = startval + i * increment;
        numaAddNumber(na, val);
    }
    return na;
}


/*!
 * \brief   numaMakeConstant()
 *
 * \param[in]    val
 * \param[in]    size     of numa
 * \return  numa of given size with all entries equal to 'val',
 *              or NULL on error
 */
NUMA *
numaMakeConstant(l_float32  val,
                 l_int32    size)
{
    return numaMakeSequence(val, 0.0, size);
}


/*!
 * \brief   numaMakeAbsval()
 *
 * \param[in]    nad   can be null for new array, or the same as nas for inplace
 * \param[in]    nas   input numa
 * \return  nad with all numbers being the absval of the input,
 *              or NULL on error
 */
NUMA *
numaMakeAbsval(NUMA  *nad,
               NUMA  *nas)
{
l_int32    i, n;
l_float32  val;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (nad && nad != nas)
        return (NUMA *)ERROR_PTR("nad and not in-place", __func__, NULL);

    if (!nad)
        nad = numaCopy(nas);
    n = numaGetCount(nad);
    for (i = 0; i < n; i++) {
        val = nad->array[i];
        nad->array[i] = L_ABS(val);
    }

    return nad;
}


/*!
 * \brief   numaAddBorder()
 *
 * \param[in]    nas
 * \param[in]    left    number of elements to add before the start
 * \param[in]    right   number of elements to add after the end
 * \param[in]    val     initialize border elements
 * \return  nad with added elements at left and right, or NULL on error
 */
NUMA *
numaAddBorder(NUMA      *nas,
              l_int32    left,
              l_int32    right,
              l_float32  val)
{
l_int32     i, n, len;
l_float32   startx, delx;
l_float32  *fas, *fad;
NUMA       *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (left < 0) left = 0;
    if (right < 0) right = 0;
    if (left == 0 && right == 0)
        return numaCopy(nas);

    n = numaGetCount(nas);
    len = n + left + right;
    nad = numaMakeConstant(val, len);
    numaGetParameters(nas, &startx, &delx);
    numaSetParameters(nad, startx - delx * left, delx);
    fas = numaGetFArray(nas, L_NOCOPY);
    fad = numaGetFArray(nad, L_NOCOPY);
    for (i = 0; i < n; i++)
        fad[left + i] = fas[i];

    return nad;
}


/*!
 * \brief   numaAddSpecifiedBorder()
 *
 * \param[in]    nas
 * \param[in]    left    number of elements to add before the start
 * \param[in]    right   number of elements to add after the end
 * \param[in]    type    L_CONTINUED_BORDER, L_MIRRORED_BORDER
 * \return  nad with added elements at left and right, or NULL on error
 */
NUMA *
numaAddSpecifiedBorder(NUMA    *nas,
                       l_int32  left,
                       l_int32  right,
                       l_int32  type)
{
l_int32     i, n;
l_float32  *fa;
NUMA       *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (left < 0) left = 0;
    if (right < 0) right = 0;
    if (left == 0 && right == 0)
        return numaCopy(nas);
    if (type != L_CONTINUED_BORDER && type != L_MIRRORED_BORDER)
        return (NUMA *)ERROR_PTR("invalid type", __func__, NULL);
    n = numaGetCount(nas);
    if (type == L_MIRRORED_BORDER && (left > n || right > n))
        return (NUMA *)ERROR_PTR("border too large", __func__, NULL);

    nad = numaAddBorder(nas, left, right, 0);
    n = numaGetCount(nad);
    fa = numaGetFArray(nad, L_NOCOPY);
    if (type == L_CONTINUED_BORDER) {
        for (i = 0; i < left; i++)
            fa[i] = fa[left];
        for (i = n - right; i < n; i++)
            fa[i] = fa[n - right - 1];
    } else {  /* type == L_MIRRORED_BORDER */
        for (i = 0; i < left; i++)
            fa[i] = fa[2 * left - 1 - i];
        for (i = 0; i < right; i++)
            fa[n - right + i] = fa[n - right - i - 1];
    }

    return nad;
}


/*!
 * \brief   numaRemoveBorder()
 *
 * \param[in]    nas
 * \param[in]    left    number of elements to remove from the start
 * \param[in]    right   number of elements to remove up to the end
 * \return  nad with removed elements at left and right, or NULL on error
 */
NUMA *
numaRemoveBorder(NUMA      *nas,
                 l_int32    left,
                 l_int32    right)
{
l_int32     i, n, len;
l_float32   startx, delx;
l_float32  *fas, *fad;
NUMA       *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (left < 0) left = 0;
    if (right < 0) right = 0;
    if (left == 0 && right == 0)
        return numaCopy(nas);

    n = numaGetCount(nas);
    if ((len = n - left - right) < 0)
        return (NUMA *)ERROR_PTR("len < 0 after removal", __func__, NULL);
    nad = numaMakeConstant(0, len);
    numaGetParameters(nas, &startx, &delx);
    numaSetParameters(nad, startx + delx * left, delx);
    fas = numaGetFArray(nas, L_NOCOPY);
    fad = numaGetFArray(nad, L_NOCOPY);
    for (i = 0; i < len; i++)
        fad[i] = fas[left + i];

    return nad;
}


/*!
 * \brief   numaCountNonzeroRuns()
 *
 * \param[in]    na      e.g., of pixel counts in rows or columns
 * \param[out]   pcount  number of nonzero runs
 * \return  0 if OK, 1 on error
 */
l_ok
numaCountNonzeroRuns(NUMA     *na,
                     l_int32  *pcount)
{
l_int32  n, i, val, count, inrun;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    count = 0;
    inrun = FALSE;
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &val);
        if (!inrun && val > 0) {
            count++;
            inrun = TRUE;
        } else if (inrun && val == 0) {
            inrun = FALSE;
        }
    }
    *pcount = count;
    return 0;
}


/*!
 * \brief   numaGetNonzeroRange()
 *
 * \param[in]    na              source numa
 * \param[in]    eps             largest value considered to be zero
 * \param[out]   pfirst, plast   interval of array indices
 *                               where values are nonzero
 * \return  0 if OK, 1 on error or if no nonzero range is found.
 */
l_ok
numaGetNonzeroRange(NUMA      *na,
                    l_float32  eps,
                    l_int32   *pfirst,
                    l_int32   *plast)
{
l_int32    n, i, found;
l_float32  val;

    if (pfirst) *pfirst = 0;
    if (plast) *plast = 0;
    if (!pfirst || !plast)
        return ERROR_INT("pfirst and plast not both defined", __func__, 1);
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    found = FALSE;
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        if (val > eps) {
            found = TRUE;
            break;
        }
    }
    if (!found) {
        *pfirst = n - 1;
        *plast = 0;
        return 1;
    }

    *pfirst = i;
    for (i = n - 1; i >= 0; i--) {
        numaGetFValue(na, i, &val);
        if (val > eps)
            break;
    }
    *plast = i;
    return 0;
}


/*!
 * \brief   numaGetCountRelativeToZero()
 *
 * \param[in]    na      source numa
 * \param[in]    type    L_LESS_THAN_ZERO, L_EQUAL_TO_ZERO, L_GREATER_THAN_ZERO
 * \param[out]   pcount  count of values of given type
 * \return  0 if OK, 1 on error
 */
l_ok
numaGetCountRelativeToZero(NUMA     *na,
                           l_int32   type,
                           l_int32  *pcount)
{
l_int32    n, i, count;
l_float32  val;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    for (i = 0, count = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        if (type == L_LESS_THAN_ZERO && val < 0.0)
            count++;
        else if (type == L_EQUAL_TO_ZERO && val == 0.0)
            count++;
        else if (type == L_GREATER_THAN_ZERO && val > 0.0)
            count++;
    }

    *pcount = count;
    return 0;
}


/*!
 * \brief   numaClipToInterval()
 *
 * \param[in]    nas
 * \param[in]    first    >= 0; <= last
 * \param[in]    last
 * \return  numa with the same values as the input, but clipped
 *              to the specified interval
 *
 * <pre>
 * Notes:
 *        If you want the indices of the array values to be unchanged,
 *        use first = 0.
 *  Usage:
 *        This is useful to clip a histogram that has a few nonzero
 *        values to its nonzero range.
 * </pre>
 */
NUMA *
numaClipToInterval(NUMA    *nas,
                   l_int32  first,
                   l_int32  last)
{
l_int32    n, i;
l_float32  val, startx, delx;
NUMA      *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if ((n = numaGetCount(nas)) == 0)
        return (NUMA *)ERROR_PTR("nas is empty", __func__, NULL);
    if (first < 0 || first > last)
        return (NUMA *)ERROR_PTR("range not valid", __func__, NULL);
    if (first >= n)
        return (NUMA *)ERROR_PTR("no elements in range", __func__, NULL);

    last = L_MIN(last, n - 1);
    if ((nad = numaCreate(last - first + 1)) == NULL)
        return (NUMA *)ERROR_PTR("nad not made", __func__, NULL);
    for (i = first; i <= last; i++) {
        numaGetFValue(nas, i, &val);
        numaAddNumber(nad, val);
    }
    numaGetParameters(nas, &startx, &delx);
    numaSetParameters(nad, startx + first * delx, delx);
    return nad;
}


/*!
 * \brief   numaMakeThresholdIndicator()
 *
 * \param[in]    nas      input numa
 * \param[in]    thresh   threshold value
 * \param[in]    type     L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                        L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \return   nad : indicator array: values are 0 and 1
 *
 * <pre>
 * Notes:
 *      (1) For each element in nas, if the constraint given by 'type'
 *          correctly specifies its relation to thresh, a value of 1
 *          is recorded in nad.
 * </pre>
 */
NUMA *
numaMakeThresholdIndicator(NUMA      *nas,
                           l_float32  thresh,
                           l_int32    type)
{
l_int32    n, i, ival;
l_float32  fval;
NUMA      *nai;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if ((n = numaGetCount(nas)) == 0)
        return (NUMA *)ERROR_PTR("nas is empty", __func__, NULL);

    nai = numaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetFValue(nas, i, &fval);
        ival = 0;
        switch (type)
        {
        case L_SELECT_IF_LT:
            if (fval < thresh) ival = 1;
            break;
        case L_SELECT_IF_GT:
            if (fval > thresh) ival = 1;
            break;
        case L_SELECT_IF_LTE:
            if (fval <= thresh) ival = 1;
            break;
        case L_SELECT_IF_GTE:
            if (fval >= thresh) ival = 1;
            break;
        default:
            numaDestroy(&nai);
            return (NUMA *)ERROR_PTR("invalid type", __func__, NULL);
        }
        numaAddNumber(nai, ival);
    }

    return nai;
}


/*!
 * \brief   numaUniformSampling()
 *
 * \param[in]    nas     input numa
 * \param[in]    nsamp   number of samples
 * \return  nad : resampled array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This resamples the values in the array, using %nsamp
 *          equal divisions.
 * </pre>
 */
NUMA *
numaUniformSampling(NUMA    *nas,
                    l_int32  nsamp)
{
l_int32     n, i, j, ileft, iright;
l_float32   left, right, binsize, lfract, rfract, sum, startx, delx;
l_float32  *array;
NUMA       *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if ((n = numaGetCount(nas)) == 0)
        return (NUMA *)ERROR_PTR("nas is empty", __func__, NULL);
    if (nsamp <= 0)
        return (NUMA *)ERROR_PTR("nsamp must be > 0", __func__, NULL);

    nad = numaCreate(nsamp);
    array = numaGetFArray(nas, L_NOCOPY);
    binsize = (l_float32)n / (l_float32)nsamp;
    numaGetParameters(nas, &startx, &delx);
    numaSetParameters(nad, startx, binsize * delx);
    left = 0.0;
    for (i = 0; i < nsamp; i++) {
        sum = 0.0;
        right = left + binsize;
        ileft = (l_int32)left;
        lfract = 1.0f - left + ileft;
        if (lfract >= 1.0)  /* on left bin boundary */
            lfract = 0.0;
        iright = (l_int32)right;
        rfract = right - iright;
        iright = L_MIN(iright, n - 1);
        if (ileft == iright) {  /* both are within the same original sample */
            sum += (lfract + rfract - 1.0f) * array[ileft];
        } else {
            if (lfract > 0.0001)  /* left fraction */
                sum += lfract * array[ileft];
            if (rfract > 0.0001)  /* right fraction */
                sum += rfract * array[iright];
            for (j = ileft + 1; j < iright; j++)  /* entire pixels */
                sum += array[j];
        }

        numaAddNumber(nad, sum);
        left = right;
    }
    return nad;
}


/*!
 * \brief   numaReverse()
 *
 * \param[in]    nad    [optional] can be null or equal to nas
 * \param[in]    nas    input numa
 * \return  nad : reversed, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Usage:
 *            numaReverse(nas, nas);   // in-place
 *            nad = numaReverse(NULL, nas);  // makes a new one
 * </pre>
 */
NUMA *
numaReverse(NUMA  *nad,
            NUMA  *nas)
{
l_int32    n, i;
l_float32  val1, val2;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (nad && nas != nad)
        return (NUMA *)ERROR_PTR("nad defined but != nas", __func__, NULL);

    n = numaGetCount(nas);
    if (nad) {  /* in-place */
        for (i = 0; i < n / 2; i++) {
            numaGetFValue(nad, i, &val1);
            numaGetFValue(nad, n - i - 1, &val2);
            numaSetValue(nad, i, val2);
            numaSetValue(nad, n - i - 1, val1);
        }
    } else {
        nad = numaCreate(n);
        for (i = n - 1; i >= 0; i--) {
            numaGetFValue(nas, i, &val1);
            numaAddNumber(nad, val1);
        }
    }

        /* Reverse the startx and delx fields */
    nad->startx = nas->startx + (n - 1) * nas->delx;
    nad->delx = -nas->delx;
    return nad;
}


/*----------------------------------------------------------------------*
 *                       Signal feature extraction                      *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaLowPassIntervals()
 *
 * \param[in]    nas      input numa
 * \param[in]    thresh   threshold fraction of max; in [0.0 ... 1.0]
 * \param[in]    maxn     for normalizing; set maxn = 0.0 to use the max in nas
 * \return  nad : interval abscissa pairs, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For each interval where the value is less than a specified
 *          fraction of the maximum, this records the left and right "x"
 *          value.
 * </pre>
 */
NUMA *
numaLowPassIntervals(NUMA      *nas,
                     l_float32  thresh,
                     l_float32  maxn)
{
l_int32    n, i, inrun;
l_float32  maxval, threshval, fval, startx, delx, x0, x1;
NUMA      *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if ((n = numaGetCount(nas)) == 0)
        return (NUMA *)ERROR_PTR("nas is empty", __func__, NULL);
    if (thresh < 0.0 || thresh > 1.0)
        return (NUMA *)ERROR_PTR("invalid thresh", __func__, NULL);

        /* The input threshold is a fraction of the max.
         * The first entry in nad is the value of the max. */
    if (maxn == 0.0)
        numaGetMax(nas, &maxval, NULL);
    else
        maxval = maxn;
    numaGetParameters(nas, &startx, &delx);
    threshval = thresh * maxval;
    nad = numaCreate(0);
    numaAddNumber(nad, maxval);

        /* Write pairs of pts (x0, x1) for the intervals */
    inrun = FALSE;
    for (i = 0; i < n; i++) {
        numaGetFValue(nas, i, &fval);
        if (fval < threshval && inrun == FALSE) {  /* start a new run */
            inrun = TRUE;
            x0 = startx + i * delx;
        } else if (fval > threshval && inrun == TRUE) {  /* end the run */
            inrun = FALSE;
            x1 = startx + i * delx;
            numaAddNumber(nad, x0);
            numaAddNumber(nad, x1);
        }
    }
    if (inrun == TRUE) {  /* must end the last run */
        x1 = startx + (n - 1) * delx;
        numaAddNumber(nad, x0);
        numaAddNumber(nad, x1);
    }

    return nad;
}


/*!
 * \brief   numaThresholdEdges()
 *
 * \param[in]    nas      input numa
 * \param[in]    thresh1  low threshold as fraction of max; in [0.0 ... 1.0]
 * \param[in]    thresh2  high threshold as fraction of max; in [0.0 ... 1.0]
 * \param[in]    maxn     for normalizing; set maxn = 0.0 to use the max in nas
 * \return  nad   edge interval triplets, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For each edge interval, where the value is less
 *          than %thresh1 on one side, greater than %thresh2 on
 *          the other, and between these thresholds throughout the
 *          interval, this records a triplet of values: the
 *          'left' and 'right' edges, and either +1 or -1, depending
 *          on whether the edge is rising or falling.
 *      (2) No assumption is made about the value outside the array,
 *          so if the value at the array edge is between the threshold
 *          values, it is not considered part of an edge.  We start
 *          looking for edge intervals only after leaving the thresholded
 *          band.
 * </pre>
 */
NUMA *
numaThresholdEdges(NUMA      *nas,
                   l_float32  thresh1,
                   l_float32  thresh2,
                   l_float32  maxn)
{
l_int32    n, i, istart, inband, output, sign;
l_int32    startbelow, below, above, belowlast, abovelast;
l_float32  maxval, threshval1, threshval2, fval, startx, delx, x0, x1;
NUMA      *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if ((n = numaGetCount(nas)) == 0)
        return (NUMA *)ERROR_PTR("nas is empty", __func__, NULL);
    if (thresh1 < 0.0 || thresh1 > 1.0 || thresh2 < 0.0 || thresh2 > 1.0)
        return (NUMA *)ERROR_PTR("invalid thresholds", __func__, NULL);
    if (thresh2 < thresh1)
        return (NUMA *)ERROR_PTR("thresh2 < thresh1", __func__, NULL);

        /* The input thresholds are fractions of the max.
         * The first entry in nad is the value of the max used
         * here for normalization. */
    if (maxn == 0.0)
        numaGetMax(nas, &maxval, NULL);
    else
        maxval = maxn;
    numaGetMax(nas, &maxval, NULL);
    numaGetParameters(nas, &startx, &delx);
    threshval1 = thresh1 * maxval;
    threshval2 = thresh2 * maxval;
    nad = numaCreate(0);
    numaAddNumber(nad, maxval);

        /* Write triplets of pts (x0, x1, sign) for the edges.
         * First make sure we start search from outside the band.
         * Only one of {belowlast, abovelast} is true. */
    for (i = 0; i < n; i++) {
        istart = i;
        numaGetFValue(nas, i, &fval);
        belowlast = (fval < threshval1) ? TRUE : FALSE;
        abovelast = (fval > threshval2) ? TRUE : FALSE;
        if (belowlast == TRUE || abovelast == TRUE)
            break;
    }
    if (istart == n)  /* no intervals found */
        return nad;

        /* x0 and x1 can only be set from outside the edge.
         * They are the values just before entering the band,
         * and just after entering the band.  We can jump through
         * the band, in which case they differ by one index in nas. */
    inband = FALSE;
    startbelow = belowlast; /* one of these is true */
    output = FALSE;
    x0 = startx + istart * delx;
    for (i = istart + 1; i < n; i++) {
        numaGetFValue(nas, i, &fval);
        below = (fval < threshval1) ? TRUE : FALSE;
        above = (fval > threshval2) ? TRUE : FALSE;
        if (!inband && belowlast && above) {  /* full jump up */
            x1 = startx + i * delx;
            sign = 1;
            startbelow = FALSE;  /* for the next transition */
            output = TRUE;
        } else if (!inband && abovelast && below) {  /* full jump down */
            x1 = startx + i * delx;
            sign = -1;
            startbelow = TRUE;  /* for the next transition */
            output = TRUE;
        } else if (inband && startbelow && above) {  /* exit rising; success */
            x1 = startx + i * delx;
            sign = 1;
            inband = FALSE;
            startbelow = FALSE;  /* for the next transition */
            output = TRUE;
        } else if (inband && !startbelow && below) {
                /* exit falling; success */
            x1 = startx + i * delx;
            sign = -1;
            inband = FALSE;
            startbelow = TRUE;  /* for the next transition */
            output = TRUE;
        } else if (inband && !startbelow && above) {  /* exit rising; failure */
            x0 = startx + i * delx;
            inband = FALSE;
        } else if (inband && startbelow && below) {  /* exit falling; failure */
            x0 = startx + i * delx;
            inband = FALSE;
        } else if (!inband && !above && !below) {  /* enter */
            inband = TRUE;
            startbelow = belowlast;
        } else if (!inband && (above || below)) {  /* outside and remaining */
            x0 = startx + i * delx;  /* update position */
        }
        belowlast = below;
        abovelast = above;
        if (output) {  /* we have exited; save new x0 */
            numaAddNumber(nad, x0);
            numaAddNumber(nad, x1);
            numaAddNumber(nad, sign);
            output = FALSE;
            x0 = startx + i * delx;
        }
    }

    return nad;
}


/*!
 * \brief   numaGetSpanValues()
 *
 * \param[in]    na       numa that is output of numaLowPassIntervals()
 * \param[in]    span     span number, zero-based
 * \param[out]   pstart   [optional] location of start of transition
 * \param[out]   pend     [optional] location of end of transition
 * \return  0 if OK, 1 on error
 */
l_int32
numaGetSpanValues(NUMA    *na,
                  l_int32  span,
                  l_int32 *pstart,
                  l_int32 *pend)
{
l_int32  n, nspans;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);
    if (n % 2 != 1)
        return ERROR_INT("n is not odd", __func__, 1);
    nspans = n / 2;
    if (nspans < 0 || span >= nspans)
        return ERROR_INT("invalid span", __func__, 1);

    if (pstart) numaGetIValue(na, 2 * span + 1, pstart);
    if (pend) numaGetIValue(na, 2 * span + 2, pend);
    return 0;
}


/*!
 * \brief   numaGetEdgeValues()
 *
 * \param[in]    na       numa that is output of numaThresholdEdges()
 * \param[in]    edge     edge number, zero-based
 * \param[out]   pstart   [optional] location of start of transition
 * \param[out]   pend     [optional] location of end of transition
 * \param[out]   psign    [optional] transition sign: +1 is rising,
 *                        -1 is falling
 * \return  0 if OK, 1 on error
 */
l_int32
numaGetEdgeValues(NUMA    *na,
                  l_int32  edge,
                  l_int32 *pstart,
                  l_int32 *pend,
                  l_int32 *psign)
{
l_int32  n, nedges;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);
    if (n % 3 != 1)
        return ERROR_INT("n % 3 is not 1", __func__, 1);
    nedges = (n - 1) / 3;
    if (edge < 0 || edge >= nedges)
        return ERROR_INT("invalid edge", __func__, 1);

    if (pstart) numaGetIValue(na, 3 * edge + 1, pstart);
    if (pend) numaGetIValue(na, 3 * edge + 2, pend);
    if (psign) numaGetIValue(na, 3 * edge + 3, psign);
    return 0;
}


/*----------------------------------------------------------------------*
 *                             Interpolation                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaInterpolateEqxVal()
 *
 * \param[in]    startx   xval corresponding to first element in array
 * \param[in]    deltax   x increment between array elements
 * \param[in]    nay      numa of ordinate values, assumed equally spaced
 * \param[in]    type     L_LINEAR_INTERP, L_QUADRATIC_INTERP
 * \param[in]    xval
 * \param[out]   pyval    interpolated value
 * \return  0 if OK, 1 on error e.g., if xval is outside range
 *
 * <pre>
 * Notes:
 *      (1) Considering nay as a function of x, the x values
 *          are equally spaced
 *      (2) Caller should check for valid return.
 *
 *  For linear Lagrangian interpolation (through 2 data pts):
 *         y(x) = y1(x-x2)/(x1-x2) + y2(x-x1)/(x2-x1)
 *
 *  For quadratic Lagrangian interpolation (through 3 data pts):
 *         y(x) = y1(x-x2)(x-x3)/((x1-x2)(x1-x3)) +
 *                y2(x-x1)(x-x3)/((x2-x1)(x2-x3)) +
 *                y3(x-x1)(x-x2)/((x3-x1)(x3-x2))
 *
 * </pre>
 */
l_ok
numaInterpolateEqxVal(l_float32   startx,
                      l_float32   deltax,
                      NUMA       *nay,
                      l_int32     type,
                      l_float32   xval,
                      l_float32  *pyval)
{
l_int32     i, n, i1, i2, i3;
l_float32   x1, x2, x3, fy1, fy2, fy3, d1, d2, d3, del, fi, maxx;
l_float32  *fa;

    if (!pyval)
        return ERROR_INT("&yval not defined", __func__, 1);
    *pyval = 0.0;
    if (!nay)
        return ERROR_INT("nay not defined", __func__, 1);
    if (deltax <= 0.0)
        return ERROR_INT("deltax not > 0", __func__, 1);
    if (type != L_LINEAR_INTERP && type != L_QUADRATIC_INTERP)
        return ERROR_INT("invalid interp type", __func__, 1);
    if ((n = numaGetCount(nay)) < 2)
        return ERROR_INT("not enough points", __func__, 1);
    if (type == L_QUADRATIC_INTERP && n == 2) {
        type = L_LINEAR_INTERP;
        L_WARNING("only 2 points; using linear interp\n", __func__);
    }
    maxx = startx + deltax * (n - 1);
    if (xval < startx || xval > maxx)
        return ERROR_INT("xval is out of bounds", __func__, 1);

    fa = numaGetFArray(nay, L_NOCOPY);
    fi = (xval - startx) / deltax;
    i = (l_int32)fi;
    del = fi - i;
    if (del == 0.0) {  /* no interpolation required */
        *pyval = fa[i];
        return 0;
    }

    if (type == L_LINEAR_INTERP) {
        *pyval = fa[i] + del * (fa[i + 1] - fa[i]);
        return 0;
    }

        /* Quadratic interpolation */
    d1 = d3 = 0.5f / (deltax * deltax);
    d2 = -2.f * d1;
    if (i == 0) {
        i1 = i;
        i2 = i + 1;
        i3 = i + 2;
    } else {
        i1 = i - 1;
        i2 = i;
        i3 = i + 1;
    }
    x1 = startx + i1 * deltax;
    x2 = startx + i2 * deltax;
    x3 = startx + i3 * deltax;
    fy1 = d1 * fa[i1];
    fy2 = d2 * fa[i2];
    fy3 = d3 * fa[i3];
    *pyval = fy1 * (xval - x2) * (xval - x3) +
             fy2 * (xval - x1) * (xval - x3) +
             fy3 * (xval - x1) * (xval - x2);
    return 0;
}


/*!
 * \brief   numaInterpolateArbxVal()
 *
 * \param[in]    nax    numa of abscissa values
 * \param[in]    nay    numa of ordinate values, corresponding to nax
 * \param[in]    type   L_LINEAR_INTERP, L_QUADRATIC_INTERP
 * \param[in]    xval
 * \param[out]   pyval  interpolated value
 * \return  0 if OK, 1 on error e.g., if xval is outside range
 *
 * <pre>
 * Notes:
 *      (1) The values in nax must be sorted in increasing order.
 *          If, additionally, they are equally spaced, you can use
 *          numaInterpolateEqxVal().
 *      (2) Caller should check for valid return.
 *      (3) Uses lagrangian interpolation.  See numaInterpolateEqxVal()
 *          for formulas.
 * </pre>
 */
l_ok
numaInterpolateArbxVal(NUMA       *nax,
                       NUMA       *nay,
                       l_int32     type,
                       l_float32   xval,
                       l_float32  *pyval)
{
l_int32     i, im, nx, ny, i1, i2, i3;
l_float32   delu, dell, fract, d1, d2, d3;
l_float32   minx, maxx;
l_float32  *fax, *fay;

    if (!pyval)
        return ERROR_INT("&yval not defined", __func__, 1);
    *pyval = 0.0;
    if (!nax)
        return ERROR_INT("nax not defined", __func__, 1);
    if (!nay)
        return ERROR_INT("nay not defined", __func__, 1);
    if (type != L_LINEAR_INTERP && type != L_QUADRATIC_INTERP)
        return ERROR_INT("invalid interp type", __func__, 1);
    ny = numaGetCount(nay);
    nx = numaGetCount(nax);
    if (nx != ny)
        return ERROR_INT("nax and nay not same size arrays", __func__, 1);
    if (ny < 2)
        return ERROR_INT("not enough points", __func__, 1);
    if (type == L_QUADRATIC_INTERP && ny == 2) {
        type = L_LINEAR_INTERP;
        L_WARNING("only 2 points; using linear interp\n", __func__);
    }
    numaGetFValue(nax, 0, &minx);
    numaGetFValue(nax, nx - 1, &maxx);
    if (xval < minx || xval > maxx)
        return ERROR_INT("xval is out of bounds", __func__, 1);

    fax = numaGetFArray(nax, L_NOCOPY);
    fay = numaGetFArray(nay, L_NOCOPY);

        /* Linear search for interval.  We are guaranteed
         * to either return or break out of the loop.
         * In addition, we are assured that fax[i] - fax[im] > 0.0 */
    if (xval == fax[0]) {
        *pyval = fay[0];
        return 0;
    }
    im = 0;
    dell = 0.0;
    for (i = 1; i < nx; i++) {
        delu = fax[i] - xval;
        if (delu >= 0.0) {  /* we've passed it */
            if (delu == 0.0) {
                *pyval = fay[i];
                return 0;
            }
            im = i - 1;
            dell = xval - fax[im];  /* >= 0 */
            break;
        }
    }
    fract = dell / (fax[i] - fax[im]);

    if (type == L_LINEAR_INTERP) {
        *pyval = fay[i] + fract * (fay[i + 1] - fay[i]);
        return 0;
    }

        /* Quadratic interpolation */
    if (im == 0) {
        i1 = im;
        i2 = im + 1;
        i3 = im + 2;
    } else {
        i1 = im - 1;
        i2 = im;
        i3 = im + 1;
    }
    d1 = (fax[i1] - fax[i2]) * (fax[i1] - fax[i3]);
    d2 = (fax[i2] - fax[i1]) * (fax[i2] - fax[i3]);
    d3 = (fax[i3] - fax[i1]) * (fax[i3] - fax[i2]);
    *pyval = fay[i1] * (xval - fax[i2]) * (xval - fax[i3]) / d1 +
             fay[i2] * (xval - fax[i1]) * (xval - fax[i3]) / d2 +
             fay[i3] * (xval - fax[i1]) * (xval - fax[i2]) / d3;
    return 0;
}


/*!
 * \brief   numaInterpolateEqxInterval()
 *
 * \param[in]    startx    xval corresponding to first element in nas
 * \param[in]    deltax    x increment between array elements in nas
 * \param[in]    nasy      numa of ordinate values, assumed equally spaced
 * \param[in]    type      L_LINEAR_INTERP, L_QUADRATIC_INTERP
 * \param[in]    x0        start value of interval
 * \param[in]    x1        end value of interval
 * \param[in]    npts      number of points to evaluate function in interval
 * \param[out]   pnax      [optional] array of x values in interval
 * \param[out]   pnay      array of y values in interval
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Considering nasy as a function of x, the x values
 *          are equally spaced.
 *      (2) This creates nay (and optionally nax) of interpolated
 *          values over the specified interval (x0, x1).
 *      (3) If the interval (x0, x1) lies partially outside the array
 *          nasy (as interpreted by startx and deltax), it is an
 *          error and returns 1.
 *      (4) Note that deltax is the intrinsic x-increment for the input
 *          array nasy, whereas delx is the intrinsic x-increment for the
 *          output interpolated array nay.
 * </pre>
 */
l_ok
numaInterpolateEqxInterval(l_float32  startx,
                           l_float32  deltax,
                           NUMA      *nasy,
                           l_int32    type,
                           l_float32  x0,
                           l_float32  x1,
                           l_int32    npts,
                           NUMA     **pnax,
                           NUMA     **pnay)
{
l_int32     i, n;
l_float32   x, yval, maxx, delx;
NUMA       *nax = NULL, *nay;

    if (pnax) *pnax = NULL;
    if (!pnay)
        return ERROR_INT("&nay not defined", __func__, 1);
    *pnay = NULL;
    if (!nasy)
        return ERROR_INT("nasy not defined", __func__, 1);
    if ((n = numaGetCount(nasy)) < 2)
        return ERROR_INT("n < 2", __func__, 1);
    if (deltax <= 0.0)
        return ERROR_INT("deltax not > 0", __func__, 1);
    if (type != L_LINEAR_INTERP && type != L_QUADRATIC_INTERP)
        return ERROR_INT("invalid interp type", __func__, 1);
    if (type == L_QUADRATIC_INTERP && n == 2) {
        type = L_LINEAR_INTERP;
        L_WARNING("only 2 points; using linear interp\n", __func__);
    }
    maxx = startx + deltax * (n - 1);
    if (x0 < startx || x1 > maxx || x1 <= x0)
        return ERROR_INT("[x0 ... x1] is not valid", __func__, 1);
    if (npts < 3)
        return ERROR_INT("npts < 3", __func__, 1);
    delx = (x1 - x0) / (l_float32)(npts - 1);  /* delx is for output nay */

    if ((nay = numaCreate(npts)) == NULL)
        return ERROR_INT("nay not made", __func__, 1);
    numaSetParameters(nay, x0, delx);
    *pnay = nay;
    if (pnax) {
        nax = numaCreate(npts);
        *pnax = nax;
    }

    for (i = 0; i < npts; i++) {
        x = x0 + i * delx;
        if (pnax)
            numaAddNumber(nax, x);
        numaInterpolateEqxVal(startx, deltax, nasy, type, x, &yval);
        numaAddNumber(nay, yval);
    }

    return 0;
}


/*!
 * \brief   numaInterpolateArbxInterval()
 *
 * \param[in]    nax     numa of abscissa values
 * \param[in]    nay     numa of ordinate values, corresponding to nax
 * \param[in]    type    L_LINEAR_INTERP, L_QUADRATIC_INTERP
 * \param[in]    x0      start value of interval
 * \param[in]    x1      end value of interval
 * \param[in]    npts    number of points to evaluate function in interval
 * \param[out]   pnadx   [optional] array of x values in interval
 * \param[out]   pnady   array of y values in interval
 * \return  0 if OK, 1 on error e.g., if x0 or x1 is outside range
 *
 * <pre>
 * Notes:
 *      (1) The values in nax must be sorted in increasing order.
 *          If they are not sorted, we do it here, and complain.
 *      (2) If the values in nax are equally spaced, you can use
 *          numaInterpolateEqxInterval().
 *      (3) Caller should check for valid return.
 *      (4) We don't call numaInterpolateArbxVal() for each output
 *          point, because that requires an O(n) search for
 *          each point.  Instead, we do a single O(n) pass through
 *          nax, saving the indices to be used for each output yval.
 *      (5) Uses lagrangian interpolation.  See numaInterpolateEqxVal()
 *          for formulas.
 * </pre>
 */
l_ok
numaInterpolateArbxInterval(NUMA       *nax,
                            NUMA       *nay,
                            l_int32     type,
                            l_float32   x0,
                            l_float32   x1,
                            l_int32     npts,
                            NUMA      **pnadx,
                            NUMA      **pnady)
{
l_int32     i, im, j, nx, ny, i1, i2, i3, sorted;
l_int32    *index;
l_float32   del, xval, yval, excess, fract, minx, maxx, d1, d2, d3;
l_float32  *fax, *fay;
NUMA       *nasx, *nasy, *nadx = NULL, *nady;

    if (pnadx) *pnadx = NULL;
    if (!pnady)
        return ERROR_INT("&nady not defined", __func__, 1);
    *pnady = NULL;
    if (!nay)
        return ERROR_INT("nay not defined", __func__, 1);
    if (!nax)
        return ERROR_INT("nax not defined", __func__, 1);
    if (type != L_LINEAR_INTERP && type != L_QUADRATIC_INTERP)
        return ERROR_INT("invalid interp type", __func__, 1);
    if (x0 > x1)
        return ERROR_INT("x0 > x1", __func__, 1);
    ny = numaGetCount(nay);
    nx = numaGetCount(nax);
    if (nx != ny)
        return ERROR_INT("nax and nay not same size arrays", __func__, 1);
    if (ny < 2)
        return ERROR_INT("not enough points", __func__, 1);
    if (type == L_QUADRATIC_INTERP && ny == 2) {
        type = L_LINEAR_INTERP;
        L_WARNING("only 2 points; using linear interp\n", __func__);
    }
    numaGetMin(nax, &minx, NULL);
    numaGetMax(nax, &maxx, NULL);
    if (x0 < minx || x1 > maxx)
        return ERROR_INT("xval is out of bounds", __func__, 1);

        /* Make sure that nax is sorted in increasing order */
    numaIsSorted(nax, L_SORT_INCREASING, &sorted);
    if (!sorted) {
        L_WARNING("we are sorting nax in increasing order\n", __func__);
        numaSortPair(nax, nay, L_SORT_INCREASING, &nasx, &nasy);
    } else {
        nasx = numaClone(nax);
        nasy = numaClone(nay);
    }

    fax = numaGetFArray(nasx, L_NOCOPY);
    fay = numaGetFArray(nasy, L_NOCOPY);

        /* Get array of indices into fax for interpolated locations */
    if ((index = (l_int32 *)LEPT_CALLOC(npts, sizeof(l_int32))) == NULL) {
        numaDestroy(&nasx);
        numaDestroy(&nasy);
        return ERROR_INT("ind not made", __func__, 1);
    }
    del = (x1 - x0) / (npts - 1.0f);
    for (i = 0, j = 0; j < nx && i < npts; i++) {
        xval = x0 + i * del;
        while (j < nx - 1 && xval > fax[j])
            j++;
        if (xval == fax[j])
            index[i] = L_MIN(j, nx - 1);
        else    /* the index of fax[] is just below xval */
            index[i] = L_MAX(j - 1, 0);
    }

        /* For each point to be interpolated, get the y value */
    nady = numaCreate(npts);
    *pnady = nady;
    if (pnadx) {
        nadx = numaCreate(npts);
        *pnadx = nadx;
    }
    for (i = 0; i < npts; i++) {
        xval = x0 + i * del;
        if (pnadx)
            numaAddNumber(nadx, xval);
        im = index[i];
        excess = xval - fax[im];
        if (excess == 0.0) {
            numaAddNumber(nady, fay[im]);
            continue;
        }
        fract = excess / (fax[im + 1] - fax[im]);

        if (type == L_LINEAR_INTERP) {
            yval = fay[im] + fract * (fay[im + 1] - fay[im]);
            numaAddNumber(nady, yval);
            continue;
        }

            /* Quadratic interpolation */
        if (im == 0) {
            i1 = im;
            i2 = im + 1;
            i3 = im + 2;
        } else {
            i1 = im - 1;
            i2 = im;
            i3 = im + 1;
        }
        d1 = (fax[i1] - fax[i2]) * (fax[i1] - fax[i3]);
        d2 = (fax[i2] - fax[i1]) * (fax[i2] - fax[i3]);
        d3 = (fax[i3] - fax[i1]) * (fax[i3] - fax[i2]);
        yval = fay[i1] * (xval - fax[i2]) * (xval - fax[i3]) / d1 +
               fay[i2] * (xval - fax[i1]) * (xval - fax[i3]) / d2 +
               fay[i3] * (xval - fax[i1]) * (xval - fax[i2]) / d3;
        numaAddNumber(nady, yval);
    }

    LEPT_FREE(index);
    numaDestroy(&nasx);
    numaDestroy(&nasy);
    return 0;
}


/*----------------------------------------------------------------------*
 *                     Functions requiring interpolation                *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaFitMax()
 *
 * \param[in]    na       numa of ordinate values, to fit a max to
 * \param[out]   pmaxval  max value
 * \param[in]    naloc    [optional] associated numa of abscissa values
 * \param[out]   pmaxloc  abscissa value that gives max value in na;
 *                        if naloc == null, this is given as an interpolated
 *                        index value
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *        If %naloc is given, there is no requirement that the
 *        data points are evenly spaced.  Lagrangian interpolation
 *        handles that.  The only requirement is that the
 *        data points are ordered so that the values in naloc
 *        are either increasing or decreasing.  We test to make
 *        sure that the sizes of na and naloc are equal, and it
 *        is assumed that the correspondences %na[i] as a function
 *        of %naloc[i] are properly arranged for all i.
 *
 *  The formula for Lagrangian interpolation through 3 data pts is:
 *       y(x) = y1(x-x2)(x-x3)/((x1-x2)(x1-x3)) +
 *              y2(x-x1)(x-x3)/((x2-x1)(x2-x3)) +
 *              y3(x-x1)(x-x2)/((x3-x1)(x3-x2))
 *
 *  Then the derivative, using the constants (c1,c2,c3) defined below,
 *  is set to 0:
 *       y'(x) = 2x(c1+c2+c3) - c1(x2+x3) - c2(x1+x3) - c3(x1+x2) = 0
 * </pre>
 */
l_ok
numaFitMax(NUMA       *na,
           l_float32  *pmaxval,
           NUMA       *naloc,
           l_float32  *pmaxloc)
{
l_float32  val;
l_float32  smaxval;  /* start value of maximum sample, before interpolating */
l_int32    n, imaxloc;
l_float32  x1, x2, x3, y1, y2, y3, c1, c2, c3, a, b, xmax, ymax;

    if (pmaxval) *pmaxval = 0.0;
    if (pmaxloc) *pmaxloc = 0.0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);
    if (!pmaxval)
        return ERROR_INT("&maxval not defined", __func__, 1);
    if (!pmaxloc)
        return ERROR_INT("&maxloc not defined", __func__, 1);
    if (naloc) {
        if (n != numaGetCount(naloc))
            return ERROR_INT("na and naloc of unequal size", __func__, 1);
    }

    numaGetMax(na, &smaxval, &imaxloc);

        /* Simple case: max is at end point */
    if (imaxloc == 0 || imaxloc == n - 1) {
        *pmaxval = smaxval;
        if (naloc) {
            numaGetFValue(naloc, imaxloc, &val);
            *pmaxloc = val;
        } else {
            *pmaxloc = imaxloc;
        }
        return 0;
    }

        /* Interior point; use quadratic interpolation */
    y2 = smaxval;
    numaGetFValue(na, imaxloc - 1, &val);
    y1 = val;
    numaGetFValue(na, imaxloc + 1, &val);
    y3 = val;
    if (naloc) {
        numaGetFValue(naloc, imaxloc - 1, &val);
        x1 = val;
        numaGetFValue(naloc, imaxloc, &val);
        x2 = val;
        numaGetFValue(naloc, imaxloc + 1, &val);
        x3 = val;
    } else {
        x1 = imaxloc - 1;
        x2 = imaxloc;
        x3 = imaxloc + 1;
    }

        /* Can't interpolate; just use the max val in na
         * and the corresponding one in naloc */
    if (x1 == x2 || x1 == x3 || x2 == x3) {
        *pmaxval = y2;
        *pmaxloc = x2;
        return 0;
    }

        /* Use lagrangian interpolation; set dy/dx = 0 */
    c1 = y1 / ((x1 - x2) * (x1 - x3));
    c2 = y2 / ((x2 - x1) * (x2 - x3));
    c3 = y3 / ((x3 - x1) * (x3 - x2));
    a = c1 + c2 + c3;
    b = c1 * (x2 + x3) + c2 * (x1 + x3) + c3 * (x1 + x2);
    xmax = b / (2 * a);
    ymax = c1 * (xmax - x2) * (xmax - x3) +
           c2 * (xmax - x1) * (xmax - x3) +
           c3 * (xmax - x1) * (xmax - x2);
    *pmaxval = ymax;
    *pmaxloc = xmax;

    return 0;
}


/*!
 * \brief   numaDifferentiateInterval()
 *
 * \param[in]    nax     numa of abscissa values
 * \param[in]    nay     numa of ordinate values, corresponding to nax
 * \param[in]    x0      start value of interval
 * \param[in]    x1      end value of interval
 * \param[in]    npts    number of points to evaluate function in interval
 * \param[out]   pnadx   [optional] array of x values in interval
 * \param[out]   pnady   array of derivatives in interval
 * \return  0 if OK, 1 on error e.g., if x0 or x1 is outside range
 *
 * <pre>
 * Notes:
 *      (1) The values in nax must be sorted in increasing order.
 *          If they are not sorted, it is done in the interpolation
 *          step, and a warning is issued.
 *      (2) Caller should check for valid return.
 * </pre>
 */
l_ok
numaDifferentiateInterval(NUMA       *nax,
                          NUMA       *nay,
                          l_float32   x0,
                          l_float32   x1,
                          l_int32     npts,
                          NUMA      **pnadx,
                          NUMA      **pnady)
{
l_int32     i, nx, ny;
l_float32   minx, maxx, der, invdel;
l_float32  *fay;
NUMA       *nady, *naiy;

    if (pnadx) *pnadx = NULL;
    if (!pnady)
        return ERROR_INT("&nady not defined", __func__, 1);
    *pnady = NULL;
    if (!nay)
        return ERROR_INT("nay not defined", __func__, 1);
    if (!nax)
        return ERROR_INT("nax not defined", __func__, 1);
    if (x0 > x1)
        return ERROR_INT("x0 > x1", __func__, 1);
    ny = numaGetCount(nay);
    nx = numaGetCount(nax);
    if (nx != ny)
        return ERROR_INT("nax and nay not same size arrays", __func__, 1);
    if (ny < 2)
        return ERROR_INT("not enough points", __func__, 1);
    numaGetMin(nax, &minx, NULL);
    numaGetMax(nax, &maxx, NULL);
    if (x0 < minx || x1 > maxx)
        return ERROR_INT("xval is out of bounds", __func__, 1);
    if (npts < 2)
        return ERROR_INT("npts < 2", __func__, 1);

        /* Generate interpolated array over specified interval */
    if (numaInterpolateArbxInterval(nax, nay, L_LINEAR_INTERP, x0, x1,
                                    npts, pnadx, &naiy))
        return ERROR_INT("interpolation failed", __func__, 1);

    nady = numaCreate(npts);
    *pnady = nady;
    invdel = 0.5f * ((l_float32)npts - 1.0f) / (x1 - x0);
    fay = numaGetFArray(naiy, L_NOCOPY);

        /* Compute and save derivatives */
    der = 0.5f * invdel * (fay[1] - fay[0]);
    numaAddNumber(nady, der);
    for (i = 1; i < npts - 1; i++)  {
        der = invdel * (fay[i + 1] - fay[i - 1]);
        numaAddNumber(nady, der);
    }
    der = 0.5f * invdel * (fay[npts - 1] - fay[npts - 2]);
    numaAddNumber(nady, der);

    numaDestroy(&naiy);
    return 0;
}


/*!
 * \brief   numaIntegrateInterval()
 *
 * \param[in]    nax     numa of abscissa values
 * \param[in]    nay     numa of ordinate values, corresponding to nax
 * \param[in]    x0      start value of interval
 * \param[in]    x1      end value of interval
 * \param[in]    npts    number of points to evaluate function in interval
 * \param[out]   psum    integral of function over interval
 * \return  0 if OK, 1 on error e.g., if x0 or x1 is outside range
 *
 * <pre>
 * Notes:
 *      (1) The values in nax must be sorted in increasing order.
 *          If they are not sorted, it is done in the interpolation
 *          step, and a warning is issued.
 *      (2) Caller should check for valid return.
 * </pre>
 */
l_ok
numaIntegrateInterval(NUMA       *nax,
                      NUMA       *nay,
                      l_float32   x0,
                      l_float32   x1,
                      l_int32     npts,
                      l_float32  *psum)
{
l_int32     i, nx, ny;
l_float32   minx, maxx, sum, del;
l_float32  *fay;
NUMA       *naiy;

    if (!psum)
        return ERROR_INT("&sum not defined", __func__, 1);
    *psum = 0.0;
    if (!nay)
        return ERROR_INT("nay not defined", __func__, 1);
    if (!nax)
        return ERROR_INT("nax not defined", __func__, 1);
    if (x0 > x1)
        return ERROR_INT("x0 > x1", __func__, 1);
    if (npts < 2)
        return ERROR_INT("npts < 2", __func__, 1);
    ny = numaGetCount(nay);
    nx = numaGetCount(nax);
    if (nx != ny)
        return ERROR_INT("nax and nay not same size arrays", __func__, 1);
    if (ny < 2)
        return ERROR_INT("not enough points", __func__, 1);
    numaGetMin(nax, &minx, NULL);
    numaGetMax(nax, &maxx, NULL);
    if (x0 < minx || x1 > maxx)
        return ERROR_INT("xval is out of bounds", __func__, 1);

        /* Generate interpolated array over specified interval */
    if (numaInterpolateArbxInterval(nax, nay, L_LINEAR_INTERP, x0, x1,
                                    npts, NULL, &naiy))
        return ERROR_INT("interpolation failed", __func__, 1);

    del = (x1 - x0) / ((l_float32)npts - 1.0f);
    fay = numaGetFArray(naiy, L_NOCOPY);

        /* Compute integral (simple trapezoid) */
    sum = 0.5f * (fay[0] + fay[npts - 1]);
    for (i = 1; i < npts - 1; i++)
        sum += fay[i];
    *psum = del * sum;

    numaDestroy(&naiy);
    return 0;
}


/*----------------------------------------------------------------------*
 *                                Sorting                               *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaSortGeneral()
 *
 * \param[in]    na          source numa
 * \param[out]   pnasort     [optional] sorted numa
 * \param[out]   pnaindex    [optional] index of elements in na associated
 *                           with each element of nasort
 * \param[out]   pnainvert   [optional] index of elements in nasort associated
 *                           with each element of na
 * \param[in]    sortorder   L_SORT_INCREASING or L_SORT_DECREASING
 * \param[in]    sorttype    L_SHELL_SORT or L_BIN_SORT
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Sorting can be confusing.  Here's an array of five values with
 *          the results shown for the 3 output arrays.
 *
 *          na      nasort   naindex   nainvert
 *          -----------------------------------
 *          3         9         2         3
 *          4         6         3         2
 *          9         4         1         0
 *          6         3         0         1
 *          1         1         4         4
 *
 *          Note that naindex is a LUT into na for the sorted array values,
 *          and nainvert directly gives the sorted index values for the
 *          input array.  It is useful to view naindex is as a map:
 *                 0  -->  2
 *                 1  -->  3
 *                 2  -->  1
 *                 3  -->  0
 *                 4  -->  4
 *          and nainvert, the inverse of this map:
 *                 0  -->  3
 *                 1  -->  2
 *                 2  -->  0
 *                 3  -->  1
 *                 4  -->  4
 *
 *          We can write these relations symbolically as:
 *              nasort[i] = na[naindex[i]]
 *              na[i] = nasort[nainvert[i]]
 * </pre>
 */
l_ok
numaSortGeneral(NUMA    *na,
                NUMA   **pnasort,
                NUMA   **pnaindex,
                NUMA   **pnainvert,
                l_int32  sortorder,
                l_int32  sorttype)
{
l_int32    isize;
l_float32  size;
NUMA      *naindex = NULL;

    if (pnasort) *pnasort = NULL;
    if (pnaindex) *pnaindex = NULL;
    if (pnainvert) *pnainvert = NULL;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return ERROR_INT("invalid sort order", __func__, 1);
    if (sorttype != L_SHELL_SORT && sorttype != L_BIN_SORT)
        return ERROR_INT("invalid sort type", __func__, 1);
    if (!pnasort && !pnaindex && !pnainvert)
        return ERROR_INT("nothing to do", __func__, 1);

    if (sorttype == L_BIN_SORT) {
        numaGetMax(na, &size, NULL);
        isize = (l_int32)size;
        if (isize > MaxInitPtraSize - 1) {
            L_WARNING("array too large; using shell sort\n", __func__);
            sorttype = L_SHELL_SORT;
        } else {
            naindex = numaGetBinSortIndex(na, sortorder);
        }
    }

    if (sorttype == L_SHELL_SORT)
        naindex = numaGetSortIndex(na, sortorder);

    if (pnasort)
        *pnasort = numaSortByIndex(na, naindex);
    if (pnainvert)
        *pnainvert = numaInvertMap(naindex);
    if (pnaindex)
        *pnaindex = naindex;
    else
        numaDestroy(&naindex);
    return 0;
}


/*!
 * \brief   numaSortAutoSelect()
 *
 * \param[in]    nas
 * \param[in]    sortorder   L_SORT_INCREASING or L_SORT_DECREASING
 * \return  naout output sorted numa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does either a shell sort or a bin sort, depending on
 *          the number of elements in nas and the dynamic range.
 * </pre>
 */
NUMA *
numaSortAutoSelect(NUMA    *nas,
                   l_int32  sortorder)
{
l_int32  type;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (numaGetCount(nas) == 0) {
        L_WARNING("nas is empty; returning copy\n", __func__);
        return numaCopy(nas);
    }
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (NUMA *)ERROR_PTR("invalid sort order", __func__, NULL);

    type = numaChooseSortType(nas);
    if (type != L_SHELL_SORT && type != L_BIN_SORT)
        return (NUMA *)ERROR_PTR("invalid sort type", __func__, NULL);

    if (type == L_BIN_SORT)
        return numaBinSort(nas, sortorder);
    else  /* shell sort */
        return numaSort(NULL, nas, sortorder);
}


/*!
 * \brief   numaSortIndexAutoSelect()
 *
 * \param[in]    nas
 * \param[in]    sortorder     L_SORT_INCREASING or L_SORT_DECREASING
 * \return  nad indices of nas, sorted by value in nas, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does either a shell sort or a bin sort, depending on
 *          the number of elements in nas and the dynamic range.
 * </pre>
 */
NUMA *
numaSortIndexAutoSelect(NUMA    *nas,
                        l_int32  sortorder)
{
l_int32  type;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (numaGetCount(nas) == 0) {
        L_WARNING("nas is empty; returning copy\n", __func__);
        return numaCopy(nas);
    }
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (NUMA *)ERROR_PTR("invalid sort order", __func__, NULL);
    type = numaChooseSortType(nas);
    if (type != L_SHELL_SORT && type != L_BIN_SORT)
        return (NUMA *)ERROR_PTR("invalid sort type", __func__, NULL);

    if (type == L_BIN_SORT)
        return numaGetBinSortIndex(nas, sortorder);
    else  /* shell sort */
        return numaGetSortIndex(nas, sortorder);
}


/*!
 * \brief   numaChooseSortType()
 *
 * \param[in]    nas     to be sorted
 * \return  sorttype  L_SHELL_SORT or L_BIN_SORT, or UNDEF on error.
 *
 * <pre>
 * Notes:
 *      (1) This selects either a shell sort or a bin sort, depending on
 *          the number of elements in nas and the dynamic range.
 *      (2) If there are negative values in nas, it selects shell sort.
 * </pre>
 */
l_int32
numaChooseSortType(NUMA  *nas)
{
l_int32    n;
l_float32  minval, maxval;

    if (!nas)
        return ERROR_INT("nas not defined", __func__, UNDEF);

        /* If small histogram or negative values; use shell sort */
    numaGetMin(nas, &minval, NULL);
    n = numaGetCount(nas);
    if (minval < 0.0 || n < 200)
        return L_SHELL_SORT;

        /* If large maxval, use shell sort */
    numaGetMax(nas, &maxval, NULL);
    if (maxval > MaxInitPtraSize - 1)
        return L_SHELL_SORT;

        /* Otherwise, need to compare nlog(n) with maxval.
         * The factor of 0.003 was determined by comparing times for
         * different histogram sizes and maxval.  It is very small
         * because binsort is fast and shell sort gets slow for large n. */
    if (n * log((l_float32)n) < 0.003 * maxval)
        return L_SHELL_SORT;
    else
        return L_BIN_SORT;
}


/*!
 * \brief   numaSort()
 *
 * \param[in]    naout       output numa; can be NULL or equal to nain
 * \param[in]    nain        input numa
 * \param[in]    sortorder   L_SORT_INCREASING or L_SORT_DECREASING
 * \return  naout   output sorted numa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Set naout = nain for in-place; otherwise, set naout = NULL.
 *      (2) Source: Shell sort, modified from K&R, 2nd edition, p.62.
 *          Slow but simple O(n logn) sort.
 * </pre>
 */
NUMA *
numaSort(NUMA    *naout,
         NUMA    *nain,
         l_int32  sortorder)
{
l_int32     i, n, gap, j;
l_float32   tmp;
l_float32  *array;

    if (!nain)
        return (NUMA *)ERROR_PTR("nain not defined", __func__, NULL);
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (NUMA *)ERROR_PTR("invalid sort order", __func__, NULL);

        /* Make naout if necessary; otherwise do in-place */
    if (!naout)
        naout = numaCopy(nain);
    else if (nain != naout)
        return (NUMA *)ERROR_PTR("invalid: not in-place", __func__, NULL);
    if ((n = numaGetCount(naout)) == 0) {
        L_WARNING("naout is empty\n", __func__);
        return naout;
    }
    array = naout->array;  /* operate directly on the array */
    n = numaGetCount(naout);

        /* Shell sort */
    for (gap = n/2; gap > 0; gap = gap / 2) {
        for (i = gap; i < n; i++) {
            for (j = i - gap; j >= 0; j -= gap) {
                if ((sortorder == L_SORT_INCREASING &&
                     array[j] > array[j + gap]) ||
                    (sortorder == L_SORT_DECREASING &&
                     array[j] < array[j + gap]))
                {
                    tmp = array[j];
                    array[j] = array[j + gap];
                    array[j + gap] = tmp;
                }
            }
        }
    }

    return naout;
}


/*!
 * \brief   numaBinSort()
 *
 * \param[in]    nas         of non-negative integers with a max that can
 *                           not exceed (MaxInitPtraSize - 1)
 * \param[in]    sortorder   L_SORT_INCREASING or L_SORT_DECREASING
 * \return  na   sorted, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Because this uses a bin sort with buckets of size 1, it
 *          is not appropriate for sorting either small arrays or
 *          arrays containing very large integer values.  For such
 *          arrays, use a standard general sort function like
 *          numaSort().
 *      (2) You can use numaSortAutoSelect() to decide which sorting
 *          method to use.
 * </pre>
 */
NUMA *
numaBinSort(NUMA    *nas,
            l_int32  sortorder)
{
NUMA  *nat, *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (numaGetCount(nas) == 0) {
        L_WARNING("nas is empty; returning copy\n", __func__);
        return numaCopy(nas);
    }
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (NUMA *)ERROR_PTR("invalid sort order", __func__, NULL);

    if ((nat = numaGetBinSortIndex(nas, sortorder)) == NULL)
        return (NUMA *)ERROR_PTR("bin sort failed", __func__, NULL);
    nad = numaSortByIndex(nas, nat);
    numaDestroy(&nat);
    return nad;
}


/*!
 * \brief   numaGetSortIndex()
 *
 * \param[in]    na          source numa
 * \param[in]    sortorder   L_SORT_INCREASING or L_SORT_DECREASING
 * \return  na  giving an array of indices that would sort
 *              the input array, or NULL on error
 */
NUMA *
numaGetSortIndex(NUMA    *na,
                 l_int32  sortorder)
{
l_int32     i, n, gap, j;
l_float32   tmp;
l_float32  *array;   /* copy of input array */
l_float32  *iarray;  /* array of indices */
NUMA       *naisort;

    if (!na)
        return (NUMA *)ERROR_PTR("na not defined", __func__, NULL);
    if (numaGetCount(na) == 0) {
        L_WARNING("na is empty\n", __func__);
        return numaCreate(1);
    }
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (NUMA *)ERROR_PTR("invalid sortorder", __func__, NULL);

    n = numaGetCount(na);
    if ((array = numaGetFArray(na, L_COPY)) == NULL)
        return (NUMA *)ERROR_PTR("array not made", __func__, NULL);
    if ((iarray = (l_float32 *)LEPT_CALLOC(n, sizeof(l_float32))) == NULL) {
        LEPT_FREE(array);
        return (NUMA *)ERROR_PTR("iarray not made", __func__, NULL);
    }
    for (i = 0; i < n; i++)
        iarray[i] = i;

        /* Shell sort */
    for (gap = n/2; gap > 0; gap = gap / 2) {
        for (i = gap; i < n; i++) {
            for (j = i - gap; j >= 0; j -= gap) {
                if ((sortorder == L_SORT_INCREASING &&
                     array[j] > array[j + gap]) ||
                    (sortorder == L_SORT_DECREASING &&
                     array[j] < array[j + gap]))
                {
                    tmp = array[j];
                    array[j] = array[j + gap];
                    array[j + gap] = tmp;
                    tmp = iarray[j];
                    iarray[j] = iarray[j + gap];
                    iarray[j + gap] = tmp;
                }
            }
        }
    }

    naisort = numaCreate(n);
    for (i = 0; i < n; i++)
        numaAddNumber(naisort, iarray[i]);

    LEPT_FREE(array);
    LEPT_FREE(iarray);
    return naisort;
}


/*!
 * \brief   numaGetBinSortIndex()
 *
 * \param[in]    nas         of non-negative integers with a max that can
 *                           not exceed (MaxInitPtraSize - 1)
 * \param[in]    sortorder   L_SORT_INCREASING or L_SORT_DECREASING
 * \return  na  sorted, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This creates an array (or lookup table) that contains
 *          the sorted position of the elements in the input Numa.
 *      (2) Because it uses a bin sort with buckets of size 1, it
 *          is not appropriate for sorting either small arrays or
 *          arrays containing very large integer values.  For such
 *          arrays, use a standard general sort function like
 *          numaGetSortIndex().
 *      (3) You can use numaSortIndexAutoSelect() to decide which
 *          sorting method to use.
 * </pre>
 */
NUMA *
numaGetBinSortIndex(NUMA    *nas,
                    l_int32  sortorder)
{
l_int32    i, n, isize, ival, imax;
l_float32  minsize, size;
NUMA      *na, *nai, *nad;
L_PTRA    *paindex;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (numaGetCount(nas) == 0) {
        L_WARNING("nas is empty\n", __func__);
        return numaCreate(1);
    }
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (NUMA *)ERROR_PTR("invalid sort order", __func__, NULL);
    numaGetMin(nas, &minsize, NULL);
    if (minsize < 0)
        return (NUMA *)ERROR_PTR("nas has negative numbers", __func__, NULL);
    numaGetMax(nas, &size, NULL);
    isize = (l_int32)size;
    if (isize > MaxInitPtraSize - 1) {
        L_ERROR("array too large: %d elements > max size = %d\n",
                __func__, isize, MaxInitPtraSize - 1);
        return NULL;
    }

        /* Set up a ptra holding numa at indices for which there
         * are values in nas.  Suppose nas has the value 230 at index
         * 7355.  A numa holding the index 7355 is created and stored
         * at the ptra index 230.  If there is another value of 230
         * in nas, its index is added to the same numa (at index 230
         * in the ptra).  When finished, the ptra can be scanned for numa,
         * and the original indices in the nas can be read out.  In this
         * way, the ptra effectively sorts the input numbers in the nas. */
    paindex = ptraCreate(isize + 1);
    n = numaGetCount(nas);
    for (i = 0; i < n; i++) {
        numaGetIValue(nas, i, &ival);
        nai = (NUMA *)ptraGetPtrToItem(paindex, ival);
        if (!nai) {  /* make it; no shifting will occur */
            nai = numaCreate(1);
            ptraInsert(paindex, ival, nai, L_MIN_DOWNSHIFT);
        }
        numaAddNumber(nai, i);
    }

        /* Sort by scanning the ptra, extracting numas and pulling
         * the (index into nas) numbers out of each numa, taken
         * successively in requested order. */
    ptraGetMaxIndex(paindex, &imax);
    nad = numaCreate(0);
    if (sortorder == L_SORT_INCREASING) {
        for (i = 0; i <= imax; i++) {
            na = (NUMA *)ptraRemove(paindex, i, L_NO_COMPACTION);
            if (!na) continue;
            numaJoin(nad, na, 0, -1);
            numaDestroy(&na);
        }
    } else {  /* L_SORT_DECREASING */
        for (i = imax; i >= 0; i--) {
            na = (NUMA *)ptraRemoveLast(paindex);
            if (!na) break;  /* they've all been removed */
            numaJoin(nad, na, 0, -1);
            numaDestroy(&na);
        }
    }

    ptraDestroy(&paindex, FALSE, FALSE);
    return nad;
}


/*!
 * \brief   numaSortByIndex()
 *
 * \param[in]    nas
 * \param[in]    naindex     na that maps from the new numa to the input numa
 * \return  nad  sorted, or NULL on error
 */
NUMA *
numaSortByIndex(NUMA  *nas,
                NUMA  *naindex)
{
l_int32    i, n, ni, index;
l_float32  val;
NUMA      *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if (!naindex)
        return (NUMA *)ERROR_PTR("naindex not defined", __func__, NULL);
    n = numaGetCount(nas);
    ni = numaGetCount(naindex);
    if (n != ni)
        return (NUMA *)ERROR_PTR("numa sizes differ", __func__, NULL);
    if (n == 0) {
        L_WARNING("nas is empty\n", __func__);
        return numaCopy(nas);
    }

    nad = numaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetIValue(naindex, i, &index);
        numaGetFValue(nas, index, &val);
        numaAddNumber(nad, val);
    }

    return nad;
}


/*!
 * \brief   numaIsSorted()
 *
 * \param[in]    nas
 * \param[in]    sortorder   L_SORT_INCREASING or L_SORT_DECREASING
 * \param[out]   psorted     1 if sorted; 0 if not
 * \return  1 if OK; 0 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a quick O(n) test if nas is sorted.  It is useful
 *          in situations where the array is likely to be already
 *          sorted, and a sort operation can be avoided.
 * </pre>
 */
l_int32
numaIsSorted(NUMA     *nas,
             l_int32   sortorder,
             l_int32  *psorted)
{
l_int32    i, n;
l_float32  prevval, val;

    if (!psorted)
        return ERROR_INT("&sorted not defined", __func__, 1);
    *psorted = FALSE;
    if (!nas)
        return ERROR_INT("nas not defined", __func__, 1);
    if ((n = numaGetCount(nas))== 0) {
        L_WARNING("nas is empty\n", __func__);
        *psorted = TRUE;
        return 0;
    }
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return ERROR_INT("invalid sortorder", __func__, 1);

    n = numaGetCount(nas);
    numaGetFValue(nas, 0, &prevval);
    for (i = 1; i < n; i++) {
        numaGetFValue(nas, i, &val);
        if ((sortorder == L_SORT_INCREASING && val < prevval) ||
            (sortorder == L_SORT_DECREASING && val > prevval))
            return 0;
    }

    *psorted = TRUE;
    return 0;
}


/*!
 * \brief   numaSortPair()
 *
 * \param[in]    nax, nay     input arrays
 * \param[in]    sortorder    L_SORT_INCREASING or L_SORT_DECREASING
 * \param[out]   pnasx        sorted
 * \param[out]   pnasy        sorted exactly in order of nasx
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function sorts the two input arrays, nax and nay,
 *          together, using nax as the key for sorting.
 * </pre>
 */
l_ok
numaSortPair(NUMA    *nax,
             NUMA    *nay,
             l_int32  sortorder,
             NUMA   **pnasx,
             NUMA   **pnasy)
{
l_int32  sorted;
NUMA    *naindex;

    if (pnasx) *pnasx = NULL;
    if (pnasy) *pnasy = NULL;
    if (!pnasx || !pnasy)
        return ERROR_INT("&nasx and/or &nasy not defined", __func__, 1);
    if (!nax)
        return ERROR_INT("nax not defined", __func__, 1);
    if (!nay)
        return ERROR_INT("nay not defined", __func__, 1);
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return ERROR_INT("invalid sortorder", __func__, 1);

    numaIsSorted(nax, sortorder, &sorted);
    if (sorted == TRUE) {
        *pnasx = numaCopy(nax);
        *pnasy = numaCopy(nay);
    } else {
        naindex = numaGetSortIndex(nax, sortorder);
        *pnasx = numaSortByIndex(nax, naindex);
        *pnasy = numaSortByIndex(nay, naindex);
        numaDestroy(&naindex);
    }

    return 0;
}


/*!
 * \brief   numaInvertMap()
 *
 * \param[in]    nas
 * \return  nad  the inverted map, or NULL on error or if not invertible
 *
 * <pre>
 * Notes:
 *      (1) This requires that nas contain each integer from 0 to n-1.
 *          The array is typically an index array into a sort or permutation
 *          of another array.
 * </pre>
 */
NUMA *
numaInvertMap(NUMA  *nas)
{
l_int32   i, n, val, error;
l_int32  *test;
NUMA     *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if ((n = numaGetCount(nas)) == 0) {
        L_WARNING("nas is empty\n", __func__);
        return numaCopy(nas);
    }

    nad = numaMakeConstant(0.0, n);
    test = (l_int32 *)LEPT_CALLOC(n, sizeof(l_int32));
    error = 0;
    for (i = 0; i < n; i++) {
        numaGetIValue(nas, i, &val);
        if (val >= n) {
            error = 1;
            break;
        }
        numaReplaceNumber(nad, val, i);
        if (test[val] == 0) {
            test[val] = 1;
        } else {
            error = 1;
            break;
        }
    }

    LEPT_FREE(test);
    if (error) {
        numaDestroy(&nad);
        return (NUMA *)ERROR_PTR("nas not invertible", __func__, NULL);
    }

    return nad;
}

/*!
 * \brief   numaAddSorted()
 *
 * \param[in]    na     sorted input
 * \param[in]    val    value to be inserted in sorted order
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The input %na is sorted.  This function determines the
 *          sort order of %na and inserts %val into the array.
 * </pre>
 */
l_ok
numaAddSorted(NUMA      *na,
              l_float32  val)
{
l_int32  index;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    if (numaFindSortedLoc(na, val, &index) == 1)
        return ERROR_INT("insert failure", __func__, 1);
    numaInsertNumber(na, index, val);
    return 0;
}


/*!
 * \brief   numaFindSortedLoc()
 *
 * \param[in]    na     sorted input
 * \param[in]    val    value to be inserted in sorted order
 * \param[out]  *ploc   index location to insert @val
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The input %na is sorted.  This determines the sort order of @na,
 *          either increasing or decreasing, and does a binary search for the
 *          location to insert %val into the array.  The search is O(log n).
 *      (2) The index returned is the location to insert into the array.
 *          The value at the index, and all values to the right, are
 *          moved to the right (increasing their index location by 1).
 *      (3) If n is the size of %na, *ploc can be anything in [0 ... n].
 *          if *ploc == 0, the value is inserted at the beginning of the
 *          array; if *ploc == n, it is inserted at the end.
 *      (4) If the size of %na is 1, insert with an increasing sort.
 * </pre>
 */
l_ok
numaFindSortedLoc(NUMA      *na,
                  l_float32  val,
                  l_int32   *pindex)
{
l_int32    n, increasing, lindex, rindex, midindex;
l_float32  val0, valn, valmid;

    if (!pindex)
        return ERROR_INT("&index not defined", __func__, 1);
    *pindex = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    n = numaGetCount(na);
    if (n == 0) return 0;
    numaGetFValue(na, 0, &val0);
    if (n == 1) {  /* use increasing sort order */
        if (val >= val0)
            *pindex = 1;
        return 0;
    }

        /* -----------------  n >= 2 ----------------- */
    numaGetFValue(na, n - 1, &valn);
    increasing = (valn >= val0) ? 1 : 0;  /* sort order */

        /* Check if outside bounds of existing array */
    if (increasing) {
        if (val < val0) {
            *pindex = 0;
            return 0;
        } else if (val > valn) {
            *pindex = n;
            return 0;
        }
    } else {  /* decreasing */
        if (val > val0) {
            *pindex = 0;
            return 0;
        } else if (val < valn) {
            *pindex = n;
            return 0;
        }
    }

        /* Within bounds of existing array; search */
    lindex = 0;
    rindex = n - 1;
    while (1) {
        midindex = (lindex + rindex) / 2;
        if (midindex == lindex || midindex == rindex) break;
        numaGetFValue(na, midindex, &valmid);
        if (increasing) {
            if (val > valmid)
                lindex = midindex;
            else
                rindex = midindex;
        } else {  /* decreasing */
            if (val > valmid)
                rindex = midindex;
            else
                lindex = midindex;
        }
    }
    *pindex = rindex;
    return 0;
}


/*----------------------------------------------------------------------*
 *                          Random permutation                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaPseudorandomSequence()
 *
 * \param[in]    size     of sequence
 * \param[in]    seed     for random number generation
 * \return  na  pseudorandom on [0,...,size - 1], or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This uses the Durstenfeld shuffle.
 *          See: http://en.wikipedia.org/wiki/Fisher–Yates_shuffle.
 *          Result is a pseudorandom permutation of the sequence of integers
 *          from 0 to size - 1.
 * </pre>
 */
NUMA *
numaPseudorandomSequence(l_int32  size,
                         l_int32  seed)
{
l_int32   i, index, temp;
l_int32  *array;
NUMA     *na;

    if (size <= 0)
        return (NUMA *)ERROR_PTR("size <= 0", __func__, NULL);

    if ((array = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32))) == NULL)
        return (NUMA *)ERROR_PTR("array not made", __func__, NULL);
    for (i = 0; i < size; i++)
        array[i] = i;
    srand(seed);
    for (i = size - 1; i > 0; i--) {
        index = (l_int32)((i + 1) * ((l_float64)rand() / (l_float64)RAND_MAX));
        index = L_MIN(index, i);
        temp = array[i];
        array[i] = array[index];
        array[index] = temp;
    }

    na = numaCreateFromIArray(array, size);
    LEPT_FREE(array);
    return na;
}


/*!
 * \brief   numaRandomPermutation()
 *
 * \param[in]    nas    input array
 * \param[in]    seed   for random number generation
 * \return  nas  randomly shuffled array, or NULL on error
 */
NUMA *
numaRandomPermutation(NUMA    *nas,
                      l_int32  seed)
{
l_int32    i, index, size;
l_float32  val;
NUMA      *naindex, *nad;

    if (!nas)
        return (NUMA *)ERROR_PTR("nas not defined", __func__, NULL);
    if ((size = numaGetCount(nas)) == 0) {
        L_WARNING("nas is empty\n", __func__);
        return numaCopy(nas);
    }

    naindex = numaPseudorandomSequence(size, seed);
    nad = numaCreate(size);
    for (i = 0; i < size; i++) {
        numaGetIValue(naindex, i, &index);
        numaGetFValue(nas, index, &val);
        numaAddNumber(nad, val);
    }
    numaDestroy(&naindex);
    return nad;
}


/*----------------------------------------------------------------------*
 *                     Functions requiring sorting                      *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaGetRankValue()
 *
 * \param[in]    na        source numa
 * \param[in]    fract     use 0.0 for smallest, 1.0 for largest
 * \param[in]    nasort    [optional] increasing sorted version of na
 * \param[in]    usebins   0 for general sort; 1 for bin sort
 * \param[out]   pval      rank val
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Computes the rank value of a number in the %na, which is
 *          the number that is a fraction %fract from the small
 *          end of the sorted version of %na.
 *      (2) If you do this multiple times for different rank values,
 *          sort the array in advance and use that for %nasort;
 *          if you're only calling this once, input %nasort == NULL.
 *      (3) If %usebins == 1, this uses a bin sorting method.
 *          Use this only where:
 *           * the numbers are non-negative integers
 *           * there are over 100 numbers
 *           * the maximum value is less than about 50,000
 *      (4) The advantage of using a bin sort is that it is O(n),
 *          instead of O(nlogn) for general sort routines.
 * </pre>
 */
l_ok
numaGetRankValue(NUMA       *na,
                 l_float32   fract,
                 NUMA       *nasort,
                 l_int32     usebins,
                 l_float32  *pval)
{
l_int32  n, index;
NUMA    *nas;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;  /* init */
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na empty", __func__, 1);
    if (fract < 0.0 || fract > 1.0)
        return ERROR_INT("fract not in [0.0 ... 1.0]", __func__, 1);

    if (nasort) {
        nas = nasort;
    } else {
        if (usebins == 0)
            nas = numaSort(NULL, na, L_SORT_INCREASING);
        else
            nas = numaBinSort(na, L_SORT_INCREASING);
        if (!nas)
            return ERROR_INT("nas not made", __func__, 1);
    }
    index = (l_int32)(fract * (l_float32)(n - 1) + 0.5);
    numaGetFValue(nas, index, pval);

    if (!nasort) numaDestroy(&nas);
    return 0;
}


/*!
 * \brief   numaGetMedian()
 *
 * \param[in]    na     source numa
 * \param[out]   pval   median value
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Computes the median value of the numbers in the numa, by
 *          sorting and finding the middle value in the sorted array.
 * </pre>
 */
l_ok
numaGetMedian(NUMA       *na,
              l_float32  *pval)
{
    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;  /* init */
    if (!na || numaGetCount(na) == 0)
        return ERROR_INT("na not defined or empty", __func__, 1);

    return numaGetRankValue(na, 0.5, NULL, 0, pval);
}


/*!
 * \brief   numaGetBinnedMedian()
 *
 * \param[in]    na      source numa
 * \param[out]   pval    integer median value
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Computes the median value of the numbers in the numa,
 *          using bin sort and finding the middle value in the sorted array.
 *      (2) See numaGetRankValue() for conditions on na for which
 *          this should be used.  Otherwise, use numaGetMedian().
 * </pre>
 */
l_ok
numaGetBinnedMedian(NUMA     *na,
                    l_int32  *pval)
{
l_int32    ret;
l_float32  fval;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0;  /* init */
    if (!na || numaGetCount(na) == 0)
        return ERROR_INT("na not defined or empty", __func__, 1);

    ret = numaGetRankValue(na, 0.5, NULL, 1, &fval);
    *pval = lept_roundftoi(fval);
    return ret;
}


/*!
 * \brief   numaGetMeanDevFromMedian()
 *
 * \param[in]      na     source numa
 * \param[in]      med    median value
 * \param[out]     pdev   average absolute value deviation from median value
 * \return  0 if OK; 1 on error
 */
l_ok
numaGetMeanDevFromMedian(NUMA       *na,
                         l_float32   med,
                         l_float32  *pdev)
{
l_int32    i, n;
l_float32  val, dev;

    if (!pdev)
        return ERROR_INT("&dev not defined", __func__, 1);
    *pdev = 0.0;  /* init */
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    dev = 0.0;
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        dev += L_ABS(val - med);
    }
    *pdev = dev / (l_float32)n;
    return 0;
}


/*!
 * \brief   numaGetMedianDevFromMedian()
 *
 * \param[in]    na        source numa
 * \param[out]   pmed      [optional] median value
 * \param[out]   pdev      median deviation from median val
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Finds the median of the absolute value of the deviation from
 *          the median value in the array.  Why take the absolute value?
 *          Consider the case where you have values equally distributed
 *          about both sides of a median value.  Without taking the absolute
 *          value of the differences, you will get 0 for the deviation,
 *          and this is not useful.
 * </pre>
 */
l_ok
numaGetMedianDevFromMedian(NUMA       *na,
                           l_float32  *pmed,
                           l_float32  *pdev)
{
l_int32    n, i;
l_float32  val, med;
NUMA      *nadev;

    if (pmed) *pmed = 0.0;
    if (!pdev)
        return ERROR_INT("&dev not defined", __func__, 1);
    *pdev = 0.0;
    if (!na || numaGetCount(na) == 0)
        return ERROR_INT("na not defined or empty", __func__, 1);

    numaGetMedian(na, &med);
    if (pmed) *pmed = med;
    n = numaGetCount(na);
    nadev = numaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        numaAddNumber(nadev, L_ABS(val - med));
    }
    numaGetMedian(nadev, pdev);

    numaDestroy(&nadev);
    return 0;
}


/*!
 * \brief   numaGetMode()
 *
 * \param[in]    na      source numa
 * \param[out]   pval    mode val
 * \param[out]   pcount  [optional] mode count
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Computes the mode value of the numbers in the numa, by
 *          sorting and finding the value of the number with the
 *          largest count.
 *      (2) Optionally, also returns that count.
 * </pre>
 */
l_ok
numaGetMode(NUMA       *na,
            l_float32  *pval,
            l_int32    *pcount)
{
l_int32     i, n, maxcount, prevcount;
l_float32   val, maxval, prevval;
l_float32  *array;
NUMA       *nasort;

    if (pcount) *pcount = 0;
    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if ((n = numaGetCount(na)) == 0)
        return ERROR_INT("na is empty", __func__, 1);

    if ((nasort = numaSort(NULL, na, L_SORT_DECREASING)) == NULL)
        return ERROR_INT("nas not made", __func__, 1);
    array = numaGetFArray(nasort, L_NOCOPY);

        /* Initialize with array[0] */
    prevval = array[0];
    prevcount = 1;
    maxval = prevval;
    maxcount = prevcount;

        /* Scan the sorted array, aggregating duplicates */
    for (i = 1; i < n; i++) {
        val = array[i];
        if (val == prevval) {
            prevcount++;
        } else {  /* new value */
            if (prevcount > maxcount) {  /* new max */
                maxcount = prevcount;
                maxval = prevval;
            }
            prevval = val;
            prevcount = 1;
        }
    }

        /* Was the mode the last run of elements? */
    if (prevcount > maxcount) {
        maxcount = prevcount;
        maxval = prevval;
    }

    *pval = maxval;
    if (pcount)
        *pcount = maxcount;

    numaDestroy(&nasort);
    return 0;
}


/*----------------------------------------------------------------------*
 *                            Rearrangements                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaJoin()
 *
 * \param[in]    nad      dest numa; add to this one
 * \param[in]    nas      [optional] source numa; add from this one
 * \param[in]    istart   starting index in nas
 * \param[in]    iend     ending index in nas; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (2) iend < 0 means 'read to the end'
 *      (3) if nas == NULL, this is a no-op
 * </pre>
 */
l_ok
numaJoin(NUMA    *nad,
         NUMA    *nas,
         l_int32  istart,
         l_int32  iend)
{
l_int32    n, i;
l_float32  val;

    if (!nad)
        return ERROR_INT("nad not defined", __func__, 1);
    if (!nas)
        return 0;

    if (istart < 0)
        istart = 0;
    n = numaGetCount(nas);
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; nothing to add", __func__, 1);

    for (i = istart; i <= iend; i++) {
        numaGetFValue(nas, i, &val);
        numaAddNumber(nad, val);
    }

    return 0;
}


/*!
 * \brief   numaaJoin()
 *
 * \param[in]    naad     dest naa; add to this one
 * \param[in]    naas     [optional] source naa; add from this one
 * \param[in]    istart   starting index in nas
 * \param[in]    iend     ending index in naas; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (2) iend < 0 means 'read to the end'
 *      (3) if naas == NULL, this is a no-op
 * </pre>
 */
l_ok
numaaJoin(NUMAA   *naad,
          NUMAA   *naas,
          l_int32  istart,
          l_int32  iend)
{
l_int32  n, i;
NUMA    *na;

    if (!naad)
        return ERROR_INT("naad not defined", __func__, 1);
    if (!naas)
        return 0;

    if (istart < 0)
        istart = 0;
    n = numaaGetCount(naas);
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; nothing to add", __func__, 1);

    for (i = istart; i <= iend; i++) {
        na = numaaGetNuma(naas, i, L_CLONE);
        numaaAddNuma(naad, na, L_INSERT);
    }

    return 0;
}


/*!
 * \brief   numaaFlattenToNuma()
 *
 * \param[in]    naa
 * \return  numa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This 'flattens' the Numaa to a Numa, by joining successively
 *          each Numa in the Numaa.
 *      (2) It doesn't make any assumptions about the location of the
 *          Numas in the Numaa array, unlike most Numaa functions.
 *      (3) It leaves the input Numaa unchanged.
 * </pre>
 */
NUMA *
numaaFlattenToNuma(NUMAA  *naa)
{
l_int32  i, nalloc;
NUMA    *na, *nad;
NUMA   **array;

    if (!naa)
        return (NUMA *)ERROR_PTR("naa not defined", __func__, NULL);

    nalloc = naa->nalloc;
    array = numaaGetPtrArray(naa);
    nad = numaCreate(0);
    for (i = 0; i < nalloc; i++) {
        na = array[i];
        if (!na) continue;
        numaJoin(nad, na, 0, -1);
    }

    return nad;
}

