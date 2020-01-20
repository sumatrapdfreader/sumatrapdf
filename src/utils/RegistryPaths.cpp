/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

// This is in HKLM. Note that on 64bit windows, if installing 32bit app
// the installer has to be 32bit as well, so that it goes into proper
// place in registry (under Software\Wow6432Node\Microsoft\Windows\...
WCHAR* getRegPathUninst(const WCHAR* appName) {
    return str::Join(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\", appName);
}

WCHAR* getRegClassesApp(const WCHAR* appName) {
    return str::Join(L"Software\\Classes\\", appName);
}
