// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#ifndef __PDBFILE_HPP_
#define __PDBFILE_HPP_

#include <Windows.h>
#include "debuginfo.hpp"

class IDiaSession;

#define logf(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)

#define dimof(x) (sizeof(x) / sizeof(x[0]))

void log(const char *s);
bool LoadDia();

class PDBFileReader : public DebugInfoReader
{
  struct SectionContrib;

  SectionContrib *contribs;
  int nContribs;

  IDiaSession *session;

  const SectionContrib *ContribFromSectionOffset(u32 section, u32 offset);
  void ProcessSymbol(class IDiaSymbol *symbol, DebugInfo &to);
  void ReadEverything(DebugInfo &to);

public:
  virtual bool ReadDebugInfo(char *fileName, DebugInfo &to);
};

struct ComInitializer
{
    ComInitializer() {
        if(FAILED(CoInitialize(0)))
        {
            log("  failed to initialize COM\n");
            exit(1);
        }
    };
    ~ComInitializer() {
        CoUninitialize();
    }
};


#endif
