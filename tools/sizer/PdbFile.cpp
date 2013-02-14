// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "BaseUtil.h"
#include "Dict.h"

#include "Dia2Subset.h"

#include <vector>
#include <map>

#include "Util.h"

#include "DebugInfo.h"
#include "PdbFile.h"

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

static char *BStrToString(BSTR str, char *defString = "", bool stripWhitespace = false )
{
    if (!str)
        return str::Dup(defString);

    int len = SysStringLen(str);
    char *buffer = (char*)malloc(len+1);

    int j = 0;
    for (int i=0; i<len; i++)
    {
        if (stripWhitespace && isspace(str[i]))
            continue;
        buffer[j] = (str[i] >= 32 && str[i] < 128) ? str[i] : '?';
        ++j;
    }

    buffer[j] = 0;
    return buffer;
}

static void BStrToString2(str::Str<char>& strInOut, BSTR str, char *defString = "", bool stripWhitespace = false)
{
    OLECHAR c;
    int len;

    strInOut.Reset();
    if (!str) {
        strInOut.Append(defString);
        return;
    }

    len = SysStringLen(str);
    for (int i=0; i<len; i++)
    {
        c = str[i];
        if (stripWhitespace && isspace(c))
            continue;
        if (c < 32 || c >= 128)
            c = '?';
        strInOut.Append((char)c);
    }
}

static int GetBStr(BSTR str, char *defString, DebugInfo *di)
{
    char *normalStr = BStrToString(str);
    int result = di->InternString(normalStr);
    free(normalStr);
    return result;
}

void PdbReader::ProcessSymbol(IDiaSymbol *symbol)
{
    // print a dot for each 1000 symbols processed
    static int counter = 0;
    ++counter;
    if (counter == 1000) {
        fputc( '.', stderr );
        counter = 0;
    }

    DWORD section,offset,rva;
    enum SymTagEnum tag;
    ULONGLONG length = 0;
    BSTR name = 0, srcFileName = 0;

    symbol->get_symTag((DWORD *) &tag);
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

    BStrToString2(strTmp, name, "<noname>", true);
    const char *nameStr = strTmp.Get();

    di->symbols.push_back( DISymbol() );
    DISymbol *outSym = &di->symbols.back();
    outSym->name = outSym->mangledName = di->InternString(nameStr);
    outSym->objFileNum = objFile;
    outSym->VA = rva;
    outSym->Size = (u32) length;
    outSym->Class = sectionType;
    outSym->NameSpNum = di->GetNameSpaceByName(nameStr);

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

    char *objFileStr = BStrToString(objFileName, "<noobjfile>");
    s->objFile = di->GetFileByName(objFileStr);
    free(objFileStr);
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

DebugInfo *ReadPdbFile(const char *fileNameA)
{
    HRESULT             hr;
    IDiaDataSource *    dia;

    dia = LoadDia();
    if (!dia)
        return NULL;

    ScopedMem<WCHAR> fileName(str::conv::FromAnsi(fileNameA));
    hr = dia->loadDataForExe(fileName, 0, 0);
    if (FAILED(hr)) {
        log("  failed to load debug symbols (PDB not found)\n");
        return NULL;
    }

    PdbReader           reader;
    hr = dia->openSession(&reader.session);
    if (FAILED(hr)) {
        log("  failed to open DIA session\n");
        return NULL;
    }

    DebugInfo *di = new DebugInfo();
    reader.di = di;
    reader.ReadEverything();
    return di;
}
