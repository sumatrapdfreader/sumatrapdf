// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "BaseUtil.h"
#include "Dict.h"
#include "Dia2Subset.h"

#include "Util.h"

#include "PdbFile.h"

int main(int argc, char** argv)
{
    ScopedCom comInitializer;

    if (argc < 2) {
        log("Usage: sizer <exefile>\n");
        return 1;
    }

    const char *fileName = argv[1];
    fprintf( stderr, "Reading debug info file %s ...\n", fileName);
    ProcessPdbFile(fileName, NULL);

    return 0;
}
