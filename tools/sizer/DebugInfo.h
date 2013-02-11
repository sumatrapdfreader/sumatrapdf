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

struct DISymbol
{
    int     name;
    int     mangledName;
    int     NameSpNum;
    int     objFileNum;
    u32     VA;
    u32     Size;
    int     Class;
};

struct TemplateSymbol
{
    string     name;
    u32        size;
    u32        count;
};

class DebugInfo
{
    typedef std::vector<string>     StringByIndexVector;
    typedef std::map<string,int>    IndexByStringMap;

    StringByIndexVector     stringByIndex;
    IndexByStringMap        indexByString;
    u32                     baseAddress;

    u32 CountSizeInClass(int type) const;

public:
    std::vector<DISymbol>           symbols;
    std::vector<TemplateSymbol>     templates;
    std::vector<DISymFile>          files;
    std::vector<DISymNameSp>        namespaces;

    void Init();
    void Exit();

    // only use those before reading is finished!!
    int MakeString(char *s);
    const char* GetStringPrep( int index ) const { return stringByIndex[index].c_str(); }

    void FinishedReading();

    int GetFile( int fileName );
    int GetFileByName( char *objName );

    int GetNameSpace(int name);
    int GetNameSpaceByName(char *name);

    void StartAnalyze();
    void FinishAnalyze();
    bool FindSymbol(u32 VA,DISymbol **sym);

    std::string WriteReport();
};


#endif