/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "AppUtil.h"

// this file should have no dependencies on src/* so that its adding
// unit tests for its content doesn't require for half of SumatraPDF
// to be included (helpers with dependencies belong into AppTools.cpp)

#include "FileUtil.h"

// the only valid chars are 0-9, . and newlines.
// a valid version has to match the regex /^\d+(\.\d+)*(\r?\n)?$/
// Return false if it contains anything else.
bool IsValidProgramVersion(const char *txt)
{
    if (!str::IsDigit(*txt))
        return false;

    for (; *txt; txt++) {
        if (str::IsDigit(*txt))
            continue;
        if (*txt == '.' && str::IsDigit(*(txt + 1)))
            continue;
        if (*txt == '\r' && *(txt + 1) == '\n')
            continue;
        if (*txt == '\n' && !*(txt + 1))
            continue;
        return false;
    }

    return true;
}

// extract the next (positive) number from the string *txt
static unsigned int ExtractNextNumber(const WCHAR **txt)
{
    unsigned int val = 0;
    const WCHAR *next = str::Parse(*txt, L"%u%?.", &val);
    *txt = next ? next : *txt + str::Len(*txt);
    return val;
}

// compare two version string. Return 0 if they are the same,
// > 0 if the first is greater than the second and < 0 otherwise.
// e.g.
//   0.9.3.900 is greater than 0.9.3
//   1.09.300 is greater than 1.09.3 which is greater than 1.9.1
//   1.2.0 is the same as 1.2
int CompareVersion(const WCHAR *txt1, const WCHAR *txt2)
{
    while (*txt1 || *txt2) {
        unsigned int v1 = ExtractNextNumber(&txt1);
        unsigned int v2 = ExtractNextNumber(&txt2);
        if (v1 != v2)
            return v1 - v2;
    }

    return 0;
}

// Updates the drive letter for a path that could have been on a removable drive,
// if that same path can be found on a different removable drive
// returns true if the path has been changed
bool AdjustVariableDriveLetter(WCHAR *path)
{
    // Don't bother if the file path is still valid
    if (file::Exists(path))
        return false;
    // Don't bother for files on non-removable drives
    if (!path::HasVariableDriveLetter(path))
        return false;

    // Iterate through all (other) removable drives and try to find the file there
    WCHAR szDrive[] = L"A:\\";
    WCHAR origDrive = path[0];
    for (DWORD driveMask = GetLogicalDrives(); driveMask; driveMask >>= 1) {
        if ((driveMask & 1) && szDrive[0] != origDrive && path::HasVariableDriveLetter(szDrive)) {
            path[0] = szDrive[0];
            if (file::Exists(path))
                return true;
        }
        szDrive[0]++;
    }
    path[0] = origDrive;
    return false;
}

// caller needs to free() the result
WCHAR *ExtractFilenameFromURL(const WCHAR *url)
{
    ScopedMem<WCHAR> urlName(str::Dup(url));
    // try to extract the file name from the URL (last path component before query or hash)
    str::TransChars(urlName, L"/?#", L"\\\0\0");
    urlName.Set(str::Dup(path::GetBaseName(urlName)));
    // unescape hex-escapes (these are usually UTF-8)
    if (str::FindChar(urlName, '%')) {
        ScopedMem<char> utf8Name(str::conv::ToUtf8(urlName));
        char *src = utf8Name, *dst = utf8Name;
        while (*src) {
            int esc;
            if ('%' == *src && str::Parse(src, "%%%2x", &esc)) {
                *dst++ = (char)esc;
                src += 3;
            }
            else
                *dst++ = *src++;
        }
        *dst = '\0';
        urlName.Set(str::conv::FromUtf8(utf8Name));
    }
    if (str::IsEmpty(urlName.Get()))
        return NULL;
    return urlName.StealData();
}

// files are considered untrusted, if they're either loaded from a
// non-file URL in plugin mode, or if they're marked as being from
// an untrusted zone (e.g. by the browser that's downloaded them)
bool IsUntrustedFile(const WCHAR *filePath, const WCHAR *fileURL)
{
    ScopedMem<WCHAR> protocol;
    if (fileURL && str::Parse(fileURL, L"%S:", &protocol))
        if (str::Len(protocol) > 1 && !str::EqI(protocol, L"file"))
            return true;

    if (file::GetZoneIdentifier(filePath) >= URLZONE_INTERNET)
        return true;

    // check all parents of embedded files and ADSs as well
    ScopedMem<WCHAR> path(str::Dup(filePath));
    while (str::Len(path) > 2 && str::FindChar(path + 2, ':')) {
        *wcsrchr(path, ':') = '\0';
        if (file::GetZoneIdentifier(path) >= URLZONE_INTERNET)
            return true;
    }

    return false;
}
