// Stands in for caj2pdf in tests/issue-6455.ts: the test's .caj is "CAJ\0" + a PDF.
// Usage: fake-caj2pdf <input> -o <output> [flags]
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 4 || strcmp(argv[2], "-o") != 0) {
        return 2;
    }
    FILE* in = fopen(argv[1], "rb");
    FILE* out = fopen(argv[3], "wb");
    if (!in || !out || fseek(in, 4, SEEK_SET) != 0) {
        return 1;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    fclose(out);
    return 0;
}
