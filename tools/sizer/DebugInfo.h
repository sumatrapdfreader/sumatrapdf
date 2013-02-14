// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#ifndef DebugInfo_h
#define DebugInfo_h

typedef unsigned int u32;

using std::string;

#define DIC_END     0
#define DIC_CODE    1
#define DIC_DATA    2
#define DIC_BSS     3 // uninitialized data
#define DIC_UNKNOWN 4

struct DISymFile // File
{
    int        fileName;
    u32        codeSize;
    u32        dataSize;
};

struct DISymNameSp // Namespace
{
    int        name;
    u32        codeSize;
    u32        dataSize;
};

struct DiaSymbol
{
    int         name;
    int         mangledName;
    int         nameSpNum;
    int         objFileNum;
    u32         va;
    u32         size;
    int         klass;
};

struct TemplateSymbol
{
    string     name;
    u32        size;
    u32        count;
};

class DebugInfo
{
    StringInterner          strInterner;
    PoolAllocator           allocator;

    u32                     CountSizeInClass(int type);

public:
    Vec<DiaSymbol*>                 symbols;

    std::vector<TemplateSymbol>     templates;
    std::vector<DISymFile>          files;
    std::vector<DISymNameSp>        namespaces;

    int                             symCounts[SymTagMax];

    DebugInfo();

    // only use those before reading is finished!!
    int InternString(const char *s) { return strInterner.Intern(s); }
    const char* GetInternedString(int n) const { return strInterner.GetByIndex(n); }

    DiaSymbol * AllocDiaSymbol() { return allocator.AllocStruct<DiaSymbol>(); }

    void AddTypeSummary(str::Str<char>& report);

    void FinishedReading();

    int GetFile(int fileName);
    int GetFileByName(const char *objName);

    int GetNameSpace(int name);
    int GetNameSpaceByName(const char *name);

    void StartAnalyze();

    void FinishAnalyze();
    bool FindSymbol(u32 va, DiaSymbol **sym);

    std::string WriteReport();
};


#endif