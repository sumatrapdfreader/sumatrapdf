

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.xx.xxxx */
/* at a redacted point in time
 */
/* Compiler settings for ../../edge_embedded_browser/client/win/current/webview2interop.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.xx.xxxx 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __webview2interop_h__
#define __webview2interop_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __ICoreWebView2Interop_FWD_DEFINED__
#define __ICoreWebView2Interop_FWD_DEFINED__
typedef interface ICoreWebView2Interop ICoreWebView2Interop;

#endif 	/* __ICoreWebView2Interop_FWD_DEFINED__ */


#ifndef __ICoreWebView2CompositionControllerInterop_FWD_DEFINED__
#define __ICoreWebView2CompositionControllerInterop_FWD_DEFINED__
typedef interface ICoreWebView2CompositionControllerInterop ICoreWebView2CompositionControllerInterop;

#endif 	/* __ICoreWebView2CompositionControllerInterop_FWD_DEFINED__ */


#ifndef __ICoreWebView2EnvironmentInterop_FWD_DEFINED__
#define __ICoreWebView2EnvironmentInterop_FWD_DEFINED__
typedef interface ICoreWebView2EnvironmentInterop ICoreWebView2EnvironmentInterop;

#endif 	/* __ICoreWebView2EnvironmentInterop_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __WebView2Interop_LIBRARY_DEFINED__
#define __WebView2Interop_LIBRARY_DEFINED__

/* library WebView2Interop */
/* [version][uuid] */ 



EXTERN_C const IID LIBID_WebView2Interop;

#ifndef __ICoreWebView2Interop_INTERFACE_DEFINED__
#define __ICoreWebView2Interop_INTERFACE_DEFINED__

/* interface ICoreWebView2Interop */
/* [unique][object][uuid] */ 


EXTERN_C __declspec(selectany) const IID IID_ICoreWebView2Interop = {0x912b34a7,0xd10b,0x49c4,{0xaf,0x18,0x7c,0xb7,0xe6,0x04,0xe0,0x1a}};

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("912b34a7-d10b-49c4-af18-7cb7e604e01a")
    ICoreWebView2Interop : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE AddHostObjectToScript( 
            /* [in] */ LPCWSTR name,
            /* [in] */ VARIANT *object) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ICoreWebView2InteropVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICoreWebView2Interop * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICoreWebView2Interop * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICoreWebView2Interop * This);
        
        HRESULT ( STDMETHODCALLTYPE *AddHostObjectToScript )( 
            ICoreWebView2Interop * This,
            /* [in] */ LPCWSTR name,
            /* [in] */ VARIANT *object);
        
        END_INTERFACE
    } ICoreWebView2InteropVtbl;

    interface ICoreWebView2Interop
    {
        CONST_VTBL struct ICoreWebView2InteropVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICoreWebView2Interop_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICoreWebView2Interop_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICoreWebView2Interop_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICoreWebView2Interop_AddHostObjectToScript(This,name,object)	\
    ( (This)->lpVtbl -> AddHostObjectToScript(This,name,object) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICoreWebView2Interop_INTERFACE_DEFINED__ */


#ifndef __ICoreWebView2CompositionControllerInterop_INTERFACE_DEFINED__
#define __ICoreWebView2CompositionControllerInterop_INTERFACE_DEFINED__

/* interface ICoreWebView2CompositionControllerInterop */
/* [unique][object][uuid] */ 


EXTERN_C __declspec(selectany) const IID IID_ICoreWebView2CompositionControllerInterop = {0x8e9922ce,0x9c80,0x42e6,{0xba,0xd7,0xfc,0xeb,0xf2,0x91,0xa4,0x95}};

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8e9922ce-9c80-42e6-bad7-fcebf291a495")
    ICoreWebView2CompositionControllerInterop : public IUnknown
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_UIAProvider( 
            /* [retval][out] */ IUnknown **provider) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_RootVisualTarget( 
            /* [retval][out] */ IUnknown **target) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_RootVisualTarget( 
            /* [in] */ IUnknown *target) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ICoreWebView2CompositionControllerInteropVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICoreWebView2CompositionControllerInterop * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICoreWebView2CompositionControllerInterop * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICoreWebView2CompositionControllerInterop * This);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_UIAProvider )( 
            ICoreWebView2CompositionControllerInterop * This,
            /* [retval][out] */ IUnknown **provider);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_RootVisualTarget )( 
            ICoreWebView2CompositionControllerInterop * This,
            /* [retval][out] */ IUnknown **target);
        
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_RootVisualTarget )( 
            ICoreWebView2CompositionControllerInterop * This,
            /* [in] */ IUnknown *target);
        
        END_INTERFACE
    } ICoreWebView2CompositionControllerInteropVtbl;

    interface ICoreWebView2CompositionControllerInterop
    {
        CONST_VTBL struct ICoreWebView2CompositionControllerInteropVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICoreWebView2CompositionControllerInterop_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICoreWebView2CompositionControllerInterop_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICoreWebView2CompositionControllerInterop_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICoreWebView2CompositionControllerInterop_get_UIAProvider(This,provider)	\
    ( (This)->lpVtbl -> get_UIAProvider(This,provider) ) 

#define ICoreWebView2CompositionControllerInterop_get_RootVisualTarget(This,target)	\
    ( (This)->lpVtbl -> get_RootVisualTarget(This,target) ) 

#define ICoreWebView2CompositionControllerInterop_put_RootVisualTarget(This,target)	\
    ( (This)->lpVtbl -> put_RootVisualTarget(This,target) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICoreWebView2CompositionControllerInterop_INTERFACE_DEFINED__ */


#ifndef __ICoreWebView2EnvironmentInterop_INTERFACE_DEFINED__
#define __ICoreWebView2EnvironmentInterop_INTERFACE_DEFINED__

/* interface ICoreWebView2EnvironmentInterop */
/* [unique][object][uuid] */ 


EXTERN_C __declspec(selectany) const IID IID_ICoreWebView2EnvironmentInterop = {0xee503a63,0xc1e2,0x4fbf,{0x8a,0x4d,0x82,0x4e,0x95,0xf8,0xbb,0x13}};

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("ee503a63-c1e2-4fbf-8a4d-824e95f8bb13")
    ICoreWebView2EnvironmentInterop : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetProviderForHwnd( 
            /* [in] */ HWND hwnd,
            /* [retval][out] */ IUnknown **provider) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ICoreWebView2EnvironmentInteropVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ICoreWebView2EnvironmentInterop * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ICoreWebView2EnvironmentInterop * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ICoreWebView2EnvironmentInterop * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetProviderForHwnd )( 
            ICoreWebView2EnvironmentInterop * This,
            /* [in] */ HWND hwnd,
            /* [retval][out] */ IUnknown **provider);
        
        END_INTERFACE
    } ICoreWebView2EnvironmentInteropVtbl;

    interface ICoreWebView2EnvironmentInterop
    {
        CONST_VTBL struct ICoreWebView2EnvironmentInteropVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICoreWebView2EnvironmentInterop_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ICoreWebView2EnvironmentInterop_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ICoreWebView2EnvironmentInterop_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ICoreWebView2EnvironmentInterop_GetProviderForHwnd(This,hwnd,provider)	\
    ( (This)->lpVtbl -> GetProviderForHwnd(This,hwnd,provider) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICoreWebView2EnvironmentInterop_INTERFACE_DEFINED__ */

#endif /* __WebView2Interop_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


