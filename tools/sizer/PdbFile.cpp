// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "BaseUtil.h"

//#include <malloc.h>
//#include <ole2.h>
#include "Dia2Subset.h"

#include <vector>
#include <map>

#include "DebugInfo.h"
#include "PdbFile.h"

void log(const char *s)
{
    fprintf(stderr, s);
}

struct PDBFileReader::SectionContrib
{
    DWORD Section;
    DWORD Offset;
    DWORD Length;
    DWORD Compiland;
    int Type;
    int ObjFile;
};

const PDBFileReader::SectionContrib *PDBFileReader::ContribFromSectionOffset(u32 sec,u32 offs)
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

static int GetBStr(BSTR str, char *defString, DebugInfo &to)
{
    char *normalStr = BStrToString(str);
    int result = to.MakeString(normalStr);
    delete[] normalStr;
    return result;
}

void PDBFileReader::ProcessSymbol(IDiaSymbol *symbol, DebugInfo &to)
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

    // fill out structure
    char *nameStr = BStrToString( name, "<noname>", true);

    to.Symbols.push_back( DISymbol() );
    DISymbol *outSym = &to.Symbols.back();
    outSym->name = outSym->mangledName = to.MakeString(nameStr);
    outSym->objFileNum = objFile;
    outSym->VA = rva;
    outSym->Size = (u32) length;
    outSym->Class = sectionType;
    outSym->NameSpNum = to.GetNameSpaceByName(nameStr);

    // clean up
    delete[] nameStr;
    if (name)
        SysFreeString(name);
}

void PDBFileReader::ReadEverything(DebugInfo &to)
{
    ULONG celt;

    contribs = 0;
    nContribs = 0;

    // read section table
    IDiaEnumTables *enumTables;
    if (session->getEnumTables(&enumTables) == S_OK)
    {
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
                contrib.ObjFile = to.GetFileByName(objFileStr);

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
    IDiaEnumSymbolsByAddr *enumByAddr;
    if (SUCCEEDED(session->getSymbolsByAddr(&enumByAddr)))
    {
        IDiaSymbol *symbol;
        // get first symbol to get first RVA (argh)
        if (SUCCEEDED(enumByAddr->symbolByAddr(1,0,&symbol)))
        {
            DWORD rva;
            if (symbol->get_relativeVirtualAddress(&rva) == S_OK)
            {
                symbol->Release();

                // now, enumerate by rva.
                if (SUCCEEDED(enumByAddr->symbolByRVA(rva,&symbol)))
                {
                    do
                    {
                        ProcessSymbol(symbol,to);
                        symbol->Release();

                        if (FAILED(enumByAddr->Next(1,&symbol,&celt)))
                            break;
                    }
                    while(celt == 1);
                }
            }
            else
                symbol->Release();
        }

        enumByAddr->Release();
    }

    delete[] contribs;
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

bool LoadDia()
{
    HRESULT hr = E_FAIL;

    // Try creating things "the official way"
    for (int i=0; msdiaDlls[i].Filename; i++)
    {
        hr = CoCreateInstance(msdiaDlls[i].UseCLSID,0,CLSCTX_INPROC_SERVER,
            __uuidof(IDiaDataSource),(void**) &g_dia_source);

        if (SUCCEEDED(hr)) {
            //logf("using registered dia %s\n", msdiaDlls[i].Filename);
            return true;
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
                return true;
            } else {
                logf("DllGetClassObject() in %s failed", dllName);
            }
        } else {
            logf("dia dll found as %s but is missing DllGetClassObject function", dllName);
        }
        FreeLibrary(hDll);
    }
    log("  couldn't find (or properly initialize) any DIA dll, copying msdia*.dll to app dir might help.\n");
    return false;
}

bool PDBFileReader::ReadDebugInfo(char *fileName, DebugInfo &to)
{
    if (!g_dia_source) {
        log("  must call LoadDia() before calling PDFFileReader::ReadDebugInfo()\n");
        return false;
    }

    wchar_t wchFileName[MAX_PATH];
    mbstowcs(wchFileName, fileName, dimof(wchFileName));
    if (FAILED(g_dia_source->loadDataForExe(wchFileName,0,0))) {
        log("  failed to load debug symbols (PDB not found)\n");
        return false;
    }

    if (FAILED(g_dia_source->openSession(&session))) {
        log("  failed to open DIA session\n");
        return false;
    }
    ReadEverything(to);

    session->Release();
    return true;
}

