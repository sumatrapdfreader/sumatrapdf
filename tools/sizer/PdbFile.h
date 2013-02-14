// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#ifndef PdbFile_h
#define PdbFile_h

struct ProcessFlags {
};

void ProcessPdbFile(const char *fileNameA, ProcessFlags *flags);

#endif
