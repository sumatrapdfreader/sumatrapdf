/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "unarr.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#if defined(DEBUG) && defined(_MSC_VER)
#include <windows.h>
#include <crtdbg.h>
#endif

#define FailIf(cond, msg, ...) if (cond) { fprintf(stderr, msg "\n", __VA_ARGS__); goto CleanUp; } step++

int main(int argc, char *argv[])
{
    ar_stream *stream = NULL;
    ar_archive *ar = NULL;
    int count = 1;
    int step = 1;

#if defined(DEBUG) && defined(_MSC_VER)
    if (!IsDebuggerPresent()) {
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    }
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    FailIf(argc < 2, "Syntax: %s <filename.ext>", argv[0]);

    stream = ar_open_file(argv[1]);
    FailIf(!stream, "Error: File \"%s\" not found!", argv[1]);

    ar = ar_open_rar_archive(stream);
    if (!ar)
        ar = ar_open_zip_archive(stream, strstr(argv[1], ".xps") || strstr(argv[1], ".epub"));
    FailIf(!ar, "Error: File \"%s\" is no valid RAR or ZIP archive!", argv[1]);

    printf("Parsing \"%s\":\n", argv[1]);
    while (ar_parse_entry(ar)) {
        size_t size = ar_entry_get_size(ar);
        printf("%02d. %s (@%" PRIi64 ")\n", count++, ar_entry_get_name(ar), ar_entry_get_offset(ar));
        while (size > 0) {
            unsigned char buffer[1024];
            size_t count = size < sizeof(buffer) ? size : sizeof(buffer);
            if (!ar_entry_uncompress(ar, buffer, count))
                break;
            size -= count;
        }
        if (size > 0)
            fprintf(stderr, "Warning: Failed to uncompress... skipping\n");
    }
    FailIf(!ar_at_eof(ar), "Error: Failed to parse entry %d!", count);
    step = 0;

CleanUp:
    ar_close_archive(ar);
    ar_close(stream);
    return step;
}
