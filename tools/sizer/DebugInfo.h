// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#ifndef DebugInfo_h
#define DebugInfo_h

#include <map>

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

    StringByIndexVector     m_StringByIndex;
    IndexByStringMap        m_IndexByString;
    u32                     BaseAddress;

    u32 CountSizeInClass(int type) const;

public:
    std::vector<DISymbol>           Symbols;
    std::vector<TemplateSymbol>     Templates;
    std::vector<DISymFile>          m_Files;
    std::vector<DISymNameSp>        NameSps;

    void Init();
    void Exit();

    // only use those before reading is finished!!
    int MakeString(char *s);
    const char* GetStringPrep( int index ) const { return m_StringByIndex[index].c_str(); }
    void SetBaseAddress(u32 base)            { BaseAddress = base; }

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

class DebugInfoReader
{
public:
    virtual bool ReadDebugInfo(char *fileName,DebugInfo &to) = 0;
};


#endif