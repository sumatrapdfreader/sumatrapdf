#ifndef Dia2Subset_h
#define Dia2Subset_h

// Dia2.h is huge (>350 kb) and not always present
// This is a subset that is good enough for us

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

#endif
