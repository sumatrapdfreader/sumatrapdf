// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "BaseUtil.h"

#include "Dia2Subset.h"

#include <vector>
#include <map>

#include "DebugInfo.h"
#include "PdbFile.h"

void log(const char *s)
{
    fprintf(stderr, s);
}

struct SectionContrib
{
    DWORD Section;
    DWORD Offset;
    DWORD Length;
    DWORD Compiland;
    int Type;
    int ObjFile;
};

class PdbReader
{
public:
    PdbReader() : session(NULL), contribs(NULL), nContribs(0), di(NULL) { }
    ~PdbReader() {
        if (session)
            session->Release();
        delete [] contribs;
    }

    IDiaSession *           session;
    struct SectionContrib * contribs;
    int                     nContribs;
    DebugInfo *             di;
    ULONG                   celt;

    const SectionContrib *ContribFromSectionOffset(u32 sec,u32 offs);
    void ProcessSymbol(IDiaSymbol *symbol);
    void ReadEverything();
    void ReadSectionTable();
    void EnumerateSymbols();
};

const SectionContrib *PdbReader::ContribFromSectionOffset(u32 sec,u32 offs)
{
    int l,r,x;

    l = 0;
    r = nContribs;

    while (l < r)
    {
        x = (l + r) / 2;
        const SectionContrib &cur = contribs[x];

        if (sec < cur.Section || sec == cur.Section && offs < cur.Offset)
            r = x;
        else if (sec > cur.Section || sec == cur.Section && offs >= cur.Offset + cur.Length)
            l = x+1;
        else if (sec == cur.Section && offs >= cur.Offset && offs < cur.Offset + cur.Length) // we got a winner
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
    {
        int len = strlen(defString);
        char *buffer = new char[len+1];
        strncpy(buffer,defString,len+1);

        return buffer;
    }
    else
    {
        int len = SysStringLen(str);
        char *buffer = new char[len+1];

        int j = 0;
        for (int i=0;i<len;i++)
        {
            if (stripWhitespace && isspace(str[i]))
                continue;
            buffer[j] = (str[i] >= 32 && str[i] < 128) ? str[i] : '?';
            ++j;
        }

        buffer[j] = 0;

        return buffer;
    }
}

static int GetBStr(BSTR str, char *defString, DebugInfo *di)
{
    char *normalStr = BStrToString(str);
    int result = di->InternString(normalStr);
    delete[] normalStr;
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

    const SectionContrib *contrib = ContribFromSectionOffset(section,offset);
    int objFile = 0;
    int sectionType = DIC_UNKNOWN;

    if (contrib)
    {
        objFile = contrib->ObjFile;
        sectionType = contrib->Type;
    }

    symbol->get_name(&name);

    char *nameStr = BStrToString( name, "<noname>", true);

    di->symbols.push_back( DISymbol() );
    DISymbol *outSym = &di->symbols.back();
    outSym->name = outSym->mangledName = di->InternString(nameStr);
    outSym->objFileNum = objFile;
    outSym->VA = rva;
    outSym->Size = (u32) length;
    outSym->Class = sectionType;
    outSym->NameSpNum = di->GetNameSpaceByName(nameStr);

    delete[] nameStr;
    if (name)
        SysFreeString(name);
}

void PdbReader::ReadSectionTable()
{
    HRESULT hr;
    IDiaEnumTables *enumTables;
    hr = session->getEnumTables(&enumTables);
    if (S_OK != hr)
        return;

    VARIANT vIndex;
    vIndex.vt = VT_BSTR;
    vIndex.bstrVal = SysAllocString(L"Sections");

    IDiaTable *secTable;
    if (enumTables->Item(vIndex, &secTable) == S_OK)
    {
        LONG count;

        secTable->get_Count(&count);
        contribs = new SectionContrib[count];
        nContribs = 0;

        IDiaSectionContrib *item;
        while (SUCCEEDED(secTable->Next(1,(IUnknown **)&item,&celt)) && celt == 1)
        {
            SectionContrib &contrib = contribs[nContribs++];

            item->get_addressOffset(&contrib.Offset);
            item->get_addressSection(&contrib.Section);
            item->get_length(&contrib.Length);
            item->get_compilandId(&contrib.Compiland);

            BOOL code=FALSE,initData=FALSE,uninitData=FALSE;
            item->get_code(&code);
            item->get_initializedData(&initData);
            item->get_uninitializedData(&uninitData);

            if (code && !initData && !uninitData)
                contrib.Type = DIC_CODE;
            else if (!code && initData && !uninitData)
                contrib.Type = DIC_DATA;
            else if (!code && !initData && uninitData)
                contrib.Type = DIC_BSS;
            else
                contrib.Type = DIC_UNKNOWN;

            BSTR objFileName = 0;

            IDiaSymbol *compiland = 0;
            item->get_compiland(&compiland);
            if(compiland)
            {
                compiland->get_name(&objFileName);
                compiland->Release();
            }

            char *objFileStr = BStrToString(objFileName,"<noobjfile>");
            contrib.ObjFile = di->GetFileByName(objFileStr);

            delete[] objFileStr;
            if (objFileName)
                SysFreeString(objFileName);

            item->Release();
        }

        secTable->Release();
    }

    SysFreeString(vIndex.bstrVal);
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

    do
    {
        ProcessSymbol(symbol);
        symbol->Release();

        hr = enumByAddr->Next(1, &symbol, &celt);
        if (FAILED(hr))
            break;
    }
    while (celt == 1);

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

static const struct DLLDesc
{
    const char *Filename;
    IID UseCLSID;
} msdiaDlls[] = {
    "msdia1110.dll", __uuidof(DiaSource110),
    "msdia100.dll", __uuidof(DiaSource100),
    "msdia90.dll", __uuidof(DiaSource90),
    "msdia80.dll", __uuidof(DiaSource80),
    "msdia71.dll", __uuidof(DiaSource71),
    // add more here as new versions appear (as long as they're backwards-compatible)
    0
};

// note: we leak g_dia_source but who cares
IDiaDataSource *g_dia_source = 0;

IDiaDataSource *LoadDia()
{
    if (g_dia_source)
        return g_dia_source;

    HRESULT hr = E_FAIL;

    // Try creating things "the official way"
    for (int i=0; msdiaDlls[i].Filename; i++)
    {
        hr = CoCreateInstance(msdiaDlls[i].UseCLSID,0,CLSCTX_INPROC_SERVER,
            __uuidof(IDiaDataSource),(void**) &g_dia_source);

        if (SUCCEEDED(hr)) {
            //logf("using registered dia %s\n", msdiaDlls[i].Filename);
            return g_dia_source;
        }
    }

    // None of the classes are registered, but most programmers will have the
    // DLLs on their system anyway and can copy it over; try loading it directly.

    for (int i=0; msdiaDlls[i].Filename; i++)
    {
        const char *dllName = msdiaDlls[i].Filename;
        // TODO: also try to find Visual Studio directories where it might exist. On
        // my system:
        // c:/Program Files/Common Files/microsoft shared/VC/msdia100.dll
        // c:/Program Files/Common Files/microsoft shared/VC/msdia90.dll
        // c:/Program Files/Microsoft Visual Studio 10.0/Common7/Packages/Debugger/msdia100.dll
        // c:/Program Files/Microsoft Visual Studio 10.0/DIA SDK/bin/msdia100.dll
        // c:/Program Files/Microsoft Visual Studio 11.0/Common7/IDE/Remote Debugger/x86/msdia110.dll
        // c:/Program Files/Microsoft Visual Studio 11.0/Common7/Packages/Debugger/msdia110.dll
        // c:/Program Files/Microsoft Visual Studio 11.0/DIA SDK/bin/msdia110.dll
        // c:/Program Files/Windows Kits/8.0/App Certification Kit/msdia100.dll
        // I'm sure Visual Studio 8 also puts them somewhere

        HMODULE hDll = LoadLibraryA(dllName);
        if (!hDll)
            continue;

        typedef HRESULT (__stdcall *PDllGetClassObject)(REFCLSID rclsid,REFIID riid,void** ppvObj);
        PDllGetClassObject DllGetClassObject = (PDllGetClassObject) GetProcAddress(hDll,"DllGetClassObject");
        if (DllGetClassObject)
        {
            // first create a class factory
            IClassFactory *classFactory;
            hr = DllGetClassObject(msdiaDlls[i].UseCLSID,IID_IClassFactory,(void**) &classFactory);
            if (SUCCEEDED(hr))
            {
                hr = classFactory->CreateInstance(0,__uuidof(IDiaDataSource),(void**) &g_dia_source);
                classFactory->Release();
                logf("using loaded dia %s\n", dllName);
                return g_dia_source;
            } else {
                logf("DllGetClassObject() in %s failed", dllName);
            }
        } else {
            logf("dia dll found as %s but is missing DllGetClassObject function", dllName);
        }
        FreeLibrary(hDll);
    }
    log("  couldn't find (or properly initialize) any DIA dll, copying msdia*.dll to app dir might help.\n");
    return NULL;
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
