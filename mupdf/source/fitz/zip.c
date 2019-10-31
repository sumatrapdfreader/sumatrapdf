#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <string.h>

#include <zlib.h>

#if !defined (INT32_MAX)
#define INT32_MAX 2147483647L
#endif

#define ZIP_LOCAL_FILE_SIG 0x04034b50
#define ZIP_CENTRAL_DIRECTORY_SIG 0x02014b50
#define ZIP_END_OF_CENTRAL_DIRECTORY_SIG 0x06054b50

struct fz_zip_writer_s
{
	fz_output *output;
	fz_buffer *central;
	int count;
	int closed;
};

void
fz_write_zip_entry(fz_context *ctx, fz_zip_writer *zip, const char *name, fz_buffer *buf, int compress)
{
	int offset = fz_tell_output(ctx, zip->output);
	int sum;

	sum = crc32(0, NULL, 0);
	sum = crc32(sum, buf->data, (uInt)buf->len);

	fz_append_int32_le(ctx, zip->central, ZIP_CENTRAL_DIRECTORY_SIG);
	fz_append_int16_le(ctx, zip->central, 0); /* version made by: MS-DOS */
	fz_append_int16_le(ctx, zip->central, 20); /* version to extract: 2.0 */
	fz_append_int16_le(ctx, zip->central, 0); /* general purpose bit flag */
	fz_append_int16_le(ctx, zip->central, 0); /* compression method: store */
	fz_append_int16_le(ctx, zip->central, 0); /* TODO: last mod file time */
	fz_append_int16_le(ctx, zip->central, 0); /* TODO: last mod file date */
	fz_append_int32_le(ctx, zip->central, sum); /* crc-32 */
	fz_append_int32_le(ctx, zip->central, (int)buf->len); /* csize */
	fz_append_int32_le(ctx, zip->central, (int)buf->len); /* usize */
	fz_append_int16_le(ctx, zip->central, (int)strlen(name)); /* file name length */
	fz_append_int16_le(ctx, zip->central, 0); /* extra field length */
	fz_append_int16_le(ctx, zip->central, 0); /* file comment length */
	fz_append_int16_le(ctx, zip->central, 0); /* disk number start */
	fz_append_int16_le(ctx, zip->central, 0); /* internal file attributes */
	fz_append_int32_le(ctx, zip->central, 0); /* external file attributes */
	fz_append_int32_le(ctx, zip->central, offset); /* relative offset of local header */
	fz_append_string(ctx, zip->central, name);

	fz_write_int32_le(ctx, zip->output, ZIP_LOCAL_FILE_SIG);
	fz_write_int16_le(ctx, zip->output, 20); /* version to extract: 2.0 */
	fz_write_int16_le(ctx, zip->output, 0); /* general purpose bit flag */
	fz_write_int16_le(ctx, zip->output, 0); /* compression method: store */
	fz_write_int16_le(ctx, zip->output, 0); /* TODO: last mod file time */
	fz_write_int16_le(ctx, zip->output, 0); /* TODO: last mod file date */
	fz_write_int32_le(ctx, zip->output, sum); /* crc-32 */
	fz_write_int32_le(ctx, zip->output, (int)buf->len); /* csize */
	fz_write_int32_le(ctx, zip->output, (int)buf->len); /* usize */
	fz_write_int16_le(ctx, zip->output, (int)strlen(name)); /* file name length */
	fz_write_int16_le(ctx, zip->output, 0); /* extra field length */
	fz_write_data(ctx, zip->output, name, strlen(name));
	fz_write_data(ctx, zip->output, buf->data, buf->len);

	++zip->count;
}

void
fz_close_zip_writer(fz_context *ctx, fz_zip_writer *zip)
{
	int64_t offset = fz_tell_output(ctx, zip->output);

	fz_write_data(ctx, zip->output, zip->central->data, zip->central->len);

	fz_write_int32_le(ctx, zip->output, ZIP_END_OF_CENTRAL_DIRECTORY_SIG);
	fz_write_int16_le(ctx, zip->output, 0); /* number of this disk */
	fz_write_int16_le(ctx, zip->output, 0); /* number of disk where central directory starts */
	fz_write_int16_le(ctx, zip->output, zip->count); /* entries in central directory in this disk */
	fz_write_int16_le(ctx, zip->output, zip->count); /* entries in central directory in total */
	fz_write_int32_le(ctx, zip->output, (int)zip->central->len); /* size of the central directory */
	fz_write_int32_le(ctx, zip->output, (int)offset); /* offset of the central directory */
	fz_write_int16_le(ctx, zip->output, 5); /* zip file comment length */

	fz_write_data(ctx, zip->output, "MuPDF", 5);

	fz_close_output(ctx, zip->output);

	zip->closed = 1;
}

void
fz_drop_zip_writer(fz_context *ctx, fz_zip_writer *zip)
{
	if (!zip)
		return;
	if (!zip->closed)
		fz_warn(ctx, "dropping unclosed zip writer");
	fz_drop_output(ctx, zip->output);
	fz_drop_buffer(ctx, zip->central);
	fz_free(ctx, zip);
}

fz_zip_writer *
fz_new_zip_writer_with_output(fz_context *ctx, fz_output *out)
{
	fz_zip_writer *zip = fz_malloc_struct(ctx, fz_zip_writer);
	fz_try(ctx)
	{
		zip->output = out;
		zip->central = fz_new_buffer(ctx, 0);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, zip->central);
		fz_free(ctx, zip);
		fz_rethrow(ctx);
	}
	return zip;
}

fz_zip_writer *
fz_new_zip_writer(fz_context *ctx, const char *filename)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, 0);
	fz_zip_writer *zip = NULL;
	fz_try(ctx)
		zip = fz_new_zip_writer_with_output(ctx, out);
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return zip;
}
