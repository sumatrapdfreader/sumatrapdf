// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#ifndef PdbFile_h
#define PdbFile_h

enum PdbProcessingOptions {
    DUMP_SECTIONS = 1 << 0,
    DUMP_FUNCTIONS = 1 << 1,
};

void ProcessPdbFile(const char *fileNameA, PdbProcessingOptions options);

#endif
