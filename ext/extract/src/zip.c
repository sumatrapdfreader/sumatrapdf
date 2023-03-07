#include "extract/alloc.h"

#include "mem.h"
#include "outf.h"
#include "zip.h"

#include <zlib.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include "compat_stdint.h"


typedef struct
{
	int16_t  mtime;
	int16_t  mdate;
	int32_t  crc_sum;
	int32_t  size_compressed;
	int32_t  size_uncompressed;
	char    *name;
	uint32_t offset;
	uint16_t attr_internal;
	uint32_t attr_external;
} extract_zip_cd_file_t;

struct extract_zip_t
{
	extract_buffer_t      *buffer;
	extract_zip_cd_file_t *cd_files;
	int                    cd_files_num;

	/* errno_ is set to non-zero if any operation fails; avoids need to check
	after every small output operation. */
	int                    errno_;
	int                    eof;
	uint16_t               compression_method;
	int                    compress_level;

	/* Defaults for various values in zip file headers etc. */
	uint16_t               mtime;
	uint16_t               mdate;
	uint16_t               version_creator;
	uint16_t               version_extract;
	uint16_t               general_purpose_bit_flag;
	uint16_t               file_attr_internal;
	uint32_t               file_attr_external;
	char                  *archive_comment;
};

int extract_zip_open(extract_buffer_t *buffer, extract_zip_t **o_zip)
{
	int              e = -1;
	extract_zip_t   *zip;
	extract_alloc_t *alloc = extract_buffer_alloc(buffer);

	if (extract_malloc(alloc, &zip, sizeof(*zip))) goto end;

	zip->cd_files = NULL;
	zip->cd_files_num = 0;
	zip->buffer = buffer;
	zip->errno_ = 0;
	zip->eof = 0;
	zip->compression_method = Z_DEFLATED;
	zip->compress_level = Z_DEFAULT_COMPRESSION;

	/* We could maybe convert current date/time to the ms-dos format required
	here, but using zeros doesn't seem to make a difference to Word etc. */

	{
		time_t     t = time(NULL);
		struct tm *tm;
		#ifdef _POSIX_SOURCE
			struct tm   tm_local;
			tm = gmtime_r(&t, &tm_local);
		#else
			tm = gmtime(&t);
		#endif
		if (tm)
		{
			/* mdate and mtime are in MS DOS format:
				mtime:
					bits 0-4: seconds / 2.
					bits 5-10: minute (0-59).
					bits 11-15: hour (0-23).
				mdate:
					bits 0-4: day of month (1-31).
					bits 5-8: month (1=jan, 2=feb, etc).
					bits 9-15: year - 1980.
			*/
			zip->mtime = (uint16_t) ((tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2));
			zip->mdate = (uint16_t) (((1900 + tm->tm_year - 1980) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday);
		}
		else
		{
			outf0("*** gmtime_r() failed");
			zip->mtime = 0;
			zip->mdate = 0;
		}
	}

	/* These are all copied from command-line zip on unix. */
	zip->version_creator = (0x3 << 8) + 30; /* 0x3 is unix, 30 means 3.0. */
	zip->version_extract = 10;              /* 10 means 1.0. */
	zip->general_purpose_bit_flag = 0;
	zip->file_attr_internal = 0;

	/* We follow command-line zip which uses 0x81a40000 which is octal
	0100644:0.  (0100644 is S_IFREG (regular file) plus rw-r-r. See stat(2) for
	details.) */
	zip->file_attr_external = (0100644 << 16) + 0;
	if (extract_strdup(alloc, "Artifex", &zip->archive_comment)) goto end;

	e = 0;
end:

	if (e) {
		if (zip) extract_free(alloc, &zip->archive_comment);
		extract_free(alloc, &zip);
		*o_zip = NULL;
	}
	else {
		*o_zip = zip;
	}

	return e;
}

static int s_native_little_endinesss(void)
{
	static const char   a[] = { 1, 2};
	uint16_t b = *(uint16_t*) a;
	if (b == 1 + 2*256) {
		/* Native little-endiness. */
		return 1;
	}
	else if (b == 2 + 1*256) {
		/* Native big-endiness. */
		return 0;
	}
	/* Would like to call abort() here, but that breaks on AIX/gcc. */
	assert(0);
	return 0;
}


/* Allocation fns for zlib. */

static void *s_zalloc(void *opaque, unsigned items, unsigned size)
{
	extract_zip_t   *zip = opaque;
	extract_alloc_t *alloc = extract_buffer_alloc(zip->buffer);
	void            *ptr;

	if (extract_malloc(alloc, &ptr, items*size)) return NULL;

	return ptr;
}

static void s_zfree(void *opaque, void *ptr)
{
	extract_zip_t   *zip = opaque;
	extract_alloc_t *alloc = extract_buffer_alloc(zip->buffer);

	extract_free(alloc, &ptr);
}


/* Uses zlib to write raw deflate compressed data to zip->buffer. */
static int
s_write_compressed(
		extract_zip_t *zip,
		const void    *data,
		size_t         data_length,
		size_t        *o_compressed_length)
{
	int      ze;
	z_stream zstream = {0};  /* Initialise to keep Coverity quiet. */

	if (zip->errno_)    return -1;
	if (zip->eof)       return +1;

	zstream.zalloc = s_zalloc;
	zstream.zfree = s_zfree;
	zstream.opaque = zip;

	/* We need to write raw deflate data, so we use deflateInit2() with -ve
	windowBits. The values we use are deflateInit()'s defaults. */
	ze = deflateInit2(&zstream,
			zip->compress_level,
			Z_DEFLATED,
			-15 /*windowBits*/,
			8 /*memLevel*/,
			Z_DEFAULT_STRATEGY);
	if (ze != Z_OK)
	{
		errno = (ze == Z_MEM_ERROR) ? ENOMEM : EINVAL;
		zip->errno_ = errno;
		outf("deflateInit2() failed ze=%i", ze);
		return -1;
	}

	/* Set zstream to read from specified data. */
	zstream.next_in = (void*) data;
	zstream.avail_in = (unsigned) data_length;

	/* We increment *o_compressed_length gradually so that if we return an
	error, we still indicate how many butes of compressed data have been
	written. */
	if (o_compressed_length)
	{
		*o_compressed_length = 0;
	}

	for(;;)
	{
		/* todo: write an extract_buffer_cache() function so we can write
		directly into output buffer if it has a fn_cache. */
		unsigned char   buffer[1024];
		zstream.next_out = &buffer[0];
		zstream.avail_out = sizeof(buffer);
		ze = deflate(&zstream, zstream.avail_in ? Z_NO_FLUSH : Z_FINISH);
		if (ze != Z_STREAM_END && ze != Z_OK)
		{
			outf("deflate() failed ze=%i", ze);
			errno = EIO;
			zip->errno_ = errno;
			return -1;
		}
		{
			/* Send the new compressed data to buffer. */
			size_t  bytes_written;
			int e = extract_buffer_write(zip->buffer, buffer, zstream.next_out - buffer, &bytes_written);
			if (o_compressed_length)
			{
				*o_compressed_length += bytes_written;
			}
			if (e)
			{
				if (e == -1)    zip->errno_ = errno;
				if (e ==  +1)   zip->eof = 1;
				outf("extract_buffer_write() failed e=%i errno=%i", e, errno);
				return e;
			}
		}
		if (ze == Z_STREAM_END)
		{
			break;
		}
	}
	ze = deflateEnd(&zstream);
	if (ze != Z_OK)
	{
		outf("deflateEnd() failed ze=%i", ze);
		errno = EIO;
		zip->errno_ = errno;
		return -1;
	}
	if (o_compressed_length)
	{
		assert(*o_compressed_length == (size_t) zstream.total_out);
	}

	return 0;
}

/* Writes uncompressed data to zip->buffer. */
static int s_write(extract_zip_t *zip, const void *data, size_t data_length)
{
	size_t actual;
	int e;

	if (zip->errno_)    return -1;
	if (zip->eof)       return +1;

	e = extract_buffer_write(zip->buffer, data, data_length, &actual);
	if (e == -1)    zip->errno_ = errno;
	if (e == +1)    zip->eof = 1;

	return e;
}

static int s_write_uint32(extract_zip_t *zip, uint32_t value)
{
	if (s_native_little_endinesss()) {
		return s_write(zip, &value, sizeof(value));
	}
	else {
		unsigned char value2[4] = {
				(unsigned char) (value >> 0),
				(unsigned char) (value >> 8),
				(unsigned char) (value >> 16),
				(unsigned char) (value >> 24)
				};
		return s_write(zip, &value2, sizeof(value2));
	}
}

static int s_write_uint16(extract_zip_t *zip, uint16_t value)
{
	if (s_native_little_endinesss()) {
		return s_write(zip, &value, sizeof(value));
	}
	else {
		unsigned char value2[2] = {
				(unsigned char) (value >> 0),
				(unsigned char) (value >> 8)
				};
		return s_write(zip, &value2, sizeof(value2));
	}
}

static int s_write_string(extract_zip_t *zip, const char *text)
{
	return s_write(zip, text, strlen(text));
}


int extract_zip_write_file(
		extract_zip_t *zip,
		const void    *data,
		size_t         data_length,
		const char    *name)
{
	int                    e = -1;
	extract_zip_cd_file_t *cd_file = NULL;
	extract_alloc_t       *alloc = extract_buffer_alloc(zip->buffer);

	if (data_length > INT_MAX) {
		assert(0);
		errno = EINVAL;
		return -1;
	}
	/* Create central directory file header for later. */
	if (extract_realloc2(
			alloc,
			&zip->cd_files,
			sizeof(extract_zip_cd_file_t) * zip->cd_files_num,
			sizeof(extract_zip_cd_file_t) * (zip->cd_files_num+1)
			)) goto end;
	cd_file = &zip->cd_files[zip->cd_files_num];
	cd_file->name = NULL;

	cd_file->mtime = zip->mtime;
	cd_file->mdate = zip->mdate;
	cd_file->crc_sum = (int32_t) crc32(crc32(0, NULL, 0), data, (int) data_length);
	cd_file->size_uncompressed = (int) data_length;
	if (zip->compression_method == 0)
	{
		cd_file->size_compressed = cd_file->size_uncompressed;
	}
	if (extract_strdup(alloc, name, &cd_file->name)) goto end;
	cd_file->offset = (int) extract_buffer_pos(zip->buffer);
	cd_file->attr_internal = zip->file_attr_internal;
	cd_file->attr_external = zip->file_attr_external;
	if (!cd_file->name) goto end;

	/* Write local file header. If we are using compression, we set bit 3 of
	General purpose bit flag and write zeros for crc-32, compressed size and
	uncompressed size; then we write the actual values in data descriptor after
	the compressed data. */
	{
		const char extra_local[] = "";  /* Modify for testing. */
		uint16_t general_purpose_bit_flag = zip->general_purpose_bit_flag;
		if (zip->compression_method)    general_purpose_bit_flag |= 8;
		s_write_uint32(zip, 0x04034b50);
		s_write_uint16(zip, zip->version_extract);          /* Version needed to extract (minimum). */
		s_write_uint16(zip, general_purpose_bit_flag);      /* General purpose bit flag */
		s_write_uint16(zip, zip->compression_method);       /* Compression method */
		s_write_uint16(zip, cd_file->mtime);                /* File last modification time */
		s_write_uint16(zip, cd_file->mdate);                /* File last modification date */
		if (zip->compression_method)
		{
			s_write_uint32(zip, 0);                         /* CRC-32 of uncompressed data */
			s_write_uint32(zip, 0);                         /* Compressed size */
			s_write_uint32(zip, 0);                         /* Uncompressed size */
		}
		else
		{
			s_write_uint32(zip, cd_file->crc_sum);          /* CRC-32 of uncompressed data */
			s_write_uint32(zip, cd_file->size_compressed);  /* Compressed size */
			s_write_uint32(zip, cd_file->size_uncompressed);/* Uncompressed size */
		}
		s_write_uint16(zip, (uint16_t) strlen(name));       /* File name length (n) */
		s_write_uint16(zip, sizeof(extra_local)-1);         /* Extra field length (m) */
		s_write_string(zip, cd_file->name);                 /* File name */
		s_write(zip, extra_local, sizeof(extra_local)-1);   /* Extra field */
	}

	if (zip->compression_method)
	{
		/* Write compressed data. */
		size_t  data_length_compressed;
		s_write_compressed(zip, data, data_length, &data_length_compressed);
		cd_file->size_compressed = (int) data_length_compressed;

		/* Write data descriptor. */
		s_write_uint32(zip, 0x08074b50);                    /* Data descriptor signature */
		s_write_uint32(zip, cd_file->crc_sum);              /* CRC-32 of uncompressed data */
		s_write_uint32(zip, cd_file->size_compressed);      /* Compressed size */
		s_write_uint32(zip, cd_file->size_uncompressed);    /* Uncompressed size */
	}
	else
	{
		s_write(zip, data, data_length);
	}

	if (zip->errno_)    e = -1;
	else if (zip->eof)  e = +1;
	else e = 0;


end:

	if (e) {
		/* Leave zip->cd_files_num unchanged, so calling extract_zip_close()
		will write out any earlier files. Free cd_file->name to avoid leak. */
		if (cd_file) extract_free(alloc, &cd_file->name);
	}
	else {
		/* cd_files[zip->cd_files_num] is valid. */
		zip->cd_files_num += 1;
	}

	return e;
}

int extract_zip_close(extract_zip_t **pzip)
{
	int              e = -1;
	size_t           pos;
	size_t           len;
	int              i;
	extract_zip_t   *zip = *pzip;
	extract_alloc_t *alloc;

	if (!zip) {
		return 0;
	}
	alloc = extract_buffer_alloc(zip->buffer);
	pos = extract_buffer_pos(zip->buffer);
	len = 0;

	/* Write Central directory file headers, freeing data as we go. */
	for (i=0; i<zip->cd_files_num; ++i) {
		const char extra[] = "";
		size_t pos2 = extract_buffer_pos(zip->buffer);
		extract_zip_cd_file_t* cd_file = &zip->cd_files[i];
		s_write_uint32(zip, 0x02014b50);
		s_write_uint16(zip, zip->version_creator);              /* Version made by, copied from command-line zip. */
		s_write_uint16(zip, zip->version_extract);              /* Version needed to extract (minimum). */
		s_write_uint16(zip, zip->general_purpose_bit_flag);     /* General purpose bit flag */
		s_write_uint16(zip, zip->compression_method);           /* Compression method */
		s_write_uint16(zip, cd_file->mtime);                    /* File last modification time */
		s_write_uint16(zip, cd_file->mdate);                    /* File last modification date */
		s_write_uint32(zip, cd_file->crc_sum);                  /* CRC-32 of uncompressed data */
		s_write_uint32(zip, cd_file->size_compressed);          /* Compressed size */
		s_write_uint32(zip, cd_file->size_uncompressed);        /* Uncompressed size */
		s_write_uint16(zip, (uint16_t) strlen(cd_file->name));  /* File name length (n) */
		s_write_uint16(zip, sizeof(extra)-1);                   /* Extra field length (m) */
		s_write_uint16(zip, 0);                                 /* File comment length (k) */
		s_write_uint16(zip, 0);                                 /* Disk number where file starts */
		s_write_uint16(zip, cd_file->attr_internal);            /* Internal file attributes */
		s_write_uint32(zip, cd_file->attr_external);            /* External file attributes. */
		s_write_uint32(zip, cd_file->offset);                   /* Offset of local file header. */
		s_write_string(zip, cd_file->name);                     /* File name */
		s_write(zip, extra, sizeof(extra)-1);                   /* Extra field */
		len += extract_buffer_pos(zip->buffer) - pos2;
		extract_free(alloc, &cd_file->name);
	}
	extract_free(alloc, &zip->cd_files);

	/* Write End of central directory record. */
	s_write_uint32(zip, 0x06054b50);
	s_write_uint16(zip, 0);                             /* Number of this disk */
	s_write_uint16(zip, 0);                             /* Disk where central directory starts */
	s_write_uint16(zip, (uint16_t) zip->cd_files_num);  /* Number of central directory records on this disk */
	s_write_uint16(zip, (uint16_t) zip->cd_files_num);  /* Total number of central directory records */
	s_write_uint32(zip, (int) len);                     /* Size of central directory (bytes) */
	s_write_uint32(zip, (int) pos);                     /* Offset of start of central directory, relative to start of archive */

	s_write_uint16(zip, (uint16_t) strlen(zip->archive_comment));  /* Comment length (n) */
	s_write_string(zip, zip->archive_comment);
	extract_free(alloc, &zip->archive_comment);

	if (zip->errno_)    e = -1;
	else if (zip->eof)  e = +1;
	else e = 0;

	extract_free(alloc, pzip);

	return e;
}
