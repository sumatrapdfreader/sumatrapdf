# Installation

SumatraPDF is distributed in several forms. Pick the one that matches how you want to deploy it.

## Download flavors

| Flavor | What you get | Best for |
| --- | --- | --- |
| **Installer** (`SumatraPDF-<ver>-install.exe`) | Installs to `%LOCALAPPDATA%\SumatraPDF` (or `%PROGRAMFILES%` with `-all-users`), registers file associations, optional preview handler | Most users |
| **Portable** (`SumatraPDF-<ver>.exe` downloaded as `SumatraPDF-<ver>.zip`) | Single self-contained `.exe` — no separate `libmupdf.dll`, settings live next to the exe | USB stick, custom folder, no installer |
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

Extracts `SumatraPDF.exe`, `libmupdf.dll`, `sumatrapdf-tool.exe`, etc. into the target directory. Does **not** register file associations. Recent builds do not create Start Menu / desktop shortcuts when extracting only.

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

## Corrupted or incomplete installation

If SumatraPDF reports a [corrupted installation](Corrupted-installation.md):

- `libmupdf.dll` is missing next to `SumatraPDF.exe`, or
- `libmupdf.dll` version does not match `SumatraPDF.exe`

**Fix:** run the official installer, use the portable exe, or extract with `-x` so both files match.

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

`sumatrapdf-tool.exe` is installed next to `SumatraPDF.exe` by the installer. It is not included in the single-file portable build. See [Tools](Tools.md).

## See also

- [FAQ](FAQ.md)
- [Corrupted installation](Corrupted-installation.md)
- [Uninstalling SumatraPDF](Uninstalling-SumatraPDF.md)
- [Installer cmd-line arguments](Installer-cmd-line-arguments.md)
