#include "utils/BaseUtil.h"
#include "utils/GdiPlusUtil.h"

#include "windrawlib.h"

#include "utils/Log.h"

#include <tchar.h>
#include <malloc.h>

/* Enable C preprocessor wrappers for COM methods in <objidl.h> */
//#ifndef COBJMACROS
//#define COBJMACROS
//#endif
#include <dcommon.h>
#include <d2dbasetypes.h>
#include <d2derr.h>
#include <wincodec.h> /* IWICBitmapSource */
#include <objidl.h>
#include <d2d1.h>
#include <dwrite.h>

#pragma warning(disable : 4201)
#pragma warning(disable : 4505) // unreferenced function with internal linkage has been removed

#pragma comment(lib, "d2d1.lib")

#ifdef _MSC_VER
/* MSVC does not understand "inline" when building as pure C (not C++).
 * However it understands "__inline" */
#ifndef __cplusplus
#define inline __inline
#endif
#endif

#define WD_PI 3.14159265358979323846f

#define WD_MIN(a, b) ((a) < (b) ? (a) : (b))
#define WD_MAX(a, b) ((a) > (b) ? (a) : (b))

#define WD_ABS(a) ((a) > 0 ? (a) : -(a))

#define WD_SIZEOF_ARRAY(a) (sizeof((a)) / sizeof((a)[0]))

#define WD_OFFSETOF(type, member) ((size_t) & ((type*)0)->member)
#define WD_CONTAINEROF(ptr, type, member) ((type*)((BYTE*)(ptr)-WD_OFFSETOF(type, member)))

extern void (*wd_fn_lock)(void);
extern void (*wd_fn_unlock)(void);

static inline void wd_lock(void) {
    if (wd_fn_lock != NULL)
        wd_fn_lock();
}

static inline void wd_unlock(void) {
    if (wd_fn_unlock != NULL)
        wd_fn_unlock();
}

extern ID2D1Factory* d2d_factory;

int d2d_init(void) {
    /* Create D2D factory object. Note we use D2D1_FACTORY_TYPE_SINGLE_THREADED
     * for performance reasons and manually synchronize calls to the factory.
     * This still allows usage in multi-threading environment but all the
     * created resources can only be used from the respective threads where
     * they were created. */
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
    if (FAILED(hr)) {
        // WD_TRACE_HR("d2d_init: D2D1CreateFactory() failed.");
        goto err_CreateFactory;
    }

    return 0;

    /* Error path unwinding */
err_CreateFactory:
    return -1;
}

void (*wd_fn_lock)(void) = NULL;
void (*wd_fn_unlock)(void) = NULL;

static DWORD wd_preinit_flags = 0;

void wdPreInitialize(void (*fnLock)(void), void (*fnUnlock)(void), DWORD dwFlags) {
    wd_fn_lock = fnLock;
    wd_fn_unlock = fnUnlock;
    wd_preinit_flags = dwFlags;
}

static int wd_init_core_api(void) {
    if (!(wd_preinit_flags & WD_DISABLE_D2D)) {
        if (d2d_init() == 0)
            return 0;
    }

#if 0 // TODO:
    if (!(wd_preinit_flags & WD_DISABLE_GDIPLUS)) {
        if (gdix_init() == 0)
            return 0;
    }
#endif

    return -1;
}
