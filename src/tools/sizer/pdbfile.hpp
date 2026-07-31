// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#ifndef __PDBFILE_HPP_
#define __PDBFILE_HPP_

#include "debuginfo.hpp"

/****************************************************************************/

class IDiaSession;

class PDBFileReader : public DebugInfoReader
{
    struct SectionContrib;

    SectionContrib *Contribs;
    sInt nContribs;

    IDiaSession *Session;

    const SectionContrib *ContribFromSectionOffset(sU32 section, sU32 offset);
    void ProcessSymbol(class IDiaSymbol *symbol, DebugInfo &to);
    void ReadEverything(DebugInfo &to);

public:
    sBool ReadDebugInfo(const sChar *fileName, DebugInfo &to);
};

/****************************************************************************/

#endif
