// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "BaseUtil.h"
#include "Dict.h"

#include <vector>
#include <map>
#include <cstdio>
#include <ctime>

#include "Util.h"

#include "DebugInfo.h"
#include "PdbFile.h"

int main(int argc, char** argv)
{
    DebugInfo * di;

    ScopedCom comInitializer;

    if (argc < 2) {
        log("Usage: sizer <exefile>\n");
        return 1;
    }

    fprintf( stderr, "Reading debug info file %s ...\n", argv[1] );
    di = ReadPdbFile(argv[1]);
    if (!di) {
        log("ERROR reading file via PDB\n");
        return 1;
    }
    log("\nProcessing info...\n");
    di->FinishedReading();
    di->StartAnalyze();
    di->FinishAnalyze();

    log("Generating report...\n");
    std::string report = di->WriteReport();
    log("Printing...\n");
    puts(report.c_str());

#if 0
    DebugInfo info;

    clock_t time1 = clock();

    PDBFileReader pdb;
    fprintf( stderr, "Reading debug info file %s ...\n", argv[1] );
    bool pdbok = pdb.ReadDebugInfo( argv[1], info );
    if( !pdbok ) {
        log("ERROR reading file via PDB\n");
        return 1;
    }
    log("\nProcessing info...\n");
    info.FinishedReading();
    info.StartAnalyze();
    info.FinishAnalyze();

    log("Generating report...\n");
    std::string report = info.WriteReport();

    clock_t time2 = clock();
    float secs = float(time2-time1) / CLOCKS_PER_SEC;

    log("Printing...\n");
    puts( report.c_str() );
    fprintf( stderr, "Done in %.2f seconds!\n", secs );
#endif

    return 0;
}
