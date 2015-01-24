/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

/* demonstration of most of the public unarr API:
   parses and decompresses an archive into memory (integrity test) */

#include "unarr.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#if !defined(NDEBUG) && defined(_MSC_VER)
#include <windows.h>
#include <crtdbg.h>
#endif

ar_archive *ar_open_any_archive(ar_stream *stream, const char *fileext)
{
    ar_archive *ar = ar_open_rar_archive(stream);
    if (!ar)
        ar = ar_open_zip_archive(stream, fileext && (strcmp(fileext, ".xps") == 0 || strcmp(fileext, ".epub") == 0));
    if (!ar)
        ar = ar_open_7z_archive(stream);
    if (!ar)
        ar = ar_open_tar_archive(stream);
    return ar;
}

#define FailIf(cond, msg, ...) if (cond) { fprintf(stderr, msg "\n", __VA_ARGS__); goto CleanUp; } error_step++

int main(int argc, char *argv[])
{
    ar_stream *stream = NULL;
    ar_archive *ar = NULL;
    int entry_count = 1;
    int entry_skips = 0;
    int error_step = 1;

#if !defined(NDEBUG) && defined(_MSC_VER)
    if (!IsDebuggerPresent()) {
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    }
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    FailIf(argc != 2, "Syntax: %s <filename.ext>", argv[0]);

    stream = ar_open_file(argv[1]);
    FailIf(!stream, "Error: File \"%s\" not found!", argv[1]);

    printf("Parsing \"%s\":\n", argv[1]);
    ar = ar_open_any_archive(stream, strrchr(argv[1], '.'));
    FailIf(!ar, "Error: No valid %s archive!", "RAR, ZIP, 7Z or TAR");

    while (ar_parse_entry(ar)) {
        size_t size = ar_entry_get_size(ar);
        printf("%02d. %s (@%" PRIi64 ")\n", entry_count++, ar_entry_get_name(ar), ar_entry_get_offset(ar));
        while (size > 0) {
            unsigned char buffer[1024];
            size_t count = size < sizeof(buffer) ? size : sizeof(buffer);
            if (!ar_entry_uncompress(ar, buffer, count))
                break;
            size -= count;
        }
        if (size > 0) {
            fprintf(stderr, "Warning: Failed to uncompress... skipping\n");
            entry_skips++;
        }
    }
    FailIf(!ar_at_eof(ar), "Error: Failed to parse entry %d!", entry_count);
    error_step = entry_skips > 0 ? 1000 + entry_skips : 0;

CleanUp:
    ar_close_archive(ar);
    ar_close(stream);
    return error_step;
}
