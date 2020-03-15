#ifndef _WIN7FILEDIALOG_H
#define _WIN7FILEDIALOG_H

#ifdef _WIN32

#include <comdef.h>
#include "wdlstring.h"

#ifndef __RPC__in_opt
//defines for msvc6
#define __RPC__in_opt
#define __RPC__in
#define __RPC__out
#define __RPC__deref_out_opt
#define __RPC__deref_out_opt_string
#define __RPC__in_ecount_full(x)
#define __RPC__out_ecount_part(x,y)
#ifndef __in
#define __in
#endif

typedef ULONG SFGAOF;

typedef /* [v1_enum] */ 
enum tagFDE_OVERWRITE_RESPONSE
{	FDEOR_DEFAULT	= 0,
FDEOR_ACCEPT	= 0x1,
FDEOR_REFUSE	= 0x2
    } 	FDE_OVERWRITE_RESPONSE;

typedef /* [v1_enum] */ 
enum tagFDE_SHAREVIOLATION_RESPONSE
{	FDESVR_DEFAULT	= 0,
FDESVR_ACCEPT	= 0x1,
FDESVR_REFUSE	= 0x2
} 	FDE_SHAREVIOLATION_RESPONSE;

typedef /* [v1_enum] */ 
enum tagFDAP
{	FDAP_BOTTOM	= 0,
FDAP_TOP	= 0x1
} 	FDAP;

typedef struct _COMDLG_FILTERSPEC
{
  LPCWSTR pszName;
  LPCWSTR pszSpec;
} 	COMDLG_FILTERSPEC;

typedef 
enum tagSHCONTF
{	SHCONTF_FOLDERS	= 0x20,
SHCONTF_NONFOLDERS	= 0x40,
SHCONTF_INCLUDEHIDDEN	= 0x80,
SHCONTF_INIT_ON_FIRST_NEXT	= 0x100,
SHCONTF_NETPRINTERSRCH	= 0x200,
SHCONTF_SHAREABLE	= 0x400,
SHCONTF_STORAGE	= 0x800,
SHCONTF_FASTITEMS	= 0x2000,
SHCONTF_FLATLIST	= 0x4000,
SHCONTF_ENABLE_ASYNC	= 0x8000
} 	SHCONT;

typedef DWORD SHCONTF;

enum tagGETPROPERTYSTOREFLAGS
{	GPS_DEFAULT	= 0,
GPS_HANDLERPROPERTIESONLY	= 0x1,
GPS_READWRITE	= 0x2,
GPS_TEMPORARY	= 0x4,
GPS_FASTPROPERTIESONLY	= 0x8,
GPS_OPENSLOWITEM	= 0x10,
GPS_DELAYCREATION	= 0x20,
GPS_BESTEFFORT	= 0x40,
GPS_MASK_VALID	= 0x7f
} ;
typedef int GETPROPERTYSTOREFLAGS;

typedef /* [v1_enum] */ 
enum tagCDCONTROLSTATE
{	CDCS_INACTIVE	= 0,
CDCS_ENABLED	= 0x1,
CDCS_VISIBLE	= 0x2
} 	CDCONTROLSTATE;

typedef DWORD CDCONTROLSTATEF;

typedef void *REFPROPERTYKEY;

class IPropertyStore;
class IPropertyDescriptionList;
class IFileOperationProgressSink;
//msvc6
#else
#if defined(_MSC_VER) && _MSC_VER >= 1600
#include <shobjidl.h>
#endif
#endif 

#ifndef __IFileDialog_FWD_DEFINED__
#define __IFileDialog_FWD_DEFINED__
typedef interface IFileDialog IFileDialog;
#endif 	/* __IFileDialog_FWD_DEFINED__ */




#ifndef __IShellItem_INTERFACE_DEFINED__
#define __IShellItem_INTERFACE_DEFINED__

/* interface IShellItem */
/* [unique][object][uuid][helpstring] */ 

typedef /* [v1_enum] */ 
enum tagSIGDN
    {	SIGDN_NORMALDISPLAY	= 0,
	SIGDN_PARENTRELATIVEPARSING	= ( int  )0x80018001,
	SIGDN_DESKTOPABSOLUTEPARSING	= ( int  )0x80028000,
	SIGDN_PARENTRELATIVEEDITING	= ( int  )0x80031001,
	SIGDN_DESKTOPABSOLUTEEDITING	= ( int  )0x8004c000,
	SIGDN_FILESYSPATH	= ( int  )0x80058000,
	SIGDN_URL	= ( int  )0x80068000,
	SIGDN_PARENTRELATIVEFORADDRESSBAR	= ( int  )0x8007c001,
	SIGDN_PARENTRELATIVE	= ( int  )0x80080001
    } 	SIGDN;

/* [v1_enum] */ 
enum tagSHELLITEMCOMPAREHINTF
    {	SICHINT_DISPLAY	= 0,
	SICHINT_ALLFIELDS	= ( int  )0x80000000,
	SICHINT_CANONICAL	= 0x10000000
    } ;
typedef DWORD SICHINTF;


EXTERN_C const IID IID_IShellItem;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("43826d1e-e718-42ee-bc55-a1e261c37bfe")
    IShellItem : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE BindToHandler( 
            /* [unique][in] */ __RPC__in_opt IBindCtx *pbc,
            /* [in] */ __RPC__in REFGUID bhid,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ __RPC__deref_out_opt void **ppv) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetParent( 
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetDisplayName( 
            /* [in] */ SIGDN sigdnName,
            /* [string][out] */ __RPC__deref_out_opt_string LPWSTR *ppszName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAttributes( 
            /* [in] */ SFGAOF sfgaoMask,
            /* [out] */ __RPC__out SFGAOF *psfgaoAttribs) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Compare( 
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [in] */ SICHINTF hint,
            /* [out] */ __RPC__out int *piOrder) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IShellItemVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IShellItem * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IShellItem * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IShellItem * This);
        
        HRESULT ( STDMETHODCALLTYPE *BindToHandler )( 
            IShellItem * This,
            /* [unique][in] */ __RPC__in_opt IBindCtx *pbc,
            /* [in] */ __RPC__in REFGUID bhid,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ __RPC__deref_out_opt void **ppv);
        
        HRESULT ( STDMETHODCALLTYPE *GetParent )( 
            IShellItem * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *GetDisplayName )( 
            IShellItem * This,
            /* [in] */ SIGDN sigdnName,
            /* [string][out] */ __RPC__deref_out_opt_string LPWSTR *ppszName);
        
        HRESULT ( STDMETHODCALLTYPE *GetAttributes )( 
            IShellItem * This,
            /* [in] */ SFGAOF sfgaoMask,
            /* [out] */ __RPC__out SFGAOF *psfgaoAttribs);
        
        HRESULT ( STDMETHODCALLTYPE *Compare )( 
            IShellItem * This,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [in] */ SICHINTF hint,
            /* [out] */ __RPC__out int *piOrder);
        
        END_INTERFACE
    } IShellItemVtbl;

    interface IShellItem
    {
        CONST_VTBL struct IShellItemVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IShellItem_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IShellItem_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IShellItem_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IShellItem_BindToHandler(This,pbc,bhid,riid,ppv)	\
    ( (This)->lpVtbl -> BindToHandler(This,pbc,bhid,riid,ppv) ) 

#define IShellItem_GetParent(This,ppsi)	\
    ( (This)->lpVtbl -> GetParent(This,ppsi) ) 

#define IShellItem_GetDisplayName(This,sigdnName,ppszName)	\
    ( (This)->lpVtbl -> GetDisplayName(This,sigdnName,ppszName) ) 

#define IShellItem_GetAttributes(This,sfgaoMask,psfgaoAttribs)	\
    ( (This)->lpVtbl -> GetAttributes(This,sfgaoMask,psfgaoAttribs) ) 

#define IShellItem_Compare(This,psi,hint,piOrder)	\
    ( (This)->lpVtbl -> Compare(This,psi,hint,piOrder) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IShellItem_INTERFACE_DEFINED__ */

#ifndef __IFileDialogEvents_INTERFACE_DEFINED__
#define __IFileDialogEvents_INTERFACE_DEFINED__

/* interface IFileDialogEvents */
/* [unique][object][uuid] */ 

EXTERN_C const IID IID_IFileDialogEvents;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("973510db-7d7f-452b-8975-74a85828d354")
    IFileDialogEvents : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE OnFileOk( 
            /* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnFolderChanging( 
            /* [in] */ __RPC__in_opt IFileDialog *pfd,
            /* [in] */ __RPC__in_opt IShellItem *psiFolder) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnFolderChange( 
            /* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnSelectionChange( 
            /* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnShareViolation( 
            /* [in] */ __RPC__in_opt IFileDialog *pfd,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [out] */ __RPC__out FDE_SHAREVIOLATION_RESPONSE *pResponse) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnTypeChange( 
            /* [in] */ __RPC__in_opt IFileDialog *pfd) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnOverwrite( 
            /* [in] */ __RPC__in_opt IFileDialog *pfd,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [out] */ __RPC__out FDE_OVERWRITE_RESPONSE *pResponse) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IFileDialogEventsVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFileDialogEvents * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFileDialogEvents * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFileDialogEvents * This);
        
        HRESULT ( STDMETHODCALLTYPE *OnFileOk )( 
            IFileDialogEvents * This,
            /* [in] */ __RPC__in_opt IFileDialog *pfd);
        
        HRESULT ( STDMETHODCALLTYPE *OnFolderChanging )( 
            IFileDialogEvents * This,
            /* [in] */ __RPC__in_opt IFileDialog *pfd,
            /* [in] */ __RPC__in_opt IShellItem *psiFolder);
        
        HRESULT ( STDMETHODCALLTYPE *OnFolderChange )( 
            IFileDialogEvents * This,
            /* [in] */ __RPC__in_opt IFileDialog *pfd);
        
        HRESULT ( STDMETHODCALLTYPE *OnSelectionChange )( 
            IFileDialogEvents * This,
            /* [in] */ __RPC__in_opt IFileDialog *pfd);
        
        HRESULT ( STDMETHODCALLTYPE *OnShareViolation )( 
            IFileDialogEvents * This,
            /* [in] */ __RPC__in_opt IFileDialog *pfd,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [out] */ __RPC__out FDE_SHAREVIOLATION_RESPONSE *pResponse);
        
        HRESULT ( STDMETHODCALLTYPE *OnTypeChange )( 
            IFileDialogEvents * This,
            /* [in] */ __RPC__in_opt IFileDialog *pfd);
        
        HRESULT ( STDMETHODCALLTYPE *OnOverwrite )( 
            IFileDialogEvents * This,
            /* [in] */ __RPC__in_opt IFileDialog *pfd,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [out] */ __RPC__out FDE_OVERWRITE_RESPONSE *pResponse);
        
        END_INTERFACE
    } IFileDialogEventsVtbl;

    interface IFileDialogEvents
    {
        CONST_VTBL struct IFileDialogEventsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFileDialogEvents_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFileDialogEvents_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFileDialogEvents_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFileDialogEvents_OnFileOk(This,pfd)	\
    ( (This)->lpVtbl -> OnFileOk(This,pfd) ) 

#define IFileDialogEvents_OnFolderChanging(This,pfd,psiFolder)	\
    ( (This)->lpVtbl -> OnFolderChanging(This,pfd,psiFolder) ) 

#define IFileDialogEvents_OnFolderChange(This,pfd)	\
    ( (This)->lpVtbl -> OnFolderChange(This,pfd) ) 

#define IFileDialogEvents_OnSelectionChange(This,pfd)	\
    ( (This)->lpVtbl -> OnSelectionChange(This,pfd) ) 

#define IFileDialogEvents_OnShareViolation(This,pfd,psi,pResponse)	\
    ( (This)->lpVtbl -> OnShareViolation(This,pfd,psi,pResponse) ) 

#define IFileDialogEvents_OnTypeChange(This,pfd)	\
    ( (This)->lpVtbl -> OnTypeChange(This,pfd) ) 

#define IFileDialogEvents_OnOverwrite(This,pfd,psi,pResponse)	\
    ( (This)->lpVtbl -> OnOverwrite(This,pfd,psi,pResponse) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFileDialogEvents_INTERFACE_DEFINED__ */


#ifndef __IModalWindow_INTERFACE_DEFINED__
#define __IModalWindow_INTERFACE_DEFINED__
    
/* interface IModalWindow */
/* [unique][object][uuid][helpstring] */ 
    
    
EXTERN_C const IID IID_IModalWindow;
    
#if defined(__cplusplus) && !defined(CINTERFACE)
    
MIDL_INTERFACE("b4db1657-70d7-485e-8e3e-6fcb5a5c1802")
IModalWindow : public IUnknown
{
    public:
      virtual /* [local] */ HRESULT STDMETHODCALLTYPE Show( 
        /* [in] */ 
        __in  HWND hwndParent) = 0;
      
    };
    
#else 	/* C style interface */
    
    typedef struct IModalWindowVtbl
    {
      BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
        IModalWindow * This,
        /* [in] */ __RPC__in REFIID riid,
        /* [iid_is][out] */ 
        __RPC__deref_out  void **ppvObject);
      
      ULONG ( STDMETHODCALLTYPE *AddRef )( 
        IModalWindow * This);
      
      ULONG ( STDMETHODCALLTYPE *Release )( 
        IModalWindow * This);
      
      /* [local] */ HRESULT ( STDMETHODCALLTYPE *Show )( 
      IModalWindow * This,
        /* [in] */ 
        __in  HWND hwndParent);
      
      END_INTERFACE
    } IModalWindowVtbl;
    
    interface IModalWindow
    {
      CONST_VTBL struct IModalWindowVtbl *lpVtbl;
    };
    
    
    
#ifdef COBJMACROS
    
    
#define IModalWindow_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 
    
#define IModalWindow_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 
    
#define IModalWindow_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 
    
    
#define IModalWindow_Show(This,hwndParent)	\
    ( (This)->lpVtbl -> Show(This,hwndParent) ) 
    
#endif /* COBJMACROS */
    
    
#endif 	/* C style interface */
    
    
    
    /* [call_as] */ HRESULT STDMETHODCALLTYPE IModalWindow_RemoteShow_Proxy( 
    IModalWindow * This,
      /* [in] */ __RPC__in HWND hwndParent);
      
      
      void __RPC_STUB IModalWindow_RemoteShow_Stub(
      IRpcStubBuffer *This,
      IRpcChannelBuffer *_pRpcChannelBuffer,
      PRPC_MESSAGE _pRpcMessage,
      DWORD *_pdwStubPhase);
    


#endif 	/* __IModalWindow_INTERFACE_DEFINED__ */

#ifndef __IShellItemFilter_INTERFACE_DEFINED__
#define __IShellItemFilter_INTERFACE_DEFINED__
    
    /* interface IShellItemFilter */
    /* [unique][uuid][object] */ 
    
    
    EXTERN_C const IID IID_IShellItemFilter;
    
#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2659B475-EEB8-48b7-8F07-B378810F48CF")
IShellItemFilter : public IUnknown
    {
    public:
      virtual HRESULT STDMETHODCALLTYPE IncludeItem( 
        /* [in] */ __RPC__in_opt IShellItem *psi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetEnumFlagsForItem( 
        /* [in] */ __RPC__in_opt IShellItem *psi,
        /* [out] */ __RPC__out SHCONTF *pgrfFlags) = 0;
        
    };
    
#else 	/* C style interface */
    
    typedef struct IShellItemFilterVtbl
    {
      BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
        IShellItemFilter * This,
        /* [in] */ __RPC__in REFIID riid,
        /* [iid_is][out] */ 
        __RPC__deref_out  void **ppvObject);
      
      ULONG ( STDMETHODCALLTYPE *AddRef )( 
        IShellItemFilter * This);
      
      ULONG ( STDMETHODCALLTYPE *Release )( 
        IShellItemFilter * This);
      
      HRESULT ( STDMETHODCALLTYPE *IncludeItem )( 
        IShellItemFilter * This,
        /* [in] */ __RPC__in_opt IShellItem *psi);
        
        HRESULT ( STDMETHODCALLTYPE *GetEnumFlagsForItem )( 
        IShellItemFilter * This,
        /* [in] */ __RPC__in_opt IShellItem *psi,
        /* [out] */ __RPC__out SHCONTF *pgrfFlags);
        
        END_INTERFACE
    } IShellItemFilterVtbl;
    
    interface IShellItemFilter
    {
      CONST_VTBL struct IShellItemFilterVtbl *lpVtbl;
    };
    
    
    
#ifdef COBJMACROS
    
    
#define IShellItemFilter_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 
    
#define IShellItemFilter_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 
    
#define IShellItemFilter_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 
    
    
#define IShellItemFilter_IncludeItem(This,psi)	\
    ( (This)->lpVtbl -> IncludeItem(This,psi) ) 
    
#define IShellItemFilter_GetEnumFlagsForItem(This,psi,pgrfFlags)	\
    ( (This)->lpVtbl -> GetEnumFlagsForItem(This,psi,pgrfFlags) ) 
    
#endif /* COBJMACROS */
    
    
#endif 	/* C style interface */
    
    
    
    
#endif 	/* __IShellItemFilter_INTERFACE_DEFINED__ */




#ifndef __IFileDialog_INTERFACE_DEFINED__
#define __IFileDialog_INTERFACE_DEFINED__

/* interface IFileDialog */
/* [unique][object][uuid] */ 


enum tagFILEOPENDIALOGOPTIONS
    {	FOS_OVERWRITEPROMPT	= 0x2,
	FOS_STRICTFILETYPES	= 0x4,
	FOS_NOCHANGEDIR	= 0x8,
	FOS_PICKFOLDERS	= 0x20,
	FOS_FORCEFILESYSTEM	= 0x40,
	FOS_ALLNONSTORAGEITEMS	= 0x80,
	FOS_NOVALIDATE	= 0x100,
	FOS_ALLOWMULTISELECT	= 0x200,
	FOS_PATHMUSTEXIST	= 0x800,
	FOS_FILEMUSTEXIST	= 0x1000,
	FOS_CREATEPROMPT	= 0x2000,
	FOS_SHAREAWARE	= 0x4000,
	FOS_NOREADONLYRETURN	= 0x8000,
	FOS_NOTESTFILECREATE	= 0x10000,
	FOS_HIDEMRUPLACES	= 0x20000,
	FOS_HIDEPINNEDPLACES	= 0x40000,
	FOS_NODEREFERENCELINKS	= 0x100000,
	FOS_DONTADDTORECENT	= 0x2000000,
	FOS_FORCESHOWHIDDEN	= 0x10000000,
	FOS_DEFAULTNOMINIMODE	= 0x20000000,
	FOS_FORCEPREVIEWPANEON	= 0x40000000
    } ;

EXTERN_C const IID IID_IFileDialog;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("42f85136-db7e-439c-85f1-e4075d135fc8")
    IFileDialog : public IModalWindow
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetFileTypes( 
            /* [in] */ UINT cFileTypes,
            /* [size_is][in] */ __RPC__in_ecount_full(cFileTypes) const COMDLG_FILTERSPEC *rgFilterSpec) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetFileTypeIndex( 
            /* [in] */ UINT iFileType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetFileTypeIndex( 
            /* [out] */ __RPC__out UINT *piFileType) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Advise( 
            /* [in] */ __RPC__in_opt IFileDialogEvents *pfde,
            /* [out] */ __RPC__out DWORD *pdwCookie) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Unadvise( 
            /* [in] */ DWORD dwCookie) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetOptions( 
            /* [in] */ DWORD fos) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetOptions( 
            /* [out] */ __RPC__out DWORD *pfos) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetDefaultFolder( 
            /* [in] */ __RPC__in_opt IShellItem *psi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetFolder( 
            /* [in] */ __RPC__in_opt IShellItem *psi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetFolder( 
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCurrentSelection( 
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetFileName( 
            /* [string][in] */ __RPC__in LPCWSTR pszName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetFileName( 
            /* [string][out] */ __RPC__deref_out_opt_string LPWSTR *pszName) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetTitle( 
            /* [string][in] */ __RPC__in LPCWSTR pszTitle) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetOkButtonLabel( 
            /* [string][in] */ __RPC__in LPCWSTR pszText) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetFileNameLabel( 
            /* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetResult( 
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddPlace( 
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [in] */ FDAP fdap) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetDefaultExtension( 
            /* [string][in] */ __RPC__in LPCWSTR pszDefaultExtension) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Close( 
            /* [in] */ HRESULT hr) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetClientGuid( 
            /* [in] */ __RPC__in REFGUID guid) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ClearClientData( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetFilter( 
            /* [in] */ __RPC__in_opt IShellItemFilter *pFilter) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IFileDialogVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFileDialog * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFileDialog * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFileDialog * This);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Show )( 
            IFileDialog * This,
            /* [in] */ 
            __in  HWND hwndParent);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileTypes )( 
            IFileDialog * This,
            /* [in] */ UINT cFileTypes,
            /* [size_is][in] */ __RPC__in_ecount_full(cFileTypes) const COMDLG_FILTERSPEC *rgFilterSpec);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileTypeIndex )( 
            IFileDialog * This,
            /* [in] */ UINT iFileType);
        
        HRESULT ( STDMETHODCALLTYPE *GetFileTypeIndex )( 
            IFileDialog * This,
            /* [out] */ __RPC__out UINT *piFileType);
        
        HRESULT ( STDMETHODCALLTYPE *Advise )( 
            IFileDialog * This,
            /* [in] */ __RPC__in_opt IFileDialogEvents *pfde,
            /* [out] */ __RPC__out DWORD *pdwCookie);
        
        HRESULT ( STDMETHODCALLTYPE *Unadvise )( 
            IFileDialog * This,
            /* [in] */ DWORD dwCookie);
        
        HRESULT ( STDMETHODCALLTYPE *SetOptions )( 
            IFileDialog * This,
            /* [in] */ DWORD fos);
        
        HRESULT ( STDMETHODCALLTYPE *GetOptions )( 
            IFileDialog * This,
            /* [out] */ __RPC__out DWORD *pfos);
        
        HRESULT ( STDMETHODCALLTYPE *SetDefaultFolder )( 
            IFileDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi);
        
        HRESULT ( STDMETHODCALLTYPE *SetFolder )( 
            IFileDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi);
        
        HRESULT ( STDMETHODCALLTYPE *GetFolder )( 
            IFileDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *GetCurrentSelection )( 
            IFileDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileName )( 
            IFileDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszName);
        
        HRESULT ( STDMETHODCALLTYPE *GetFileName )( 
            IFileDialog * This,
            /* [string][out] */ __RPC__deref_out_opt_string LPWSTR *pszName);
        
        HRESULT ( STDMETHODCALLTYPE *SetTitle )( 
            IFileDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszTitle);
        
        HRESULT ( STDMETHODCALLTYPE *SetOkButtonLabel )( 
            IFileDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszText);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileNameLabel )( 
            IFileDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel);
        
        HRESULT ( STDMETHODCALLTYPE *GetResult )( 
            IFileDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *AddPlace )( 
            IFileDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [in] */ FDAP fdap);
        
        HRESULT ( STDMETHODCALLTYPE *SetDefaultExtension )( 
            IFileDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszDefaultExtension);
        
        HRESULT ( STDMETHODCALLTYPE *Close )( 
            IFileDialog * This,
            /* [in] */ HRESULT hr);
        
        HRESULT ( STDMETHODCALLTYPE *SetClientGuid )( 
            IFileDialog * This,
            /* [in] */ __RPC__in REFGUID guid);
        
        HRESULT ( STDMETHODCALLTYPE *ClearClientData )( 
            IFileDialog * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetFilter )( 
            IFileDialog * This,
            /* [in] */ __RPC__in_opt IShellItemFilter *pFilter);
        
        END_INTERFACE
    } IFileDialogVtbl;

    interface IFileDialog
    {
        CONST_VTBL struct IFileDialogVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFileDialog_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFileDialog_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFileDialog_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFileDialog_Show(This,hwndParent)	\
    ( (This)->lpVtbl -> Show(This,hwndParent) ) 


#define IFileDialog_SetFileTypes(This,cFileTypes,rgFilterSpec)	\
    ( (This)->lpVtbl -> SetFileTypes(This,cFileTypes,rgFilterSpec) ) 

#define IFileDialog_SetFileTypeIndex(This,iFileType)	\
    ( (This)->lpVtbl -> SetFileTypeIndex(This,iFileType) ) 

#define IFileDialog_GetFileTypeIndex(This,piFileType)	\
    ( (This)->lpVtbl -> GetFileTypeIndex(This,piFileType) ) 

#define IFileDialog_Advise(This,pfde,pdwCookie)	\
    ( (This)->lpVtbl -> Advise(This,pfde,pdwCookie) ) 

#define IFileDialog_Unadvise(This,dwCookie)	\
    ( (This)->lpVtbl -> Unadvise(This,dwCookie) ) 

#define IFileDialog_SetOptions(This,fos)	\
    ( (This)->lpVtbl -> SetOptions(This,fos) ) 

#define IFileDialog_GetOptions(This,pfos)	\
    ( (This)->lpVtbl -> GetOptions(This,pfos) ) 

#define IFileDialog_SetDefaultFolder(This,psi)	\
    ( (This)->lpVtbl -> SetDefaultFolder(This,psi) ) 

#define IFileDialog_SetFolder(This,psi)	\
    ( (This)->lpVtbl -> SetFolder(This,psi) ) 

#define IFileDialog_GetFolder(This,ppsi)	\
    ( (This)->lpVtbl -> GetFolder(This,ppsi) ) 

#define IFileDialog_GetCurrentSelection(This,ppsi)	\
    ( (This)->lpVtbl -> GetCurrentSelection(This,ppsi) ) 

#define IFileDialog_SetFileName(This,pszName)	\
    ( (This)->lpVtbl -> SetFileName(This,pszName) ) 

#define IFileDialog_GetFileName(This,pszName)	\
    ( (This)->lpVtbl -> GetFileName(This,pszName) ) 

#define IFileDialog_SetTitle(This,pszTitle)	\
    ( (This)->lpVtbl -> SetTitle(This,pszTitle) ) 

#define IFileDialog_SetOkButtonLabel(This,pszText)	\
    ( (This)->lpVtbl -> SetOkButtonLabel(This,pszText) ) 

#define IFileDialog_SetFileNameLabel(This,pszLabel)	\
    ( (This)->lpVtbl -> SetFileNameLabel(This,pszLabel) ) 

#define IFileDialog_GetResult(This,ppsi)	\
    ( (This)->lpVtbl -> GetResult(This,ppsi) ) 

#define IFileDialog_AddPlace(This,psi,fdap)	\
    ( (This)->lpVtbl -> AddPlace(This,psi,fdap) ) 

#define IFileDialog_SetDefaultExtension(This,pszDefaultExtension)	\
    ( (This)->lpVtbl -> SetDefaultExtension(This,pszDefaultExtension) ) 

#define IFileDialog_Close(This,hr)	\
    ( (This)->lpVtbl -> Close(This,hr) ) 

#define IFileDialog_SetClientGuid(This,guid)	\
    ( (This)->lpVtbl -> SetClientGuid(This,guid) ) 

#define IFileDialog_ClearClientData(This)	\
    ( (This)->lpVtbl -> ClearClientData(This) ) 

#define IFileDialog_SetFilter(This,pFilter)	\
    ( (This)->lpVtbl -> SetFilter(This,pFilter) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */


#ifndef __IEnumShellItems_INTERFACE_DEFINED__
#define __IEnumShellItems_INTERFACE_DEFINED__

/* interface IEnumShellItems */
/* [unique][object][uuid][helpstring] */ 


EXTERN_C const IID IID_IEnumShellItems;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("70629033-e363-4a28-a567-0db78006e6d7")
    IEnumShellItems : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE Next( 
            /* [in] */ ULONG celt,
            /* [length_is][size_is][out] */ __RPC__out_ecount_part(celt, *pceltFetched) IShellItem **rgelt,
            /* [out] */ __RPC__out ULONG *pceltFetched) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Skip( 
            /* [in] */ ULONG celt) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Reset( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE Clone( 
            /* [out] */ __RPC__deref_out_opt IEnumShellItems **ppenum) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IEnumShellItemsVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IEnumShellItems * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IEnumShellItems * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IEnumShellItems * This);
        
        HRESULT ( STDMETHODCALLTYPE *Next )( 
            IEnumShellItems * This,
            /* [in] */ ULONG celt,
            /* [length_is][size_is][out] */ __RPC__out_ecount_part(celt, *pceltFetched) IShellItem **rgelt,
            /* [out] */ __RPC__out ULONG *pceltFetched);
        
        HRESULT ( STDMETHODCALLTYPE *Skip )( 
            IEnumShellItems * This,
            /* [in] */ ULONG celt);
        
        HRESULT ( STDMETHODCALLTYPE *Reset )( 
            IEnumShellItems * This);
        
        HRESULT ( STDMETHODCALLTYPE *Clone )( 
            IEnumShellItems * This,
            /* [out] */ __RPC__deref_out_opt IEnumShellItems **ppenum);
        
        END_INTERFACE
    } IEnumShellItemsVtbl;

    interface IEnumShellItems
    {
        CONST_VTBL struct IEnumShellItemsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IEnumShellItems_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IEnumShellItems_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IEnumShellItems_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IEnumShellItems_Next(This,celt,rgelt,pceltFetched)	\
    ( (This)->lpVtbl -> Next(This,celt,rgelt,pceltFetched) ) 

#define IEnumShellItems_Skip(This,celt)	\
    ( (This)->lpVtbl -> Skip(This,celt) ) 

#define IEnumShellItems_Reset(This)	\
    ( (This)->lpVtbl -> Reset(This) ) 

#define IEnumShellItems_Clone(This,ppenum)	\
    ( (This)->lpVtbl -> Clone(This,ppenum) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IEnumShellItems_INTERFACE_DEFINED__ */


#ifndef __IShellItemArray_INTERFACE_DEFINED__
#define __IShellItemArray_INTERFACE_DEFINED__

/* interface IShellItemArray */
/* [unique][object][uuid][helpstring] */ 

typedef /* [v1_enum] */ 
enum tagSIATTRIBFLAGS
    {	SIATTRIBFLAGS_AND	= 0x1,
	SIATTRIBFLAGS_OR	= 0x2,
	SIATTRIBFLAGS_APPCOMPAT	= 0x3,
	SIATTRIBFLAGS_MASK	= 0x3
    } 	SIATTRIBFLAGS;


EXTERN_C const IID IID_IShellItemArray;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("b63ea76d-1f85-456f-a19c-48159efa858b")
    IShellItemArray : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE BindToHandler( 
            /* [unique][in] */ __RPC__in_opt IBindCtx *pbc,
            /* [in] */ __RPC__in REFGUID rbhid,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ __RPC__deref_out_opt void **ppvOut) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetPropertyStore( 
            /* [in] */ GETPROPERTYSTOREFLAGS flags,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ __RPC__deref_out_opt void **ppv) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetPropertyDescriptionList( 
            /* [in] */ __RPC__in REFPROPERTYKEY keyType,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ __RPC__deref_out_opt void **ppv) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetAttributes( 
            /* [in] */ SIATTRIBFLAGS dwAttribFlags,
            /* [in] */ SFGAOF sfgaoMask,
            /* [out] */ __RPC__out SFGAOF *psfgaoAttribs) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCount( 
            /* [out] */ __RPC__out DWORD *pdwNumItems) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetItemAt( 
            /* [in] */ DWORD dwIndex,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE EnumItems( 
            /* [out] */ __RPC__deref_out_opt IEnumShellItems **ppenumShellItems) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IShellItemArrayVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IShellItemArray * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IShellItemArray * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IShellItemArray * This);
        
        HRESULT ( STDMETHODCALLTYPE *BindToHandler )( 
            IShellItemArray * This,
            /* [unique][in] */ __RPC__in_opt IBindCtx *pbc,
            /* [in] */ __RPC__in REFGUID rbhid,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ __RPC__deref_out_opt void **ppvOut);
        
        HRESULT ( STDMETHODCALLTYPE *GetPropertyStore )( 
            IShellItemArray * This,
            /* [in] */ GETPROPERTYSTOREFLAGS flags,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ __RPC__deref_out_opt void **ppv);
        
        HRESULT ( STDMETHODCALLTYPE *GetPropertyDescriptionList )( 
            IShellItemArray * This,
            /* [in] */ __RPC__in REFPROPERTYKEY keyType,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ __RPC__deref_out_opt void **ppv);
        
        HRESULT ( STDMETHODCALLTYPE *GetAttributes )( 
            IShellItemArray * This,
            /* [in] */ SIATTRIBFLAGS dwAttribFlags,
            /* [in] */ SFGAOF sfgaoMask,
            /* [out] */ __RPC__out SFGAOF *psfgaoAttribs);
        
        HRESULT ( STDMETHODCALLTYPE *GetCount )( 
            IShellItemArray * This,
            /* [out] */ __RPC__out DWORD *pdwNumItems);
        
        HRESULT ( STDMETHODCALLTYPE *GetItemAt )( 
            IShellItemArray * This,
            /* [in] */ DWORD dwIndex,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *EnumItems )( 
            IShellItemArray * This,
            /* [out] */ __RPC__deref_out_opt IEnumShellItems **ppenumShellItems);
        
        END_INTERFACE
    } IShellItemArrayVtbl;

    interface IShellItemArray
    {
        CONST_VTBL struct IShellItemArrayVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IShellItemArray_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IShellItemArray_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IShellItemArray_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IShellItemArray_BindToHandler(This,pbc,rbhid,riid,ppvOut)	\
    ( (This)->lpVtbl -> BindToHandler(This,pbc,rbhid,riid,ppvOut) ) 

#define IShellItemArray_GetPropertyStore(This,flags,riid,ppv)	\
    ( (This)->lpVtbl -> GetPropertyStore(This,flags,riid,ppv) ) 

#define IShellItemArray_GetPropertyDescriptionList(This,keyType,riid,ppv)	\
    ( (This)->lpVtbl -> GetPropertyDescriptionList(This,keyType,riid,ppv) ) 

#define IShellItemArray_GetAttributes(This,dwAttribFlags,sfgaoMask,psfgaoAttribs)	\
    ( (This)->lpVtbl -> GetAttributes(This,dwAttribFlags,sfgaoMask,psfgaoAttribs) ) 

#define IShellItemArray_GetCount(This,pdwNumItems)	\
    ( (This)->lpVtbl -> GetCount(This,pdwNumItems) ) 

#define IShellItemArray_GetItemAt(This,dwIndex,ppsi)	\
    ( (This)->lpVtbl -> GetItemAt(This,dwIndex,ppsi) ) 

#define IShellItemArray_EnumItems(This,ppenumShellItems)	\
    ( (This)->lpVtbl -> EnumItems(This,ppenumShellItems) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IShellItemArray_INTERFACE_DEFINED__ */





#endif 	/* __IFileDialog_INTERFACE_DEFINED__ */

#ifndef __IFileOpenDialog_INTERFACE_DEFINED__
#define __IFileOpenDialog_INTERFACE_DEFINED__

/* interface IFileOpenDialog */
/* [unique][object][uuid] */ 


EXTERN_C const IID IID_IFileOpenDialog;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("d57c7288-d4ad-4768-be02-9d969532d960")
    IFileOpenDialog : public IFileDialog
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetResults( 
            /* [out] */ __RPC__deref_out_opt IShellItemArray **ppenum) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSelectedItems( 
            /* [out] */ __RPC__deref_out_opt IShellItemArray **ppsai) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IFileOpenDialogVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFileOpenDialog * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFileOpenDialog * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFileOpenDialog * This);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Show )( 
            IFileOpenDialog * This,
            /* [in] */ 
            __in  HWND hwndParent);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileTypes )( 
            IFileOpenDialog * This,
            /* [in] */ UINT cFileTypes,
            /* [size_is][in] */ __RPC__in_ecount_full(cFileTypes) const COMDLG_FILTERSPEC *rgFilterSpec);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileTypeIndex )( 
            IFileOpenDialog * This,
            /* [in] */ UINT iFileType);
        
        HRESULT ( STDMETHODCALLTYPE *GetFileTypeIndex )( 
            IFileOpenDialog * This,
            /* [out] */ __RPC__out UINT *piFileType);
        
        HRESULT ( STDMETHODCALLTYPE *Advise )( 
            IFileOpenDialog * This,
            /* [in] */ __RPC__in_opt IFileDialogEvents *pfde,
            /* [out] */ __RPC__out DWORD *pdwCookie);
        
        HRESULT ( STDMETHODCALLTYPE *Unadvise )( 
            IFileOpenDialog * This,
            /* [in] */ DWORD dwCookie);
        
        HRESULT ( STDMETHODCALLTYPE *SetOptions )( 
            IFileOpenDialog * This,
            /* [in] */ DWORD fos);
        
        HRESULT ( STDMETHODCALLTYPE *GetOptions )( 
            IFileOpenDialog * This,
            /* [out] */ __RPC__out DWORD *pfos);
        
        HRESULT ( STDMETHODCALLTYPE *SetDefaultFolder )( 
            IFileOpenDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi);
        
        HRESULT ( STDMETHODCALLTYPE *SetFolder )( 
            IFileOpenDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi);
        
        HRESULT ( STDMETHODCALLTYPE *GetFolder )( 
            IFileOpenDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *GetCurrentSelection )( 
            IFileOpenDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileName )( 
            IFileOpenDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszName);
        
        HRESULT ( STDMETHODCALLTYPE *GetFileName )( 
            IFileOpenDialog * This,
            /* [string][out] */ __RPC__deref_out_opt_string LPWSTR *pszName);
        
        HRESULT ( STDMETHODCALLTYPE *SetTitle )( 
            IFileOpenDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszTitle);
        
        HRESULT ( STDMETHODCALLTYPE *SetOkButtonLabel )( 
            IFileOpenDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszText);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileNameLabel )( 
            IFileOpenDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel);
        
        HRESULT ( STDMETHODCALLTYPE *GetResult )( 
            IFileOpenDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *AddPlace )( 
            IFileOpenDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [in] */ FDAP fdap);
        
        HRESULT ( STDMETHODCALLTYPE *SetDefaultExtension )( 
            IFileOpenDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszDefaultExtension);
        
        HRESULT ( STDMETHODCALLTYPE *Close )( 
            IFileOpenDialog * This,
            /* [in] */ HRESULT hr);
        
        HRESULT ( STDMETHODCALLTYPE *SetClientGuid )( 
            IFileOpenDialog * This,
            /* [in] */ __RPC__in REFGUID guid);
        
        HRESULT ( STDMETHODCALLTYPE *ClearClientData )( 
            IFileOpenDialog * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetFilter )( 
            IFileOpenDialog * This,
            /* [in] */ __RPC__in_opt IShellItemFilter *pFilter);
        
        HRESULT ( STDMETHODCALLTYPE *GetResults )( 
            IFileOpenDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItemArray **ppenum);
        
        HRESULT ( STDMETHODCALLTYPE *GetSelectedItems )( 
            IFileOpenDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItemArray **ppsai);
        
        END_INTERFACE
    } IFileOpenDialogVtbl;

    interface IFileOpenDialog
    {
        CONST_VTBL struct IFileOpenDialogVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFileOpenDialog_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFileOpenDialog_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFileOpenDialog_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFileOpenDialog_Show(This,hwndParent)	\
    ( (This)->lpVtbl -> Show(This,hwndParent) ) 


#define IFileOpenDialog_SetFileTypes(This,cFileTypes,rgFilterSpec)	\
    ( (This)->lpVtbl -> SetFileTypes(This,cFileTypes,rgFilterSpec) ) 

#define IFileOpenDialog_SetFileTypeIndex(This,iFileType)	\
    ( (This)->lpVtbl -> SetFileTypeIndex(This,iFileType) ) 

#define IFileOpenDialog_GetFileTypeIndex(This,piFileType)	\
    ( (This)->lpVtbl -> GetFileTypeIndex(This,piFileType) ) 

#define IFileOpenDialog_Advise(This,pfde,pdwCookie)	\
    ( (This)->lpVtbl -> Advise(This,pfde,pdwCookie) ) 

#define IFileOpenDialog_Unadvise(This,dwCookie)	\
    ( (This)->lpVtbl -> Unadvise(This,dwCookie) ) 

#define IFileOpenDialog_SetOptions(This,fos)	\
    ( (This)->lpVtbl -> SetOptions(This,fos) ) 

#define IFileOpenDialog_GetOptions(This,pfos)	\
    ( (This)->lpVtbl -> GetOptions(This,pfos) ) 

#define IFileOpenDialog_SetDefaultFolder(This,psi)	\
    ( (This)->lpVtbl -> SetDefaultFolder(This,psi) ) 

#define IFileOpenDialog_SetFolder(This,psi)	\
    ( (This)->lpVtbl -> SetFolder(This,psi) ) 

#define IFileOpenDialog_GetFolder(This,ppsi)	\
    ( (This)->lpVtbl -> GetFolder(This,ppsi) ) 

#define IFileOpenDialog_GetCurrentSelection(This,ppsi)	\
    ( (This)->lpVtbl -> GetCurrentSelection(This,ppsi) ) 

#define IFileOpenDialog_SetFileName(This,pszName)	\
    ( (This)->lpVtbl -> SetFileName(This,pszName) ) 

#define IFileOpenDialog_GetFileName(This,pszName)	\
    ( (This)->lpVtbl -> GetFileName(This,pszName) ) 

#define IFileOpenDialog_SetTitle(This,pszTitle)	\
    ( (This)->lpVtbl -> SetTitle(This,pszTitle) ) 

#define IFileOpenDialog_SetOkButtonLabel(This,pszText)	\
    ( (This)->lpVtbl -> SetOkButtonLabel(This,pszText) ) 

#define IFileOpenDialog_SetFileNameLabel(This,pszLabel)	\
    ( (This)->lpVtbl -> SetFileNameLabel(This,pszLabel) ) 

#define IFileOpenDialog_GetResult(This,ppsi)	\
    ( (This)->lpVtbl -> GetResult(This,ppsi) ) 

#define IFileOpenDialog_AddPlace(This,psi,fdap)	\
    ( (This)->lpVtbl -> AddPlace(This,psi,fdap) ) 

#define IFileOpenDialog_SetDefaultExtension(This,pszDefaultExtension)	\
    ( (This)->lpVtbl -> SetDefaultExtension(This,pszDefaultExtension) ) 

#define IFileOpenDialog_Close(This,hr)	\
    ( (This)->lpVtbl -> Close(This,hr) ) 

#define IFileOpenDialog_SetClientGuid(This,guid)	\
    ( (This)->lpVtbl -> SetClientGuid(This,guid) ) 

#define IFileOpenDialog_ClearClientData(This)	\
    ( (This)->lpVtbl -> ClearClientData(This) ) 

#define IFileOpenDialog_SetFilter(This,pFilter)	\
    ( (This)->lpVtbl -> SetFilter(This,pFilter) ) 


#define IFileOpenDialog_GetResults(This,ppenum)	\
    ( (This)->lpVtbl -> GetResults(This,ppenum) ) 

#define IFileOpenDialog_GetSelectedItems(This,ppsai)	\
    ( (This)->lpVtbl -> GetSelectedItems(This,ppsai) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFileOpenDialog_INTERFACE_DEFINED__ */


#ifndef __IFileDialogCustomize_INTERFACE_DEFINED__
#define __IFileDialogCustomize_INTERFACE_DEFINED__

/* interface IFileDialogCustomize */
/* [unique][object][uuid] */ 


EXTERN_C const IID IID_IFileDialogCustomize;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("e6fdd21a-163f-4975-9c8c-a69f1ba37034")
    IFileDialogCustomize : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE EnableOpenDropDown( 
            /* [in] */ DWORD dwIDCtl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddMenu( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddPushButton( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddComboBox( 
            /* [in] */ DWORD dwIDCtl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddRadioButtonList( 
            /* [in] */ DWORD dwIDCtl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddCheckButton( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel,
            /* [in] */ BOOL bChecked) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddEditBox( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszText) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddSeparator( 
            /* [in] */ DWORD dwIDCtl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddText( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszText) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetControlLabel( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetControlState( 
            /* [in] */ DWORD dwIDCtl,
            /* [out] */ __RPC__out CDCONTROLSTATEF *pdwState) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetControlState( 
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ CDCONTROLSTATEF dwState) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetEditBoxText( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][out] */ __RPC__deref_out_opt_string WCHAR **ppszText) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetEditBoxText( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszText) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetCheckButtonState( 
            /* [in] */ DWORD dwIDCtl,
            /* [out] */ __RPC__out BOOL *pbChecked) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetCheckButtonState( 
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ BOOL bChecked) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddControlItem( 
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem,
            /* [in] */ __RPC__in LPCWSTR pszLabel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RemoveControlItem( 
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE RemoveAllControlItems( 
            /* [in] */ DWORD dwIDCtl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetControlItemState( 
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem,
            /* [out] */ __RPC__out CDCONTROLSTATEF *pdwState) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetControlItemState( 
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem,
            /* [in] */ CDCONTROLSTATEF dwState) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetSelectedControlItem( 
            /* [in] */ DWORD dwIDCtl,
            /* [out] */ __RPC__out DWORD *pdwIDItem) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetSelectedControlItem( 
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE StartVisualGroup( 
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE EndVisualGroup( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE MakeProminent( 
            /* [in] */ DWORD dwIDCtl) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetControlItemText( 
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IFileDialogCustomizeVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFileDialogCustomize * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFileDialogCustomize * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFileDialogCustomize * This);
        
        HRESULT ( STDMETHODCALLTYPE *EnableOpenDropDown )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl);
        
        HRESULT ( STDMETHODCALLTYPE *AddMenu )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel);
        
        HRESULT ( STDMETHODCALLTYPE *AddPushButton )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel);
        
        HRESULT ( STDMETHODCALLTYPE *AddComboBox )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl);
        
        HRESULT ( STDMETHODCALLTYPE *AddRadioButtonList )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl);
        
        HRESULT ( STDMETHODCALLTYPE *AddCheckButton )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel,
            /* [in] */ BOOL bChecked);
        
        HRESULT ( STDMETHODCALLTYPE *AddEditBox )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszText);
        
        HRESULT ( STDMETHODCALLTYPE *AddSeparator )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl);
        
        HRESULT ( STDMETHODCALLTYPE *AddText )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszText);
        
        HRESULT ( STDMETHODCALLTYPE *SetControlLabel )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel);
        
        HRESULT ( STDMETHODCALLTYPE *GetControlState )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [out] */ __RPC__out CDCONTROLSTATEF *pdwState);
        
        HRESULT ( STDMETHODCALLTYPE *SetControlState )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ CDCONTROLSTATEF dwState);
        
        HRESULT ( STDMETHODCALLTYPE *GetEditBoxText )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][out] */ __RPC__deref_out_opt_string WCHAR **ppszText);
        
        HRESULT ( STDMETHODCALLTYPE *SetEditBoxText )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszText);
        
        HRESULT ( STDMETHODCALLTYPE *GetCheckButtonState )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [out] */ __RPC__out BOOL *pbChecked);
        
        HRESULT ( STDMETHODCALLTYPE *SetCheckButtonState )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ BOOL bChecked);
        
        HRESULT ( STDMETHODCALLTYPE *AddControlItem )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem,
            /* [in] */ __RPC__in LPCWSTR pszLabel);
        
        HRESULT ( STDMETHODCALLTYPE *RemoveControlItem )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem);
        
        HRESULT ( STDMETHODCALLTYPE *RemoveAllControlItems )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl);
        
        HRESULT ( STDMETHODCALLTYPE *GetControlItemState )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem,
            /* [out] */ __RPC__out CDCONTROLSTATEF *pdwState);
        
        HRESULT ( STDMETHODCALLTYPE *SetControlItemState )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem,
            /* [in] */ CDCONTROLSTATEF dwState);
        
        HRESULT ( STDMETHODCALLTYPE *GetSelectedControlItem )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [out] */ __RPC__out DWORD *pdwIDItem);
        
        HRESULT ( STDMETHODCALLTYPE *SetSelectedControlItem )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem);
        
        HRESULT ( STDMETHODCALLTYPE *StartVisualGroup )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel);
        
        HRESULT ( STDMETHODCALLTYPE *EndVisualGroup )( 
            IFileDialogCustomize * This);
        
        HRESULT ( STDMETHODCALLTYPE *MakeProminent )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl);
        
        HRESULT ( STDMETHODCALLTYPE *SetControlItemText )( 
            IFileDialogCustomize * This,
            /* [in] */ DWORD dwIDCtl,
            /* [in] */ DWORD dwIDItem,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel);
        
        END_INTERFACE
    } IFileDialogCustomizeVtbl;

    interface IFileDialogCustomize
    {
        CONST_VTBL struct IFileDialogCustomizeVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFileDialogCustomize_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFileDialogCustomize_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFileDialogCustomize_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFileDialogCustomize_EnableOpenDropDown(This,dwIDCtl)	\
    ( (This)->lpVtbl -> EnableOpenDropDown(This,dwIDCtl) ) 

#define IFileDialogCustomize_AddMenu(This,dwIDCtl,pszLabel)	\
    ( (This)->lpVtbl -> AddMenu(This,dwIDCtl,pszLabel) ) 

#define IFileDialogCustomize_AddPushButton(This,dwIDCtl,pszLabel)	\
    ( (This)->lpVtbl -> AddPushButton(This,dwIDCtl,pszLabel) ) 

#define IFileDialogCustomize_AddComboBox(This,dwIDCtl)	\
    ( (This)->lpVtbl -> AddComboBox(This,dwIDCtl) ) 

#define IFileDialogCustomize_AddRadioButtonList(This,dwIDCtl)	\
    ( (This)->lpVtbl -> AddRadioButtonList(This,dwIDCtl) ) 

#define IFileDialogCustomize_AddCheckButton(This,dwIDCtl,pszLabel,bChecked)	\
    ( (This)->lpVtbl -> AddCheckButton(This,dwIDCtl,pszLabel,bChecked) ) 

#define IFileDialogCustomize_AddEditBox(This,dwIDCtl,pszText)	\
    ( (This)->lpVtbl -> AddEditBox(This,dwIDCtl,pszText) ) 

#define IFileDialogCustomize_AddSeparator(This,dwIDCtl)	\
    ( (This)->lpVtbl -> AddSeparator(This,dwIDCtl) ) 

#define IFileDialogCustomize_AddText(This,dwIDCtl,pszText)	\
    ( (This)->lpVtbl -> AddText(This,dwIDCtl,pszText) ) 

#define IFileDialogCustomize_SetControlLabel(This,dwIDCtl,pszLabel)	\
    ( (This)->lpVtbl -> SetControlLabel(This,dwIDCtl,pszLabel) ) 

#define IFileDialogCustomize_GetControlState(This,dwIDCtl,pdwState)	\
    ( (This)->lpVtbl -> GetControlState(This,dwIDCtl,pdwState) ) 

#define IFileDialogCustomize_SetControlState(This,dwIDCtl,dwState)	\
    ( (This)->lpVtbl -> SetControlState(This,dwIDCtl,dwState) ) 

#define IFileDialogCustomize_GetEditBoxText(This,dwIDCtl,ppszText)	\
    ( (This)->lpVtbl -> GetEditBoxText(This,dwIDCtl,ppszText) ) 

#define IFileDialogCustomize_SetEditBoxText(This,dwIDCtl,pszText)	\
    ( (This)->lpVtbl -> SetEditBoxText(This,dwIDCtl,pszText) ) 

#define IFileDialogCustomize_GetCheckButtonState(This,dwIDCtl,pbChecked)	\
    ( (This)->lpVtbl -> GetCheckButtonState(This,dwIDCtl,pbChecked) ) 

#define IFileDialogCustomize_SetCheckButtonState(This,dwIDCtl,bChecked)	\
    ( (This)->lpVtbl -> SetCheckButtonState(This,dwIDCtl,bChecked) ) 

#define IFileDialogCustomize_AddControlItem(This,dwIDCtl,dwIDItem,pszLabel)	\
    ( (This)->lpVtbl -> AddControlItem(This,dwIDCtl,dwIDItem,pszLabel) ) 

#define IFileDialogCustomize_RemoveControlItem(This,dwIDCtl,dwIDItem)	\
    ( (This)->lpVtbl -> RemoveControlItem(This,dwIDCtl,dwIDItem) ) 

#define IFileDialogCustomize_RemoveAllControlItems(This,dwIDCtl)	\
    ( (This)->lpVtbl -> RemoveAllControlItems(This,dwIDCtl) ) 

#define IFileDialogCustomize_GetControlItemState(This,dwIDCtl,dwIDItem,pdwState)	\
    ( (This)->lpVtbl -> GetControlItemState(This,dwIDCtl,dwIDItem,pdwState) ) 

#define IFileDialogCustomize_SetControlItemState(This,dwIDCtl,dwIDItem,dwState)	\
    ( (This)->lpVtbl -> SetControlItemState(This,dwIDCtl,dwIDItem,dwState) ) 

#define IFileDialogCustomize_GetSelectedControlItem(This,dwIDCtl,pdwIDItem)	\
    ( (This)->lpVtbl -> GetSelectedControlItem(This,dwIDCtl,pdwIDItem) ) 

#define IFileDialogCustomize_SetSelectedControlItem(This,dwIDCtl,dwIDItem)	\
    ( (This)->lpVtbl -> SetSelectedControlItem(This,dwIDCtl,dwIDItem) ) 

#define IFileDialogCustomize_StartVisualGroup(This,dwIDCtl,pszLabel)	\
    ( (This)->lpVtbl -> StartVisualGroup(This,dwIDCtl,pszLabel) ) 

#define IFileDialogCustomize_EndVisualGroup(This)	\
    ( (This)->lpVtbl -> EndVisualGroup(This) ) 

#define IFileDialogCustomize_MakeProminent(This,dwIDCtl)	\
    ( (This)->lpVtbl -> MakeProminent(This,dwIDCtl) ) 

#define IFileDialogCustomize_SetControlItemText(This,dwIDCtl,dwIDItem,pszLabel)	\
    ( (This)->lpVtbl -> SetControlItemText(This,dwIDCtl,dwIDItem,pszLabel) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFileDialogCustomize_INTERFACE_DEFINED__ */


#ifndef __IFileSaveDialog_INTERFACE_DEFINED__
#define __IFileSaveDialog_INTERFACE_DEFINED__

/* interface IFileSaveDialog */
/* [unique][object][uuid] */ 


EXTERN_C const IID IID_IFileSaveDialog;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("84bccd23-5fde-4cdb-aea4-af64b83d78ab")
    IFileSaveDialog : public IFileDialog
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetSaveAsItem( 
            /* [in] */ __RPC__in_opt IShellItem *psi) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetProperties( 
            /* [in] */ __RPC__in_opt IPropertyStore *pStore) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SetCollectedProperties( 
            /* [in] */ __RPC__in_opt IPropertyDescriptionList *pList,
            /* [in] */ BOOL fAppendDefault) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE GetProperties( 
            /* [out] */ __RPC__deref_out_opt IPropertyStore **ppStore) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ApplyProperties( 
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [in] */ __RPC__in_opt IPropertyStore *pStore,
            /* [unique][in] */ __RPC__in_opt HWND hwnd,
            /* [unique][in] */ __RPC__in_opt IFileOperationProgressSink *pSink) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IFileSaveDialogVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IFileSaveDialog * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IFileSaveDialog * This);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Show )( 
            IFileSaveDialog * This,
            /* [in] */ 
            __in  HWND hwndParent);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileTypes )( 
            IFileSaveDialog * This,
            /* [in] */ UINT cFileTypes,
            /* [size_is][in] */ __RPC__in_ecount_full(cFileTypes) const COMDLG_FILTERSPEC *rgFilterSpec);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileTypeIndex )( 
            IFileSaveDialog * This,
            /* [in] */ UINT iFileType);
        
        HRESULT ( STDMETHODCALLTYPE *GetFileTypeIndex )( 
            IFileSaveDialog * This,
            /* [out] */ __RPC__out UINT *piFileType);
        
        HRESULT ( STDMETHODCALLTYPE *Advise )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IFileDialogEvents *pfde,
            /* [out] */ __RPC__out DWORD *pdwCookie);
        
        HRESULT ( STDMETHODCALLTYPE *Unadvise )( 
            IFileSaveDialog * This,
            /* [in] */ DWORD dwCookie);
        
        HRESULT ( STDMETHODCALLTYPE *SetOptions )( 
            IFileSaveDialog * This,
            /* [in] */ DWORD fos);
        
        HRESULT ( STDMETHODCALLTYPE *GetOptions )( 
            IFileSaveDialog * This,
            /* [out] */ __RPC__out DWORD *pfos);
        
        HRESULT ( STDMETHODCALLTYPE *SetDefaultFolder )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi);
        
        HRESULT ( STDMETHODCALLTYPE *SetFolder )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi);
        
        HRESULT ( STDMETHODCALLTYPE *GetFolder )( 
            IFileSaveDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *GetCurrentSelection )( 
            IFileSaveDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileName )( 
            IFileSaveDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszName);
        
        HRESULT ( STDMETHODCALLTYPE *GetFileName )( 
            IFileSaveDialog * This,
            /* [string][out] */ __RPC__deref_out_opt_string LPWSTR *pszName);
        
        HRESULT ( STDMETHODCALLTYPE *SetTitle )( 
            IFileSaveDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszTitle);
        
        HRESULT ( STDMETHODCALLTYPE *SetOkButtonLabel )( 
            IFileSaveDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszText);
        
        HRESULT ( STDMETHODCALLTYPE *SetFileNameLabel )( 
            IFileSaveDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszLabel);
        
        HRESULT ( STDMETHODCALLTYPE *GetResult )( 
            IFileSaveDialog * This,
            /* [out] */ __RPC__deref_out_opt IShellItem **ppsi);
        
        HRESULT ( STDMETHODCALLTYPE *AddPlace )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [in] */ FDAP fdap);
        
        HRESULT ( STDMETHODCALLTYPE *SetDefaultExtension )( 
            IFileSaveDialog * This,
            /* [string][in] */ __RPC__in LPCWSTR pszDefaultExtension);
        
        HRESULT ( STDMETHODCALLTYPE *Close )( 
            IFileSaveDialog * This,
            /* [in] */ HRESULT hr);
        
        HRESULT ( STDMETHODCALLTYPE *SetClientGuid )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in REFGUID guid);
        
        HRESULT ( STDMETHODCALLTYPE *ClearClientData )( 
            IFileSaveDialog * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetFilter )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IShellItemFilter *pFilter);
        
        HRESULT ( STDMETHODCALLTYPE *SetSaveAsItem )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi);
        
        HRESULT ( STDMETHODCALLTYPE *SetProperties )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IPropertyStore *pStore);
        
        HRESULT ( STDMETHODCALLTYPE *SetCollectedProperties )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IPropertyDescriptionList *pList,
            /* [in] */ BOOL fAppendDefault);
        
        HRESULT ( STDMETHODCALLTYPE *GetProperties )( 
            IFileSaveDialog * This,
            /* [out] */ __RPC__deref_out_opt IPropertyStore **ppStore);
        
        HRESULT ( STDMETHODCALLTYPE *ApplyProperties )( 
            IFileSaveDialog * This,
            /* [in] */ __RPC__in_opt IShellItem *psi,
            /* [in] */ __RPC__in_opt IPropertyStore *pStore,
            /* [unique][in] */ __RPC__in_opt HWND hwnd,
            /* [unique][in] */ __RPC__in_opt IFileOperationProgressSink *pSink);
        
        END_INTERFACE
    } IFileSaveDialogVtbl;

    interface IFileSaveDialog
    {
        CONST_VTBL struct IFileSaveDialogVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IFileSaveDialog_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IFileSaveDialog_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IFileSaveDialog_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IFileSaveDialog_Show(This,hwndParent)	\
    ( (This)->lpVtbl -> Show(This,hwndParent) ) 


#define IFileSaveDialog_SetFileTypes(This,cFileTypes,rgFilterSpec)	\
    ( (This)->lpVtbl -> SetFileTypes(This,cFileTypes,rgFilterSpec) ) 

#define IFileSaveDialog_SetFileTypeIndex(This,iFileType)	\
    ( (This)->lpVtbl -> SetFileTypeIndex(This,iFileType) ) 

#define IFileSaveDialog_GetFileTypeIndex(This,piFileType)	\
    ( (This)->lpVtbl -> GetFileTypeIndex(This,piFileType) ) 

#define IFileSaveDialog_Advise(This,pfde,pdwCookie)	\
    ( (This)->lpVtbl -> Advise(This,pfde,pdwCookie) ) 

#define IFileSaveDialog_Unadvise(This,dwCookie)	\
    ( (This)->lpVtbl -> Unadvise(This,dwCookie) ) 

#define IFileSaveDialog_SetOptions(This,fos)	\
    ( (This)->lpVtbl -> SetOptions(This,fos) ) 

#define IFileSaveDialog_GetOptions(This,pfos)	\
    ( (This)->lpVtbl -> GetOptions(This,pfos) ) 

#define IFileSaveDialog_SetDefaultFolder(This,psi)	\
    ( (This)->lpVtbl -> SetDefaultFolder(This,psi) ) 

#define IFileSaveDialog_SetFolder(This,psi)	\
    ( (This)->lpVtbl -> SetFolder(This,psi) ) 

#define IFileSaveDialog_GetFolder(This,ppsi)	\
    ( (This)->lpVtbl -> GetFolder(This,ppsi) ) 

#define IFileSaveDialog_GetCurrentSelection(This,ppsi)	\
    ( (This)->lpVtbl -> GetCurrentSelection(This,ppsi) ) 

#define IFileSaveDialog_SetFileName(This,pszName)	\
    ( (This)->lpVtbl -> SetFileName(This,pszName) ) 

#define IFileSaveDialog_GetFileName(This,pszName)	\
    ( (This)->lpVtbl -> GetFileName(This,pszName) ) 

#define IFileSaveDialog_SetTitle(This,pszTitle)	\
    ( (This)->lpVtbl -> SetTitle(This,pszTitle) ) 

#define IFileSaveDialog_SetOkButtonLabel(This,pszText)	\
    ( (This)->lpVtbl -> SetOkButtonLabel(This,pszText) ) 

#define IFileSaveDialog_SetFileNameLabel(This,pszLabel)	\
    ( (This)->lpVtbl -> SetFileNameLabel(This,pszLabel) ) 

#define IFileSaveDialog_GetResult(This,ppsi)	\
    ( (This)->lpVtbl -> GetResult(This,ppsi) ) 

#define IFileSaveDialog_AddPlace(This,psi,fdap)	\
    ( (This)->lpVtbl -> AddPlace(This,psi,fdap) ) 

#define IFileSaveDialog_SetDefaultExtension(This,pszDefaultExtension)	\
    ( (This)->lpVtbl -> SetDefaultExtension(This,pszDefaultExtension) ) 

#define IFileSaveDialog_Close(This,hr)	\
    ( (This)->lpVtbl -> Close(This,hr) ) 

#define IFileSaveDialog_SetClientGuid(This,guid)	\
    ( (This)->lpVtbl -> SetClientGuid(This,guid) ) 

#define IFileSaveDialog_ClearClientData(This)	\
    ( (This)->lpVtbl -> ClearClientData(This) ) 

#define IFileSaveDialog_SetFilter(This,pFilter)	\
    ( (This)->lpVtbl -> SetFilter(This,pFilter) ) 


#define IFileSaveDialog_SetSaveAsItem(This,psi)	\
    ( (This)->lpVtbl -> SetSaveAsItem(This,psi) ) 

#define IFileSaveDialog_SetProperties(This,pStore)	\
    ( (This)->lpVtbl -> SetProperties(This,pStore) ) 

#define IFileSaveDialog_SetCollectedProperties(This,pList,fAppendDefault)	\
    ( (This)->lpVtbl -> SetCollectedProperties(This,pList,fAppendDefault) ) 

#define IFileSaveDialog_GetProperties(This,ppStore)	\
    ( (This)->lpVtbl -> GetProperties(This,ppStore) ) 

#define IFileSaveDialog_ApplyProperties(This,psi,pStore,hwnd,pSink)	\
    ( (This)->lpVtbl -> ApplyProperties(This,psi,pStore,hwnd,pSink) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IFileSaveDialog_INTERFACE_DEFINED__ */


class DECLSPEC_UUID("DC1C5A9C-E88A-4dde-A5A1-60F82A20AEF7") FileOpenDialog;
class DECLSPEC_UUID("C0B4E2F3-BA21-4773-8DBA-335EC946EB8B") FileSaveDialog;

_COM_SMARTPTR_TYPEDEF(IFileDialog, __uuidof(IFileDialog)); 
_COM_SMARTPTR_TYPEDEF(IFileOpenDialog, __uuidof(IFileOpenDialog)); 
_COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem)); 
_COM_SMARTPTR_TYPEDEF(IFileDialogCustomize, __uuidof(IFileDialogCustomize)); 
_COM_SMARTPTR_TYPEDEF(IShellItemArray, __uuidof(IShellItemArray));

//helper class
class Win7FileDialog
{
public:
  Win7FileDialog(const char *name, int issave=0);
  ~Win7FileDialog();

  int inited() { return m_fod != NULL; }
  int show(HWND parent);
  
  void setFilterList(const char *list);
  void setDefaultExtension(const char *ext);
  void setFileTypeIndex(int i); //1-based
  void setFolder(const char *folder, int def=1); //def is for default folder
  void setFilename(const char *fn);
  void setTemplate(HINSTANCE inst, const char *dlgid, LPOFNHOOKPROC proc);

  void addOptions(DWORD o);

  void startGroup(DWORD id, char *label);
  void addText(DWORD id, char *txt);
  void addCheckbox(char *name, DWORD id, int defval);
  void endGroup();

  int getState(DWORD id);
  void getResult(char *fn, int maxlen);
  int getResult(int i, char *fn, int maxlen); // returns the number of written bytes, including ending null-character
  int getResultCount();

private:
  IFileDialogPtr m_fod;
  IFileDialogCustomizePtr m_fdc;

  HINSTANCE m_inst;
  const char *m_dlgid;
  LPOFNHOOKPROC m_proc;

  WDL_String m_statictxt;
};
#endif

#endif
