#include "BaseUtil.h"
#include "BitManip.h"
#include "Dict.h"

#include "Dia2Subset.h"
#include "Util.h"

enum PdbProcessingOptions {
    DUMP_SECTIONS = 1 << 0,
    DUMP_SYMBOLS  = 1 << 1,
    DUMP_TYPES    = 1 << 2,
    // when set, we intern the strings
    DUMP_COMPACT  = 1 << 3,

    DUMP_ALL = DUMP_SECTIONS | DUMP_SYMBOLS | DUMP_TYPES | DUMP_COMPACT,
};

// must match order of enum SymTagEnum in Dia2Subset.h
#if 0
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
#endif

const char *g_symTypeNames[SymTagMax] = {
    "Null",
    "Exe",
    "Compiland",
    "CompilandDetails",
    "CompilandEnv",
    "Function",
    "Block",
    "Data",
    "Annotation",
    "Label",
    "PublicSymbol",
    "UDT",
    "Enum",
    "FunctionType",
    "PointerType",
    "ArrayType",
    "BaseType",
    "Typedef",
    "BaseClass",
    "Friend",
    "FunctionArgType",
    "FuncDebugStart",
    "FuncDebugEnd",
    "UsingNamespace",
    "VTableShape",
    "VTable",
    "Custom",
    "Thunk",
    "CustomType",
    "ManagedType",
    "Dimension"
};

static str::Str<char> g_strTmp;

static str::Str<char> g_report;
static StringInterner g_strInterner;

static bool           g_dumpSections = false;
static bool           g_dumpSymbols = false;
static bool           g_dumpTypes = false;
static bool           g_compact = false;

static int InternString(const char *s)
{
    return g_strInterner.Intern(s);
}

static void GetInternedStringsReport(str::Str<char>& resOut)
{
    resOut.Append("Strings:\n");

    int n = g_strInterner.StringsCount();
    for (int i = 0; i < n; i++) {
        resOut.AppendFmt("%d|%s\n", i, g_strInterner.GetByIndex(i));
    }

    resOut.Append("\n");
}

static const char *GetSymTypeName(int i)
{
    if (i >= SymTagMax)
        return "<unknown type>";
    return g_symTypeNames[i];
}

static const char *GetSectionType(IDiaSectionContrib *item)
{
    BOOL code=FALSE,initData=FALSE,uninitData=FALSE;
    item->get_code(&code);
    item->get_initializedData(&initData);
    item->get_uninitializedData(&uninitData);

    if (code && !initData && !uninitData)
        return "code";
    if (!code && initData && !uninitData)
        return "data";
    if (!code && !initData && uninitData)
        return "bss";
    return "unknown";
}

static void DumpSection(IDiaSectionContrib *item)
{
    DWORD           sectionNo;
    DWORD           offset;
    DWORD           length;
    BSTR            objFileName = 0;
    int             objFileId;
    IDiaSymbol *    compiland = 0;

    item->get_addressSection(&sectionNo);
    item->get_addressOffset(&offset);
    item->get_length(&length);

    //DWORD compilandId;
    //item->get_compilandId(&compilandId);

    const char *sectionType = GetSectionType(item);

    item->get_compiland(&compiland);
    if (compiland)
    {
        compiland->get_name(&objFileName);
        compiland->Release();
    }

    BStrToString(g_strTmp, objFileName, "<noobjfile>");

    if (g_compact) {
        // sectionNo | offset | length | type | objFileId
        objFileId = InternString(g_strTmp.Get());
        g_report.AppendFmt("%d|%d|%d|%s|%d\n", sectionNo, offset, length, sectionType, objFileId);
    } else {
        // sectionNo | offset | length | type | objFile
        g_report.AppendFmt("%d|%d|%d|%s|%s\n", sectionNo, offset, length, sectionType, g_strTmp.Get());
    }
    if (objFileName)
        SysFreeString(objFileName);
}

static void DumpSymbol(IDiaSymbol *symbol)
{
    DWORD               section, offset, rva;
    DWORD               dwTag;
    enum SymTagEnum     tag;
    ULONGLONG           length = 0;
    BSTR                name = 0;
    BSTR                srcFileName = 0;
    const char *        typeName;

    symbol->get_symTag(&dwTag);
    tag = (enum SymTagEnum)dwTag;
    typeName = GetSymTypeName(tag);

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

    symbol->get_name(&name);
    BStrToString(g_strTmp, name, "<noname>", true);
    const char *nameStr = g_strTmp.Get();

    // name | section | offset | length | rva | type
    g_report.AppendFmt("%s|%d|%d|%d|%d|%s\n", nameStr, (int)section, (int)offset, (int)length, (int)rva, typeName);

    if (name)
        SysFreeString(name);
}

static void DumpSymbols(IDiaSession *session)
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

    g_report.Append("Symbols:\n");

    ULONG numFetched;
    for (;;)
    {
        DumpSymbol(symbol);
        symbol->Release();
        symbol = NULL;

        hr = enumByAddr->Next(1, &symbol, &numFetched);
        if (FAILED(hr) || (numFetched != 1))
            break;
    }
    g_report.Append("\n");

Exit:
    if (symbol)
        symbol->Release();
    if (enumByAddr)
        enumByAddr->Release();
}

static void DumpSections(IDiaSession *session)
{
    HRESULT             hr;
    IDiaEnumTables *    enumTables = NULL;
    IDiaTable *         secTable = NULL;

    hr = session->getEnumTables(&enumTables);
    if (S_OK != hr)
        return;

    g_report.Append("Sections:\n");

    VARIANT vIndex;
    vIndex.vt = VT_BSTR;
    vIndex.bstrVal = SysAllocString(L"Sections");

    hr = enumTables->Item(vIndex, &secTable);
    if (S_OK != hr)
        goto Exit;

    LONG count;

    secTable->get_Count(&count);

    IDiaSectionContrib *item;
    ULONG numFetched;
    for (;;)
    {
        hr = secTable->Next(1,(IUnknown **)&item, &numFetched);
        if (FAILED(hr) || (numFetched != 1))
            break;

        DumpSection(item);
        item->Release();
    }

Exit:
    g_report.Append("\n");
    if (secTable)
        secTable->Release();
    SysFreeString(vIndex.bstrVal);
    if (enumTables)
        enumTables->Release();
}

static void ProcessPdbFile(const char *fileNameA)
{
    HRESULT             hr;
    IDiaDataSource *    dia = NULL;
    IDiaSession *       session = NULL;
    str::Str<char>      report;

    dia = LoadDia();
    if (!dia)
        return;

    ScopedMem<WCHAR> fileName(str::conv::FromAnsi(fileNameA));
    hr = dia->loadDataForExe(fileName, 0, 0);
    if (FAILED(hr)) {
        log("  failed to load debug symbols (PDB not found)\n");
        goto Exit;
    }

    hr = dia->openSession(&session);
    if (FAILED(hr)) {
        log("  failed to open DIA session\n");
        goto Exit;
    }

    if (g_dumpSections) {
        DumpSections(session);
    }

    if (g_dumpSymbols) {
        DumpSymbols(session);
    }

    if (g_compact) {
        str::Str<char> res;
        GetInternedStringsReport(res);
        puts(res.Get());
    }
    puts(g_report.Get());

Exit:
    if (session)
        session->Release();
}

int main(int argc, char** argv)
{
    ScopedCom comInitializer;

    if (argc < 2) {
        log("Usage: sizer <exefile>\n");
        return 1;
    }

    const char *fileName = argv[1];
    //fprintf(stderr, "Reading debug info file %s ...\n", fileName);

    g_dumpSymbols = true;
    g_compact = true;
    ProcessPdbFile(fileName);

    return 0;
}
