/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

// this file should have no further dependencies on src/* so that adding
// unit tests for its content doesn't require for half of SumatraPDF
// to be included (helpers with dependencies belong into AppTools.cpp)
#include "SumatraConfig.h"
#include "AppUtil.h"

#include "utils/Log.h"

// the only valid chars are 0-9, . and newlines.
// a valid version has to match the regex /^\d+(\.\d+)*(\r?\n)?$/
// Return false if it contains anything else.
bool IsValidProgramVersion(const char* txt) {
    if (!str::IsDigit(*txt)) {
        return false;
    }

    for (; *txt; txt++) {
        if (str::IsDigit(*txt)) {
            continue;
        }
        if (*txt == '.' && str::IsDigit(*(txt + 1))) {
            continue;
        }
        if (*txt == '\r' && *(txt + 1) == '\n') {
            continue;
        }
        if (*txt == '\n' && !*(txt + 1)) {
            continue;
        }
        return false;
    }

    return true;
}

static unsigned int ExtractNextNumber(const char** txt) {
    unsigned int val = 0;
    const char* next = str::Parse(*txt, "%u%?.", &val);
    *txt = next ? next : *txt + str::Len(*txt);
    return val;
}

// compare two version string. Return 0 if they are the same,
// > 0 if the first is greater than the second and < 0 otherwise.
// e.g.
//   0.9.3.900 is greater than 0.9.3
//   1.09.300 is greater than 1.09.3 which is greater than 1.9.1
//   1.2.0 is the same as 1.2
int CompareVersion(const char* txt1, const char* txt2) {
    while (*txt1 || *txt2) {
        unsigned int v1 = ExtractNextNumber(&txt1);
        unsigned int v2 = ExtractNextNumber(&txt2);
        if (v1 != v2) {
            return v1 - v2;
        }
    }
    return 0;
}

// Updates the drive letter for a path that could have been on a removable drive,
// if that same path can be found on a different removable drive
// returns true if the path has been changed
bool AdjustVariableDriveLetter(char* path) {
    // Don't bother if the file path is still valid
    if (file::Exists(path)) {
        return false;
    }
    // Don't bother for files on non-removable drives
    if (!path::HasVariableDriveLetter(path)) {
        return false;
    }

    // Iterate through all (other) removable drives and try to find the file there
    char szDrive[] = "A:\\";
    char origDrive = path[0];
    for (DWORD driveMask = GetLogicalDrives(); driveMask; driveMask >>= 1) {
        if ((driveMask & 1) && szDrive[0] != origDrive && path::HasVariableDriveLetter(szDrive)) {
            path[0] = szDrive[0];
            if (file::Exists(path)) {
                return true;
            }
        }
        szDrive[0]++;
    }
    path[0] = origDrive;
    return false;
}

// files are considered untrusted, if they're either loaded from a
// non-file URL in plugin mode, or if they're marked as being from
// an untrusted zone (e.g. by the browser that's downloaded them)
bool IsUntrustedFile(const char* filePath, const char* fileURL) {
    AutoFreeStr protocol;
    if (fileURL && str::Parse(fileURL, "%S:", &protocol)) {
        if (str::Len(protocol) > 1 && !str::EqI(protocol, "file")) {
            return true;
        }
    }

    if (file::GetZoneIdentifier(filePath) >= URLZONE_INTERNET) {
        return true;
    }

    // check all parents of embedded files and ADSs as well
    AutoFreeStr path(str::Dup(filePath));
    while (str::Len(path) > 2 && str::FindChar(path + 2, ':')) {
        *str::FindCharLast(path, ':') = '\0';
        if (file::GetZoneIdentifier(path) >= URLZONE_INTERNET) {
            return true;
        }
    }

    return false;
}

#define kColCloseX RGB(0xa0, 0xa0, 0xa0)
#define kColCloseXHover RGB(0xf9, 0xeb, 0xeb)   // white-ish
#define kColCloseXHoverBg RGB(0xC1, 0x35, 0x35) // red-ish

// Draws the 'x' close button in regular state or onhover state
// Tries to mimic visual style of Chrome tab close button
void DrawCloseButton(HWND hwnd, HDC hdc, Rect& r) {
    Point curPos = HwndGetCursorPos(hwnd);
    bool isHover = r.Contains(curPos);
    Gdiplus::Graphics g(hdc);
    g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPageUnit(Gdiplus::UnitPixel);
    // GDI+ doesn't pick up the window's orientation through the device context,
    // so we have to explicitly mirror all rendering horizontally
    if (IsRtl(hwnd)) {
        g.ScaleTransform(-1, 1);
        g.TranslateTransform((float)ClientRect(hwnd).dx, 0, Gdiplus::MatrixOrderAppend);
    }
    Gdiplus::Color c;

    // in onhover state, background is a red-ish circle
    if (isHover) {
        c.SetFromCOLORREF(kColCloseXHoverBg);
        Gdiplus::SolidBrush b(c);
        g.FillEllipse(&b, r.x, r.y, r.dx - 2, r.dy - 2);
    }

    // draw 'x'
    c.SetFromCOLORREF(isHover ? kColCloseXHover : kColCloseX);
    g.TranslateTransform((float)r.x, (float)r.y);
    Gdiplus::Pen p(c, 2);
    if (isHover) {
        g.DrawLine(&p, Gdiplus::Point(4, 4), Gdiplus::Point(r.dx - 6, r.dy - 6));
        g.DrawLine(&p, Gdiplus::Point(r.dx - 6, 4), Gdiplus::Point(4, r.dy - 6));
    } else {
        g.DrawLine(&p, Gdiplus::Point(4, 5), Gdiplus::Point(r.dx - 6, r.dy - 5));
        g.DrawLine(&p, Gdiplus::Point(r.dx - 6, 5), Gdiplus::Point(4, r.dy - 5));
    }
}
