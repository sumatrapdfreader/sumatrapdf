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

#ifndef  LEPTONICA_REGUTILS_H
#define  LEPTONICA_REGUTILS_H

/*!
 * \file regutils.h
 *
 * <pre>
 *   Contains this regression test parameter packaging struct
 *       struct L_RegParams
 *
 *   The regression test utility allows you to write regression tests
 *   that compare results with existing "golden files" and with
 *   compiled in data.
 *
 *   Regression tests can be called in three ways.
 *   For example, for distance_reg:
 *
 *       Case 1: distance_reg [compare]
 *           This runs the test against the set of golden files.  It
 *           appends to 'outfile.txt' either "SUCCESS" or "FAILURE",
 *           as well as the details of any parts of the test that failed.
 *           It writes to a temporary file stream (fp).
 *           Using 'compare' on the command line is optional.
 *
 *       Case 2: distance_reg generate
 *           This generates golden files in /tmp for the reg test.
 *
 *       Case 3: distance_reg display
 *           This runs the test but makes no comparison of the output
 *           against the set of golden files.  In addition, this displays
 *           images and plots that are specified in the test under
 *           control of the display variable.  Display is enabled only
 *           for this case.
 *
 *   Regression tests follow the pattern given below.  Tests are
 *   automatically numbered sequentially, and it is convenient to
 *   comment each with a number to keep track (for comparison tests
 *   and for debugging).  In an actual case, comparisons of pix and
 *   of files can occur in any order.  We give a specific order here
 *   for clarity.
 *
 *       L_REGPARAMS  *rp;  // holds data required by the test functions
 *
 *       // Setup variables; optionally open stream
 *       if (regTestSetup(argc, argv, &rp))
 *           return 1;
 *
 *       // Test pairs of generated pix for identity.  This compares
 *       // two pix; no golden file is generated.
 *       regTestComparePix(rp, pix1, pix2);  // 0
 *
 *       // Test pairs of generated pix for similarity.  This compares
 *       // two pix; no golden file is generated.  The last arg determines
 *       // if stats are to be written to stderr.
 *       regTestCompareSimilarPix(rp, pix1, pix2, 15, 0.001, 0);  // 1
 *
 *       // Generation of <newfile*> outputs and testing for identity
 *       // These files can be anything, of course.
 *       regTestCheckFile(rp, <newfile0>);  // 2
 *       regTestCheckFile(rp, <newfile1>);  // 3
 *
 *       // Test pairs of output golden files for identity.  Here we
 *       // are comparing golden files 2 and 3.
 *       regTestCompareFiles(rp, 2, 3);  // 4
 *
 *       // "Write and check".  This writes a pix using a canonical
 *       // formulation for the local filename and either:
 *       //     case 1: generates a golden file
 *       //     case 2: compares the local file with a golden file
 *       //     case 3: generates local files and displays
 *       // Here we write the pix compressed with png and jpeg, respectively;
 *       // Then check against the golden file.  The internal %index
 *       // is incremented; it is embedded in the local filename and,
 *       // if generating, in the golden file as well.
 *       regTestWritePixAndCheck(rp, pix1, IFF_PNG);  // 5
 *       regTestWritePixAndCheck(rp, pix2, IFF_JFIF_JPEG);  // 6
 *
 *       // Display if reg test was called in 'display' mode
 *       pixDisplayWithTitle(pix1, 100, 100, NULL, rp->display);
 *
 *       // Clean up and output result
 *       regTestCleanup(rp);
 * </pre>
 */

/*----------------------------------------------------------------------------*
 *                      Regression test parameter packer                      *
 *----------------------------------------------------------------------------*/

/*! Regression test parameter packer */
struct L_RegParams
{
    FILE    *fp;        /*!< stream to temporary output file for compare mode */
    char    *testname;  /*!< name of test, without '_reg'                     */
    char    *tempfile;  /*!< name of temp file for compare mode output        */
    l_int32  mode;      /*!< generate, compare or display                     */
    l_atomic index;     /*!< index into saved files for this test; 0-based    */
    l_int32  success;   /*!< overall result of the test                       */
    l_int32  display;   /*!< 1 if in display mode; 0 otherwise                */
    L_TIMER  tstart;    /*!< marks beginning of the reg test                  */
};
typedef struct L_RegParams  L_REGPARAMS;


    /*! Running modes for the test */
/*! Regtest Mode */
enum {
    L_REG_GENERATE = 0,
    L_REG_COMPARE = 1,
    L_REG_DISPLAY = 2
};


#endif  /* LEPTONICA_REGUTILS_H */

