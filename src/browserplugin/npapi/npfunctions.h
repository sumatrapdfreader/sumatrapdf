/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef npfunctions_h_
#define npfunctions_h_

#ifdef __OS2__
#pragma pack(1)
#define NP_LOADDS _System
#else
#define NP_LOADDS
#endif

#ifndef GENERATINGCFM
#define GENERATINGCFM 0
#endif

#include "npapi/npapi.h"
#include "npapi/npruntime.h"

typedef void         (* NP_LOADDS NPP_InitializeProcPtr)();
typedef void         (* NP_LOADDS NPP_ShutdownProcPtr)();
typedef NPError      (* NP_LOADDS NPP_NewProcPtr)(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved);
typedef NPError      (* NP_LOADDS NPP_DestroyProcPtr)(NPP instance, NPSavedData** save);
typedef NPError      (* NP_LOADDS NPP_SetWindowProcPtr)(NPP instance, NPWindow* window);
typedef NPError      (* NP_LOADDS NPP_NewStreamProcPtr)(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype);
typedef NPError      (* NP_LOADDS NPP_DestroyStreamProcPtr)(NPP instance, NPStream* stream, NPReason reason);
typedef int32_t      (* NP_LOADDS NPP_WriteReadyProcPtr)(NPP instance, NPStream* stream);
typedef int32_t      (* NP_LOADDS NPP_WriteProcPtr)(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer);
typedef void         (* NP_LOADDS NPP_StreamAsFileProcPtr)(NPP instance, NPStream* stream, const char* fname);
typedef void         (* NP_LOADDS NPP_PrintProcPtr)(NPP instance, NPPrint* platformPrint);
typedef int16_t      (* NP_LOADDS NPP_HandleEventProcPtr)(NPP instance, void* event);
typedef void         (* NP_LOADDS NPP_URLNotifyProcPtr)(NPP instance, const char* url, NPReason reason, void* notifyData);
typedef NPError      (* NP_LOADDS NPP_GetValueProcPtr)(NPP instance, NPPVariable variable, void *ret_value);
typedef NPError      (* NP_LOADDS NPP_SetValueProcPtr)(NPP instance, NPNVariable variable, void *value);

typedef NPError      (*NPN_GetValueProcPtr)(NPP instance, NPNVariable variable, void *ret_value);
typedef NPError      (*NPN_SetValueProcPtr)(NPP instance, NPPVariable variable, void *value);
typedef NPError      (*NPN_GetURLNotifyProcPtr)(NPP instance, const char* url, const char* window, void* notifyData);
typedef NPError      (*NPN_PostURLNotifyProcPtr)(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file, void* notifyData);
typedef NPError      (*NPN_GetURLProcPtr)(NPP instance, const char* url, const char* window);
typedef NPError      (*NPN_PostURLProcPtr)(NPP instance, const char* url, const char* window, uint32_t len, const char* buf, NPBool file);
typedef NPError      (*NPN_RequestReadProcPtr)(NPStream* stream, NPByteRange* rangeList);
typedef NPError      (*NPN_NewStreamProcPtr)(NPP instance, NPMIMEType type, const char* window, NPStream** stream);
typedef int32_t      (*NPN_WriteProcPtr)(NPP instance, NPStream* stream, int32_t len, void* buffer);
typedef NPError      (*NPN_DestroyStreamProcPtr)(NPP instance, NPStream* stream, NPReason reason);
typedef void         (*NPN_StatusProcPtr)(NPP instance, const char* message);
typedef const char*  (*NPN_UserAgentProcPtr)(NPP instance);
typedef void*        (*NPN_MemAllocProcPtr)(uint32_t size);
typedef void         (*NPN_MemFreeProcPtr)(void* ptr);
typedef uint32_t     (*NPN_MemFlushProcPtr)(uint32_t size);
typedef void         (*NPN_ReloadPluginsProcPtr)(NPBool reloadPages);
typedef void*        (*NPN_GetJavaEnvProcPtr)(void);
typedef void*        (*NPN_GetJavaPeerProcPtr)(NPP instance);
typedef void         (*NPN_InvalidateRectProcPtr)(NPP instance, NPRect *rect);
typedef void         (*NPN_InvalidateRegionProcPtr)(NPP instance, NPRegion region);
typedef void         (*NPN_ForceRedrawProcPtr)(NPP instance);
typedef NPIdentifier (*NPN_GetStringIdentifierProcPtr)(const NPUTF8* name);
typedef void         (*NPN_GetStringIdentifiersProcPtr)(const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers);
typedef NPIdentifier (*NPN_GetIntIdentifierProcPtr)(int32_t intid);
typedef bool         (*NPN_IdentifierIsStringProcPtr)(NPIdentifier identifier);
typedef NPUTF8*      (*NPN_UTF8FromIdentifierProcPtr)(NPIdentifier identifier);
typedef int32_t      (*NPN_IntFromIdentifierProcPtr)(NPIdentifier identifier);
typedef NPObject*    (*NPN_CreateObjectProcPtr)(NPP npp, NPClass *aClass);
typedef NPObject*    (*NPN_RetainObjectProcPtr)(NPObject *obj);
typedef void         (*NPN_ReleaseObjectProcPtr)(NPObject *obj);
typedef bool         (*NPN_InvokeProcPtr)(NPP npp, NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result);
typedef bool         (*NPN_InvokeDefaultProcPtr)(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
typedef bool         (*NPN_EvaluateProcPtr)(NPP npp, NPObject *obj, NPString *script, NPVariant *result);
typedef bool         (*NPN_GetPropertyProcPtr)(NPP npp, NPObject *obj, NPIdentifier propertyName, NPVariant *result);
typedef bool         (*NPN_SetPropertyProcPtr)(NPP npp, NPObject *obj, NPIdentifier propertyName, const NPVariant *value);
typedef bool         (*NPN_RemovePropertyProcPtr)(NPP npp, NPObject *obj, NPIdentifier propertyName);
typedef bool         (*NPN_HasPropertyProcPtr)(NPP npp, NPObject *obj, NPIdentifier propertyName);
typedef bool         (*NPN_HasMethodProcPtr)(NPP npp, NPObject *obj, NPIdentifier propertyName);
typedef void         (*NPN_ReleaseVariantValueProcPtr)(NPVariant *variant);
typedef void         (*NPN_SetExceptionProcPtr)(NPObject *obj, const NPUTF8 *message);
typedef bool         (*NPN_PushPopupsEnabledStateProcPtr)(NPP npp, NPBool enabled);
typedef bool         (*NPN_PopPopupsEnabledStateProcPtr)(NPP npp);
typedef bool         (*NPN_EnumerateProcPtr)(NPP npp, NPObject *obj, NPIdentifier **identifier, uint32_t *count);
typedef void         (*NPN_PluginThreadAsyncCallProcPtr)(NPP instance, void (*func)(void *), void *userData);
typedef bool         (*NPN_ConstructProcPtr)(NPP npp, NPObject* obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
typedef NPError      (*NPN_GetValueForURLPtr)(NPP npp, NPNURLVariable variable, const char *url, char **value, uint32_t *len);
typedef NPError      (*NPN_SetValueForURLPtr)(NPP npp, NPNURLVariable variable, const char *url, const char *value, uint32_t len);
typedef NPError      (*NPN_GetAuthenticationInfoPtr)(NPP npp, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen);

typedef struct _NPPluginFuncs {
  uint16_t size;
  uint16_t version;
  NPP_NewProcPtr newp;
  NPP_DestroyProcPtr destroy;
  NPP_SetWindowProcPtr setwindow;
  NPP_NewStreamProcPtr newstream;
  NPP_DestroyStreamProcPtr destroystream;
  NPP_StreamAsFileProcPtr asfile;
  NPP_WriteReadyProcPtr writeready;
  NPP_WriteProcPtr write;
  NPP_PrintProcPtr print;
  NPP_HandleEventProcPtr event;
  NPP_URLNotifyProcPtr urlnotify;
  void* javaClass;
  NPP_GetValueProcPtr getvalue;
  NPP_SetValueProcPtr setvalue;
} NPPluginFuncs;

typedef struct _NPNetscapeFuncs {
  uint16_t size;
  uint16_t version;
  NPN_GetURLProcPtr geturl;
  NPN_PostURLProcPtr posturl;
  NPN_RequestReadProcPtr requestread;
  NPN_NewStreamProcPtr newstream;
  NPN_WriteProcPtr write;
  NPN_DestroyStreamProcPtr destroystream;
  NPN_StatusProcPtr status;
  NPN_UserAgentProcPtr uagent;
  NPN_MemAllocProcPtr memalloc;
  NPN_MemFreeProcPtr memfree;
  NPN_MemFlushProcPtr memflush;
  NPN_ReloadPluginsProcPtr reloadplugins;
  NPN_GetJavaEnvProcPtr getJavaEnv;
  NPN_GetJavaPeerProcPtr getJavaPeer;
  NPN_GetURLNotifyProcPtr geturlnotify;
  NPN_PostURLNotifyProcPtr posturlnotify;
  NPN_GetValueProcPtr getvalue;
  NPN_SetValueProcPtr setvalue;
  NPN_InvalidateRectProcPtr invalidaterect;
  NPN_InvalidateRegionProcPtr invalidateregion;
  NPN_ForceRedrawProcPtr forceredraw;
  NPN_GetStringIdentifierProcPtr getstringidentifier;
  NPN_GetStringIdentifiersProcPtr getstringidentifiers;
  NPN_GetIntIdentifierProcPtr getintidentifier;
  NPN_IdentifierIsStringProcPtr identifierisstring;
  NPN_UTF8FromIdentifierProcPtr utf8fromidentifier;
  NPN_IntFromIdentifierProcPtr intfromidentifier;
  NPN_CreateObjectProcPtr createobject;
  NPN_RetainObjectProcPtr retainobject;
  NPN_ReleaseObjectProcPtr releaseobject;
  NPN_InvokeProcPtr invoke;
  NPN_InvokeDefaultProcPtr invokeDefault;
  NPN_EvaluateProcPtr evaluate;
  NPN_GetPropertyProcPtr getproperty;
  NPN_SetPropertyProcPtr setproperty;
  NPN_RemovePropertyProcPtr removeproperty;
  NPN_HasPropertyProcPtr hasproperty;
  NPN_HasMethodProcPtr hasmethod;
  NPN_ReleaseVariantValueProcPtr releasevariantvalue;
  NPN_SetExceptionProcPtr setexception;
  NPN_PushPopupsEnabledStateProcPtr pushpopupsenabledstate;
  NPN_PopPopupsEnabledStateProcPtr poppopupsenabledstate;
  NPN_EnumerateProcPtr enumerate;
  NPN_PluginThreadAsyncCallProcPtr pluginthreadasynccall;
  NPN_ConstructProcPtr construct;
  NPN_GetValueForURLPtr getvalueforurl;
  NPN_SetValueForURLPtr setvalueforurl;
  NPN_GetAuthenticationInfoPtr getauthenticationinfo;
} NPNetscapeFuncs;

#ifdef XP_MACOSX
/* Don't use this, it is going away. */
typedef NPError (* NPP_MainEntryProcPtr)(NPNetscapeFuncs*, NPPluginFuncs*, NPP_ShutdownProcPtr*);
/*
 * Mac OS X version(s) of NP_GetMIMEDescription(const char *)
 * These can be called to retreive MIME information from the plugin dynamically
 *
 * Note: For compatibility with Quicktime, BPSupportedMIMEtypes is another way
 *       to get mime info from the plugin only on OSX and may not be supported 
 *       in furture version -- use NP_GetMIMEDescription instead
 */
enum
{
 kBPSupportedMIMETypesStructVers_1    = 1
};
typedef struct _BPSupportedMIMETypes
{
 SInt32    structVersion;      /* struct version */
 Handle    typeStrings;        /* STR# formated handle, allocated by plug-in */
 Handle    infoStrings;        /* STR# formated handle, allocated by plug-in */
} BPSupportedMIMETypes;
OSErr BP_GetSupportedMIMETypes(BPSupportedMIMETypes *mimeInfo, UInt32 flags);
#define NP_GETMIMEDESCRIPTION_NAME "NP_GetMIMEDescription"
typedef const char* (*NP_GetMIMEDescriptionProcPtr)();
typedef OSErr (*BP_GetSupportedMIMETypesProcPtr)(BPSupportedMIMETypes*, UInt32);
#endif

#if defined(_WINDOWS)
#define OSCALL WINAPI
#else
#if defined(__OS2__)
#define OSCALL _System
#else
#define OSCALL
#endif
#endif

#if defined(XP_UNIX)
/* GCC 3.3 and later support the visibility attribute. */
#if defined(__GNUC__) && ((__GNUC__ >= 4) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3))
#define NP_VISIBILITY_DEFAULT __attribute__((visibility("default")))
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#define NP_VISIBILITY_DEFAULT __global
#else
#define NP_VISIBILITY_DEFAULT
#endif
#define NP_EXPORT(__type) NP_VISIBILITY_DEFAULT __type
#endif

#if defined(_WINDOWS) || defined (__OS2__)
#ifdef __cplusplus
extern "C" {
#endif
/* plugin meta member functions */
#if defined(__OS2__)
typedef struct _NPPluginData {   /* Alternate OS2 Plugin interface */
  char *pMimeTypes;
  char *pFileExtents;
  char *pFileOpenTemplate;
  char *pProductName;
  char *pProductDescription;
  unsigned long dwProductVersionMS;
  unsigned long dwProductVersionLS;
} NPPluginData;
NPError OSCALL NP_GetPluginData(NPPluginData * pPluginData);
#endif
NPError OSCALL NP_GetEntryPoints(NPPluginFuncs* pFuncs);
NPError OSCALL NP_Initialize(NPNetscapeFuncs* pFuncs);
NPError OSCALL NP_Shutdown();
char*          NP_GetMIMEDescription();
#ifdef __cplusplus
}
#endif
#endif

#if defined(__OS2__)
#pragma pack()
#endif

#ifdef XP_UNIX
#ifdef __cplusplus
extern "C" {
#endif
NP_EXPORT(char*)   NP_GetPluginVersion();
NP_EXPORT(char*)   NP_GetMIMEDescription();
#ifdef XP_MACOSX
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs);
#else
NP_EXPORT(NPError) NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs);
#endif
NP_EXPORT(NPError) NP_Shutdown();
NP_EXPORT(NPError) NP_GetValue(void *future, NPPVariable aVariable, void *aValue);
#ifdef __cplusplus
}
#endif
#endif

#endif /* npfunctions_h_ */
