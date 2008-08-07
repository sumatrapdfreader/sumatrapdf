/*
 * Copyright (c) 2001-2003 Michael David Adams.
 * All rights reserved.
 */

/* __START_OF_JASPER_LICENSE__
 * 
 * JasPer License Version 2.0
 * 
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 * Copyright (c) 2001-2003 Michael David Adams
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 * 
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 * 
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 * 
 * __END_OF_JASPER_LICENSE__
 */

/*
 * Image Comparison Program
 *
 * $Id$
 */

/******************************************************************************\
* Includes.
\******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <assert.h>

#include <jasper/jasper.h>


/******************************************************************************\
*
\******************************************************************************/

typedef enum {
	OPT_HELP,
	OPT_VERSION,
	OPT_VERBOSE,
	OPT_ORIG,
	OPT_RECON,
	OPT_METRIC,
	OPT_MAXONLY,
	OPT_MINONLY,
	OPT_DIFFIMAGE
} optid_t;

typedef enum {
	metricid_none = 0,
	metricid_equal,
	metricid_psnr,
	metricid_mse,
	metricid_rmse,
	metricid_pae,
	metricid_mae
} metricid_t;

/******************************************************************************\
*
\******************************************************************************/

double getdistortion(jas_matrix_t *orig, jas_matrix_t *recon, int depth, int metric);
double pae(jas_matrix_t *x, jas_matrix_t *y);
double msen(jas_matrix_t *x, jas_matrix_t *y, int n);
double psnr(jas_matrix_t *x, jas_matrix_t *y, int depth);
jas_image_t *makediffimage(jas_matrix_t *origdata, jas_matrix_t *recondata);
void usage(void);
void cmdinfo(void);

/******************************************************************************\
*
\******************************************************************************/

static jas_taginfo_t metrictab[] = {
	{metricid_mse, "mse"},
	{metricid_pae, "pae"},
	{metricid_rmse, "rmse"},
	{metricid_psnr, "psnr"},
	{metricid_mae, "mae"},
	{metricid_equal, "equal"},
	{-1, 0}
};

static jas_opt_t opts[] = {
	{OPT_HELP, "help", 0},
	{OPT_VERSION, "version", 0},
	{OPT_VERBOSE, "verbose", 0},
	{OPT_ORIG, "f", JAS_OPT_HASARG},
	{OPT_RECON, "F", JAS_OPT_HASARG},
	{OPT_METRIC, "m", JAS_OPT_HASARG},
	{OPT_MAXONLY, "max", 0},
	{OPT_MINONLY, "min", 0},
	{OPT_DIFFIMAGE, "d", JAS_OPT_HASARG},
	{-1, 0, 0}
};

static char *cmdname = 0;

/******************************************************************************\
* Main program.
\******************************************************************************/

int main(int argc, char **argv)
{
	char *origpath;
	char *reconpath;
	int verbose;
	char *metricname;
	int metric;

	int id;
	jas_image_t *origimage;
	jas_image_t *reconimage;
	jas_matrix_t *origdata;
	jas_matrix_t *recondata;
	jas_image_t *diffimage;
	jas_stream_t *diffstream;
	int width;
	int height;
	int depth;
	int numcomps;
	double d;
	double maxdist;
	double mindist;
	int compno;
	jas_stream_t *origstream;
	jas_stream_t *reconstream;
	char *diffpath;
	int maxonly;
	int minonly;
	int fmtid;

	verbose = 0;
	origpath = 0;
	reconpath = 0;
	metricname = 0;
	metric = metricid_none;
	diffpath = 0;
	maxonly = 0;
	minonly = 0;

	if (jas_init()) {
		abort();
	}

	cmdname = argv[0];

	/* Parse the command line options. */
	while ((id = jas_getopt(argc, argv, opts)) >= 0) {
		switch (id) {
		case OPT_MAXONLY:
			maxonly = 1;
			break;
		case OPT_MINONLY:
			minonly = 1;
			break;
		case OPT_METRIC:
			metricname = jas_optarg;
			break;
		case OPT_ORIG:
			origpath = jas_optarg;
			break;
		case OPT_RECON:
			reconpath = jas_optarg;
			break;
		case OPT_VERBOSE:
			verbose = 1;
			break;
		case OPT_DIFFIMAGE:
			diffpath = jas_optarg;
			break;
		case OPT_VERSION:
			printf("%s\n", JAS_VERSION);
			exit(EXIT_SUCCESS);
			break;
		case OPT_HELP:
		default:
			usage();
			break;
		}
	}

	if (verbose) {
		cmdinfo();
	}

	/* Ensure that files are given for both the original and reconstructed
	  images. */
	if (!origpath || !reconpath) {
		usage();
	}

	/* If a metric was specified, process it. */
	if (metricname) {
		if ((metric = (jas_taginfo_nonull(jas_taginfos_lookup(metrictab,
		  metricname))->id)) < 0) {
			usage();
		}
	}

	/* Open the original image file. */
	if (!(origstream = jas_stream_fopen(origpath, "rb"))) {
		jas_eprintf("cannot open %s\n", origpath);
		return EXIT_FAILURE;
	}

	/* Open the reconstructed image file. */
	if (!(reconstream = jas_stream_fopen(reconpath, "rb"))) {
		jas_eprintf("cannot open %s\n", reconpath);
		return EXIT_FAILURE;
	}

	/* Decode the original image. */
	if (!(origimage = jas_image_decode(origstream, -1, 0))) {
		jas_eprintf("cannot load original image\n");
		return EXIT_FAILURE;
	}

	/* Decoder the reconstructed image. */
	if (!(reconimage = jas_image_decode(reconstream, -1, 0))) {
		jas_eprintf("cannot load reconstructed image\n");
		return EXIT_FAILURE;
	}

	/* Close the original image file. */
	jas_stream_close(origstream);

	/* Close the reconstructed image file. */
	jas_stream_close(reconstream);

	/* Ensure that both images have the same number of components. */
	numcomps = jas_image_numcmpts(origimage);
	if (jas_image_numcmpts(reconimage) != numcomps) {
		jas_eprintf("number of components differ\n");
		return EXIT_FAILURE;
	}

	/* Compute the difference for each component. */
	maxdist = 0;
	mindist = FLT_MAX;
	for (compno = 0; compno < numcomps; ++compno) {
		width = jas_image_cmptwidth(origimage, compno);
		height = jas_image_cmptheight(origimage, compno);
		depth = jas_image_cmptprec(origimage, compno);
		if (jas_image_cmptwidth(reconimage, compno) != width ||
		 jas_image_cmptheight(reconimage, compno) != height) {
			jas_eprintf("image dimensions differ\n");
			return EXIT_FAILURE;
		}
		if (jas_image_cmptprec(reconimage, compno) != depth) {
			jas_eprintf("precisions differ\n");
			return EXIT_FAILURE;
		}

		if (!(origdata = jas_matrix_create(height, width))) {
			jas_eprintf("internal error\n");
			return EXIT_FAILURE;
		}
		if (!(recondata = jas_matrix_create(height, width))) {
			jas_eprintf("internal error\n");
			return EXIT_FAILURE;
		}
		if (jas_image_readcmpt(origimage, compno, 0, 0, width, height,
		  origdata)) {
			jas_eprintf("cannot read component data\n");
			return EXIT_FAILURE;
		}
		if (jas_image_readcmpt(reconimage, compno, 0, 0, width, height,
		  recondata)) {
			jas_eprintf("cannot read component data\n");
			return EXIT_FAILURE;
		}

		if (diffpath) {
			if (!(diffstream = jas_stream_fopen(diffpath, "rwb"))) {
				jas_eprintf("cannot open diff stream\n");
				return EXIT_FAILURE;
			}
			if (!(diffimage = makediffimage(origdata, recondata))) {
				jas_eprintf("cannot make diff image\n");
				return EXIT_FAILURE;
			}
			fmtid = jas_image_strtofmt("pnm");
			if (jas_image_encode(diffimage, diffstream, fmtid, 0)) {
				jas_eprintf("cannot save\n");
				return EXIT_FAILURE;
			}
			jas_stream_close(diffstream);
			jas_image_destroy(diffimage);
		}

		if (metric != metricid_none) {
			d = getdistortion(origdata, recondata, depth, metric);
			if (d > maxdist) {
				maxdist = d;
			}
			if (d < mindist) {
				mindist = d;
			}
			if (!maxonly && !minonly) {
				if (metric == metricid_pae || metric == metricid_equal) {
					printf("%ld\n", (long) ceil(d));
				} else {
					printf("%f\n", d);
				}
			}
		}
		jas_matrix_destroy(origdata);
		jas_matrix_destroy(recondata);
	}

	if (metric != metricid_none && (maxonly || minonly)) {
		if (maxonly) {
			d = maxdist;
		} else if (minonly) {
			d = mindist;
		} else {
			abort();
		}
		
		if (metric == metricid_pae || metric == metricid_equal) {
			jas_eprintf("%ld\n", (long) ceil(d));
		} else {
			jas_eprintf("%f\n", d);
		}
	}

	jas_image_destroy(origimage);
	jas_image_destroy(reconimage);
	jas_image_clearfmts();

	return EXIT_SUCCESS;
}

/******************************************************************************\
* Distortion metric computation functions.
\******************************************************************************/

double getdistortion(jas_matrix_t *orig, jas_matrix_t *recon, int depth, int metric)
{
	double d;

	switch (metric) {
	case metricid_psnr:
	default:
		d = psnr(orig, recon, depth);
		break;
	case metricid_mae:
		d = msen(orig, recon, 1);
		break;
	case metricid_mse:
		d = msen(orig, recon, 2);
		break;
	case metricid_rmse:
		d = sqrt(msen(orig, recon, 2));
		break;
	case metricid_pae:
		d = pae(orig, recon);
		break;
	case metricid_equal:
		d = (pae(orig, recon) == 0) ? 0 : 1;
		break;
	}
	return d;
}

/* Compute peak absolute error. */

double pae(jas_matrix_t *x, jas_matrix_t *y)
{
	double s;
	double d;
	int i;
	int j;

	s = 0.0;
	for (i = 0; i < jas_matrix_numrows(x); i++) {
		for (j = 0; j < jas_matrix_numcols(x); j++) {
			d = abs(jas_matrix_get(y, i, j) - jas_matrix_get(x, i, j));
			if (d > s) {
				s = d;
			}
		}
	}

	return s;
}

/* Compute either mean-squared error or mean-absolute error. */

double msen(jas_matrix_t *x, jas_matrix_t *y, int n)
{
	double s;
	double d;
	int i;
	int j;

	s = 0.0;
	for (i = 0; i < jas_matrix_numrows(x); i++) {
		for (j = 0; j < jas_matrix_numcols(x); j++) {
			d = jas_matrix_get(y, i, j) - jas_matrix_get(x, i, j);
			if (n == 1) {
				s += fabs(d);
			} else if (n == 2) {
				s += d * d;
			} else {
				abort();
			}
		}
	}

	return s / ((double) jas_matrix_numrows(x) * jas_matrix_numcols(x));
}

/* Compute peak signal-to-noise ratio. */

double psnr(jas_matrix_t *x, jas_matrix_t *y, int depth)
{
	double m;
	double p;
	m = msen(x, y, 2);
	p = ((1 << depth) - 1);
	return 20.0 * log10(p / sqrt(m));
}

/******************************************************************************\
*
\******************************************************************************/

jas_image_t *makediffimage(jas_matrix_t *origdata, jas_matrix_t *recondata)
{
	jas_image_t *diffimage;
	jas_matrix_t *diffdata[3];
	int width;
	int height;
	int i;
	int j;
	int k;
	jas_image_cmptparm_t compparms[3];
	jas_seqent_t a;
	jas_seqent_t b;

	width = jas_matrix_numcols(origdata);
	height = jas_matrix_numrows(origdata);

	for (i = 0; i < 3; ++i) {
		compparms[i].tlx = 0;
		compparms[i].tly = 0;
		compparms[i].hstep = 1;
		compparms[i].vstep = 1;
		compparms[i].width = width;
		compparms[i].height = height;
		compparms[i].prec = 8;
		compparms[i].sgnd = jas_false;
	}
	if (!(diffimage = jas_image_create(3, compparms, JAS_CLRSPC_SRGB))) {
		abort();
	}

	for (i = 0; i < 3; ++i) {
		if (!(diffdata[i] = jas_matrix_create(height, width))) {
			jas_eprintf("internal error\n");
			return 0;
		}
	}

	for (j = 0; j < height; ++j) {
		for (k = 0; k < width; ++k) {
			a = jas_matrix_get(origdata, j, k);
			b = jas_matrix_get(recondata, j, k);
			if (a > b) {
				jas_matrix_set(diffdata[0], j, k, 255);
				jas_matrix_set(diffdata[1], j, k, 0);
				jas_matrix_set(diffdata[2], j, k, 0);
			} else if (a < b) {
				jas_matrix_set(diffdata[0], j, k, 0);
				jas_matrix_set(diffdata[1], j, k, 255);
				jas_matrix_set(diffdata[2], j, k, 0);
			} else {
				jas_matrix_set(diffdata[0], j, k, a);
				jas_matrix_set(diffdata[1], j, k, a);
				jas_matrix_set(diffdata[2], j, k, a);
			}
		}
	}

	for (i = 0; i < 3; ++i) {
		if (jas_image_writecmpt(diffimage, i, 0, 0, width, height, diffdata[i])) {
			return 0;
		}
	}

	return diffimage;
}

/******************************************************************************\
*
\******************************************************************************/

void cmdinfo()
{
	jas_eprintf("Image Comparison Utility (Version %s).\n",
	  JAS_VERSION);
	jas_eprintf(
	  "Copyright (c) 2001 Michael David Adams.\n"
	  "All rights reserved.\n"
	  );
}

void usage()
{
	cmdinfo();
	jas_eprintf("usage:\n");
	jas_eprintf("%s ", cmdname);
	jas_eprintf(
	  "-f reference_image_file -F other_image_file [-m metric]\n"
	  );
	jas_eprintf(
	  "The metric argument may assume one of the following values:\n"
	  "    psnr .... peak signal to noise ratio\n"
	  "    mse ..... mean squared error\n"
	  "    rmse .... root mean squared error\n"
	  "    pae ..... peak absolute error\n"
	  "    mae ..... mean absolute error\n"
	  "    equal ... equality (boolean)\n"
	  );
	exit(EXIT_FAILURE);
}
