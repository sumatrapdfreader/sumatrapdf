// Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
// License: GPLv3
//
// ext/a-libarchive/libarchive.c is amalgamated from a Windows build, so it
// carries archive_windows.c and filter_fork_windows.c but not their POSIX
// siblings. These are the entry points the amalgamation references and the
// missing files would have defined. None of them is on a path the port uses:
// the "program" filter runs an external decompressor (lzop, lrzip, grzip) and
// the disk reader walks a directory to create an archive.

#include <sys/types.h>
#include <archive.h>

int __archive_create_child(const char* cmd, int* child_stdin, int* child_stdout, pid_t* out_child);
void __archive_check_child(int in, int out);

int __archive_create_child(const char* cmd, int* child_stdin, int* child_stdout, pid_t* out_child) {
    (void)cmd;
    (void)child_stdin;
    (void)child_stdout;
    (void)out_child;
    return ARCHIVE_FAILED;
}

void __archive_check_child(int in, int out) {
    (void)in;
    (void)out;
}

int archive_read_disk_set_gname_lookup(struct archive* a, void* data, const char* (*lookup)(void*, la_int64_t),
                                       void (*cleanup)(void*)) {
    (void)a;
    if (cleanup && data) {
        cleanup(data);
    }
    (void)lookup;
    return ARCHIVE_FATAL;
}

int archive_read_disk_set_uname_lookup(struct archive* a, void* data, const char* (*lookup)(void*, la_int64_t),
                                       void (*cleanup)(void*)) {
    (void)a;
    if (cleanup && data) {
        cleanup(data);
    }
    (void)lookup;
    return ARCHIVE_FATAL;
}
