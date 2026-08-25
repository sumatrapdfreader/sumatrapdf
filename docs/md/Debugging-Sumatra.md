# Debugging Sumatra

**Help us debug Sumatra hangs or crashes**

If Sumatra crashes or hangs reliably on your machine but we can't reproduce it ourselves, you can help us by debugging it. Here's a tutorial on how to do it.

Please first test with the latest pre-release version from [https://www.sumatrapdfreader.org/prerelease.html](https://www.sumatrapdfreader.org/prerelease.html).

Some bugs might already be fixed there.

These instructions require SumatraPDF 3.2 or later.

## Getting logs

We log information that might be helpful in diagnosing issues.

To see the logs, open the [Command Palette](Command-Palette.md) with `Ctrl + K` and type `show log`.

This saves the logs to a file and opens it in the default editor for `.txt` files.

## Install necessary software

- Install the WinDbg debugger from the [Microsoft Store](https://www.microsoft.com/en-us/p/windbg-preview/9pgjgd53tn86#activetab=pivot:overviewtab). Alternatively, follow Microsoft's [debugger installation instructions](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-download-tools).
- In SumatraPDF, use the `Debug` / `Download Symbols` menu. Symbols are important because they let debuggers such as WinDbg resolve addresses to names.

First, vocabulary. `%ProgramFiles%` means the standard directory where Windows installs programs. It's `c:\Program Files` on 32-bit Windows and `c:\Program Files (x86)` on 64-bit Windows.

## Debugging a crash

To debug crashes:

- start SumatraPDF.exe under the control of WinDBG
  - start WinDBG.exe
  - File/Open (Ctrl-E), find and open SumatraPDF.exe executable
  - In WinDBG, type:
    - `.sympath+ SRV*c:\symbols*https://msdl.microsoft.com/download/symbols`
    - `g`
- when Sumatra crashes, type: `!analyze -v` and paste the result of that to the bug report

## Debugging a hang

Here are the steps to follow if Sumatra hangs:

1. Start SumatraPDF.exe and get it to hang
2. Start WinDbg.exe
3. Use File / Attach to Process (`F6`) and select SumatraPDF.exe from the list
4. In WinDbg, type:
   - `.sympath+ SRV*c:\symbols*https://msdl.microsoft.com/download/symbols`
   - `~*kb`
   - `lmf`

Attach the output to the bug report.
