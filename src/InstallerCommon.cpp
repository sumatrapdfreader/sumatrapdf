/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// code used in both Installer.cpp and Uninstaller.cpp

#include "base/Base.h"
#include "base/AutoWin.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/FrameTimeoutCalculator.h"

#include "Translations.h"

#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"

#include "Settings.h"
#include "SumatraConfig.h"
#include "Flags.h"
#include "Version.h"
#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/Dpi.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Installer.h"
#include "InstallerUtil.h"

// set to true to enable shadow effect
constexpr bool kDrawTextShadow = true;
constexpr bool kDrawMsgTextShadow = false;

constexpr Color kInstallerWinBgColor = MkRgb(0xff, 0xf2, 0); // yellow

using Gdiplus::Bitmap;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::FontStyleRegular;
using Gdiplus::Graphics;
using Gdiplus::Image;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsDirectionRightToLeft;
using Gdiplus::UnitPixel;

Gdiplus::Color gCol1(196, 64, 50);
Gdiplus::Color gCol1Shadow(134, 48, 39);
Gdiplus::Color gCol2(227, 107, 35);
Gdiplus::Color gCol2Shadow(155, 77, 31);
Gdiplus::Color gCol3(93, 160, 40);
Gdiplus::Color gCol3Shadow(51, 87, 39);
Gdiplus::Color gCol4(69, 132, 190);
Gdiplus::Color gCol4Shadow(47, 89, 127);
Gdiplus::Color gCol5(112, 115, 207);
Gdiplus::Color gCol5Shadow(66, 71, 118);

Gdiplus::Color kColorMsgWelcome(gCol5);
Gdiplus::Color kColorMsgOk(gCol5);
Gdiplus::Color kColorMsgInstallation(gCol5);
Gdiplus::Color kColorMsgFailed(gCol1);

HWND gHwndFrame = nullptr;
__unused static bool gForceCrash = false;
int gBottomPartDy = 0;
int gButtonDy = 0;

static Str gMsg;
static Gdiplus::Color gMsgColor;

void SetMsg(Str msg, Gdiplus::Color color) {
    gMsg = str::Dup(GetPermArena(), msg);
    gMsgColor = color;
}

// whether the previous installation registered the search filter / previewer
void GetPrevInstallShellExt(PreviousInstallationInfo* info) {
    info->searchFilterInstalled = IsSearchFilterInstalled();
    info->previewInstalled = IsPreviewInstalled();
    logf("GetPrevInstallShellExt: search filter: %d, preview: %d\n", (int)info->searchFilterInstalled,
         (int)info->previewInstalled);
}

static TempStr GetExistingInstallationFilePathTemp(Str name) {
    TempStr dir = GetExistingInstallationDirTemp();
    if (len(dir) == 0) {
        return {};
    }
    return path::JoinTemp(dir, name);
}

TempStr GetShortcutPathTemp(int csidl) {
    TempStr dir = GetSpecialFolderTemp(csidl, false);
    if (len(dir) == 0) {
        return {};
    }
    TempStr lnkName = str::JoinTemp(StrL(kAppName), StrL(".lnk"));
    return path::JoinTemp(dir, lnkName);
}

constexpr const char* kSearchFilterDllName = "PdfFilter.dll";

void RegisterSearchFilter(bool allUsers, Str installDir) {
    TempStr dllPath = GetInstallationFilePathTemp(installDir, Str(kSearchFilterDllName));
    logf("RegisterSearchFilter() dllPath=%s\n", dllPath);
    bool ok = InstallSearchFilter(dllPath, allUsers);
    if (ok) {
        log(StrL("  did register\n"));
        return;
    }
    log(StrL("  failed to register\n"));
    NotifyFailed(Tr("Couldn't install PDF search filter"));
}

void UnRegisterSearchFilter() {
    TempStr dllPath = GetExistingInstallationFilePathTemp(Str(kSearchFilterDllName));
    logf("UnRegisterSearchFilter() dllPath=%s\n", dllPath);
    bool ok = UninstallSearchFilter();
    if (ok) {
        log(StrL("  did unregister\n"));
        return;
    }
    log(StrL("  failed to unregister\n"));
    NotifyFailed(Tr("Couldn't uninstall Sumatra search filter"));
}

constexpr const char* kPreviewDllName = "PdfPreview.dll";

void RegisterPreviewer(bool allUsers, Str installDir) {
    TempStr dllPath = GetInstallationFilePathTemp(installDir, Str(kPreviewDllName));
    logf("RegisterPreviewer() dllPath=%s\n", dllPath);
    bool ok = InstallPreviewDll(dllPath, allUsers);
    if (ok) {
        log(StrL("  did register\n"));
        return;
    }
    log(StrL("  failed to register\n"));
    NotifyFailed(Tr("Couldn't install PDF previewer"));
}

void UnRegisterPreviewer() {
    TempStr dllPath = GetExistingInstallationFilePathTemp(Str(kPreviewDllName));
    logf("UnRegisterPreviewer() dllPath=%s\n", dllPath);
    bool ok = UninstallPreviewDll();
    if (ok) {
        log(StrL("  did unregister\n"));
        return;
    }
    log(StrL(" failed to unregister\n"));
    NotifyFailed(Tr("Couldn't uninstall PDF previewer"));
}

// Kill a process with given <processId> if it has a module (dll or exe) <modulePath>.
// If <waitUntilTerminated> is true, will wait until process is fully killed.
// Returns TRUE if killed a process
bool KillProcWithIdAndModule(DWORD processId, Str modulePath, bool waitUntilTerminated) {
    if (!IsProcWithModule(processId, modulePath)) {
        return false;
    }
    logf("KillProcWithIdAndModule() processId=%d, modulePath=%s\n", processId, modulePath);
    return KillProcWithId(processId, waitUntilTerminated);
}

// Kill processes that have any of our install-dir modules loaded:
// libsumatrapdf.dll, PdfFilter.dll, PdfPreview.dll, SumatraPDF.exe.
// dllhost/prevhost/SearchFilterHost load the shell-extension DLLs; they may
// keep PdfFilter.dll locked even after libsumatrapdf.dll was renamed aside.
// returns false if there are processes and we failed to kill them
bool KillProcessesUsingInstallationDir(Str dir) {
    logf("KillProcessesUsingInstallationDir('%s')\n", dir);
    if (len(dir) == 0) {
        return true;
    }
    TempStr libsumatrapdf = path::JoinTemp(dir, StrL("libsumatrapdf.dll"));
    TempStr libmupdfLegacy = path::JoinTemp(dir, StrL("libmupdf.dll")); // through 3.6
    TempStr filterDll = path::JoinTemp(dir, Str(kSearchFilterDllName));
    TempStr previewDll = path::JoinTemp(dir, Str(kPreviewDllName));
    TempStr exePath = path::JoinTemp(dir, Str(kExeName));

    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snap) {
        return false;
    }

    bool killedAllProcesses = true;
    PROCESSENTRY32W proc{};
    proc.dwSize = sizeof(proc);
    BOOL ok = Process32First(snap, &proc);
    while (ok) {
        DWORD procID = proc.th32ProcessID;
        bool uses = IsProcessUsingFiles(procID, libsumatrapdf, libmupdfLegacy) ||
                    IsProcessUsingFiles(procID, filterDll, previewDll) || IsProcessUsingFiles(procID, exePath, {});
        if (uses) {
            TempStr s = ToUtf8Temp(proc.szExeFile);
            logf("  attempting to kill process %d '%s'\n", (int)procID, s);
            bool didKill = KillProcWithId(procID, true);
            logf("  KillProcWithId(%d) returned %d\n", (int)procID, (int)didKill);
            if (!didKill) {
                killedAllProcesses = false;
            }
        }
        proc.dwSize = sizeof(proc);
        ok = Process32Next(snap, &proc);
    }

    // Also target by module path (covers short-lived filter hosts).
    const TempStr modulePaths[] = {libsumatrapdf, libmupdfLegacy, filterDll, previewDll, exePath};
    for (TempStr mod : modulePaths) {
        if (file::Exists(mod)) {
            int n = KillProcessesWithModule(mod, true);
            if (n > 0) {
                logf("  KillProcessesWithModule('%s') killed=%d\n", mod, n);
            }
        }
    }
    return killedAllProcesses;
}

// Unregister PdfFilter/PdfPreview (so Windows Search / Explorer stop loading
// them) and kill processes still holding install-dir files. Must run before
// overwriting those DLLs — especially on elevated -run-install-now, which
// skips the GUI path's CheckInstallUninstallPossible().
// removedOut (optional): state to pass to RestoreShellExtensions if install fails.
void FreeInstallationFilesInUse(Str installDir, bool allUsers, ShellExtInstallState* removedOut) {
    logf("FreeInstallationFilesInUse('%s' allUsers=%d)\n", installDir, (int)allUsers);

    ShellExtInstallState removed{};
    removed.searchFilter = IsSearchFilterInstalled();
    removed.preview = IsPreviewInstalled();
    removed.allUsers = allUsers;
    if (installDir) {
        removed.installDir = str::Dup(installDir);
    }

    if (removed.searchFilter) {
        log(StrL("  unregistering search filter before file overwrite\n"));
        UninstallSearchFilter();
    }
    if (removed.preview) {
        log(StrL("  unregistering previewer before file overwrite\n"));
        UninstallPreviewDll();
    }

    if (installDir) {
        KillProcessesUsingInstallationDir(installDir);
    }
    TempStr existing = GetExistingInstallationDirTemp();
    if (existing && (len(installDir) == 0 || !str::EqI(existing, installDir))) {
        KillProcessesUsingInstallationDir(existing);
    }

    // Brief pause so terminated SearchFilterHost / dllhost release file handles.
    Sleep(250);

    if (removedOut) {
        str::Free(removedOut->installDir);
        *removedOut = removed;
        // ownership of installDir transferred to caller
        removed.installDir = {};
    } else {
        str::Free(removed.installDir);
    }
}

void RestoreShellExtensions(const ShellExtInstallState& state) {
    if (len(state.installDir) == 0) {
        log(StrL("RestoreShellExtensions: no installDir, skip\n"));
        return;
    }
    logf("RestoreShellExtensions: filter=%d preview=%d allUsers=%d dir='%s'\n", (int)state.searchFilter,
         (int)state.preview, (int)state.allUsers, state.installDir);
    if (state.searchFilter) {
        RegisterSearchFilter(state.allUsers, state.installDir);
    }
    if (state.preview) {
        RegisterPreviewer(state.allUsers, state.installDir);
    }
}

// return names of processes that are running part of the installation
// (i.e. have libsumatrapdf.dll, PdfFilter.dll, or PdfPreview.dll loaded)
void ProcessesUsingInstallation(StrVec& names) {
    log(StrL("ProcessesUsingInstallation()\n"));
    TempStr dir = GetExistingInstallationDirTemp();
    if (len(dir) == 0) {
        return;
    }
    TempStr libsumatrapdf = path::JoinTemp(dir, StrL("libsumatrapdf.dll"));
    TempStr libmupdfLegacy = path::JoinTemp(dir, StrL("libmupdf.dll")); // through 3.6
    TempStr filterDll = path::JoinTemp(dir, Str(kSearchFilterDllName));
    TempStr previewDll = path::JoinTemp(dir, Str(kPreviewDllName));
    TempStr exePath = path::JoinTemp(dir, Str(kExeName));

    AutoCloseHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == snap) {
        return;
    }

    PROCESSENTRY32W proc{};
    proc.dwSize = sizeof(proc);
    BOOL ok = Process32First(snap, &proc);
    while (ok) {
        DWORD procID = proc.th32ProcessID;
        bool uses = IsProcessUsingFiles(procID, libsumatrapdf, libmupdfLegacy) ||
                    IsProcessUsingFiles(procID, filterDll, previewDll) || IsProcessUsingFiles(procID, exePath, {});
        if (uses) {
            // TODO: this kils ReadableProcName logic
            TempStr s = ToUtf8Temp(proc.szExeFile);
            TempStr name = fmt("%s (%d)", s, (int)procID);
            names.Append(name);
        }
        proc.dwSize = sizeof(proc);
        ok = Process32Next(snap, &proc);
    }
}

// clang-format on

static void InvalidateFrame() {
    HwndRepaintNow(gHwndFrame);
}

// shows a changed message before a long operation blocks the message loop
void InstallerRepaintNow() {
    HwndRepaintNow(gHwndFrame);
}

// This display is inspired by http://letteringjs.com/
typedef struct {
    // part that doesn't change
    char c;
    Gdiplus::Color col, colShadow;
    float rotation;
    float dyOff; // displacement

    // part calculated during layout
    float dx, dy;
    float x;
} LetterInfo;

// clang-format off
static LetterInfo gLetters[] = {
    {'S', gCol1, gCol1Shadow, -3.f, 0, 0, 0},
    {'U', gCol2, gCol2Shadow, 0.f, 0, 0, 0},
    {'M', gCol3, gCol3Shadow, 2.f, -2.f, 0, 0},
    {'A', gCol4, gCol4Shadow, 0.f, -2.4f, 0, 0},
    {'T', gCol5, gCol5Shadow, 0.f, 0, 0, 0},
    {'R', gCol5, gCol5Shadow, 2.3f, -1.4f, 0, 0},
    {'A', gCol4, gCol4Shadow, 0.f, 0, 0, 0},
    {'P', gCol3, gCol3Shadow, 0.f, -2.3f, 0, 0},
    {'D', gCol2, gCol2Shadow, 0.f, 3.f, 0, 0},
    {'F', gCol1, gCol1Shadow, 0.f, 0, 0, 0}
};
// clang-format on

constexpr int kSumatraLettersCount = dimofi(gLetters);

static void SetLettersSumatraUpTo(size_t n) {
    Str s = StrL("SUMATRAPDF");
    for (size_t i = 0; i < kSumatraLettersCount; i++) {
        char c = ' ';
        if (i < n) {
            c = s.s[i];
        }
        gLetters[i].c = c;
    }
}

static void SetLettersSumatra() {
    SetLettersSumatraUpTo(kSumatraLettersCount);
}

// an animation that reveals letters one by one

static FrameTimeoutCalculator* gRevealingLettersAnim = nullptr;

static int gRevealingLettersAnimLettersToShow;

static void RevealingLettersAnimStart() {
    int framesPerSec = (int)(double(kSumatraLettersCount) / kRevealingAnimDur);
    gRevealingLettersAnim = new FrameTimeoutCalculator(framesPerSec);
    gRevealingLettersAnimLettersToShow = 0;
    SetLettersSumatraUpTo(0);
}

static void RevealingLettersAnimStop() {
    delete gRevealingLettersAnim;
    gRevealingLettersAnim = nullptr;
    SetLettersSumatra();
    InvalidateFrame();
}

bool IsRevealingLettersAnimRunning() {
    return gRevealingLettersAnim != nullptr;
}

void RevealingLettersAnim() {
    if (gRevealingLettersAnim->ElapsedTotal() > kRevealingAnimDur) {
        RevealingLettersAnimStop();
        return;
    }
    DWORD timeOut = gRevealingLettersAnim->GetTimeoutInMilliseconds();
    if (timeOut != 0) {
        return;
    }
    SetLettersSumatraUpTo(++gRevealingLettersAnimLettersToShow);
    gRevealingLettersAnim->Step();
    InvalidateFrame();
}

// GDI+ Font(name, emSize) defaults to UnitPoint and converts with the Graphics
// DPI. The installer window is already DpiScale'd; on Windows 7 GDI+ may still
// use the real screen DPI while the process is 96-DPI virtualized, so the logo
// is scaled twice (issue #6025). Draw in pixels at our layout DPI.
static float ImpactPx(int sizePt) {
    return (float)DpiGet() * (float)sizePt / 72.f;
}

static void CalcLettersLayout(Graphics& g, Font* f, int dx) {
    static int laidOutDx = 0;
    if (laidOutDx == dx) {
        return;
    }
    laidOutDx = dx;

    StringFormat sfmt;
    const float letterSpacing = -(float)DpiScale(12);
    float totalDx = -letterSpacing; // counter last iteration of the loop
    WCHAR s[2]{};
    Gdiplus::PointF origin(0.f, 0.f);
    Gdiplus::RectF bbox;
    for (LetterInfo& li : gLetters) {
        s[0] = li.c;
        g.MeasureString(s, 1, f, origin, &sfmt, &bbox);
        li.dx = bbox.Width;
        li.dy = bbox.Height;
        totalDx += li.dx;
        totalDx += letterSpacing;
    }

    float x = ((float)dx - totalDx) / 2.f;
    for (LetterInfo& li : gLetters) {
        li.x = x;
        x += li.dx;
        x += letterSpacing;
    }
    RevealingLettersAnimStart();
}

static float DrawMessage(Graphics& g, Str msg, float y, float dx, Gdiplus::Color color) {
    WCHAR* s = CWStrTemp(msg);

    Font f(L"Impact", ImpactPx(16), FontStyleRegular, UnitPixel);
    Gdiplus::RectF maxbox(0, y, dx, 0);
    Gdiplus::RectF bbox;
    g.MeasureString(s, -1, &f, maxbox, &bbox);

    bbox.X += (dx - bbox.Width) / 2.f;
    StringFormat sft;
    sft.SetAlignment(StringAlignmentCenter);
    if (trans::IsCurrLangRtl()) {
        sft.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
    }

    if (kDrawMsgTextShadow) {
        bbox.X--;
        bbox.Y++;
        SolidBrush b(Gdiplus::Color(0xff, 0xff, 0xff));
        g.DrawString(s, -1, &f, bbox, &sft, &b);
        bbox.X++;
        bbox.Y--;
    }

    SolidBrush b(color);
    g.DrawString(s, -1, &f, bbox, &sft, &b);

    return bbox.Height;
}

static void DrawSumatraLetters(Graphics& g, Font* f, Font* fVer, float y) {
    WCHAR s[2]{};
    for (const LetterInfo& li : gLetters) {
        s[0] = li.c;
        if (s[0] == ' ') {
            return;
        }

        g.RotateTransform(li.rotation, MatrixOrderAppend);
        float dyOff = li.dyOff * (float)DpiGet() / 96.f;
        if (kDrawTextShadow) {
            // draw shadow first
            SolidBrush b2(li.colShadow);
            Gdiplus::PointF o2(li.x - (float)DpiScale(3), y + (float)DpiScale(4) + dyOff);
            g.DrawString(s, 1, f, o2, &b2);
        }

        SolidBrush b1(li.col);
        Gdiplus::PointF o1(li.x, y + dyOff);
        g.DrawString(s, 1, f, o1, &b1);
        g.RotateTransform(li.rotation, MatrixOrderAppend);
        g.ResetTransform();
    }

    // draw version number
    float x = gLetters[dimof(gLetters) - 1].x;
    g.TranslateTransform(x, y);
    g.RotateTransform(45.f);
    float x2 = (float)DpiScale(15);
    float y2 = -(float)DpiScale(34);

    const WCHAR* ver_s = L"v" CURR_VERSION_STR;
    if (kDrawTextShadow) {
        SolidBrush b1(Gdiplus::Color(0, 0, 0));
        g.DrawString(ver_s, -1, fVer, Gdiplus::PointF(x2 - (float)DpiScale(2), y2 - (float)DpiScale(1)), &b1);
    }
    SolidBrush b2(Gdiplus::Color(0xff, 0xff, 0xff));
    g.DrawString(ver_s, -1, fVer, Gdiplus::PointF(x2, y2), &b2);
    g.ResetTransform();
}

static void DrawFrame2(Graphics& g, Rect r, bool skipMessage) {
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(Gdiplus::UnitPixel);

    Font f(L"Impact", ImpactPx(40), FontStyleRegular, UnitPixel);
    CalcLettersLayout(g, &f, r.dx);

    Gdiplus::Color bgCol;
    bgCol.SetFromCOLORREF(kInstallerWinBgColor);
    SolidBrush bgBrush(bgCol);
    Gdiplus::Rect r2(ToGdipRect(r));
    r2.Inflate(1, 1);
    g.FillRectangle(&bgBrush, r2);

    Font f2(L"Impact", ImpactPx(16), FontStyleRegular, UnitPixel);
    DrawSumatraLetters(g, &f, &f2, (float)DpiScale(18));

    if (skipMessage) {
        return;
    }

    float msgY = (float)(r.dy / 2);
    if (gMsg) {
        msgY += DrawMessage(g, gMsg, msgY, (float)r.dx, gMsgColor) + (float)DpiScale(5);
    }
    if (gMsgError) {
        DrawMessage(g, gMsgError, msgY, (float)r.dx, kColorMsgFailed);
    }
}

static void DrawFrame(HWND hwnd, HDC dc, PAINTSTRUCT* /*ps*/, bool skipMessage) {
    // TODO: cache bmp object?
    Graphics g(dc);
    Rect rc = HwndClientRect(hwnd);
    Bitmap bmp(rc.dx, rc.dy, &g);
    Graphics g2((Image*)&bmp);
    DrawFrame2(g2, rc, skipMessage);
    g.DrawImage(&bmp, 0, 0);
}

void OnPaintFrame(HWND hwnd, bool skipMessage, VirtRoot* virt) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    DrawFrame(hwnd, dc, &ps, skipMessage);
    if (virt) {
        // DrawFrame() has just painted the background for us, so the virtual
        // controls only draw themselves on top of it
        SetBkMode(dc, TRANSPARENT);
        GfxHdc gfx(dc);
        virt->Paint(&gfx, HwndClientRect(hwnd));
    }
    EndPaint(hwnd, &ps);
}
