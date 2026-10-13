/* Sed: http://msdn.microsoft.com/library/en-us/shellcc/platform/shell/programmersguide/versions.asp */
/*
    libzint - the open source barcode library
    Copyright (C) 2008-2022 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#if defined (_WIN32) && (defined(_USRDLL) || defined(DLL_EXPORT) || defined(PIC))
#include <windows.h>
#include <shlwapi.h>
#include "zintconfig.h"

#ifdef __cplusplus
extern "C"
{
#endif

__declspec(dllexport) HRESULT DllGetVersion(DLLVERSIONINFO2 *pdvi);

#ifdef __cplusplus
}
#endif

HRESULT DllGetVersion(DLLVERSIONINFO2 *pdvi) {
    if (!pdvi || (sizeof(*pdvi) != pdvi->info1.cbSize))
        return (E_INVALIDARG);

    pdvi->info1.dwMajorVersion = ZINT_VERSION_MAJOR;
    pdvi->info1.dwMinorVersion = ZINT_VERSION_MINOR;
    pdvi->info1.dwBuildNumber = ZINT_VERSION_RELEASE;
    pdvi->info1.dwPlatformID = DLLVER_PLATFORM_WINDOWS;
    if (sizeof(DLLVERSIONINFO2) == pdvi->info1.cbSize)
        pdvi->ullVersion = MAKEDLLVERULL(ZINT_VERSION_MAJOR, ZINT_VERSION_MINOR, ZINT_VERSION_RELEASE,
                                        ZINT_VERSION_BUILD);

    return S_OK;
}
#else
/* https://stackoverflow.com/a/26541331 Suppresses gcc warning ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy;
/* vim: set ts=4 sw=4 et : */
#endif /* _WIN32 */
