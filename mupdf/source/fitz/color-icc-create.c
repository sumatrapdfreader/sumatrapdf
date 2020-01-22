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
static const char copy_right[] = "Copyright Artifex Software 2017";
#if SAVEICCPROFILE
unsigned int icc_debug_index = 0;
#endif

typedef struct fz_icc_tag_s fz_icc_tag;

struct fz_icc_tag_s
{
	icTagSignature sig;
	icUInt32Number offset;
	icUInt32Number size;
	unsigned char byte_padding;
};

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

/* Create ICC profile from PDF calGray and calRGB definitions */
fz_buffer *
fz_new_icc_data_from_cal(fz_context *ctx,
	float wp[3],
	float bp[3],
	float gamma[3],
	float matrix[9],
	int n)
{
	fz_icc_tag *tag_list;
	icProfile iccprofile;
	icHeader *header = &(iccprofile.header);
	fz_buffer *profile;
	size_t profile_size;
	int k;
	int num_tags;
	unsigned short encode_gamma;
	int last_tag;
	icS15Fixed16Number temp_XYZ[3];
	int tag_location;
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

	/* Allocate buffer */
	fz_try(ctx)
	{
		profile = fz_new_buffer(ctx, profile_size);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, tag_list);
		fz_rethrow(ctx);
	}

	/* Header */
	header->size = (icUInt32Number)profile_size;
	copy_header(ctx, profile, header);

	/* Tag table */
	copy_tagtable(ctx, profile, tag_list, num_tags);

	/* Common tags */
	add_common_tag_data(ctx, profile, tag_list, desc_name);
	tag_location = ICC_NUMBER_COMMON_TAGS;

	/* Get the cat02 matrix */
	gsicc_create_compute_cam(ctx, wp, cat02);

	/* The matrix */
	if (n == 3)
	{
		float primary[3];

		for (k = 0; k < 3; k++)
		{
			/* Apply the cat02 matrix to the primaries */
			apply_adaption(ctx, cat02, &(matrix[k * 3]), &(primary[0]));
			get_XYZ_doubletr(ctx, temp_XYZ, &(primary[0]));
			add_xyzdata(ctx, profile, temp_XYZ);
			tag_location++;
		}
	}

	/* White and black points. WP is D50 */
	get_D50(ctx, temp_XYZ);
	add_xyzdata(ctx, profile, temp_XYZ);
	tag_location++;

	/* Black point. Apply cat02*/
	apply_adaption(ctx, cat02, bp, &(black_adapt[0]));
	get_XYZ_doubletr(ctx, temp_XYZ, &(black_adapt[0]));
	add_xyzdata(ctx, profile, temp_XYZ);
	tag_location++;

	/* Gamma */
	for (k = 0; k < n; k++)
	{
		encode_gamma = float2u8Fixed8(ctx, gamma[k]);
		add_gammadata(ctx, profile, encode_gamma, icSigCurveType);
		tag_location++;
	}

	fz_free(ctx, tag_list);

#if SAVEICCPROFILE
	if (n == 3)
		save_profile(ctx, profile, "calRGB");
	else
		save_profile(ctx, profile, "calGray");
#endif
	return profile;
}
