# Portable vs installer (and how `libsumatrapdf.dll` is loaded)

This page explains how SumatraPDF’s **portable** and **installable** layouts work, what changed in **3.7**, and why you sometimes see `libsumatrapdf.dll` on disk even with a “single” executable.

**DLL name:** through **3.6** the companion engine DLL was **`libmupdf.dll`**. From **3.7** it is **`libsumatrapdf.dll`**. This page uses the 3.7+ name; older installs and logs may still show `libmupdf.dll`.

See also [Installation](Installation.md), [Corrupted installation](Corrupted-installation.md), and [Failed to load libsumatrapdf.dll](Failed-to-load-libmupdf.md).

## Before 3.7: two different product shapes

| Flavor | What users got | Layout |
|--------|----------------|--------|
| **Installer** | Run an installer; files under Program Files or `%LOCALAPPDATA%` | Multiple files on disk: `SumatraPDF.exe`, **`libsumatrapdf.dll`**, and optional helpers such as **`PdfPreview.dll`** (Explorer preview) and the **search filter** (ifilter) DLL when registered |
| **Portable** | Download a zip / single program | **One self-contained `.exe`** — no separate `libsumatrapdf.dll` next to the app for normal use |

That meant two build/distribution artifacts: an installable multi-file executable and a portable single-file executable.

## From 3.7: one executable is both installer and portable

Starting with **3.7**, the **same** `SumatraPDF.exe` binary can:

- act as the **installer** (register associations, write files into an install directory, etc.), or  
- run as the **application** in portable or installed form.

**Benefit:** we only need to **build and ship one executable** for the main app (plus optional tools). Installer and portable are no longer two different executables.

### Why `libsumatrapdf.dll` still appears on disk

MuPDF lives in **`libsumatrapdf.dll`**, which is **delay-loaded** and also **embedded** inside the EXE (compressed resource). Windows can only load a normal DLL from a **file path** with `LoadLibrary` — not straight out of the EXE’s resources.

So for the unified single-EXE design to work, SumatraPDF must **extract** `libsumatrapdf.dll` to disk at least once, then load it. That is the cost of merging portable and installer into one binary.

(Static-linked special builds that bake MuPDF into the EXE do not embed/extract `libsumatrapdf.dll`; the public downloads use the DLL model.)

## How we choose installer mode vs app mode

Installer UI / install logic runs when any of these is true:

1. **Executable name contains `install`** (case-insensitive), e.g.  
   `SumatraPDF-3.7-64-install.exe`  
   Official installers are named this way. Names containing **`uninstall`** are treated as uninstaller, not installer.
2. **Command-line flags** that mean “install now”, notably **`-install`** (and related install flags such as fast/silent install variants documented under [Installer cmd-line arguments](Installer-cmd-line-arguments.md)).

So:

- Renaming the file so the name includes `-install` (or downloading the official `*-install.exe`) → installer behavior.  
- Running without that name as a normal app → viewer mode (portable or already-installed layout).  
- You can force install with `-install` even if you renamed the EXE.

If the name looks like an installed app but `libsumatrapdf.dll` next to the EXE is **missing** or the **wrong size** (does not match the copy embedded in this EXE), Sumatra may treat the layout as a **corrupted installation** and stop — see [Corrupted installation](Corrupted-installation.md). Use the installer, portable load path, or **`-x`** extract so EXE and DLL match.

## Where `libsumatrapdf.dll` is extracted / loaded

On startup, SumatraPDF tries to load a DLL whose **file size matches** the embedded `libsumatrapdf.dll` (avoids using a leftover DLL from another build).

Order of attempts (simplified):

1. **Next to the executable**  
   `…\libsumatrapdf.dll` beside `SumatraPDF.exe`  
   - Typical **after a full install** or after **`-x` extract**.  
   - Load only (no extract) if the size already matches.

2. **Build-specific data directory under Local AppData** (portable / single-exe path when the DLL is not already beside the EXE):  
   `%LOCALAPPDATA%\SumatraPDF-data\<build-id>\libsumatrapdf.dll`  
   - `<build-id>` is a short hex id derived from a hash of **this** EXE (so different builds do not share one DLL).  
   - Sumatra **extracts** the embedded DLL here if needed, then loads it.  
   - Same tree is used for other non-settings data (logs, crash info, caches) for that build.

3. **Fallback:** extract next to the EXE (if AppData is unusable) and load from there.

So:

| Situation | Typical `libsumatrapdf.dll` location |
|-----------|----------------------------------|
| Installed via installer / extracted with `-x` | Same folder as `SumatraPDF.exe` |
| Portable / single-exe first run | `%LOCALAPPDATA%\SumatraPDF-data\<build-id>\` |

Settings still follow portable vs installed rules separately (settings next to the EXE when running portable; `%LOCALAPPDATA%\SumatraPDF` when installed) — see [How we store settings](How-we-store-settings.md). That is independent of where the MuPDF DLL is extracted.

## Portable mode (settings / data next to the EXE)

“Portable mode” for **settings** means: not considered an installed copy (not under Program Files / known install location). Then settings and some caches prefer the directory of the executable (if writable). That is separate from “single EXE that still extracts `libsumatrapdf.dll` to AppData.”

## Summary

| Topic | Detail |
|-------|--------|
| **Through 3.6** | Installer = multi-file (`SumatraPDF.exe` + **`libmupdf.dll`** + preview/ifilter DLLs as installed); portable = single self-contained EXE |
| **3.7+** | Same EXE is installer *and* app; engine DLL is **`libsumatrapdf.dll`** (renamed from `libmupdf.dll`); still needs a real DLL file on disk |
| **Installer trigger** | Name contains `install` (not `uninstall`), and/or `-install` (etc.) |
| **DLL extract** | Embedded DLL → preferably next to EXE if already installed; else `%LOCALAPPDATA%\SumatraPDF-data\<build-id>\` |
| **Why** | One binary to build and distribute; Windows requires loading DLL from disk |

## Related

- [Installation](Installation.md)  
- [Installer cmd-line arguments](Installer-cmd-line-arguments.md)  
- [Corrupted installation](Corrupted-installation.md)  
- [Failed to load libsumatrapdf.dll](Failed-to-load-libmupdf.md)  
- [How we store settings](How-we-store-settings.md)
