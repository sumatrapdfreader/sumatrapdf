// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#ifndef PdbFile_h
#define PdbFile_h

class IDiaSession;

#define logf(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

void log(const char *s);
bool LoadDia();

class PDBFileReader
{
  struct SectionContrib;

  SectionContrib *contribs;
  int nContribs;

  IDiaSession *session;

  const SectionContrib *ContribFromSectionOffset(u32 section, u32 offset);
  void ProcessSymbol(class IDiaSymbol *symbol, DebugInfo &to);
  void ReadEverything(DebugInfo &to);

public:
  bool ReadDebugInfo(char *fileName, DebugInfo &to);
};

#endif
