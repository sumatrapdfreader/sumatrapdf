/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef Scopes_h
#define Scopes_h

#include "BaseUtil.h"

class ScopedCritSec
{
    CRITICAL_SECTION *cs;
public:
    explicit ScopedCritSec(CRITICAL_SECTION *cs) : cs(cs) {
        EnterCriticalSection(cs);
    }
    ~ScopedCritSec() {
        LeaveCriticalSection(cs);
    }
};

class ScopedHandle {
    HANDLE handle;
public:
    ScopedHandle(HANDLE handle) : handle(handle) { }
    ~ScopedHandle() { CloseHandle(handle); }
    operator HANDLE() const { return handle; }
};

template <class T>
class ScopedComPtr {
protected:
    T *ptr;
public:
    ScopedComPtr() : ptr(NULL) { }
    explicit ScopedComPtr(T *ptr) : ptr(ptr) { }
    ~ScopedComPtr() {
        if (ptr)
            ptr->Release();
    }
    operator T*() const { return ptr; }
    T** operator&() { return &ptr; }
    T* operator->() const { return ptr; }
    T* operator=(T* newPtr) {
        if (ptr)
            ptr->Release();
        return (ptr = newPtr);
    }
};

template <class T>
class ScopedComQIPtr : public ScopedComPtr<T> {
public:
    ScopedComQIPtr() : ScopedComPtr() { }
    explicit ScopedComQIPtr(IUnknown *unk) {
        HRESULT hr = unk->QueryInterface(__uuidof(T), (void **)&ptr);
        if (FAILED(hr))
            ptr = NULL;
    }
    T* operator=(IUnknown *newUnk) {
        if (ptr)
            ptr->Release();
        HRESULT hr = newUnk->QueryInterface(__uuidof(T), (void **)&ptr);
        if (FAILED(hr))
            ptr = NULL;
        return ptr;
    }
};

template <typename T>
class ScopedGdiObj {
    T obj;
public:
    ScopedGdiObj(T obj) : obj(obj) { }
    ~ScopedGdiObj() { DeleteObject(obj); }
    operator T() const { return obj; }
};
typedef ScopedGdiObj<HFONT> ScopedFont;

class ScopedCom {
public:
    ScopedCom() { CoInitialize(NULL); }
    ~ScopedCom() { CoUninitialize(); }
};

class ScopedOle {
public:
    ScopedOle() { OleInitialize(NULL); }
    ~ScopedOle() { OleUninitialize(); }
};

class ScopedGdiPlus {
protected:
    Gdiplus::GdiplusStartupInput si;
    Gdiplus::GdiplusStartupOutput so;
    ULONG_PTR token, hookToken;
    bool noBgThread;

public:
    // suppress the GDI+ background thread when initiating in WinMain,
    // as that thread causes DDE messages to be sent too early and
    // thus causes unexpected timeouts
    ScopedGdiPlus(bool inWinMain=false) : noBgThread(inWinMain) {
        si.SuppressBackgroundThread = noBgThread;
        Gdiplus::GdiplusStartup(&token, &si, &so);
        if (noBgThread)
            so.NotificationHook(&hookToken);
    }
    ~ScopedGdiPlus() {
        if (noBgThread)
            so.NotificationUnhook(hookToken);
        Gdiplus::GdiplusShutdown(token);
    }
};

#endif
