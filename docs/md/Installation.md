# Installation

SumatraPDF is distributed in several forms. Pick the one that matches how you want to deploy it.

## Download flavors

| Flavor | What you get | Best for |
| --- | --- | --- |
| **Installer** (`SumatraPDF-<ver>-install.exe`) | Installs to `%LOCALAPPDATA%\SumatraPDF` (or `%PROGRAMFILES%` with `-all-users`), registers file associations, optional preview handler | Most users |
| **Portable** (`SumatraPDF-<ver>.exe` downloaded as `SumatraPDF-<ver>.zip`) | Single self-contained `.exe` — no separate `libsumatrapdf.dll`, settings live next to the exe | USB stick, custom folder, no installer |
| **Extract only** (`-x`) | Unpack files without installing | IT scripts, inspection |

Download from [sumatrapdfreader.org](https://www.sumatrapdfreader.org/download-free-pdf-viewer) or [pre-release](https://www.sumatrapdfreader.org/prerelease).

## Running the installer

Double-click the installer, or silently:

```
SumatraPDF-<ver>-install.exe -install -silent
```

Common options — full list in [Installer cmd-line arguments](Installer-cmd-line-arguments.md):

| Option | Meaning |
| --- | --- |
| `-d <dir>` | Install directory |
| `-all-users` | Install to `%PROGRAMFILES%\SumatraPDF` for all users (**ver 3.4+**) |
| `-with-preview` | Register PDF preview in File Explorer |
| `-with-filter` | Register Windows Search PDF filter |
| `-log` | Write log to `%LOCALAPPDATA%\sumatra-install-log.txt` |

## Extract without installing

```
SumatraPDF-64-install.exe -x -d "C:\Temp\Sumatra"
```

Extracts `SumatraPDF.exe`, `libsumatrapdf.dll`, `sumatrapdf-tool.exe`, etc. into the target directory. Does **not** register file associations. Recent builds do not create Start Menu / desktop shortcuts when extracting only.

## Portable vs installed settings

| | Portable | Installed |
| --- | --- | --- |
| Settings file | Same folder as `.exe` | `%LOCALAPPDATA%\SumatraPDF\SumatraPDF-settings.txt` |
| Thumbnail cache | `sumatrapdfcache` next to exe | `%LOCALAPPDATA%\SumatraPDF\sumatrapdfcache` |

Override with `-appdata <directory>` — see [Command-line arguments](Command-line-arguments.md) and [How we store settings](How-we-store-settings.md).

### Custom settings directory (enterprise / RDP)

Launch shortcuts with:

```
SumatraPDF.exe -appdata "D:\Shared\SumatraSettings"
```

All users sharing that path use the same `SumatraPDF-settings.txt`. Ensure the directory is writable by every account that runs SumatraPDF.

## Upgrade fails: file in use (PdfFilter.dll / PdfPreview.dll / libsumatrapdf.dll)

Through **3.6** the engine DLL was named `libmupdf.dll`; from **3.7** it is `libsumatrapdf.dll`.

When upgrading, the installer renames existing DLLs aside before writing new ones. That can fail if Windows still has the old file open:

| File | Often locked by |
| --- | --- |
| **PdfFilter.dll** | **Windows Search** (`SearchIndexer.exe`, `SearchFilterHost.exe`) after PDF IFilter registration |
| **PdfPreview.dll** | **File Explorer** preview pane / `dllhost.exe` / `prevhost.exe` |
| **libsumatrapdf.dll** | Running **SumatraPDF**, Explorer preview, or Search filter hosts |

Typical Windows errors during install: **32** (sharing violation / file in use), **5** (access denied on replace/delete).

### What the installer does

1. Unregisters the search filter and preview handler (if installed)
2. Stops the **Windows Search** service (`WSearch`) when possible
3. Kills processes that still load install-dir modules
4. Renames `PdfFilter.dll`, `PdfPreview.dll`, and `libsumatrapdf.dll` to `*.copy`
5. If a rename stays blocked, shows a dialog and **retries every few seconds** until success or you abort

### What you can do

1. **Close** all SumatraPDF windows and other PDF apps  
2. **Close** Explorer windows that show a **PDF preview** pane (or turn off the preview pane)  
3. Temporarily **stop Windows Search**:
   - `Win+R` → `services.msc` → **Windows Search** → Stop  
   - Or elevated: `net stop WSearch`  
4. Run the installer again (as admin for “all users” under Program Files)  
5. After install, start Windows Search again if you stopped it (`net start WSearch`)

If the install still fails, reboot and run the installer **before** opening PDFs or browsing folders with preview enabled.

## Corrupted or incomplete installation

If SumatraPDF reports a [corrupted installation](Corrupted-installation.md):

- `libsumatrapdf.dll` is missing next to `SumatraPDF.exe`, or
- `libsumatrapdf.dll` version does not match `SumatraPDF.exe`

**Fix:** run the official installer, use the portable exe, or extract with `-x` so both files match.

Also see [Failed to load libsumatrapdf.dll](Failed-to-load-libmupdf.md) if the app starts but cannot load the DLL (often antivirus).

## Installer runs when I open a PDF

SumatraPDF only shows the installer UI when the executable name contains `-install` (official installers rename themselves that way) or you pass `-install`. If double-clicking a PDF launches an installer:

- File association may point at the **installer .exe** instead of installed `SumatraPDF.exe`
- You may be running a copied installer from Downloads instead of the installed copy

Re-run the [official installer](https://www.sumatrapdfreader.org/download-free-pdf-viewer), then check [Set as default PDF viewer](Set-as-default-pdf-viewer.md).

## Multiple copies / old versions

An older SumatraPDF in `%LOCALAPPDATA%` or a renamed portable copy can confuse file associations. [Uninstall](Uninstalling-SumatraPDF.md) via Settings → Apps, then install fresh.

## Admin vs normal user

Do **not** run SumatraPDF as administrator for everyday reading. An elevated instance cannot open files dropped from a normal Explorer session ("running as admin and cannot open files from a non-admin process").

## Set as default viewer

After installing, follow [Set as default PDF viewer](Set-as-default-pdf-viewer.md). The portable exe alone does not register defaults.

## Command-line tools

After a full install, `sumatrapdf-tool.exe` is next to `SumatraPDF.exe` and on PATH. With the portable EXE you can run the same commands as `SumatraPDF.exe <tool>` (see [Tools](Tools.md)), or extract `sumatrapdf-tool.exe` with `-x`. No install required.

## See also

- [Portable vs installer](SumatraPDF-portable.md) — pre-3.7 vs 3.7 single-EXE layout, where the DLL is extracted, installer name / `-install` flag
- [FAQ](FAQ.md)
- [Corrupted installation](Corrupted-installation.md)
- [Uninstalling SumatraPDF](Uninstalling-SumatraPDF.md)
- [Installer cmd-line arguments](Installer-cmd-line-arguments.md)
