#include "utils/BaseUtil.h"

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
#include <tchar.h>
#include <malloc.h>
#include <objidl.h> /* IStream */

#include "utils/GdiPlusUtil.h"
#include "utils/Log.h"

#include "windrawlib.h"

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

void wd_lock(void) {
    // TODO: lock
}

void wd_unlock(void) {
    // TODO: unlock
}

static ID2D1Factory* d2d_factory = nullptr;

int d2d_init(void) {
    static const D2D1_FACTORY_OPTIONS factory_options = {
        D2D1_DEBUG_LEVEL_NONE
    };
    /* Create D2D factory object. Note we use D2D1_FACTORY_TYPE_SINGLE_THREADED
     * for performance reasons and manually synchronize calls to the factory.
     * This still allows usage in multi-threading environment but all the
     * created resources can only be used from the respective threads where
     * they were created. */
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_options, &d2d_factory);
    if (FAILED(hr)) {
        // WD_TRACE_HR("d2d_init: D2D1CreateFactory() failed.");
        goto err_CreateFactory;
    }

    return 0;

    /* Error path unwinding */
err_CreateFactory:
    return -1;
}

bool d2d_enabled(void) {
    return (d2d_factory != NULL);
}

void d2d_fini(void) {
    if (d2d_factory) {
        d2d_factory->Release();
    }
}

void d2d_init_color(D2D1_COLOR_F* c, WD_COLOR color) {
    c->r = WD_RVALUE(color) / 255.0f;
    c->g = WD_GVALUE(color) / 255.0f;
    c->b = WD_BVALUE(color) / 255.0f;
    c->a = WD_AVALUE(color) / 255.0f;
}

void d2d_matrix_mult(D2D1_MATRIX_3X2_F* res,
                const D2D1_MATRIX_3X2_F* a, const D2D1_MATRIX_3X2_F* b) {
    res->_11 = a->_11 * b->_11 + a->_12 * b->_21;
    res->_12 = a->_11 * b->_12 + a->_12 * b->_22;
    res->_21 = a->_21 * b->_11 + a->_22 * b->_21;
    res->_22 = a->_21 * b->_12 + a->_22 * b->_22;
    res->_31 = a->_31 * b->_11 + a->_32 * b->_21 + b->_31;
    res->_32 = a->_31 * b->_12 + a->_32 * b->_22 + b->_32;
}

void d2d_reset_transform(d2d_canvas_t* c) {
    D2D1_MATRIX_3X2_F m;

    if (c->flags & D2D_CANVASFLAG_RTL) {
        m._11 = -1.0f;  m._12 = 0.0f;
        m._21 = 0.0f;   m._22 = 1.0f;
        m._31 = (float)c->width - 1.0f + D2D_BASEDELTA_X;
        m._32 = D2D_BASEDELTA_Y;
    } else {
        m._11 = 1.0f;   m._12 = 0.0f;
        m._21 = 0.0f;   m._22 = 1.0f;
        m._31 = D2D_BASEDELTA_X;
        m._32 = D2D_BASEDELTA_Y;
    }

    c->target->SetTransform(&m);
}

void d2d_reset_clip(d2d_canvas_t* c) {
    if (c->clip_layer != NULL) {
        c->target->PopLayer();
        c->clip_layer->Release();
        c->clip_layer = NULL;
    }
    if (c->flags & D2D_CANVASFLAG_RECTCLIP) {
        c->target->PopAxisAlignedClip();
        c->flags &= ~D2D_CANVASFLAG_RECTCLIP;
    }
}

void d2d_apply_transform(d2d_canvas_t* c, const D2D1_MATRIX_3X2_F* matrix) {
    D2D1_MATRIX_3X2_F res;
    D2D1_MATRIX_3X2_F old_matrix;

    c->target->GetTransform(&old_matrix);
    d2d_matrix_mult(&res, matrix, &old_matrix);
    c->target->SetTransform(&res);
}

d2d_canvas_t* d2d_canvas_alloc(ID2D1RenderTarget* target, WORD type, UINT width, BOOL rtl) {
    d2d_canvas_t* c;

    c = (d2d_canvas_t*) malloc(sizeof(d2d_canvas_t));
    if (c == NULL) {
        //WD_TRACE("d2d_canvas_alloc: malloc() failed.");
        return NULL;
    }

    memset(c, 0, sizeof(d2d_canvas_t));

    c->type = type;
    c->flags = (rtl ? D2D_CANVASFLAG_RTL : 0);
    c->width = width;
    c->target = target;

    /* We use raw pixels as units. D2D by default works with DIPs ("device
     * independent pixels"), which map 1:1 to physical pixels when DPI is 96.
     * So we enforce the render target to think we have this DPI. */
    c->target->SetDpi(96.0f, 96.0f);

    d2d_reset_transform(c);

    return c;
}

static int wd_init_core_api(void) {
    if (d2d_init() == 0) {
        return 0;
    }

#if 0 // TODO:
    if (gdix_init() == 0) {
        return 0;
    }
#endif

    return -1;
}
