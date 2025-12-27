// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_FITZ_COLOR_H
#define MUPDF_FITZ_COLOR_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/store.h"

#if FZ_ENABLE_ICC
/**
	Opaque type for an ICC Profile.
*/
typedef struct fz_icc_profile fz_icc_profile;
#endif

/**
	Describes a given colorspace.
*/
typedef struct fz_colorspace fz_colorspace;

/**
	Pixmaps represent a set of pixels for a 2 dimensional region of
	a plane. Each pixel has n components per pixel. The components
	are in the order process-components, spot-colors, alpha, where
	there can be 0 of any of those types. The data is in
	premultiplied alpha when rendering, but non-premultiplied for
	colorspace conversions and rescaling.
*/
typedef struct fz_pixmap fz_pixmap;

/* Color handling parameters: rendering intent, overprint, etc. */

enum
{
	/* Same order as needed by lcms */
	FZ_RI_PERCEPTUAL,
	FZ_RI_RELATIVE_COLORIMETRIC,
	FZ_RI_SATURATION,
	FZ_RI_ABSOLUTE_COLORIMETRIC,
};

/* We abuse the top bit of the rendering intent to hold details of
 * whether we are in a softmask or not. This should not be used by
 * non-internal code. */
enum
{
	FZ_RI_IN_SOFTMASK = 0x80
};

typedef struct
{
	uint8_t ri;	/* rendering intent */
	uint8_t bp;	/* black point compensation */
	uint8_t op;	/* overprinting */
	uint8_t opm;	/* overprint mode */
}  fz_color_params;

FZ_DATA extern const fz_color_params fz_default_color_params;

/**
	Map from (case sensitive) rendering intent string to enumeration
	value.
*/
int fz_lookup_rendering_intent(const char *name);

/**
	Map from enumerated rendering intent to string.

	The returned string is static and therefore must not be freed.
*/
const char *fz_rendering_intent_name(int ri);

/**
	The maximum number of colorants available in any given
	color/colorspace (not including alpha).

	Changing this value will alter the amount of memory being used
	(both stack and heap space), but not hugely. Speed should
	(largely) be determined by the number of colors actually used.
*/
enum { FZ_MAX_COLORS = 32 };

enum fz_colorspace_type
{
	FZ_COLORSPACE_NONE,
	FZ_COLORSPACE_GRAY,
	FZ_COLORSPACE_RGB,
	FZ_COLORSPACE_BGR,
	FZ_COLORSPACE_CMYK,
	FZ_COLORSPACE_LAB,
	FZ_COLORSPACE_INDEXED,
	FZ_COLORSPACE_SEPARATION,
};

enum
{
	FZ_COLORSPACE_IS_DEVICE = 1,
	FZ_COLORSPACE_IS_ICC = 2,
	FZ_COLORSPACE_HAS_CMYK = 4,
	FZ_COLORSPACE_HAS_SPOTS = 8,
	FZ_COLORSPACE_HAS_CMYK_AND_SPOTS = 4|8,
};

/**
	Creates a new colorspace instance and returns a reference.

	No internal checking is done that the colorspace type (e.g.
	CMYK) matches with the flags (e.g. FZ_COLORSPACE_HAS_CMYK) or
	colorant count (n) or name.

	The reference should be dropped when it is finished with.

	Colorspaces are immutable once created (with the exception of
	setting up colorant names for separation spaces).
*/
fz_colorspace *fz_new_colorspace(fz_context *ctx, enum fz_colorspace_type type, int flags, int n, const char *name);

/**
	Increment the reference count for the colorspace.

	Returns the same pointer. Never throws an exception.
*/
fz_colorspace *fz_keep_colorspace(fz_context *ctx, fz_colorspace *colorspace);

/**
	Drops a reference to the colorspace.

	When the reference count reaches zero, the colorspace is
	destroyed.
*/
void fz_drop_colorspace(fz_context *ctx, fz_colorspace *colorspace);

/**
	Create an indexed colorspace.

	The supplied lookup table is high palette entries long. Each
	entry is n bytes long, where n is given by the number of
	colorants in the base colorspace, one byte per colorant.

	Ownership of lookup is passed it; it will be freed on
	destruction, so must be heap allocated.

	The colorspace will keep an additional reference to the base
	colorspace that will be dropped on destruction.

	The returned reference should be dropped when it is finished
	with.

	Colorspaces are immutable once created.
*/
fz_colorspace *fz_new_indexed_colorspace(fz_context *ctx, fz_colorspace *base, int high, unsigned char *lookup);

/**
	Create a colorspace from an ICC profile supplied in buf.

	Limited checking is done to ensure that the colorspace type is
	appropriate for the supplied ICC profile.

	An additional reference is taken to buf, which will be dropped
	on destruction. Ownership is NOT passed in.

	The returned reference should be dropped when it is finished
	with.

	Colorspaces are immutable once created.
*/
fz_colorspace *fz_new_icc_colorspace(fz_context *ctx, enum fz_colorspace_type type, int flags, const char *name, fz_buffer *buf);


/**
	Create a calibrated gray colorspace.

	The returned reference should be dropped when it is finished
	with.

	Colorspaces are immutable once created.
*/
fz_colorspace *fz_new_cal_gray_colorspace(fz_context *ctx, float wp[3], float bp[3], float gamma);

/**
	Create a calibrated rgb colorspace.

	The returned reference should be dropped when it is finished
	with.

	Colorspaces are immutable once created.
*/
fz_colorspace *fz_new_cal_rgb_colorspace(fz_context *ctx, float wp[3], float bp[3], float gamma[3], float matrix[9]);

/**
	Query the type of colorspace.
*/
enum fz_colorspace_type fz_colorspace_type(fz_context *ctx, fz_colorspace *cs);

/**
	Query the name of a colorspace.

	The returned string has the same lifespan as the colorspace
	does. Caller should not free it.
*/
const char *fz_colorspace_name(fz_context *ctx, fz_colorspace *cs);

/**
	Query the number of colorants in a colorspace.
*/
int fz_colorspace_n(fz_context *ctx, fz_colorspace *cs);

/**
	True for CMYK, Separation and DeviceN colorspaces.
*/
int fz_colorspace_is_subtractive(fz_context *ctx, fz_colorspace *cs);

/**
	True if DeviceN color space has only colorants from the CMYK set.
*/
int fz_colorspace_device_n_has_only_cmyk(fz_context *ctx, fz_colorspace *cs);

/**
	True if DeviceN color space has cyan magenta yellow or black as
	one of its colorants.
*/
int fz_colorspace_device_n_has_cmyk(fz_context *ctx, fz_colorspace *cs);

/**
	Tests for particular types of colorspaces
*/
int fz_colorspace_is_gray(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_rgb(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_cmyk(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_lab(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_indexed(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_icc(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_device_n(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_device(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_device_gray(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_device_cmyk(fz_context *ctx, fz_colorspace *cs);
int fz_colorspace_is_lab_icc(fz_context *ctx, fz_colorspace *cs);

/**
	Get checksum of underlying ICC profile.
*/
void fz_colorspace_digest(fz_context *ctx, fz_colorspace *cs, unsigned char digest[16]);

/**
	Check to see that a colorspace is appropriate to be used as
	a blending space (i.e. only grey, rgb or cmyk).
*/
int fz_is_valid_blend_colorspace(fz_context *ctx, fz_colorspace *cs);

/**
	Get the 'base' colorspace for a colorspace.

	For indexed colorspaces, this is the colorspace the index
	decodes into. For all other colorspaces, it is the colorspace
	itself.

	The returned colorspace is 'borrowed' (i.e. no additional
	references are taken or dropped).
*/
fz_colorspace *fz_base_colorspace(fz_context *ctx, fz_colorspace *cs);

/**
	Retrieve global default colorspaces.

	These return borrowed references that should not be dropped,
	unless they are kept first.
*/
fz_colorspace *fz_device_gray(fz_context *ctx);
fz_colorspace *fz_device_rgb(fz_context *ctx);
fz_colorspace *fz_device_bgr(fz_context *ctx);
fz_colorspace *fz_device_cmyk(fz_context *ctx);
fz_colorspace *fz_device_lab(fz_context *ctx);

/**
	Assign a name for a given colorant in a colorspace.

	Used while initially setting up a colorspace. The string is
	copied into local storage, so need not be retained by the
	caller.
*/
void fz_colorspace_name_colorant(fz_context *ctx, fz_colorspace *cs, int n, const char *name);

/**
	Retrieve a the name for a colorant.

	Returns a pointer with the same lifespan as the colorspace.
*/
const char *fz_colorspace_colorant(fz_context *ctx, fz_colorspace *cs, int n);

/* Color conversion */

/**
	Clamp the samples in a color to the correct ranges for a
	given colorspace.
*/
void fz_clamp_color(fz_context *ctx, fz_colorspace *cs, const float *in, float *out);

/**
	Convert color values sv from colorspace ss into colorvalues dv
	for colorspace ds, via an optional intervening space is,
	respecting the given color_params.
*/
void fz_convert_color(fz_context *ctx, fz_colorspace *ss, const float *sv, fz_colorspace *ds, float *dv, fz_colorspace *is, fz_color_params params);

/* Default (fallback) colorspace handling */

/**
	Structure to hold default colorspaces.
*/
typedef struct
{
	int refs;
	fz_colorspace *gray;
	fz_colorspace *rgb;
	fz_colorspace *cmyk;
	fz_colorspace *oi;
}  fz_default_colorspaces;

/**
	Create a new default colorspace structure with values inherited
	from the context, and return a reference to it.

	These can be overridden using fz_set_default_xxxx.

	These should not be overridden while more than one caller has
	the reference for fear of race conditions.

	The caller should drop this reference once finished with it.
*/
fz_default_colorspaces *fz_new_default_colorspaces(fz_context *ctx);

/**
	Keep an additional reference to the default colorspaces
	structure.

	Never throws exceptions.
*/
fz_default_colorspaces* fz_keep_default_colorspaces(fz_context *ctx, fz_default_colorspaces *default_cs);

/**
	Drop a reference to the default colorspaces structure. When the
	reference count reaches 0, the references it holds internally
	to the underlying colorspaces will be dropped, and the structure
	will be destroyed.

	Never throws exceptions.
*/
void fz_drop_default_colorspaces(fz_context *ctx, fz_default_colorspaces *default_cs);

/**
	Returns a reference to a newly cloned default colorspaces
	structure.

	The new clone may safely be altered without fear of race
	conditions as the caller is the only reference holder.
*/
fz_default_colorspaces *fz_clone_default_colorspaces(fz_context *ctx, fz_default_colorspaces *base);

/**
	Retrieve default colorspaces (typically page local).

	If default_cs is non NULL, the default is retrieved from there,
	otherwise the global default is retrieved.

	These return borrowed references that should not be dropped,
	unless they are kept first.
*/
fz_colorspace *fz_default_gray(fz_context *ctx, const fz_default_colorspaces *default_cs);
fz_colorspace *fz_default_rgb(fz_context *ctx, const fz_default_colorspaces *default_cs);
fz_colorspace *fz_default_cmyk(fz_context *ctx, const fz_default_colorspaces *default_cs);
fz_colorspace *fz_default_output_intent(fz_context *ctx, const fz_default_colorspaces *default_cs);

/**
	Set new defaults within the default colorspace structure.

	New references are taken to the new default, and references to
	the old defaults dropped.

	Never throws exceptions.
*/
void fz_set_default_gray(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs);
void fz_set_default_rgb(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs);
void fz_set_default_cmyk(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs);
void fz_set_default_output_intent(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs);

/* Implementation details: subject to change. */

struct fz_colorspace
{
	fz_key_storable key_storable;
	enum fz_colorspace_type type;
	int flags;
	int n;
	char *name;
	union {
#if FZ_ENABLE_ICC
		struct {
			fz_buffer *buffer;
			unsigned char md5[16];
			fz_icc_profile *profile;
		} icc;
#endif
		struct {
			fz_colorspace *base;
			int high;
			unsigned char *lookup;
		} indexed;
		struct {
			fz_colorspace *base;
			void (*eval)(fz_context *ctx, void *tint, const float *s, int sn, float *d, int dn);
			void (*drop)(fz_context *ctx, void *tint);
			void *tint;
			char *colorant[FZ_MAX_COLORS];
		} separation;
	} u;
};

void fz_drop_colorspace_imp(fz_context *ctx, fz_storable *cs_);

#endif
