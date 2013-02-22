// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "BaseUtil.h"
#include "BitManip.h"
#include "Dict.h"

#include "Dia2Subset.h"

#include <string>
#include <vector>
#include <map>

#include "Util.h"

#include "PdbFile.h"

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

DebugInfo::DebugInfo()
{
    ZeroMemory(symCounts, sizeof(symCounts));
}

u32 DebugInfo::CountSizeInClass(int type)
{
    DiaSymbol **symPtr;
    DiaSymbol *sym;
    u32 size = 0;
    for (symPtr = symbols.IterStart(); symPtr; symPtr = symbols.IterNext()) {
        sym = *symPtr;
        if (sym->klass == type)
            size += sym->size;
    }
    return size;
}

bool virtAddressComp(const DiaSymbol &a,const DiaSymbol &b)
{
    return a.va < b.va;
}

static bool StripTemplateParams(std::string& str)
{
    bool isTemplate = false;
    size_t start = str.find('<', 0);
    while (start != std::string::npos)
    {
        isTemplate = true;
        // scan to matching closing '>'
        size_t i = start + 1;
        int depth = 1;
        while (i < str.size())
        {
            char ch = str[i];
            if (ch == '<')
                ++depth;
            if (ch == '>')
            {
                --depth;
                if (depth == 0)
                    break;
            }
            ++i;
        }
        if (depth != 0)
            return isTemplate; // no matching '>', just return

        str = str.erase( start, i-start+1 );
        start = str.find( '<', start );
    }

    return isTemplate;
}

// must match order of enum SymTagEnum in Dia2Subset.h
const char *g_symTypeNames[SymTagMax] = {
    "SymTagNull",
    "SymTagExe",
    "SymTagCompiland",
    "SymTagCompilandDetails",
    "SymTagCompilandEnv",
    "SymTagFunction",
    "SymTagBlock",
    "SymTagData",
    "SymTagAnnotation",
    "SymTagLabel",
    "SymTagPublicSymbol",
    "SymTagUDT",
    "SymTagEnum",
    "SymTagFunctionType",
    "SymTagPointerType",
    "SymTagArrayType",
    "SymTagBaseType",
    "SymTagTypedef",
    "SymTagBaseClass",
    "SymTagFriend",
    "SymTagFunctionArgType",
    "SymTagFuncDebugStart",
    "SymTagFuncDebugEnd",
    "SymTagUsingNamespace",
    "SymTagVTableShape",
    "SymTagVTable",
    "SymTagCustom",
    "SymTagThunk",
    "SymTagCustomType",
    "SymTagManagedType",
    "SymTagDimension"
};

const char *GetSymTypeName(int i)
{
    if (i >= SymTagMax)
        return "<unknown type>";
    return g_symTypeNames[i];
}

void DebugInfo::AddTypeSummary(str::Str<char>& report)
{
    report.Append("Symbol type summary:\n");
    for (int i=0; i < SymTagMax; i++) {
        int n = symCounts[i];
        if (0 == n)
            continue;
        const char *symTypeName = GetSymTypeName(i);
        report.AppendFmt("  %s: %d\n", symTypeName, n);
    }
}

void DebugInfo::FinishedReading()
{
#if 0
    // fix strings and aggregate templates
    typedef std::map<std::string, int> StringIntMap;
    StringIntMap templateToIndex;

    for (int i=0; i<symbols.Count(); i++)
    {
        DiaSymbol *sym = symbols.At(i);

        std::string templateName = GetInternedString(sym->name);
        bool isTemplate = StripTemplateParams(templateName);
        if (isTemplate)
        {
            StringIntMap::iterator it = templateToIndex.find(templateName);
            int index;
            if (it != templateToIndex.end())
            {
                index = it->second;
                templates[index].size += sym->size;
                templates[index].count++;
            }
            else
            {
                index = templates.size();
                templateToIndex.insert(std::make_pair(templateName, index));
                TemplateSymbol tsym;
                tsym.name = templateName;
                tsym.count = 1;
                tsym.size = sym->size;
                templates.push_back(tsym);
            }
        }
    }
#endif

#if 0
    // sort symbols by virtual address
    std::sort(symbols.begin(), symbols.end(), virtAddressComp);

    // remove address double-covers
    int symCount = symbols.size();
    DISymbol *syms = new DISymbol[symCount];
    memcpy(syms,&symbols[0],symCount * sizeof(DISymbol));

    symbols.clear();
    u32 oldVA = 0;
    int oldSize = 0;

    for (int i=0; i<symCount; i++)
    {
        DISymbol *in = &syms[i];
        u32 newVA = in->VA;
        u32 newSize = in->Size;

        if (oldVA != 0)
        {
            int adjust = newVA - oldVA;
            if (adjust < 0) // we have to shorten
            {
                newVA = oldVA;
                if (newSize >= -adjust)
                    newSize += adjust;
            }
        }

        if (newSize || in->Class == DIC_END)
        {
            symbols.push_back(DISymbol());
            DISymbol *out = &symbols.back();
            *out = *in;
            out->VA = newVA;
            out->Size = newSize;

            oldVA = newVA + newSize;
            oldSize = newSize;
        }
    }

    delete[] syms;
#endif
}

int DebugInfo::GetFile(int fileName)
{
    for (int i=0;i<files.size();i++) {
        if (files[i].fileName == fileName)
            return i;
    }

    files.push_back( DISymFile() );
    DISymFile *file = &files.back();
    file->fileName = fileName;
    file->codeSize = file->dataSize = 0;

    return files.size() - 1;
}

int DebugInfo::GetFileByName(const char *objName)
{
    const char *p;

    // skip path seperators
    while ((p = strstr(objName,"\\")))
        objName = p + 1;

    while ((p = strstr(objName,"/")))
        objName = p + 1;

    return GetFile(InternString(objName));
}

int DebugInfo::GetNameSpace(int name)
{
    for (int i=0; i<namespaces.size(); i++) {
        if (namespaces[i].name == name)
            return i;
    }

    DISymNameSp namesp;
    namesp.name = name;
    namesp.codeSize = namesp.dataSize = 0;
    namespaces.push_back(namesp);

    return namespaces.size() - 1;
}

int DebugInfo::GetNameSpaceByName(const char *name)
{
    const char *pp = name - 2;
    const char *p;
    int cname;

    while ((p = strstr(pp+2,"::")))
        pp = p;

    while ((p = strstr(pp+1,".")))
        pp = p;

    if (pp != name - 2)
    {
        char buffer[2048];
        strncpy(buffer,name,2048);

        if (pp - name < 2048)
            buffer[pp - name] = 0;

        cname = InternString(buffer);
    }
    else
        cname = InternString("<global>");

    return GetNameSpace(cname);
}

void DebugInfo::StartAnalyze()
{
    int i;

    for (i=0;i<files.size();i++)
    {
        files[i].codeSize = files[i].dataSize = 0;
    }

    for (i=0;i<namespaces.size();i++)
    {
        namespaces[i].codeSize = namespaces[i].dataSize = 0;
    }
}

void DebugInfo::FinishAnalyze()
{
    for (size_t i=0; i<symbols.Count(); i++)
    {
        DiaSymbol *sym = symbols.At(i);
        u32 symSize = sym->size;
        if (sym->klass == DIC_CODE ) {
            files[sym->objFileNum].codeSize += symSize;
            namespaces[sym->nameSpNum].codeSize += symSize;
        } else if (sym->klass == DIC_DATA ) {
            files[sym->objFileNum].dataSize += symSize;
            namespaces[sym->nameSpNum].dataSize += symSize;
        }
    }
}

bool DebugInfo::FindSymbol(u32 va, DiaSymbol **symOut)
{
    DiaSymbol *sym;
    int l,r,x;

    l = 0;
    r = (int)symbols.Count();
    while (l<r)
    {
        x = (l + r) / 2;
        sym = symbols.At(x);
        if (va < sym->va)
            r = x; // continue in left half
        else if (va >= sym->va + sym->size)
            l = x + 1; // continue in left half
        else
        {
            *symOut = sym; // we found a match
            return true;
        }
    }

    if (l + 1 < symbols.Count())
        *symOut = symbols.At(l+1);
    else
        *symOut = 0;

    return false;
}

static bool symSizeComp(const DiaSymbol &a,const DiaSymbol &b)
{
    return a.size > b.size;
}

static bool templateSizeComp(const TemplateSymbol& a, const TemplateSymbol& b)
{
    return a.size > b.size;
}

static bool nameCodeSizeComp( const DISymNameSp &a,const DISymNameSp &b )
{
    return a.codeSize > b.codeSize;
}

static bool fileCodeSizeComp(const DISymFile &a,const DISymFile &b)
{
    return a.codeSize > b.codeSize;
}

static void sAppendPrintF(std::string &str,const char *format,...)
{
    static const int bufferSize = 512; // cut off after this
    char buffer[bufferSize];
    va_list arg;

    va_start(arg,format);
    _vsnprintf(buffer,bufferSize-1,format,arg);
    va_end(arg);

    strcpy(&buffer[bufferSize-4],"...");
    str += buffer;
}

std::string DebugInfo::WriteReport()
{
#if 0
    const int kMinSymbolSize = 512;
    const int kMinTemplateSize = 512;
    const int kMinDataSize = 1024;
    const int kMinClassSize = 2048;
    const int kMinFileSize = 2048;

    std::string Report;
    int i; //,j;
    u32 size;

    Report.reserve(16384); // start out with 16k space

    // symbols
    sAppendPrintF(Report,"Functions by size (kilobytes):\n");
    std::sort(symbols.begin(),symbols.end(),symSizeComp);

    for(i=0;i<symbols.size();i++)
    {
        if( symbols[i].Size < kMinSymbolSize )
            break;
        if(symbols[i].Class == DIC_CODE)
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
            symbols[i].Size/1024,(symbols[i].Size%1024)*100/1024,
            GetInternedString(symbols[i].name), GetInternedString(files[symbols[i].objFileNum].fileName));
    }

    // templates
    sAppendPrintF(Report,"\nAggregated templates by size (kilobytes):\n");

    std::sort(templates.begin(),templates.end(),templateSizeComp);

    for(i=0;i<templates.size();i++)
    {
        if( templates[i].size < kMinTemplateSize )
            break;
        sAppendPrintF(Report,"%5d.%02d #%5d: %s\n",
            templates[i].size/1024,(templates[i].size%1024)*100/1024,
            templates[i].count,
            templates[i].name.c_str() );
    }

    sAppendPrintF(Report,"\nData by size (kilobytes):\n");
    for(i=0;i<symbols.size();i++)
    {
        if( symbols[i].Size < kMinDataSize )
            break;
        if(symbols[i].Class == DIC_DATA)
        {
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
                symbols[i].Size/1024,(symbols[i].Size%1024)*100/1024,
                GetInternedString(symbols[i].name), GetInternedString(files[symbols[i].objFileNum].fileName));
        }
    }

    sAppendPrintF(Report,"\nBSS by size (kilobytes):\n");
    for(i=0;i<symbols.size();i++)
    {
        if( symbols[i].Size < kMinDataSize )
            break;
        if(symbols[i].Class == DIC_BSS)
        {
            sAppendPrintF(Report,"%5d.%02d: %-50s %s\n",
                symbols[i].Size/1024,(symbols[i].Size%1024)*100/1024,
                GetInternedString(symbols[i].name), GetInternedString(files[symbols[i].objFileNum].fileName));
        }
    }

    /*
    sSPrintF(Report,512,"\nFunctions by object file and size:\n");
    Report += strlen(Report);

    for(i=1;i<symbols.size();i++)
    for(j=i;j>0;j--)
    {
    int f1 = symbols[j].FileNum;
    int f2 = symbols[j-1].FileNum;

    if(f1 == -1 || f2 != -1 && stricmp(Files[f1].Name.String,Files[f2].Name.String) < 0)
    std::swap(symbols[j],symbols[j-1]);
    }

    for(i=0;i<symbols.size();i++)
    {
    if(symbols[i].Class == DIC_CODE)
    {
    sSPrintF(Report,512,"%5d.%02d: %-50s %s\n",
    symbols[i].Size/1024,(symbols[i].Size%1024)*100/1024,
    symbols[i].Name,Files[symbols[i].FileNum].Name);

    Report += strlen(Report);
    }
    }
    */

    sAppendPrintF(Report,"\nClasses/Namespaces by code size (kilobytes):\n");
    std::sort(namespaces.begin(),namespaces.end(),nameCodeSizeComp);

    for(i=0;i<namespaces.size();i++)
    {
        if( namespaces[i].codeSize < kMinClassSize )
            break;
        sAppendPrintF(Report,"%5d.%02d: %s\n",
            namespaces[i].codeSize/1024,(namespaces[i].codeSize%1024)*100/1024, GetInternedString(namespaces[i].name) );
    }

    sAppendPrintF(Report,"\nObject files by code size (kilobytes):\n");
    std::sort(files.begin(),files.end(),fileCodeSizeComp);

    for(i=0;i<files.size();i++)
    {
        if( files[i].codeSize < kMinFileSize )
            break;
        sAppendPrintF(Report,"%5d.%02d: %s\n",files[i].codeSize/1024,
            (files[i].codeSize%1024)*100/1024, GetInternedString(files[i].fileName) );
    }

    size = CountSizeInClass(DIC_CODE);
    sAppendPrintF(Report,"\nOverall code: %5d.%02d kb\n",size/1024,
        (size%1024)*100/1024 );

    size = CountSizeInClass(DIC_DATA);
    sAppendPrintF(Report,"Overall data: %5d.%02d kb\n",size/1024,
        (size%1024)*100/1024);

    size = CountSizeInClass(DIC_BSS);
    sAppendPrintF(Report,"Overall BSS:  %5d.%02d kb\n",size/1024,
        (size%1024)*100/1024);

    return Report;
#else
    return string();
#endif
}

struct Section {
    DWORD       section;
    DWORD       offset;
    DWORD       length;
    DWORD       compiland;
    int         type;
    int         objFile;
};

class PdbReader
{
public:
    PdbReader() : session(NULL), di(NULL), sections(NULL), 
                  nSections(0), currSection(0) {
    }

    ~PdbReader() {
        if (session)
            session->Release();
    }

    str::Str<char>  strTmp;

    IDiaSession *   session;
    DebugInfo *     di;

    int             nSections;
    int             currSection;
    Section *       sections;

    Section *   SectionFromOffset(u32 sec,u32 offs);
    void        ProcessSymbol(IDiaSymbol *symbol);
    void        AddSection(IDiaSectionContrib *item);
    void        ReadEverything();
    void        ReadSectionTable();
    void        EnumerateSymbols();

    void        DumpSections(str::Str<char>& report);
    void        DumpSection(IDiaSectionContrib *item, str::Str<char>& report);
};

Section *PdbReader::SectionFromOffset(u32 sec,u32 offs)
{
    int l,r,x;

    l = 0;
    r = nSections;

    while (l < r)
    {
        x = (l + r) / 2;
        Section &cur = sections[x];

        if (sec < cur.section || sec == cur.section && offs < cur.offset)
            r = x;
        else if (sec > cur.section || sec == cur.section && offs >= cur.offset + cur.length)
            l = x+1;
        else if (sec == cur.section && offs >= cur.offset && offs < cur.offset + cur.length) // we got a winner
            return &cur;
        else
            break; // there's nothing here!
    }

    // normally, this shouldn't happen!
    return 0;
}

void PdbReader::ProcessSymbol(IDiaSymbol *symbol)
{
    // print a dot for each 1000 symbols processed
    static int counter = 0;
    ++counter;
    if (counter == 1000) {
        fputc('.', stderr);
        counter = 0;
    }

    DWORD section, offset, rva;
    DWORD dwTag;
    enum SymTagEnum tag;
    ULONGLONG length = 0;
    BSTR name = 0, srcFileName = 0;

    symbol->get_symTag(&dwTag);
    tag = (enum SymTagEnum)dwTag;
    if (dwTag < SymTagMax)
        di->symCounts[dwTag]++;

    symbol->get_relativeVirtualAddress(&rva);
    symbol->get_length(&length);
    symbol->get_addressSection(&section);
    symbol->get_addressOffset(&offset);

    // get length from type for data
    if (tag == SymTagData)
    {
        IDiaSymbol *type = NULL;
        if (symbol->get_type(&type) == S_OK) // no SUCCEEDED test as may return S_FALSE!
        {
            if (FAILED(type->get_length(&length)))
                length = 0;
            type->Release();
        }
        else
            length = 0;
    }

    Section *contrib = SectionFromOffset(section, offset);
    int objFile = 0;
    int sectionType = DIC_UNKNOWN;

    if (contrib)
    {
        objFile = contrib->objFile;
        sectionType = contrib->type;
    }

    symbol->get_name(&name);

    const char *nameStr = BStrToString(strTmp, name, "<noname>", true);

    DiaSymbol *sym = di->AllocDiaSymbol();
    sym->name = sym->mangledName = di->InternString(nameStr);
    sym->objFileNum = objFile;
    sym->va = rva;
    sym->size = (u32) length;
    sym->klass = sectionType;
    sym->nameSpNum = di->GetNameSpaceByName(nameStr);
    di->symbols.Append(sym);

    if (name)
        SysFreeString(name);
}

void PdbReader::AddSection(IDiaSectionContrib *item)
{
    Section *s = &sections[currSection++];
    item->get_addressOffset(&s->offset);
    item->get_addressSection(&s->section);
    item->get_length(&s->length);
    item->get_compilandId(&s->compiland);

    BOOL code=FALSE,initData=FALSE,uninitData=FALSE;
    item->get_code(&code);
    item->get_initializedData(&initData);
    item->get_uninitializedData(&uninitData);

    if (code && !initData && !uninitData)
        s->type = DIC_CODE;
    else if (!code && initData && !uninitData)
        s->type = DIC_DATA;
    else if (!code && !initData && uninitData)
        s->type = DIC_BSS;
    else
        s->type = DIC_UNKNOWN;

    BSTR objFileName = 0;

    IDiaSymbol *compiland = 0;
    item->get_compiland(&compiland);
    if (compiland)
    {
        compiland->get_name(&objFileName);
        compiland->Release();
    }

    const char *str = BStrToString(strTmp, objFileName, "<noobjfile>");
    s->objFile = di->GetFileByName(str);
    if (objFileName)
        SysFreeString(objFileName);
}

const char *SectionTypeName(int type)
{
    if (type == DIC_CODE) return "code";
    if (type == DIC_DATA) return "data";
    if (type == DIC_BSS) return "bss";
    return "unknown";
}

void PdbReader::DumpSection(IDiaSectionContrib *item, str::Str<char>& report)
{
    Section s2 = { 0 };
    Section *s = &s2;

    item->get_addressOffset(&s->offset);
    item->get_addressSection(&s->section);
    item->get_length(&s->length);
    item->get_compilandId(&s->compiland);

    BOOL code=FALSE,initData=FALSE,uninitData=FALSE;
    item->get_code(&code);
    item->get_initializedData(&initData);
    item->get_uninitializedData(&uninitData);

    if (code && !initData && !uninitData)
        s->type = DIC_CODE;
    else if (!code && initData && !uninitData)
        s->type = DIC_DATA;
    else if (!code && !initData && uninitData)
        s->type = DIC_BSS;
    else
        s->type = DIC_UNKNOWN;

    BSTR objFileName = 0;

    IDiaSymbol *compiland = 0;
    item->get_compiland(&compiland);
    if (compiland)
    {
        compiland->get_name(&objFileName);
        compiland->Release();
    }

    const char *str = BStrToString(strTmp, objFileName, "<noobjfile>");

    // section | offset | length | type | objFile
    report.AppendFmt("%d|%d|%d|%s|%s\n", s->section, s->offset, s->length, SectionTypeName(s->type), str);

    if (objFileName)
        SysFreeString(objFileName);
}

void PdbReader::ReadSectionTable()
{
    HRESULT             hr;
    IDiaEnumTables *    enumTables = NULL;
    IDiaTable *         secTable = NULL;

    hr = session->getEnumTables(&enumTables);
    if (S_OK != hr)
        return;

    VARIANT vIndex;
    vIndex.vt = VT_BSTR;
    vIndex.bstrVal = SysAllocString(L"Sections");

    hr = enumTables->Item(vIndex, &secTable);
    if (S_OK != hr)
        goto Exit;

    LONG count;

    secTable->get_Count(&count);
    sections = (Section*)malloc(sizeof(Section)*count);
    nSections = count;
    currSection = 0;

    IDiaSectionContrib *item;
    ULONG numFetched;
    for (;;)
    {
        hr = secTable->Next(1,(IUnknown **)&item, &numFetched);
        if (FAILED(hr) || (numFetched != 1))
            break;

        AddSection(item);
        item->Release();
    }

Exit:
    if (secTable)
        secTable->Release();
    SysFreeString(vIndex.bstrVal);
    if (enumTables)
        enumTables->Release();
}

// enumerate symbols by (virtual) address
void PdbReader::EnumerateSymbols()
{
    HRESULT                 hr;
    IDiaEnumSymbolsByAddr * enumByAddr = NULL;
    IDiaSymbol *            symbol = NULL;

    hr = session->getSymbolsByAddr(&enumByAddr);
    if (!SUCCEEDED(hr))
        goto Exit;

    // get first symbol to get first RVA (argh)
    hr = enumByAddr->symbolByAddr(1, 0, &symbol);
    if (!SUCCEEDED(hr))
        goto Exit;

    DWORD rva;
    hr = symbol->get_relativeVirtualAddress(&rva);
    if (S_OK != hr)
        goto Exit;

    symbol->Release();
    symbol = NULL;

    // enumerate by rva
    hr = enumByAddr->symbolByRVA(rva, &symbol);
    if (!SUCCEEDED(hr))
        goto Exit;

    ULONG numFetched;
    for (;;)
    {
        ProcessSymbol(symbol);
        symbol->Release();
        symbol = NULL;

        hr = enumByAddr->Next(1, &symbol, &numFetched);
        if (FAILED(hr) || (numFetched != 1))
            break;
    }

Exit:
    if (symbol)
        symbol->Release();
    if (enumByAddr)
        enumByAddr->Release();
}

void PdbReader::ReadEverything()
{
    // TODO: if ReadSectionTable() fails, we probably shouldn't proceed
    ReadSectionTable();
    EnumerateSymbols();
}

void PdbReader::DumpSections(str::Str<char>& report)
{
    HRESULT             hr;
    IDiaEnumTables *    enumTables = NULL;
    IDiaTable *         secTable = NULL;

    hr = session->getEnumTables(&enumTables);
    if (S_OK != hr)
        return;

    report.Append("Sections:\n");

    VARIANT vIndex;
    vIndex.vt = VT_BSTR;
    vIndex.bstrVal = SysAllocString(L"Sections");

    hr = enumTables->Item(vIndex, &secTable);
    if (S_OK != hr)
        goto Exit;

    LONG count;

    secTable->get_Count(&count);
    sections = (Section*)malloc(sizeof(Section)*count);
    nSections = count;
    currSection = 0;

    IDiaSectionContrib *item;
    ULONG numFetched;
    for (;;)
    {
        hr = secTable->Next(1,(IUnknown **)&item, &numFetched);
        if (FAILED(hr) || (numFetched != 1))
            break;

        DumpSection(item, report);
        item->Release();
    }

Exit:
    report.Append("\n");
    if (secTable)
        secTable->Release();
    SysFreeString(vIndex.bstrVal);
    if (enumTables)
        enumTables->Release();
}

void ProcessPdbFile(const char *fileNameA, PdbProcessingOptions options)
{
    HRESULT             hr;
    IDiaDataSource *    dia;

    dia = LoadDia();
    if (!dia)
        return;

    ScopedMem<WCHAR> fileName(str::conv::FromAnsi(fileNameA));
    hr = dia->loadDataForExe(fileName, 0, 0);
    if (FAILED(hr)) {
        log("  failed to load debug symbols (PDB not found)\n");
        return;
    }

    PdbReader           reader;
    hr = dia->openSession(&reader.session);
    if (FAILED(hr)) {
        log("  failed to open DIA session\n");
        return;
    }

    str::Str<char> report;
    if (bit::IsMaskSet(options, DUMP_SECTIONS)) {
        reader.DumpSections(report);
    }

    /*
    DebugInfo *di = new DebugInfo();
    reader.di = di;
    reader.ReadEverything();

    if (!di) {
        log("ERROR reading file via PDB\n");
        return;
    }
    log("\nProcessing info...\n");
    di->AddTypeSummary(report);
    */
#if 0
    di->FinishedReading();
    di->StartAnalyze();
    di->FinishAnalyze();

    log("Generating report...\n");
    std::string report = di->WriteReport();
    log("Printing...\n");
#endif
    puts(report.Get());

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
}
