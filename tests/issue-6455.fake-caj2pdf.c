// Stands in for caj2pdf in tests/issue-6455.ts: the test's .caj is "CAJ\0" + a PDF,
// or "CAJ\0EXIT<n>" to exit with n without writing anything.
// Usage: fake-caj2pdf <input> -o <output> [flags]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 4 || strcmp(argv[2], "-o") != 0) {
        return 2;
    }
    FILE* in = fopen(argv[1], "rb");
    if (!in || fseek(in, 4, SEEK_SET) != 0) {
        return 1;
    }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, in);
    buf[n] = 0;
    if (strncmp(buf, "EXIT", 4) == 0) {
        return (int)strtoul(buf + 4, NULL, 10);
    }
    FILE* out = fopen(argv[3], "wb");
    if (!out || fseek(in, 4, SEEK_SET) != 0) {
        return 1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return 0;
}
