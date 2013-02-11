// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/

#include "Types.h"
#include "DebugInfo.h"
#include "PdbFile.h"

#include <malloc.h>
#include <windows.h>
#include <ole2.h>

// I don't want to use the complete huge dia2.h headers (>350k), so here
// goes the minimal version...

enum SymTagEnum
{
    SymTagNull,
    SymTagExe,
    SymTagCompiland,
    SymTagCompilandDetails,
    SymTagCompilandEnv,
    SymTagFunction,
    SymTagBlock,
    SymTagData,
    SymTagAnnotation,
    SymTagLabel,
    SymTagPublicSymbol,
    SymTagUDT,
    SymTagEnum,
    SymTagFunctionType,
    SymTagPointerType,
    SymTagArrayType,
    SymTagBaseType,
    SymTagTypedef,
    SymTagBaseClass,
    SymTagFriend,
    SymTagFunctionArgType,
    SymTagFuncDebugStart,
    SymTagFuncDebugEnd,
    SymTagUsingNamespace,
    SymTagVTableShape,
    SymTagVTable,
    SymTagCustom,
    SymTagThunk,
    SymTagCustomType,
    SymTagManagedType,
    SymTagDimension,
    SymTagMax
};

class IDiaEnumSymbols;
class IDiaEnumSymbolsByAddr;
class IDiaEnumTables;

class IDiaDataSource;
class IDiaSession;

class IDiaSymbol;
class IDiaSectionContrib;

class IDiaTable;

// not transcribed here:
class IDiaSourceFile;

class IDiaEnumSourceFiles;
class IDiaEnumLineNumbers;
class IDiaEnumDebugStreams;
class IDiaEnumInjectedSources;

class IDiaEnumSymbols : public IUnknown
{
public:
    virtual HRESULT __stdcall get__NewEnum(IUnknown **ret) = 0;
    virtual HRESULT __stdcall get_Count(LONG *ret) = 0;

    virtual HRESULT __stdcall Item(DWORD index,IDiaSymbol **symbol) = 0;
    virtual HRESULT __stdcall Next(ULONG celt,IDiaSymbol **rgelt,ULONG *pceltFetched) = 0;
    virtual HRESULT __stdcall Skip(ULONG celt) = 0;
    virtual HRESULT __stdcall Reset() = 0;

    virtual HRESULT __stdcall Clone(IDiaEnumSymbols **penum) = 0;
};

class IDiaEnumSymbolsByAddr : public IUnknown
{
public:
    virtual HRESULT __stdcall symbolByAddr(DWORD isect,DWORD offset,IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall symbolByRVA(DWORD relativeVirtualAddress,IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall symbolByVA(ULONGLONG virtualAddress,IDiaSymbol** ppSymbol) = 0;

    virtual HRESULT __stdcall Next(ULONG celt,IDiaSymbol ** rgelt,ULONG* pceltFetched) = 0;
    virtual HRESULT __stdcall Prev(ULONG celt,IDiaSymbol ** rgelt,ULONG * pceltFetched) = 0;

    virtual HRESULT __stdcall Clone(IDiaEnumSymbolsByAddr **ppenum) = 0;
};

class IDiaEnumTables : public IUnknown
{
public:
    virtual HRESULT __stdcall get__NewEnum(IUnknown **ret) = 0;
    virtual HRESULT __stdcall get_Count(LONG *ret) = 0;

    virtual HRESULT __stdcall Item(VARIANT index,IDiaTable **table) = 0;
    virtual HRESULT __stdcall Next(ULONG celt,IDiaTable ** rgelt,ULONG *pceltFetched) = 0;
    virtual HRESULT __stdcall Skip(ULONG celt) = 0;
    virtual HRESULT __stdcall Reset() = 0;

    virtual HRESULT __stdcall Clone(IDiaEnumTables **ppenum) = 0;
};

class IDiaDataSource : public IUnknown
{
public:
    virtual HRESULT __stdcall get_lastError(BSTR *ret) = 0;

    virtual HRESULT __stdcall loadDataFromPdb(LPCOLESTR pdbPath) = 0;
    virtual HRESULT __stdcall loadAndValidateDataFromPdb(LPCOLESTR pdbPath,GUID *pcsig70,DWORD sig,DWORD age) = 0;
    virtual HRESULT __stdcall loadDataForExe(LPCOLESTR executable,LPCOLESTR searchPath,IUnknown *pCallback) = 0;
    virtual HRESULT __stdcall loadDataFromIStream(IStream *pIStream) = 0;

    virtual HRESULT __stdcall openSession(IDiaSession **ppSession) = 0;
};

class IDiaSession : public IUnknown
{
public:
    virtual HRESULT __stdcall get_loadAddress(ULONGLONG *ret) = 0;
    virtual HRESULT __stdcall put_loadAddress(ULONGLONG val) = 0;
    virtual HRESULT __stdcall get_globalScape(IDiaSymbol **sym) = 0;

    virtual HRESULT __stdcall getEnumTables(IDiaEnumTables** ppEnumTables) = 0;
    virtual HRESULT __stdcall getSymbolsByAddr(IDiaEnumSymbolsByAddr** ppEnumbyAddr) = 0;

    virtual HRESULT __stdcall findChildren(IDiaSymbol* parent,enum SymTagEnum symtag,LPCOLESTR name,DWORD compareFlags,IDiaEnumSymbols** ppResult) = 0;
    virtual HRESULT __stdcall findSymbolByAddr(DWORD isect,DWORD offset,enum SymTagEnum symtag,IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall findSymbolByRVA(DWORD rva,enum SymTagEnum symtag,IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall findSymbolByVA(ULONGLONG va,enum SymTagEnum symtag,IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall findSymbolByToken(ULONG token,enum SymTagEnum symtag,IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall symsAreEquiv(IDiaSymbol* symbolA,IDiaSymbol* symbolB) = 0;
    virtual HRESULT __stdcall symbolById(DWORD id,IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall findSymbolByRVAEx(DWORD rva,enum SymTagEnum symtag,IDiaSymbol** ppSymbol,long* displacement) = 0;
    virtual HRESULT __stdcall findSymbolByVAEx(ULONGLONG va,enum SymTagEnum symtag,IDiaSymbol** ppSymbol,long* displacement) = 0;

    virtual HRESULT __stdcall findFile(IDiaSymbol* pCompiland,LPCOLESTR name,DWORD compareFlags,IDiaEnumSourceFiles** ppResult) = 0;
    virtual HRESULT __stdcall findFileById(DWORD uniqueId,IDiaSourceFile** ppResult) = 0;

    virtual HRESULT __stdcall findLines(IDiaSymbol* compiland,IDiaSourceFile* file,IDiaEnumLineNumbers** ppResult) = 0;
    virtual HRESULT __stdcall findLinesByAddr(DWORD seg,DWORD offset,DWORD length,IDiaEnumLineNumbers** ppResult) = 0;
    virtual HRESULT __stdcall findLinesByRVA(DWORD rva,DWORD length,IDiaEnumLineNumbers** ppResult) = 0;
    virtual HRESULT __stdcall findLinesByVA(ULONGLONG va,DWORD length,IDiaEnumLineNumbers** ppResult) = 0;
    virtual HRESULT __stdcall findLinesByLinenum(IDiaSymbol* compiland,IDiaSourceFile* file,DWORD linenum,DWORD column,IDiaEnumLineNumbers** ppResult) = 0;

    virtual HRESULT __stdcall findInjectedSource(LPCOLESTR srcFile,IDiaEnumInjectedSources** ppResult) = 0;
    virtual HRESULT __stdcall getEnumDebugStreams(IDiaEnumDebugStreams** ppEnumDebugStreams) = 0;
};

class IDiaSymbol : public IUnknown
{
public:
    virtual HRESULT __stdcall get_symIndexId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_symTag(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_name(BSTR *ret) = 0;
    virtual HRESULT __stdcall get_lexicalParent(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_classParent(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_type(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_dataKind(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_locationType(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_addressSection(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_addressOffset(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_relativeVirtualAddress(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_virtualAddress(ULONGLONG *ret) = 0;
    virtual HRESULT __stdcall get_registerId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_offset(LONG *ret) = 0;
    virtual HRESULT __stdcall get_length(ULONGLONG *ret) = 0;
    virtual HRESULT __stdcall get_slot(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_volatileType(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_constType(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_unalignedType(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_access(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_libraryName(BSTR *ret) = 0;
    virtual HRESULT __stdcall get_platform(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_language(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_editAndContinueEnabled(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_frontEndMajor(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_frontEndMinor(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_frontEndBuild(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_backEndMajor(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_backEndMinor(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_backEndBuild(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_sourceFileName(BSTR *ret) = 0;
    virtual HRESULT __stdcall get_unused(BSTR *ret) = 0;
    virtual HRESULT __stdcall get_thunkOrdinal(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_thisAdjust(LONG *ret) = 0;
    virtual HRESULT __stdcall get_virtualBaseOffset(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_virtual(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_intro(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_pure(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_callingConvention(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_value(VARIANT *ret) = 0;
    virtual HRESULT __stdcall get_baseType(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_token(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_timeStamp(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_guid(GUID *ret) = 0;
    virtual HRESULT __stdcall get_symbolsFileName(BSTR *ret) = 0;
    virtual HRESULT __stdcall get_reference(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_count(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_bitPosition(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_arrayIndexType(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_packed(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_constructor(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_overloadedOperator(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_nested(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_hasNestedTypes(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_hasAssignmentOperator(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_hasCastOperator(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_scoped(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_virtualBaseClass(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_indirectVirtualBaseClass(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_virtualBasePointerOffset(LONG *ret) = 0;
    virtual HRESULT __stdcall get_virtualTableShape(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_lexicalParentId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_classParentId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_typeId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_arrayIndexTypeId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_virtualTableShapeId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_code(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_function(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_managed(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_msil(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_virtualBaseDispIndex(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_undecoratedName(BSTR *ret) = 0;
    virtual HRESULT __stdcall get_age(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_signature(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_compilerGenerated(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_addressTaken(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_rank(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_lowerBound(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_upperBound(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_lowerBoundId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_upperBoundId(DWORD *ret) = 0;

    virtual HRESULT __stdcall get_dataBytes(DWORD cbData,DWORD *pcbData,BYTE data[]) = 0;
    virtual HRESULT __stdcall findChildren(enum SymTagEnum symtag,LPCOLESTR name,DWORD compareFlags,IDiaEnumSymbols** ppResult) = 0;

    virtual HRESULT __stdcall get_targetSection(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_targetOffset(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_targetRelativeVirtualAddress(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_targetVirtualAddress(ULONGLONG *ret) = 0;
    virtual HRESULT __stdcall get_machineType(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_oemId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_oemSymbolId(DWORD *ret) = 0;

    virtual HRESULT __stdcall get_types(DWORD cTypes,DWORD *pcTypes,IDiaSymbol* types[]) = 0;
    virtual HRESULT __stdcall get_typeIds(DWORD cTypes,DWORD *pcTypeIds,DWORD typeIds[]) = 0;

    virtual HRESULT __stdcall get_objectPointerType(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_udtKind(DWORD *ret) = 0;

    virtual HRESULT __stdcall get_undecoratedNameEx(DWORD undecorateOptions,BSTR *name) = 0;
};

class IDiaSectionContrib : public IUnknown
{
public:
    virtual HRESULT __stdcall get_compiland(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_addressSection(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_addressOffset(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_relativeVirtualAddress(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_virtualAddress(ULONGLONG *ret) = 0;
    virtual HRESULT __stdcall get_length(DWORD *ret) = 0;

    virtual HRESULT __stdcall get_notPaged(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_code(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_initializedData(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_uninitializedData(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_remove(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_comdat(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_discardable(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_notCached(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_share(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_execute(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_read(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_write(BOOL *ret) = 0;
    virtual HRESULT __stdcall get_dataCrc(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_relocationsaCrc(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_compilandId(DWORD *ret) = 0;
};

class IDiaTable : public IEnumUnknown
{
public:
    virtual HRESULT __stdcall get__NewEnum(IUnknown **ret) = 0;
    virtual HRESULT __stdcall get_name(BSTR *ret) = 0;
    virtual HRESULT __stdcall get_Count(LONG *ret) = 0;

    virtual HRESULT __stdcall Item(DWORD index,IUnknown **element) = 0;
};

class DECLSPEC_UUID("e60afbee-502d-46ae-858f-8272a09bd707") DiaSource71;
class DECLSPEC_UUID("bce36434-2c24-499e-bf49-8bd99b0eeb68") DiaSource80;
class DECLSPEC_UUID("4C41678E-887B-4365-A09E-925D28DB33C2") DiaSource90;
class DECLSPEC_UUID("B86AE24D-BF2F-4ac9-B5A2-34B14E4CE11D") DiaSource100;
class DECLSPEC_UUID("761D3BCD-1304-41D5-94E8-EAC54E4AC172") DiaSource110;
class DECLSPEC_UUID("79f1bb5f-b66e-48e5-b6a9-1545c323ca3d") IDiaDataSource;

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
            logf("using registered dia %s\n", msdiaDlls[i].Filename);
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

