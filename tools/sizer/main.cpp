// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#include "pdbfile.hpp"
#include "parg.h"
#include <cstdio>
#include <ctime>

static void print_help()
{
    DebugFilters def;
    fprintf(stderr, "Usage: Sizer [options] exefile\n");
    fprintf(stderr, " -n str  or --name=str           Only include things containing 'str' into report\n");
    fprintf(stderr, " -a size or --all                Include all symbols, same as --min=0\n");
    fprintf(stderr, " -m size or --min=size           Minimum size for anything to be reported (default varies, see below)\n");
    fprintf(stderr, " -f size or --funcmin=size       Minimum size for functions to be reported (default %.1f)\n", def.minFunction / 1024.0);
    fprintf(stderr, " -d size or --datamin=size       Minimum size for data to be reported (default %.1f)\n", def.minData / 1024.0);
    fprintf(stderr, " -c size or --classmin=size      Minimum size for class to be reported (default %.1f)\n", def.minClass / 1024.0);
    fprintf(stderr, " -F size or --filemin=size       Minimum size for file to be reported (default %.1f)\n", def.minFile / 1024.0);
    fprintf(stderr, " -t size or --templatemin=size   Minimum size for template to be reported (default %.1f)\n", def.minTemplate / 1024.0);
    fprintf(stderr, " -T cnt  or --templatecount=cnt  Minimum instantiation count for template to be reported (default %i)\n", def.minTemplateCount);
    fprintf(stderr, " -h or --help                    Print this help\n");
}

static bool parse_cmdline(int argc,char * const * argv, DebugFilters& outFilters, std::string& outFile)
{
    parg_state args;
    parg_init(&args);

    static const struct parg_option argsTable[] =
    {
        { "name", PARG_REQARG, NULL, 'n' },
        { "all", PARG_NOARG, NULL, 'a' },
        { "min", PARG_REQARG, NULL, 'm' },
        { "funcmin", PARG_REQARG, NULL, 'f' },
        { "datamin", PARG_REQARG, NULL, 'd' },
        { "classmin", PARG_REQARG, NULL, 'c' },
        { "filemin", PARG_REQARG, NULL, 'F' },
        { "templatemin", PARG_REQARG, NULL, 't' },
        { "templatecount", PARG_REQARG, NULL, 'T' },
        { "help", PARG_NOARG, NULL, 'h' },
        { 0, 0, 0, 0 }
    };

    int c;
    while ((c = parg_getopt_long(&args, argc, argv, "an:m:f:d:c:F:t:T:h", argsTable, NULL)) != -1)
    {
        switch (c)
        {
        case 1: outFile = args.optarg; break;
        case 'n': outFilters.name = args.optarg; break;
        case 'a': outFilters.SetMinSize(0); break;
        case 'm': outFilters.SetMinSize(atof(args.optarg) * 1024); break;
        case 'f': outFilters.minFunction = atof(args.optarg) * 1024; break;
        case 'd': outFilters.minData = atof(args.optarg) * 1024; break;
        case 'c': outFilters.minClass = atof(args.optarg) * 1024; break;
        case 'F': outFilters.minFile = atof(args.optarg) * 1024; break;
        case 't': outFilters.minTemplate = atof(args.optarg) * 1024; break;
        case 'T': outFilters.minTemplateCount = atoi(args.optarg); break;
        case '?':
            fprintf(stderr, "Unknown argument or missing value for '%c'\n", args.optopt);
            // fall through
        case 'h':
        default:
            print_help();
            return false;
        }
    }

    if (outFile.empty())
    {
        print_help();
        return false;
    }

    return true;
}

int main(int argc, char * const * argv)
{
    DebugFilters filters;
    std::string file;
    if (!parse_cmdline(argc, argv, filters, file))
    {
        return 0;
    }

    DebugInfo info;

    clock_t time1 = clock();

    info.Init();
    PDBFileReader pdb;
    fprintf(stderr, "Reading debug info for %s ...\n", file.c_str());
    bool pdbok = pdb.ReadDebugInfo(file.c_str(), info);
    if (!pdbok)
    {
        fprintf(stderr, "ERROR reading file via PDB\n");
        return 1;
    }
    fprintf(stderr, "\nProcessing info...\n");
    info.FinishedReading();
    info.StartAnalyze();
    info.FinishAnalyze();

    fprintf(stderr, "Generating report...\n");
    std::string report = info.WriteReport(filters);

    clock_t time2 = clock();
    float secs = float(time2 - time1) / CLOCKS_PER_SEC;

    fprintf(stderr, "Printing...\n");
    puts(report.c_str());
    fprintf(stderr, "Done in %.2f seconds!\n", secs);


    return 0;
}
