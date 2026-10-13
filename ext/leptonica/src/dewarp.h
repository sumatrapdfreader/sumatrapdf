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

#ifndef  LEPTONICA_DEWARP_H
#define  LEPTONICA_DEWARP_H

/*!
 * \file dewarp.h
 *
 * <pre>
 *     Data structure to hold arrays and results for generating
 *     horizontal and vertical disparity arrays based on textlines.
 *     Each disparity array is two-dimensional.  The vertical disparity
 *     array gives a vertical displacement, relative to the lowest point
 *     in the textlines.  The horizontal disparty array gives a horizontal
 *     displacement, relative to the minimum values (for even pages)
 *     or maximum values (for odd pages) of the left and right ends of
 *     full textlines.  Horizontal alignment always involves translations
 *     away from the book gutter.
 *
 *     We have intentionally separated the process of building models
 *     from the rendering process that uses the models.  For any page,
 *     the building operation either creates an actual model (that is,
 *     a model with at least the vertical disparity being computed, and
 *     for which the 'success' flag is set) or fails to create a model.
 *     However, at rendering time, a page can have one of two different
 *     types of models.
 *     (1) A valid model is an actual model that meets the rendering
 *         constraints, which are limits on model curvature parameters.
 *         See dewarpaTestForValidModel() for details.
 *         Valid models are identified by dewarpaInsertRefModels(),
 *         which sets the 'vvalid' and 'hvalid' fields.  Only valid
 *         models are used for rendering.
 *     (2) A reference model is used by a page that doesn't have
 *         a valid model, but has a nearby valid model of the same
 *         parity (even/odd page) that it can use.  The range in pages
 *         to search for a valid model is given by the 'maxdist' field.
 *
 *     At the rendering stage, vertical and horizontal disparities are
 *     treated differently.  It is somewhat more robust to generate
 *     vertical disparity models (VDM) than horizontal disparity
 *     models (HDM). A valid VDM is required for any correction to
 *     be made; if a valid VDM is not available, just use the input
 *     image.  Otherwise, assuming it is available, the use of the
 *     HDM is controlled by two fields: 'useboth' and 'check_columns'.
 *       (a) With useboth == 0, we use only the VDM.
 *       (b) With useboth == 1, we require using the VDM and, if a valid
 *           horizontal disparity model (HDM) is available, we also use it.
 *       (c) With check_columns == 1, check for multiple columns and if
 *           true, only use the VDM, even if a valid HDM is available.
 *           Note that 'check_columns' takes precedence over 'useboth'
 *           when there is more than 1 column of text.  By default,
 *           check_columns == 0.
 *
 *     The 'maxdist' parameter is input when the dewarpa is created.
 *     The other rendering parameters have default values given in dewarp1.c.
 *     All parameters used by rendering can be set (or reset) using accessors.
 *
 *     After dewarping, use of the VDM will cause all points on each
 *     altered curve to have a y-value equal to the minimum.  Use of
 *     the HDA will cause the left and right edges of the textlines
 *     to be vertically aligned if they had been typeset flush-left
 *     and flush-right, respectively.
 *
 *     The sampled disparity arrays are expanded to full resolution,
 *     using linear interpolation, and this is further expanded
 *     by slope continuation to the right and below if the image
 *     is larger than the full resolution disparity arrays.  Then
 *     the disparity correction can be applied to the input image.
 *     If the input pix are 2x reduced, the expansion from sampled
 *     to full res uses the product of (sampling) * (redfactor).
 *
 *     The most accurate results are produced at full resolution, and
 *     this is generally recommended.
 * </pre>
 */

    /*! Dewarp version for serialization
     * <pre>
     * Note on versioning of the serialization of this data structure:
     * The dewarping utility and the stored data can be expected to change.
     * In most situations, the serialized version is ephemeral -- it is
     * not needed after being used.  No functions will be provided to
     * convert between different versions.
     * </pre>
     */
#define  DEWARP_VERSION_NUMBER      4

/*! Data structure to hold a number of Dewarp */
struct L_Dewarpa
{
    l_int32            nalloc;        /*!< size of dewarp ptr array          */
    l_int32            maxpage;       /*!< maximum page number in array      */
    struct L_Dewarp  **dewarp;        /*!< array of ptrs to page dewarp      */
    struct L_Dewarp  **dewarpcache;   /*!< array of ptrs to cached dewarps   */
    struct Numa       *namodels;      /*!< list of page numbers for pages    */
                                      /*!< with page models                  */
    struct Numa       *napages;       /*!< list of page numbers with either  */
                                      /*!< page models or ref page models    */
    l_int32            redfactor;     /*!< reduction factor of input: 1 or 2 */
    l_int32            sampling;      /*!< disparity arrays sampling factor  */
    l_int32            minlines;      /*!< min number of long lines required */
    l_int32            maxdist;       /*!< max distance for getting ref page */
    l_int32            max_linecurv;  /*!< maximum abs line curvature,       */
                                      /*!< in micro-units                    */
    l_int32            min_diff_linecurv; /*!< minimum abs diff line         */
                                          /*!< curvature in micro-units      */
    l_int32            max_diff_linecurv; /*!< maximum abs diff line         */
                                          /*!< curvature in micro-units      */
    l_int32            max_edgeslope; /*!< maximum abs left or right edge    */
                                      /*!< slope, in milli-units             */
    l_int32            max_edgecurv;  /*!< maximum abs left or right edge    */
                                      /*!< curvature, in micro-units         */
    l_int32            max_diff_edgecurv; /*!< maximum abs diff left-right   */
                                      /*!< edge curvature, in micro-units    */
    l_int32            useboth;       /*!< use both disparity arrays if      */
                                      /*!< available; only vertical otherwise */
    l_int32            check_columns; /*!< if there are multiple columns,    */
                                      /*!< only use the vertical disparity   */
                                      /*!< array                             */
    l_int32            modelsready;   /*!< invalid models have been removed  */
                                      /*!< and refs built against valid set  */
};
typedef struct L_Dewarpa L_DEWARPA;


/*! Data structure for a single dewarp */
struct L_Dewarp
{
    struct L_Dewarpa  *dewa;         /*!< ptr to parent (not owned)          */
    struct Pix        *pixs;         /*!< source pix, 1 bpp                  */
    struct FPix       *sampvdispar;  /*!< sampled vert disparity array       */
    struct FPix       *samphdispar;  /*!< sampled horiz disparity array      */
    struct FPix       *sampydispar;  /*!< sampled slope h-disparity array    */
    struct FPix       *fullvdispar;  /*!< full vert disparity array          */
    struct FPix       *fullhdispar;  /*!< full horiz disparity array         */
    struct FPix       *fullydispar;  /*!< full slope h-disparity array       */
    struct Numa       *namidys;      /*!< sorted y val of midpoint each line */
    struct Numa       *nacurves;     /*!< sorted curvature of each line      */
    l_int32            w;            /*!< width of source image              */
    l_int32            h;            /*!< height of source image             */
    l_int32            pageno;       /*!< page number; important for reuse   */
    l_int32            sampling;     /*!< sampling factor of disparity arrays */
    l_int32            redfactor;    /*!< reduction factor of pixs: 1 or 2   */
    l_int32            minlines;     /*!< min number of long lines required  */
    l_int32            nlines;       /*!< number of long lines found         */
    l_int32            mincurv;      /*!< min line curvature in micro-units  */
    l_int32            maxcurv;      /*!< max line curvature in micro-units  */
    l_int32            leftslope;    /*!< left edge slope in milli-units     */
    l_int32            rightslope;   /*!< right edge slope in milli-units    */
    l_int32            leftcurv;     /*!< left edge curvature in micro-units */
    l_int32            rightcurv;    /*!< right edge curvature in micro-units*/
    l_int32            nx;           /*!< number of sampling pts in x-dir    */
    l_int32            ny;           /*!< number of sampling pts in y-dir    */
    l_int32            hasref;       /*!< 0 if normal; 1 if has a refpage    */
    l_int32            refpage;      /*!< page with disparity model to use   */
    l_int32            vsuccess;     /*!< sets to 1 if vert disparity builds */
    l_int32            hsuccess;     /*!< sets to 1 if horiz disparity builds */
    l_int32            ysuccess;     /*!< sets to 1 if slope disparity builds */
    l_int32            vvalid;       /*!< sets to 1 if valid vert disparity  */
    l_int32            hvalid;       /*!< sets to 1 if valid horiz disparity */
    l_int32            skip_horiz;   /*!< if 1, skip horiz disparity         */
                                     /*!< correction                         */
    l_int32            debug;        /*!< set to 1 if debug output requested */
};
typedef struct L_Dewarp L_DEWARP;

#endif  /* LEPTONICA_DEWARP_H */
