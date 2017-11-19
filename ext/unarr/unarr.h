/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#ifndef unarr_h
#define unarr_h

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
typedef int64_t off64_t;
typedef int64_t time64_t;

#define UNARR_API_VERSION 100

/***** common/stream *****/

typedef struct ar_stream_s ar_stream;

/* opens a read-only stream for the given FILE; returns NULL on error */
ar_stream *ar_open(FILE *f);
/* opens a read-only stream for the given file path; returns NULL on error */
ar_stream *ar_open_file(const char *path);
#ifdef _WIN32
ar_stream *ar_open_file_w(const wchar_t *path);
#endif
/* opens a read-only stream for the given chunk of memory; the pointer must be valid until ar_close is called */
ar_stream *ar_open_memory(const void *data, size_t datalen);
#ifdef _WIN32
typedef struct IStream IStream;
/* opens a read-only stream based on the given IStream */
ar_stream *ar_open_istream(IStream *stream);
#endif

/* closes the stream and releases underlying resources */
void ar_close(ar_stream *stream);
/* tries to read 'count' bytes into buffer, advancing the read offset pointer; returns the actual number of bytes read */
size_t ar_read(ar_stream *stream, void *buffer, size_t count);
/* moves the read offset pointer (same as fseek); returns false on failure */
bool ar_seek(ar_stream *stream, off64_t offset, int origin);
/* shortcut for ar_seek(stream, count, SEEK_CUR); returns false on failure */
bool ar_skip(ar_stream *stream, off64_t count);
/* returns the current read offset (or 0 on error) */
off64_t ar_tell(ar_stream *stream);

/***** common/unarr *****/

typedef struct ar_archive_s ar_archive;

/* frees all data stored for the given archive; does not close the underlying stream */
void ar_close_archive(ar_archive *ar);
/* reads the next archive entry; returns false on error or at the end of the file (use ar_at_eof to distinguish the two cases) */
bool ar_parse_entry(ar_archive *ar);
/* reads the archive entry at the given offset as returned by ar_entry_get_offset (offset 0 always restarts at the first entry); should always succeed */
bool ar_parse_entry_at(ar_archive *ar, off64_t offset);
/* reads the (first) archive entry associated with the given name; returns false if the entry couldn't be found */
bool ar_parse_entry_for(ar_archive *ar, const char *entry_name);
/* returns whether the last ar_parse_entry call has reached the file's expected end */
bool ar_at_eof(ar_archive *ar);

/* returns the name of the current entry as UTF-8 string; this pointer is only valid until the next call to ar_parse_entry; returns NULL on failure */
const char *ar_entry_get_name(ar_archive *ar);
/* returns the stream offset of the current entry for use with ar_parse_entry_at */
off64_t ar_entry_get_offset(ar_archive *ar);
/* returns the total size of uncompressed data of the current entry; read exactly that many bytes using ar_entry_uncompress */
size_t ar_entry_get_size(ar_archive *ar);
/* returns the stored modification date of the current entry in 100ns since 1601/01/01 */
time64_t ar_entry_get_filetime(ar_archive *ar);
/* WARNING: don't manually seek in the stream between ar_parse_entry and the last corresponding ar_entry_uncompress call! */
/* uncompresses the next 'count' bytes of the current entry into buffer; returns false on error */
bool ar_entry_uncompress(ar_archive *ar, void *buffer, size_t count);

/* copies at most 'count' bytes of the archive's global comment (if any) into buffer; returns the actual amout of bytes copied (or, if 'buffer' is NULL, the required buffer size) */
size_t ar_get_global_comment(ar_archive *ar, void *buffer, size_t count);

/***** rar/rar *****/

/* checks whether 'stream' could contain RAR data and prepares for archive listing/extraction; returns NULL on failure */
ar_archive *ar_open_rar_archive(ar_stream *stream);

/***** tar/tar *****/

/* checks whether 'stream' could contain TAR data and prepares for archive listing/extraction; returns NULL on failure */
ar_archive *ar_open_tar_archive(ar_stream *stream);

/***** zip/zip *****/

/* checks whether 'stream' could contain ZIP data and prepares for archive listing/extraction; returns NULL on failure */
/* set deflatedonly for extracting XPS, EPUB, etc. documents where non-Deflate compression methods are not supported by specification */
ar_archive *ar_open_zip_archive(ar_stream *stream, bool deflatedonly);

/***** _7z/_7z *****/

/* checks whether 'stream' could contain 7Z data and prepares for archive listing/extraction; returns NULL on failure */
ar_archive *ar_open_7z_archive(ar_stream *stream);

#endif
