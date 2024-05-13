#include "mupdf/fitz.h"
#include "icc34.h"

#include <string.h>

#define SAVEICCPROFILE 0
#define ICC_HEADER_SIZE 128
#define ICC_TAG_SIZE 12
#define ICC_NUMBER_COMMON_TAGS 2
#define ICC_XYZPT_SIZE 12
#define ICC_DATATYPE_SIZE 8
#define D50_X 0.9642f
#define D50_Y 1.0f
#define D50_Z 0.8249f
static const char copy_right[] = "Copyright Artifex Software 2020";
#if SAVEICCPROFILE
unsigned int icc_debug_index = 0;
#endif

typedef struct
{
	icTagSignature sig;
	icUInt32Number offset;
	icUInt32Number size;
	unsigned char byte_padding;
} fz_icc_tag;

#if SAVEICCPROFILE
static void
save_profile(fz_context *ctx, fz_buffer *buf, const char *name)
{
	char full_file_name[50];
	fz_snprintf(full_file_name, sizeof full_file_name, "profile%d-%s.icc", icc_debug_index, name);
	fz_save_buffer(ctx, buf, full_file_name);
	icc_debug_index++;
}
#endif

static void
fz_append_byte_n(fz_context *ctx, fz_buffer *buf, int c, int n)
{
	int k;
	for (k = 0; k < n; k++)
		fz_append_byte(ctx, buf, c);
}

static int
get_padding(int x)
{
	return (4 - x % 4) % 4;
}

static void
setdatetime(fz_context *ctx, icDateTimeNumber *datetime)
{
	datetime->day = 0;
	datetime->hours = 0;
	datetime->minutes = 0;
	datetime->month = 0;
	datetime->seconds = 0;
	datetime->year = 0;
}

static void
add_gammadata(fz_context *ctx, fz_buffer *buf, unsigned short gamma, icTagTypeSignature curveType)
{
	fz_append_int32_be(ctx, buf, curveType);
	fz_append_byte_n(ctx, buf, 0, 4);

	/* one entry for gamma */
	fz_append_int32_be(ctx, buf, 1);

	/* The encode (8frac8) gamma, with padding */
	fz_append_int16_be(ctx, buf, gamma);

	/* pad two bytes */
	fz_append_byte_n(ctx, buf, 0, 2);
}

static unsigned short
float2u8Fixed8(fz_context *ctx, float number_in)
{
	return (unsigned short)(number_in * 256);
}

static void
add_xyzdata(fz_context *ctx, fz_buffer *buf, icS15Fixed16Number temp_XYZ[])
{
	int j;

	fz_append_int32_be(ctx, buf, icSigXYZType);
	fz_append_byte_n(ctx, buf, 0, 4);

	for (j = 0; j < 3; j++)
		fz_append_int32_be(ctx, buf, temp_XYZ[j]);
}

static icS15Fixed16Number
double2XYZtype(fz_context *ctx, float number_in)
{
	short s;
	unsigned short m;

	if (number_in < 0)
		number_in = 0;
	s = (short)number_in;
	m = (unsigned short)((number_in - s) * 65536);
	return (icS15Fixed16Number) ((s << 16) | m);
}

static void
get_D50(fz_context *ctx, icS15Fixed16Number XYZ[])
{
	XYZ[0] = double2XYZtype(ctx, D50_X);
	XYZ[1] = double2XYZtype(ctx, D50_Y);
	XYZ[2] = double2XYZtype(ctx, D50_Z);
}

static void
get_XYZ_doubletr(fz_context *ctx, icS15Fixed16Number XYZ[], float vector[])
{
	XYZ[0] = double2XYZtype(ctx, vector[0]);
	XYZ[1] = double2XYZtype(ctx, vector[1]);
	XYZ[2] = double2XYZtype(ctx, vector[2]);
}

static void
add_desc_tag(fz_context *ctx, fz_buffer *buf, const char text[], fz_icc_tag tag_list[], int curr_tag)
{
	size_t len = strlen(text);

	fz_append_int32_be(ctx, buf, icSigTextDescriptionType);
	fz_append_byte_n(ctx, buf, 0, 4);
	fz_append_int32_be(ctx, buf, (int)len + 1);
	fz_append_string(ctx, buf, text);
	/* 1 + 4 + 4 + 2 + 1 + 67 */
	fz_append_byte_n(ctx, buf, 0, 79);
	fz_append_byte_n(ctx, buf, 0, tag_list[curr_tag].byte_padding);
}

static void
add_text_tag(fz_context *ctx, fz_buffer *buf, const char text[], fz_icc_tag tag_list[], int curr_tag)
{
	fz_append_int32_be(ctx, buf, icSigTextType);
	fz_append_byte_n(ctx, buf, 0, 4);
	fz_append_string(ctx, buf, text);
	fz_append_byte(ctx, buf, 0);
	fz_append_byte_n(ctx, buf, 0, tag_list[curr_tag].byte_padding);
}

static void
add_common_tag_data(fz_context *ctx, fz_buffer *buf, fz_icc_tag tag_list[], const char *desc_name)
{
	add_desc_tag(ctx, buf, desc_name, tag_list, 0);
	add_text_tag(ctx, buf, copy_right, tag_list, 1);
}

static void
init_common_tags(fz_context *ctx, fz_icc_tag tag_list[], int num_tags, int *last_tag, const char *desc_name)
{
	int curr_tag, temp_size;

	if (*last_tag < 0)
		curr_tag = 0;
	else
		curr_tag = (*last_tag) + 1;

	tag_list[curr_tag].offset = ICC_HEADER_SIZE + num_tags * ICC_TAG_SIZE + 4;
	tag_list[curr_tag].sig = icSigProfileDescriptionTag;

	/* temp_size = DATATYPE_SIZE + 4 (zeros) + 4 (len) + strlen(desc_name) + 1 (null) + 4 + 4 + 2 + 1 + 67 + bytepad; */
	temp_size = (int)strlen(desc_name) + 91;

	tag_list[curr_tag].byte_padding = get_padding(temp_size);
	tag_list[curr_tag].size = temp_size + tag_list[curr_tag].byte_padding;
	curr_tag++;
	tag_list[curr_tag].offset = tag_list[curr_tag - 1].offset + tag_list[curr_tag - 1].size;
	tag_list[curr_tag].sig = icSigCopyrightTag;

	/* temp_size = DATATYPE_SIZE + 4 (zeros) + strlen(copy_right) + 1 (null); */
	temp_size = (int)strlen(copy_right) + 9;
	tag_list[curr_tag].byte_padding = get_padding(temp_size);
	tag_list[curr_tag].size = temp_size + tag_list[curr_tag].byte_padding;
	*last_tag = curr_tag;
}

static void
copy_header(fz_context *ctx, fz_buffer *buffer, icHeader *header)
{
	fz_append_int32_be(ctx, buffer, header->size);
	fz_append_byte_n(ctx, buffer, 0, 4);
	fz_append_int32_be(ctx, buffer, header->version);
	fz_append_int32_be(ctx, buffer, header->deviceClass);
	fz_append_int32_be(ctx, buffer, header->colorSpace);
	fz_append_int32_be(ctx, buffer, header->pcs);
	fz_append_byte_n(ctx, buffer, 0, 12);
	fz_append_int32_be(ctx, buffer, header->magic);
	fz_append_int32_be(ctx, buffer, header->platform);
	fz_append_byte_n(ctx, buffer, 0, 24);
	fz_append_int32_be(ctx, buffer, header->illuminant.X);
	fz_append_int32_be(ctx, buffer, header->illuminant.Y);
	fz_append_int32_be(ctx, buffer, header->illuminant.Z);
	fz_append_byte_n(ctx, buffer, 0, 48);
}

static void
setheader_common(fz_context *ctx, icHeader *header)
{
	header->cmmId = 0;
	header->version = 0x02200000;
	setdatetime(ctx, &(header->date));
	header->magic = icMagicNumber;
	header->platform = icSigMacintosh;
	header->flags = 0;
	header->manufacturer = 0;
	header->model = 0;
	header->attributes[0] = 0;
	header->attributes[1] = 0;
	header->renderingIntent = 3;
	header->illuminant.X = double2XYZtype(ctx, (float) 0.9642);
	header->illuminant.Y = double2XYZtype(ctx, (float) 1.0);
	header->illuminant.Z = double2XYZtype(ctx, (float) 0.8249);
	header->creator = 0;
	memset(header->reserved, 0, 44);
}

static void
copy_tagtable(fz_context *ctx, fz_buffer *buf, fz_icc_tag *tag_list, int num_tags)
{
	int k;

	fz_append_int32_be(ctx, buf, num_tags);
	for (k = 0; k < num_tags; k++)
	{
		fz_append_int32_be(ctx, buf, tag_list[k].sig);
		fz_append_int32_be(ctx, buf, tag_list[k].offset);
		fz_append_int32_be(ctx, buf, tag_list[k].size);
	}
}

static void
init_tag(fz_context *ctx, fz_icc_tag tag_list[], int *last_tag, icTagSignature tagsig, int datasize)
{
	int curr_tag = (*last_tag) + 1;

	tag_list[curr_tag].offset = tag_list[curr_tag - 1].offset + tag_list[curr_tag - 1].size;
	tag_list[curr_tag].sig = tagsig;
	tag_list[curr_tag].byte_padding = get_padding(ICC_DATATYPE_SIZE + datasize);
	tag_list[curr_tag].size = ICC_DATATYPE_SIZE + datasize + tag_list[curr_tag].byte_padding;
	*last_tag = curr_tag;
}

static void
matrixmult(fz_context *ctx, float leftmatrix[], int nlrow, int nlcol, float rightmatrix[], int nrrow, int nrcol, float result[])
{
	float *curr_row;
	int k, l, j, ncols, nrows;
	float sum;

	nrows = nlrow;
	ncols = nrcol;
	if (nlcol == nrrow)
	{
		for (k = 0; k < nrows; k++)
		{
			curr_row = &(leftmatrix[k*nlcol]);
			for (l = 0; l < ncols; l++)
			{
				sum = 0.0;
				for (j = 0; j < nlcol; j++)
					sum = sum + curr_row[j] * rightmatrix[j*nrcol + l];
				result[k*ncols + l] = sum;
			}
		}
	}
}

static void
apply_adaption(fz_context *ctx, float matrix[], float in[], float out[])
{
	out[0] = matrix[0] * in[0] + matrix[1] * in[1] + matrix[2] * in[2];
	out[1] = matrix[3] * in[0] + matrix[4] * in[1] + matrix[5] * in[2];
	out[2] = matrix[6] * in[0] + matrix[7] * in[1] + matrix[8] * in[2];
}

/*
	Compute the CAT02 transformation to get us from the Cal White point to the
	D50 white point
*/
static void
gsicc_create_compute_cam(fz_context *ctx, float white_src[], float *cam)
{
	float cat02matrix[] = { 0.7328f, 0.4296f, -0.1624f, -0.7036f, 1.6975f, 0.0061f, 0.003f, 0.0136f, 0.9834f };
	float cat02matrixinv[] = { 1.0961f, -0.2789f, 0.1827f, 0.4544f, 0.4735f, 0.0721f, -0.0096f, -0.0057f, 1.0153f };
	float vonkries_diag[9];
	float temp_matrix[9];
	float lms_wp_src[3], lms_wp_des[3];
	int k;
	float d50[3] = { D50_X, D50_Y, D50_Z };

	matrixmult(ctx, cat02matrix, 3, 3, white_src, 3, 1, lms_wp_src);
	matrixmult(ctx, cat02matrix, 3, 3, d50, 3, 1, lms_wp_des);
	memset(&(vonkries_diag[0]), 0, sizeof(float) * 9);

	for (k = 0; k < 3; k++)
	{
		if (lms_wp_src[k] > 0)
			vonkries_diag[k * 3 + k] = lms_wp_des[k] / lms_wp_src[k];
		else
			vonkries_diag[k * 3 + k] = 1;
	}
	matrixmult(ctx, &(vonkries_diag[0]), 3, 3, cat02matrix, 3, 3, temp_matrix);
	matrixmult(ctx, &(cat02matrixinv[0]), 3, 3, temp_matrix, 3, 3, cam);
}

/* The algorithm used by the following few routines comes from
 * LCMS2. We use this for converting the XYZ primaries + whitepoint
 * supplied to fz_new_icc_data_from_cal into the correct form for
 * writing into the profile.
 *
 * Prior to this code, we were, in some cases (such as with
 * the color data from pal8v4.bmp) ending up with a profile where
 * the whitepoint did not match properly.
 */

// Determinant lower than that are assumed zero (used on matrix invert)
#define MATRIX_DET_TOLERANCE    0.0001

static int
matrix_invert(double *out, const double *in)
{
	double det, c0, c1, c2, absdet;

	c0 =  in[4]*in[8] - in[5]*in[7];
	c1 = -in[3]*in[8] + in[5]*in[6];
	c2 =  in[3]*in[7] - in[4]*in[6];

	det = in[0]*c0 + in[1]*c1 + in[2]*c2;
	absdet = det;
	if (absdet < 0)
		absdet = -absdet;
	if (absdet < MATRIX_DET_TOLERANCE)
		return 1;  // singular matrix; can't invert

	out[0] = c0/det;
	out[1] = (in[2]*in[7] - in[1]*in[8])/det;
	out[2] = (in[1]*in[5] - in[2]*in[4])/det;
	out[3] = c1/det;
	out[4] = (in[0]*in[8] - in[2]*in[6])/det;
	out[5] = (in[2]*in[3] - in[0]*in[5])/det;
	out[6] = c2/det;
	out[7] = (in[1]*in[6] - in[0]*in[7])/det;
	out[8] = (in[0]*in[4] - in[1]*in[3])/det;

	return 0;
}

static void
transform_vector(double *out, const double *mat, const double *v)
{
	out[0] = mat[0]*v[0] + mat[1]*v[1] + mat[2]*v[2];
	out[1] = mat[3]*v[0] + mat[4]*v[1] + mat[5]*v[2];
	out[2] = mat[6]*v[0] + mat[7]*v[1] + mat[8]*v[2];
}

static void
matrix_compose(double *out, const double *a, const double *b)
{
	out[0] = a[0]*b[0] + a[1]*b[3] + a[2]*b[6];
	out[1] = a[0]*b[1] + a[1]*b[4] + a[2]*b[7];
	out[2] = a[0]*b[2] + a[1]*b[5] + a[2]*b[8];
	out[3] = a[3]*b[0] + a[4]*b[3] + a[5]*b[6];
	out[4] = a[3]*b[1] + a[4]*b[4] + a[5]*b[7];
	out[5] = a[3]*b[2] + a[4]*b[5] + a[5]*b[8];
	out[6] = a[6]*b[0] + a[7]*b[3] + a[8]*b[6];
	out[7] = a[6]*b[1] + a[7]*b[4] + a[8]*b[7];
	out[8] = a[6]*b[2] + a[7]*b[5] + a[8]*b[8];
}

static int
adaptation_matrix(double *out, const double *fromXYZ, const double *toXYZ)
{
	// Bradford matrix
	static const double LamRigg[9] = {
		0.8951,  0.2664, -0.1614,
		-0.7502,  1.7135,  0.0367,
		0.0389, -0.0685,  1.0296
	};
	double chad_inv[9];
	double fromRGB[3];
	double toRGB[3];
	double cone[9];
	double tmp[9];

	/* This should never fail, because it's the same operation
	 * every time! Could save the work here... */
	if (matrix_invert(chad_inv, LamRigg))
		return 1;

	transform_vector(fromRGB, LamRigg, fromXYZ);
	transform_vector(toRGB, LamRigg, toXYZ);

	// Build matrix
	cone[0] = toRGB[0]/fromRGB[0];
	cone[1] = 0.0;
	cone[2] = 0.0;
	cone[3] = 0.0;
	cone[4] = toRGB[1]/fromRGB[1];
	cone[5] = 0.0;
	cone[6] = 0.0;
	cone[7] = 0.0;
	cone[8] = toRGB[2]/fromRGB[2];

	// Normalize
	matrix_compose(tmp, cone, LamRigg);
	matrix_compose(out, chad_inv, tmp);

	return 0;
}

static int
build_rgb2XYZ_transfer_matrix(double *out, double *xyYwhite, const double *xyYprimaries)
{
	double whitepoint[3], coef[3];
	double xn, yn;
	double xr, yr;
	double xg, yg;
	double xb, yb;
	double primaries[9];
	double result[9];
	double mat[9];
	double bradford[9];
	static double d50XYZ[3] = {  0.9642, 1.0, 0.8249 };

	xn = xyYwhite[0];
	yn = xyYwhite[1];
	xr = xyYprimaries[0];
	yr = xyYprimaries[1];
	xg = xyYprimaries[3];
	yg = xyYprimaries[4];
	xb = xyYprimaries[6];
	yb = xyYprimaries[7];

	// Build Primaries matrix
	primaries[0] = xr;
	primaries[1] = xg;
	primaries[2] = xb;
	primaries[3] = yr;
	primaries[4] = yg;
	primaries[5] = yb;
	primaries[6] = (1-xr-yr);
	primaries[7] = (1-xg-yg);
	primaries[8] = (1-xb-yb);

	// Result = Primaries ^ (-1) inverse matrix
	if (matrix_invert(&result[0], &primaries[0]))
		return 1;

	/* Convert whitepoint from xyY to XYZ. Isn't this where
	 * we came in? I think we've effectively normalised during
	 * this process. */
	whitepoint[0] = xn/yn;
	whitepoint[1] = 1;
	whitepoint[2] = (1-xn-yn)/yn;

	// Across inverse primaries ...
	transform_vector(coef, result, whitepoint);

	// Give us the Coefs, then I build transformation matrix
	mat[0] = coef[0]*xr;
	mat[1] = coef[1]*xg;
	mat[2] = coef[2]*xb;
	mat[3] = coef[0]*yr;
	mat[4] = coef[1]*yg;
	mat[5] = coef[2]*yb;
	mat[6] = coef[0]*(1.0-xr-yr);
	mat[7] = coef[1]*(1.0-xg-yg);
	mat[8] = coef[2]*(1.0-xb-yb);

	if (adaptation_matrix(bradford, whitepoint, d50XYZ))
		return 1;

	matrix_compose(out, bradford, mat);

	return 0;
}


fz_buffer *
fz_new_icc_data_from_cal(fz_context *ctx,
	float wp[3],
	float bp[3],
	float *gamma,
	float matrix[9],
	int n)
{
	fz_icc_tag *tag_list;
	icProfile iccprofile;
	icHeader *header = &(iccprofile.header);
	fz_buffer *profile = NULL;
	size_t profile_size;
	int k;
	int num_tags;
	unsigned short encode_gamma;
	int last_tag;
	icS15Fixed16Number temp_XYZ[3];
	icTagSignature TRC_Tags[3] = { icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag };
	int trc_tag_size;
	float cat02[9];
	float black_adapt[3];
	const char *desc_name;

	/* common */
	setheader_common(ctx, header);
	header->pcs = icSigXYZData;
	profile_size = ICC_HEADER_SIZE;
	header->deviceClass = icSigInputClass;

	if (n == 3)
	{
		desc_name = "CalRGB";
		header->colorSpace = icSigRgbData;
		num_tags = 10; /* common (2) + rXYZ, gXYZ, bXYZ, rTRC, gTRC, bTRC, bkpt, wtpt */
	}
	else
	{
		desc_name = "CalGray";
		header->colorSpace = icSigGrayData;
		num_tags = 5; /* common (2) + GrayTRC, bkpt, wtpt */
		TRC_Tags[0] = icSigGrayTRCTag;
	}

	tag_list = Memento_label(fz_malloc(ctx, sizeof(fz_icc_tag) * num_tags), "icc_tag_list");

	/* precompute sizes and offsets */
	profile_size += ICC_TAG_SIZE * num_tags;
	profile_size += 4; /* number of tags.... */
	last_tag = -1;
	init_common_tags(ctx, tag_list, num_tags, &last_tag, desc_name);
	if (n == 3)
	{
		init_tag(ctx, tag_list, &last_tag, icSigRedColorantTag, ICC_XYZPT_SIZE);
		init_tag(ctx, tag_list, &last_tag, icSigGreenColorantTag, ICC_XYZPT_SIZE);
		init_tag(ctx, tag_list, &last_tag, icSigBlueColorantTag, ICC_XYZPT_SIZE);
	}
	init_tag(ctx, tag_list, &last_tag, icSigMediaWhitePointTag, ICC_XYZPT_SIZE);
	init_tag(ctx, tag_list, &last_tag, icSigMediaBlackPointTag, ICC_XYZPT_SIZE);

	/* 4 for count, 2 for gamma, Extra 2 bytes for 4 byte alignment requirement */
	trc_tag_size = 8;
	for (k = 0; k < n; k++)
		init_tag(ctx, tag_list, &last_tag, TRC_Tags[k], trc_tag_size);
	for (k = 0; k < num_tags; k++)
		profile_size += tag_list[k].size;

	fz_var(profile);

	/* Allocate buffer */
	fz_try(ctx)
	{
		profile = fz_new_buffer(ctx, profile_size);

		/* Header */
		header->size = (icUInt32Number)profile_size;
		copy_header(ctx, profile, header);

		/* Tag table */
		copy_tagtable(ctx, profile, tag_list, num_tags);

		/* Common tags */
		add_common_tag_data(ctx, profile, tag_list, desc_name);

		/* Get the cat02 matrix */
		gsicc_create_compute_cam(ctx, wp, cat02);

		/* The matrix */
		if (n == 3)
		{
			/* Convert whitepoint from XYZ to xyY */
			double xyz = wp[0] + wp[1] + wp[2];
			double whitexyY[3] = { wp[0] / xyz, wp[1] / xyz, 1.0 };
			/* Convert primaries from XYZ to xyY */
			double matrix012 = matrix[0] + matrix[1] + matrix[2];
			double matrix345 = matrix[3] + matrix[4] + matrix[5];
			double matrix678 = matrix[6] + matrix[7] + matrix[8];
			double primariesxyY[9] = {
				matrix[0] / matrix012,
				matrix[1] / matrix012,
				matrix[1],
				matrix[3] / matrix345,
				matrix[4] / matrix345,
				matrix[5],
				matrix[6] / matrix678,
				matrix[7] / matrix678,
				matrix[8]
				};
			double primaries[9];

			if (build_rgb2XYZ_transfer_matrix(primaries, whitexyY, primariesxyY))
				fz_throw(ctx, FZ_ERROR_ARGUMENT, "CalRGB profile creation failed; bad values");

			for (k = 0; k < 3; k++)
			{
				float primary[3] = { primaries[k+0], primaries[k+3], primaries[k+6] };
				get_XYZ_doubletr(ctx, temp_XYZ, primary);
				add_xyzdata(ctx, profile, temp_XYZ);
			}
		}

		/* White and black points. WP is D50 */
		get_D50(ctx, temp_XYZ);
		add_xyzdata(ctx, profile, temp_XYZ);

		/* Black point. Apply cat02*/
		apply_adaption(ctx, cat02, bp, &(black_adapt[0]));
		get_XYZ_doubletr(ctx, temp_XYZ, &(black_adapt[0]));
		add_xyzdata(ctx, profile, temp_XYZ);

		/* Gamma */
		for (k = 0; k < n; k++)
		{
			encode_gamma = float2u8Fixed8(ctx, gamma[k]);
			add_gammadata(ctx, profile, encode_gamma, icSigCurveType);
		}
	}
	fz_always(ctx)
		fz_free(ctx, tag_list);
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, profile);
		fz_rethrow(ctx);
	}

#if SAVEICCPROFILE
	if (n == 3)
		save_profile(ctx, profile, "calRGB");
	else
		save_profile(ctx, profile, "calGray");
#endif
	return profile;
}
