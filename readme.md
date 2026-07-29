[![Build](https://github.com/sumatrapdfreader/sumatrapdf/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/sumatrapdfreader/sumatrapdf/actions/workflows/build.yml)
## SumatraPDF Reader

SumatraPDF is a multi-format (PDF, EPUB, MOBI, CBZ, CBR, FB2, CHM, XPS, DjVu) reader
for Windows under (A)GPLv3 license, with some code under BSD license (see
AUTHORS).

More Information:
* [Website](https://www.sumatrapdfreader.org/free-pdf-reader)
* [Manual](https://www.sumatrapdfreader.org/manual)
* [Developer Information](https://www.sumatrapdfreader.org/docs/Contribute-to-SumatraPDF)

---

## This fork: the Chatterbox audiobook engine

This build adds a second Read Aloud engine. Instead of the built-in Windows TTS, 
it hands the document to a local [Chatterbox TTS](../Chatterbox-TTS-Extended-main) 
install, which reads the book in **per-character voices** — a local LLM works out 
who speaks each line once per book, each character is cast to a trained voice, and 
the narrator reads everything else. The reading is highlighted in the window as it goes.

SumatraPDF is the whole UI for it. There is no separate Python window: the
engine runs headless and SumatraPDF drives it.

* **Read Aloud ▸ Voices ▸ Use Chatterbox voices** — the engine switch. Ticked,
  Read Aloud uses Chatterbox and the Windows voices grey out; unticked, you get
  ordinary Windows TTS. Only one of the two ever speaks.
* **The playback bar** — play from the top, previous line, pause, play, stop,
  skip line, skip page. Stop remembers where you were, so Read Aloud picks up
  from that line next time.
* **Read Aloud ▸ Audiobook Characters** — a panel docked down the left, beside
  the page. Every character the LLM found, the voice each one is cast to, and
  buttons to test or train a voice. Opening it starts the engine (loading the
  TTS model) without reading anything.

  Analysis is a button there, never a surprise on the way to reading: a novel
  is hundreds of LLM calls and takes minutes. You choose how much of the book
  to do, and Stop keeps what it has worked out so far — unanalysed lines just
  read in the narrator's voice.

**Analysing on more than one computer.** The panel's *Analyse on* list shows
every machine sharing the work, each with the model it would use and whether it
answers. *Look for computers on my network* finds them (see `audiobook/discover.py`);
you can also type an address in. Both LM Studio and Ollama work, and a computer
running both is two workers, not one — they use different ports and don't know
about each other.

**No path to configure.** SumatraPDF finds the Chatterbox install itself
(`AudiobookResolveDir` in `src/SumatraPDF.cpp`): the saved setting if it still
resolves, then the usual spots under Documents and the user profile, then a
bounded scan of the root of every fixed drive. It scans drive roots because the
models run to gigabytes and get put on a real disk, never in OneDrive.

If it can't find one, it says so in the window, and you can set the folder
yourself in **Settings ▸ Advanced Settings…**. Type `audiobook` in the filter
box to narrow the list to these eight:

| setting | meaning |
|---|---|
| `Audiobook.UseChatterbox` | same switch as the Voices menu item |
| `Audiobook.ChatterboxDir` | the install folder; auto-detected |
| `Audiobook.PythonExe` | empty = `<ChatterboxDir>\.venv-amd\Scripts\pythonw.exe` |
| `Audiobook.TtsServerPort` | port of the headless TTS server (7861) |
| `Audiobook.LmStudioUrl` | the local LLM used to work out who speaks each line |
| `Audiobook.LmModel` | which model to analyse with; empty = decide automatically |
| `Audiobook.LmUrls` | other computers to analyse on; the panel fills this in |
| `Audiobook.NarratorVoice` | empty = the first available trained voice |

All are optional; `ChatterboxDir` fills itself in once the install is found.
Double-click a setting to edit it (a bool toggles, Enter confirms, Esc cancels),
and bold marks a value you've changed. Save writes the settings file and reloads
it.

**Settings ▸ Advanced Options…** opens the same settings as raw text in your
editor, if you prefer that. Note that the text file omits settings whose value
is empty, so `PythonExe` and `NarratorVoice` won't appear there until they're
set — the Advanced Settings dialog always lists them.

Without a Chatterbox install, or with the box unticked, this is just SumatraPDF.

## Building from source

**Prerequisites**

* Visual Studio 2022 (the free **Build Tools** are enough — the full IDE is not
  required). Install the *Desktop development with C++* workload.
* [bun](https://bun.sh), for the build and code-generation scripts.

Everything else — MakeLZSA, premake, nasm and the rest — is checked into `bin/`.

**The normal build**

```
bun ./cmd/build.ts
```

This produces `out/dbg64/SumatraPDF-dll.exe`. That is the binary to run and
test. Note that `out/dbg64/SumatraPDF.exe` is a *different*, statically linked
target that `build.ts` does not update, so it can be stale.

**Building a single target with msbuild**

Faster when you only touched `src/` and just want the app relinked:

```
msbuild vs2022\SumatraPDF.sln -t:SumatraPDF-dll -p:Configuration=Debug -p:Platform=x64
```

Use `-p:Configuration=Release` for the optimized build. If the link fails with
`LNK1104: cannot open file 'libmupdf.dll'`, a copy of SumatraPDF is still
running and holding the DLL — close it and build again.

The build treats warnings as errors, so a warning fails the build.

**Code generation.** Commands, settings and command-line flags are generated,
not hand-written. After editing `cmd/gen-commands.ts`, `cmd/gen-settings.ts` or
`cmd/gen-flags.ts`, run `bun cmd/gen-code.ts` to regenerate `src/Commands.*`,
`src/Settings.*` and `src/Flags.cpp` before building. New source files must be
added to **both** `premake5.files.lua` and `vs2022/SumatraPDF-dll.vcxproj`.

See `agents.md` for the rest of the house style (the `Str` type, `fmt()`,
include order, tests).

## Building the installer

The installer is a real Windows installer executable, not a script. It is the
application itself: SumatraPDF switches into installer mode when its own
filename contains `install` (`IsInstallerOrUninstallerExe` in
`src/AppTools.cpp`). So building the installer means building the Release app
and renaming a copy.

The payload is embedded at compile time. A pre-build step packs `libmupdf.dll`,
`PdfFilter.dll`, `PdfPreview.dll` and `sumatrapdf-tool.exe` into
`out/rel64/InstallerData.dat` with `bin/MakeLZSA.exe`, and the `INSTALL_PAYLOAD_ZIP`
define compiles that archive into the exe. Nothing extra needs to ship
alongside it.

```
msbuild vs2022\SumatraPDF.sln -t:SumatraPDF-dll -p:Configuration=Release -p:Platform=x64
copy out\rel64\SumatraPDF-dll.exe out\rel64\SumatraPDF-install.exe
```

`out/rel64/SumatraPDF-install.exe` is then a self-contained installer — a byte-for-byte
copy of the app, which is why the two files are the same size. Run it to
install normally, or `SumatraPDF-install.exe -s` to install silently. Run it
with `-h` for the full list of installer options.

To uninstall, use Windows' Apps & Features, or run the installed
`SumatraPDF.exe -uninstall`.

## Running and testing

Pass `-for-testing` whenever you launch a build for an ad-hoc check:

```
out\dbg64\SumatraPDF-dll.exe -for-testing <file.pdf>
```

It starts a separate instance, only opens the files you name, and — importantly —
**does not save settings**, so a test run can't overwrite your real
configuration.

Other things worth knowing:

* `-log-to-file <path>` writes a log; the audiobook engine keeps its own at
  `Chatterbox-TTS-Extended-main/audiobook/cache/engine.log`.
* `SumatraPDF-dll.exe -dde "[CmdName]"` fires a command at a running instance.
* `bun cmd/run-unit-tests.ts -dbg` runs the unit tests with readable output.
* `bun tests/all.ts` runs the regression tests.
