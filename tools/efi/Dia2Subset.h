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

enum NameSearchOptions { 
    nsNone,
    nsfCaseSensitive     = 0x1,
    nsfCaseInsensitive   = 0x2,
    nsfFNameExt          = 0x4,
    nsfRegularExpression = 0x8,
    nsfUndecoratedName   = 0x10,

    // For backward compatibility:
    nsCaseSensitive           = nsfCaseSensitive,
    nsCaseInsensitive         = nsfCaseInsensitive,
    nsFNameExt                = nsfCaseInsensitive | nsfFNameExt,
    nsRegularExpression       = nsfRegularExpression | nsfCaseSensitive,
    nsCaseInRegularExpression = nsfRegularExpression | nsfCaseInsensitive
};

enum LocationType { 
   LocIsNull,
   LocIsStatic,
   LocIsTLS,
   LocIsRegRel,
   LocIsThisRel,
   LocIsEnregistered,
   LocIsBitField,
   LocIsSlot,
   LocIsIlRel,
   LocInMetaData,
   LocIsConstant,
   LocTypeMax
};

enum UdtKind { 
   UdtStruct,
   UdtClass,
   UdtUnion
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
    virtual HRESULT __stdcall get_globalScope(IDiaSymbol **sym) = 0;

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

//MIDL_INTERFACE("cb787b2f-bd6c-4635-ba52-933126bd2dcd")
class IDiaSymbol : public IUnknown
{
public:
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_symIndexId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_symTag( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_name( 
        /* [retval][out] */ BSTR *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_lexicalParent( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_classParent( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_type( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_dataKind( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_locationType( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_addressSection( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_addressOffset( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_relativeVirtualAddress( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtualAddress( 
        /* [retval][out] */ ULONGLONG *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_registerId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_offset( 
        /* [retval][out] */ LONG *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_length( 
        /* [retval][out] */ ULONGLONG *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_slot( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_volatileType( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_constType( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_unalignedType( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_access( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_libraryName( 
        /* [retval][out] */ BSTR *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_platform( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_language( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_editAndContinueEnabled( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_frontEndMajor( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_frontEndMinor( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_frontEndBuild( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_backEndMajor( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_backEndMinor( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_backEndBuild( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_sourceFileName( 
        /* [retval][out] */ BSTR *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_unused( 
        /* [retval][out] */ BSTR *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_thunkOrdinal( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_thisAdjust( 
        /* [retval][out] */ LONG *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtualBaseOffset( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtual( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_intro( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_pure( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_callingConvention( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_value( 
        /* [retval][out] */ VARIANT *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_baseType( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_token( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_timeStamp( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_guid( 
        /* [retval][out] */ GUID *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_symbolsFileName( 
        /* [retval][out] */ BSTR *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_reference( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_count( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_bitPosition( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_arrayIndexType( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_packed( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_constructor( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_overloadedOperator( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_nested( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasNestedTypes( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasAssignmentOperator( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasCastOperator( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_scoped( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtualBaseClass( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_indirectVirtualBaseClass( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtualBasePointerOffset( 
        /* [retval][out] */ LONG *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtualTableShape( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_lexicalParentId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_classParentId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_typeId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_arrayIndexTypeId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtualTableShapeId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_code( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_function( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_managed( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_msil( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtualBaseDispIndex( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_undecoratedName( 
        /* [retval][out] */ BSTR *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_age( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_signature( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_compilerGenerated( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_addressTaken( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_rank( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_lowerBound( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_upperBound( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_lowerBoundId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_upperBoundId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_dataBytes( 
        /* [in] */ DWORD cbData,
        /* [out] */ DWORD *pcbData,
        /* [size_is][out] */ BYTE *pbData) = 0;
    virtual HRESULT STDMETHODCALLTYPE findChildren( 
        /* [in] */ enum SymTagEnum symtag,
        /* [in] */ LPCOLESTR name,
        /* [in] */ DWORD compareFlags,
        /* [out] */ IDiaEnumSymbols **ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE findChildrenEx( 
        /* [in] */ enum SymTagEnum symtag,
        /* [in] */ LPCOLESTR name,
        /* [in] */ DWORD compareFlags,
        /* [out] */ IDiaEnumSymbols **ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE findChildrenExByAddr( 
        /* [in] */ enum SymTagEnum symtag,
        /* [in] */ LPCOLESTR name,
        /* [in] */ DWORD compareFlags,
        /* [in] */ DWORD isect,
        /* [in] */ DWORD offset,
        /* [out] */ IDiaEnumSymbols **ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE findChildrenExByVA( 
        /* [in] */ enum SymTagEnum symtag,
        /* [in] */ LPCOLESTR name,
        /* [in] */ DWORD compareFlags,
        /* [in] */ ULONGLONG va,
        /* [out] */ IDiaEnumSymbols **ppResult) = 0;
    virtual HRESULT STDMETHODCALLTYPE findChildrenExByRVA( 
        /* [in] */ enum SymTagEnum symtag,
        /* [in] */ LPCOLESTR name,
        /* [in] */ DWORD compareFlags,
        /* [in] */ DWORD rva,
        /* [out] */ IDiaEnumSymbols **ppResult) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_targetSection( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_targetOffset( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_targetRelativeVirtualAddress( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_targetVirtualAddress( 
        /* [retval][out] */ ULONGLONG *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_machineType( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_oemId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_oemSymbolId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_types( 
        /* [in] */ DWORD cTypes,
        /* [out] */ DWORD *pcTypes,
        /* [size_is][size_is][out] */ IDiaSymbol **pTypes) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_typeIds( 
        /* [in] */ DWORD cTypeIds,
        /* [out] */ DWORD *pcTypeIds,
        /* [size_is][out] */ DWORD *pdwTypeIds) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_objectPointerType( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_udtKind( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_undecoratedNameEx( 
        /* [in] */ DWORD undecorateOptions,
        /* [out] */ BSTR *name) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_noReturn( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_customCallingConvention( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_noInline( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_optimizedCodeDebugInfo( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_notReached( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_interruptReturn( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_farReturn( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isStatic( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasDebugInfo( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isLTCG( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isDataAligned( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasSecurityChecks( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_compilerName( 
        /* [retval][out] */ BSTR *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasAlloca( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasSetJump( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasLongJump( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasInlAsm( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasEH( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasSEH( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasEHa( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isNaked( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isAggregated( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isSplitted( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_container( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_inlSpec( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_noStackOrdering( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_virtualBaseTableType( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hasManagedCode( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isHotpatchable( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isCVTCIL( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isMSILNetmodule( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isCTypes( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isStripped( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_frontEndQFE( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_backEndQFE( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_wasInlined( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_strictGSCheck( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isCxxReturnUdt( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isConstructorVirtualBase( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_RValueReference( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_unmodifiedType( 
        /* [retval][out] */ IDiaSymbol **pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_framePointerPresent( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_isSafeBuffers( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_intrinsic( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_sealed( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hfaFloat( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_hfaDouble( 
        /* [retval][out] */ BOOL *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_liveRangeStartAddressSection( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_liveRangeStartAddressOffset( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_liveRangeStartRelativeVirtualAddress( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_countLiveRanges( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_liveRangeLength( 
        /* [retval][out] */ ULONGLONG *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_offsetInUdt( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_paramBasePointerRegisterId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
    virtual /* [id][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_localBasePointerRegisterId( 
        /* [retval][out] */ DWORD *pRetVal) = 0;
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
