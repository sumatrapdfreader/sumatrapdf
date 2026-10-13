/* SPDX-License-Identifier: BSD-3-Clause */

#include "common.h"

#define STUB_PIXEL_PLOT(NAME) \
	INTERNAL int NAME(struct zint_symbol* symbol, const unsigned char* pixelbuf) \
	{ \
		(void)symbol; \
		(void)pixelbuf; \
		return ZINT_ERROR_ENCODING_PROBLEM; \
	}

#define STUB_FUNC_CHAR(NAME) \
	INTERNAL int NAME(struct zint_symbol* symbol, unsigned char source[], int length) \
	{ \
		(void)symbol; \
		(void)source; \
		(void)length; \
		return ZINT_ERROR_ENCODING_PROBLEM; \
	}

#define STUB_FUNC_SEGS(NAME) \
	INTERNAL int NAME(struct zint_symbol* symbol, struct zint_seg segs[], const int seg_count) \
	{ \
		(void)symbol; \
		(void)segs; \
		(void)seg_count; \
		return ZINT_ERROR_ENCODING_PROBLEM; \
	}

STUB_PIXEL_PLOT(png_pixel_plot)
STUB_PIXEL_PLOT(bmp_pixel_plot)
STUB_PIXEL_PLOT(pcx_pixel_plot)
STUB_PIXEL_PLOT(gif_pixel_plot)
STUB_PIXEL_PLOT(tif_pixel_plot)

INTERNAL int ps_plot(struct zint_symbol* symbol)
{
	(void)symbol;
	return ZINT_ERROR_ENCODING_PROBLEM;
}
INTERNAL int emf_plot(struct zint_symbol* symbol, int rotate_angle)
{
	(void)symbol;
	(void)rotate_angle;
	return ZINT_ERROR_ENCODING_PROBLEM;
}

// STUB_FUNC_CHAR(pzn)
// STUB_FUNC_CHAR(c25ind)
// STUB_FUNC_CHAR(c25iata)
// STUB_FUNC_CHAR(c25inter)
// STUB_FUNC_CHAR(c25logic)
// STUB_FUNC_CHAR(itf14)
// STUB_FUNC_CHAR(dpleit)
// STUB_FUNC_CHAR(dpident)
// STUB_FUNC_CHAR(code11)
STUB_FUNC_CHAR(msi_plessey)
STUB_FUNC_CHAR(telepen)
STUB_FUNC_CHAR(telepen_num)
STUB_FUNC_CHAR(plessey)
// STUB_FUNC_CHAR(pharma)
STUB_FUNC_CHAR(flat)
STUB_FUNC_CHAR(fim)
// STUB_FUNC_CHAR(pharma_two)
STUB_FUNC_CHAR(postnet)
STUB_FUNC_CHAR(planet)
STUB_FUNC_CHAR(usps_imail)
STUB_FUNC_CHAR(rm4scc)
STUB_FUNC_CHAR(auspost)
STUB_FUNC_CHAR(code16k)
STUB_FUNC_CHAR(composite)
STUB_FUNC_CHAR(kix)
// STUB_FUNC_CHAR(code32)
STUB_FUNC_CHAR(daft)
// STUB_FUNC_CHAR(nve18)
STUB_FUNC_CHAR(koreapost)
STUB_FUNC_CHAR(japanpost)
STUB_FUNC_CHAR(code49)
// STUB_FUNC_CHAR(channel)
STUB_FUNC_SEGS(codeone)
STUB_FUNC_SEGS(gridmatrix)
STUB_FUNC_SEGS(hanxin)
STUB_FUNC_SEGS(dotcode)
STUB_FUNC_SEGS(codablockf)
// STUB_FUNC_CHAR(vin)
STUB_FUNC_CHAR(mailmark_2d)
STUB_FUNC_CHAR(mailmark_4s)
// STUB_FUNC_CHAR(upu_s10)
STUB_FUNC_SEGS(ultra)
// STUB_FUNC_CHAR(dpd)
STUB_FUNC_CHAR(bc412)
