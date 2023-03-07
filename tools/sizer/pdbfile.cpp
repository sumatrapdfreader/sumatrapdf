// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#include "types.hpp"
#include "debuginfo.hpp"
#include "pdbfile.hpp"

#include <malloc.h>
#include <windows.h>
#include <ole2.h>

/****************************************************************************/

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

    virtual HRESULT __stdcall Item(DWORD index, IDiaSymbol **symbol) = 0;
    virtual HRESULT __stdcall Next(ULONG celt, IDiaSymbol **rgelt, ULONG *pceltFetched) = 0;
    virtual HRESULT __stdcall Skip(ULONG celt) = 0;
    virtual HRESULT __stdcall Reset() = 0;

    virtual HRESULT __stdcall Clone(IDiaEnumSymbols **penum) = 0;
};

class IDiaEnumSymbolsByAddr : public IUnknown
{
public:
    virtual HRESULT __stdcall symbolByAddr(DWORD isect, DWORD offset, IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall symbolByRVA(DWORD relativeVirtualAddress, IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall symbolByVA(ULONGLONG virtualAddress, IDiaSymbol** ppSymbol) = 0;

    virtual HRESULT __stdcall Next(ULONG celt, IDiaSymbol ** rgelt, ULONG* pceltFetched) = 0;
    virtual HRESULT __stdcall Prev(ULONG celt, IDiaSymbol ** rgelt, ULONG * pceltFetched) = 0;

    virtual HRESULT __stdcall Clone(IDiaEnumSymbolsByAddr **ppenum) = 0;
};

class IDiaEnumTables : public IUnknown
{
public:
    virtual HRESULT __stdcall get__NewEnum(IUnknown **ret) = 0;
    virtual HRESULT __stdcall get_Count(LONG *ret) = 0;

    virtual HRESULT __stdcall Item(VARIANT index, IDiaTable **table) = 0;
    virtual HRESULT __stdcall Next(ULONG celt, IDiaTable ** rgelt, ULONG *pceltFetched) = 0;
    virtual HRESULT __stdcall Skip(ULONG celt) = 0;
    virtual HRESULT __stdcall Reset() = 0;

    virtual HRESULT __stdcall Clone(IDiaEnumTables **ppenum) = 0;
};

class IDiaDataSource : public IUnknown
{
public:
    virtual HRESULT __stdcall get_lastError(BSTR *ret) = 0;

    virtual HRESULT __stdcall loadDataFromPdb(LPCOLESTR pdbPath) = 0;
    virtual HRESULT __stdcall loadAndValidateDataFromPdb(LPCOLESTR pdbPath, GUID *pcsig70, DWORD sig, DWORD age) = 0;
    virtual HRESULT __stdcall loadDataForExe(LPCOLESTR executable, LPCOLESTR searchPath, IUnknown *pCallback) = 0;
    virtual HRESULT __stdcall loadDataFromIStream(IStream *pIStream) = 0;

    virtual HRESULT __stdcall openSession(IDiaSession **ppSession) = 0;
};

class IDiaSession : public IUnknown
{
public:
    virtual HRESULT __stdcall get_loadAddress(ULONGLONG *ret) = 0;
    virtual HRESULT __stdcall put_loadAddress(ULONGLONG val) = 0;
    virtual HRESULT __stdcall get_globalScope(IDiaSymbol **sym) = 0;

    virtual HRESULT __stdcall getEnumTables(IDiaEnumTables** ppEnumTables) = 0;
    virtual HRESULT __stdcall getSymbolsByAddr(IDiaEnumSymbolsByAddr** ppEnumbyAddr) = 0;

    virtual HRESULT __stdcall findChildren(IDiaSymbol* parent, enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, IDiaEnumSymbols** ppResult) = 0;
    virtual HRESULT __stdcall findSymbolByAddr(DWORD isect, DWORD offset, enum SymTagEnum symtag, IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall findSymbolByRVA(DWORD rva, enum SymTagEnum symtag, IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall findSymbolByVA(ULONGLONG va, enum SymTagEnum symtag, IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall findSymbolByToken(ULONG token, enum SymTagEnum symtag, IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall symsAreEquiv(IDiaSymbol* symbolA, IDiaSymbol* symbolB) = 0;
    virtual HRESULT __stdcall symbolById(DWORD id, IDiaSymbol** ppSymbol) = 0;
    virtual HRESULT __stdcall findSymbolByRVAEx(DWORD rva, enum SymTagEnum symtag, IDiaSymbol** ppSymbol, long* displacement) = 0;
    virtual HRESULT __stdcall findSymbolByVAEx(ULONGLONG va, enum SymTagEnum symtag, IDiaSymbol** ppSymbol, long* displacement) = 0;

    virtual HRESULT __stdcall findFile(IDiaSymbol* pCompiland, LPCOLESTR name, DWORD compareFlags, IDiaEnumSourceFiles** ppResult) = 0;
    virtual HRESULT __stdcall findFileById(DWORD uniqueId, IDiaSourceFile** ppResult) = 0;

    virtual HRESULT __stdcall findLines(IDiaSymbol* compiland, IDiaSourceFile* file, IDiaEnumLineNumbers** ppResult) = 0;
    virtual HRESULT __stdcall findLinesByAddr(DWORD seg, DWORD offset, DWORD length, IDiaEnumLineNumbers** ppResult) = 0;
    virtual HRESULT __stdcall findLinesByRVA(DWORD rva, DWORD length, IDiaEnumLineNumbers** ppResult) = 0;
    virtual HRESULT __stdcall findLinesByVA(ULONGLONG va, DWORD length, IDiaEnumLineNumbers** ppResult) = 0;
    virtual HRESULT __stdcall findLinesByLinenum(IDiaSymbol* compiland, IDiaSourceFile* file, DWORD linenum, DWORD column, IDiaEnumLineNumbers** ppResult) = 0;

    virtual HRESULT __stdcall findInjectedSource(LPCOLESTR srcFile, IDiaEnumInjectedSources** ppResult) = 0;
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

    virtual HRESULT __stdcall get_dataBytes(DWORD cbData, DWORD *pcbData, BYTE data[]) = 0;
    virtual HRESULT __stdcall findChildren(enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, IDiaEnumSymbols** ppResult) = 0;

    virtual HRESULT __stdcall get_targetSection(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_targetOffset(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_targetRelativeVirtualAddress(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_targetVirtualAddress(ULONGLONG *ret) = 0;
    virtual HRESULT __stdcall get_machineType(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_oemId(DWORD *ret) = 0;
    virtual HRESULT __stdcall get_oemSymbolId(DWORD *ret) = 0;

    virtual HRESULT __stdcall get_types(DWORD cTypes, DWORD *pcTypes, IDiaSymbol* types[]) = 0;
    virtual HRESULT __stdcall get_typeIds(DWORD cTypes, DWORD *pcTypeIds, DWORD typeIds[]) = 0;

    virtual HRESULT __stdcall get_objectPointerType(IDiaSymbol **ret) = 0;
    virtual HRESULT __stdcall get_udtKind(DWORD *ret) = 0;

    virtual HRESULT __stdcall get_undecoratedNameEx(DWORD undecorateOptions, BSTR *name) = 0;
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

    virtual HRESULT __stdcall Item(DWORD index, IUnknown **element) = 0;
};

class DECLSPEC_UUID("e60afbee-502d-46ae-858f-8272a09bd707")DiaSource71;
class DECLSPEC_UUID("bce36434-2c24-499e-bf49-8bd99b0eeb68")DiaSource80;
class DECLSPEC_UUID("4C41678E-887B-4365-A09E-925D28DB33C2")DiaSource90;
class DECLSPEC_UUID("B86AE24D-BF2F-4ac9-B5A2-34B14E4CE11D")DiaSource100;
class DECLSPEC_UUID("761D3BCD-1304-41D5-94E8-EAC54E4AC172")DiaSource110;
class DECLSPEC_UUID("3bfcea48-620f-4b6b-81f7-b9af75454c7d")DiaSource120;
class DECLSPEC_UUID("e6756135-1e65-4d17-8576-610761398c3c")DiaSource140;
class DECLSPEC_UUID("79f1bb5f-b66e-48e5-b6a9-1545c323ca3d")IDiaDataSource;

/****************************************************************************/

struct PDBFileReader::SectionContrib
{
    DWORD Section;
    DWORD Offset;
    DWORD Length;
    DWORD Compiland;
    sInt Type;
    sInt ObjFile;
};

const PDBFileReader::SectionContrib *PDBFileReader::ContribFromSectionOffset(sU32 sec, sU32 offs)
{
    sInt l, r, x;

    l = 0;
    r = nContribs;

    while (l < r)
    {
        x = (l + r) / 2;
        const SectionContrib &cur = Contribs[x];

        if (sec < cur.Section || sec == cur.Section && offs < cur.Offset)
            r = x;
        else if (sec > cur.Section || sec == cur.Section && offs >= cur.Offset + cur.Length)
            l = x + 1;
        else if (sec == cur.Section && offs >= cur.Offset && offs < cur.Offset + cur.Length) // we got a winner
            return &cur;
        else
            break; // there's nothing here!
    }

    // normally, this shouldn't happen!
    return 0;
}

// helpers
static sChar *BStrToString(BSTR str, const char *defString = "", bool stripWhitespace = false)
{
    if (!str)
    {
        sInt len = sGetStringLen(defString);
        sChar *buffer = new sChar[len + 1];
        sCopyString(buffer, len + 1, defString, len + 1);

        return buffer;
    }
    else
    {
        sInt len = SysStringLen(str);
        sChar *buffer = new sChar[len + 1];

        sInt j = 0;
        for (sInt i = 0; i < len; i++)
        {
            if (stripWhitespace && iswspace(str[i]))
                continue;
            buffer[j] = (str[i] >= 32 && str[i] < 128) ? str[i] : '?';
            ++j;
        }

        buffer[j] = 0;

        return buffer;
    }
}

static sInt GetBStr(BSTR str, sChar *defString, DebugInfo &to)
{
    sChar *normalStr = BStrToString(str);
    sInt result = to.MakeString(normalStr);
    delete[] normalStr;

    return result;
}

void PDBFileReader::ProcessSymbol(IDiaSymbol *symbol, DebugInfo &to)
{
    DWORD section, offset, rva;
    enum SymTagEnum tag;
    ULONGLONG length = 0;
    BSTR name = 0, undName = 0, srcFileName = 0;

    symbol->get_symTag((DWORD*)&tag);
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

    const SectionContrib *contrib = ContribFromSectionOffset(section, offset);
    sInt objFile = 0;
    sInt sectionType = DIC_UNKNOWN;

    if (contrib)
    {
        objFile = contrib->ObjFile;
        sectionType = contrib->Type;
    }

    symbol->get_name(&name);
    symbol->get_undecoratedName(&undName);

    // fill out structure
    sChar *nameStr = BStrToString(name, "<noname>", true);
    sChar *undNameStr = BStrToString(undName, nameStr, false);

    to.Symbols.push_back(DISymbol());
    DISymbol *outSym = &to.Symbols.back();
    outSym->mangledName = to.MakeString(nameStr);
    outSym->name = to.MakeString(undNameStr);
    outSym->objFileNum = objFile;
    outSym->VA = rva;
    outSym->Size = (sU32)length;
    outSym->Class = sectionType;
    outSym->NameSpNum = to.GetNameSpaceByName(nameStr);

    // clean up
    delete[] nameStr;
    if (name)
        SysFreeString(name);
    if (undName)
        SysFreeString(undName);
}

void PDBFileReader::ReadEverything(DebugInfo &to)
{
    ULONG celt;

    Contribs = 0;
    nContribs = 0;

    // read section table
    IDiaEnumTables *enumTables;
    if (Session->getEnumTables(&enumTables) == S_OK)
    {
        VARIANT vIndex;
        vIndex.vt = VT_BSTR;
        vIndex.bstrVal = SysAllocString(L"Sections");

        IDiaTable *secTable;
        if (enumTables->Item(vIndex, &secTable) == S_OK)
        {
            LONG count;

            secTable->get_Count(&count);
            Contribs = new SectionContrib[count];
            nContribs = 0;

            IDiaSectionContrib *item;
            while (SUCCEEDED(secTable->Next(1, (IUnknown**)&item, &celt)) && celt == 1)
            {
                SectionContrib &contrib = Contribs[nContribs++];

                item->get_addressOffset(&contrib.Offset);
                item->get_addressSection(&contrib.Section);
                item->get_length(&contrib.Length);
                item->get_compilandId(&contrib.Compiland);

                BOOL code = FALSE, initData = FALSE, uninitData = FALSE;
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
                if (compiland)
                {
                    compiland->get_name(&objFileName);
                    compiland->Release();
                }

                sChar *objFileStr = BStrToString(objFileName, "<noobjfile>");
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

    /*
    // Note: this was the original code; that was however extremely slow especially on larger or 64 bit binaries.
    // New code that replaces it (however it does not produce 100% identical results) is below.

    // enumerate symbols by (virtual) address
    IDiaEnumSymbolsByAddr *enumByAddr;
    if(SUCCEEDED(Session->getSymbolsByAddr(&enumByAddr)))
    {
      IDiaSymbol *symbol;
      // get first symbol to get first RVA (argh)
      if(SUCCEEDED(enumByAddr->symbolByAddr(1,0,&symbol)))
      {
        DWORD rva;
        if(symbol->get_relativeVirtualAddress(&rva) == S_OK)
        {
          symbol->Release();

          // now, enumerate by rva.
          if(SUCCEEDED(enumByAddr->symbolByRVA(rva,&symbol)))
          {
            do
            {
              ProcessSymbol(symbol,to);
              symbol->Release();

              if(FAILED(enumByAddr->Next(1,&symbol,&celt)))
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
    */

    // This is new code that replaces commented out code above. On one not-too-big executable this gets Sizer execution time from
    // 448 seconds down to 1.5 seconds. However it does not list some symbols that are "weird" and are likely due to linker padding
    // or somesuch; I did not dig in. On that particular executable, e.g. 128 kb that is coming from "* Linker *" file is gone.
    IDiaSymbol* globalSymbol = NULL;
    if (SUCCEEDED(Session->get_globalScope(&globalSymbol)))
    {
        // Retrieve the compilands first
        IDiaEnumSymbols *enumSymbols;
        if (SUCCEEDED(globalSymbol->findChildren(SymTagCompiland, NULL, 0, &enumSymbols)))
        {
            LONG compilandCount = 0;
            enumSymbols->get_Count(&compilandCount);
            if (compilandCount == 0)
                compilandCount = 1;
            LONG processedCount = 0;
            IDiaSymbol *compiland;
            fprintf(stderr, "[      ]");
            while (SUCCEEDED(enumSymbols->Next(1, &compiland, &celt)) && (celt == 1))
            {
                ++processedCount;
                fprintf(stderr, "\b\b\b\b\b\b\b\b[%5.1f%%]", processedCount * 100.0 / compilandCount);
                // Find all the symbols defined in this compiland and treat their info
                IDiaEnumSymbols *enumChildren;
                if (SUCCEEDED(compiland->findChildren(SymTagNull, NULL, 0, &enumChildren)))
                {
                    IDiaSymbol *pSymbol;
                    ULONG celtChildren = 0;
                    while (SUCCEEDED(enumChildren->Next(1, &pSymbol, &celtChildren)) && (celtChildren == 1))
                    {
                        ProcessSymbol(pSymbol, to);
                        pSymbol->Release();
                    }
                    enumChildren->Release();
                }
                compiland->Release();
            }
            enumSymbols->Release();
        }
        globalSymbol->Release();
    }

    // clean up
    delete[] Contribs;
}

/****************************************************************************/

sBool PDBFileReader::ReadDebugInfo(const sChar *fileName, DebugInfo &to)
{
    static const struct DLLDesc
    {
        const char *Filename;
        IID UseCLSID;
    } DLLs[] =
    {
        "msdia71.dll", __uuidof(DiaSource71),
        "msdia80.dll", __uuidof(DiaSource80),
        "msdia90.dll", __uuidof(DiaSource90),
        "msdia100.dll", __uuidof(DiaSource100), // VS 2010
        "msdia110.dll", __uuidof(DiaSource110), // VS 2012
        "msdia120.dll", __uuidof(DiaSource120), // VS 2013
        "msdia140.dll", __uuidof(DiaSource140), // VS 2015
        // add more here as new versions appear (as long as they're backwards-compatible)
        0
    };

    sBool readOk = false;

    if (FAILED(CoInitialize(0)))
    {
        fprintf(stderr, "  failed to initialize COM\n");
        return false;
    }

    IDiaDataSource *source = 0;
    HRESULT hr = E_FAIL;

    // Try creating things "the official way"
    for (sInt i = 0; DLLs[i].Filename; i++)
    {
        hr = CoCreateInstance(DLLs[i].UseCLSID, 0, CLSCTX_INPROC_SERVER,
                __uuidof(IDiaDataSource), (void**)&source);

        if (SUCCEEDED(hr))
            break;
    }

    if (FAILED(hr))
    {
        // None of the classes are registered, but most programmers will have the
        // DLLs on their system anyway and can copy it over; try loading it directly.

        for (sInt i = 0; DLLs[i].Filename; i++)
        {
            HMODULE hDIADll = LoadLibraryA(DLLs[i].Filename);
            if (hDIADll)
            {
                typedef HRESULT (__stdcall *PDllGetClassObject)(REFCLSID rclsid, REFIID riid, void** ppvObj);
                PDllGetClassObject DllGetClassObject = (PDllGetClassObject)GetProcAddress(hDIADll, "DllGetClassObject");
                if (DllGetClassObject)
                {
                    // first create a class factory
                    IClassFactory *classFactory;
                    hr = DllGetClassObject(DLLs[i].UseCLSID, IID_IClassFactory, (void**)&classFactory);
                    if (SUCCEEDED(hr))
                    {
                        hr = classFactory->CreateInstance(0, __uuidof(IDiaDataSource), (void**)&source);
                        classFactory->Release();
                    }
                }

                if (SUCCEEDED(hr))
                    break;
                else
                    FreeLibrary(hDIADll);
            }
        }
    }

    if (source)
    {
        wchar_t wideFileName[260];
        mbstowcs(wideFileName, fileName, 260);
        if (SUCCEEDED(source->loadDataForExe(wideFileName, 0, 0)))
        {
            if (SUCCEEDED(source->openSession(&Session)))
            {
                ReadEverything(to);

                readOk = true;
                Session->Release();
            }
            else
                fprintf(stderr, "  failed to open DIA session\n");
        }
        else
            fprintf(stderr, "  failed to load debug symbols (PDB not found)\n");

        source->Release();
    }
    else
        fprintf(stderr, "  couldn't find (or properly initialize) any DIA dll, copying msdia*.dll to app dir might help.\n");

    CoUninitialize();

    return readOk;
}

/****************************************************************************/
