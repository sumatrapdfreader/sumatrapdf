# Failed to load libmupdf.dll

When installed, SumatraPDF consists of 2 files:

* `SumatraPDF.exe`
* `libmupdf.dll`

You're reading this page because loading of `libmupdf.dll` failed and we can't proceed.

**How can that happen?**

There are many possible reasons. The error code and message is what Windows reports.

## Antivirus / security software (most common for portable / pre-release)

Portable and single-file builds extract `libmupdf.dll` next to the executable or under `%LocalAppData%\SumatraPDF-data\` the first time they run.

Some antivirus products:

* block `LoadLibrary` on a just-written DLL
* quarantine or delete the DLL immediately after it is extracted
* return unusual error codes (for example **87** / “Invalid parameter”, **5** / access denied, **32** / sharing violation)

This is especially common with real-time protection and “behavior” or “ransomware” shields.

**What to try:**

1. **Add exclusions** for:
   * the folder that contains `SumatraPDF.exe` (portable install)
   * `%LocalAppData%\SumatraPDF-data\` (extracted build-specific files)
2. Temporarily **disable real-time scanning**, start SumatraPDF once so `libmupdf.dll` can load, then re-enable scanning (keep the exclusions).
3. Check the antivirus quarantine for `libmupdf.dll` and restore it if found.
4. Prefer the **official installer** from [sumatrapdfreader.org](https://www.sumatrapdfreader.org/download-free-pdf-viewer) if portable keeps failing.
5. Re-download SumatraPDF (ensure the download is complete and not modified).

If the dialog shows a Windows error code, note it when asking for help.

## Other causes

* Incomplete copy of a multi-file install (`libmupdf.dll` missing next to the `.exe`)
* Wrong / leftover `libmupdf.dll` from another version (size mismatch)
* Corrupted download or disk errors
* Security policies blocking unsigned or newly written modules

**How to investigate a specific error code:**

* Search: `LoadLibrary <error-code>`
* Ask an AI: `when calling LoadLibrary() Win32 API I got error <code>. What does it mean?`
* Ask in [SumatraPDF discussions](https://github.com/sumatrapdfreader/sumatrapdf/discussions)

See also [Corrupted installation](Corrupted-installation.md) and [Installation](Installation.md).
