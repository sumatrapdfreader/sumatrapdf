# SumatraPDF 3.6.1 → native Linux parity baseline

Reference: upstream `3.6.1rel`. Target: `src/ng/` and its shared
implementation. This is **not** a 3.6.1-equivalent release certification.

## Evidence from this checkout

| Area | Native Linux source / evidence | Status |
| --- | --- | --- |
| Document rendering | `src/shared/EngineMupdf.cpp`; `linux-daily.yml` exercises PDF, EPUB, CBZ, DjVu and PNG | Implemented; smoke-tested by existing CI |
| Command Palette | `src/ng/CommandPalette.cpp` follows the original's collection model | Implemented; interactive behavior unverified |
| Annotation editing | `src/ng/AnnotEditToolbar.cpp`, `src/ng/AnnotPlacement.cpp` | Implemented; round-trip and GUI parity unverified |
| Tab and session UI | `src/ng/Tabs.cpp`, `src/ng/SessionState.cpp` | Implemented; behavior unverified |
| Printing | `src/ng/Print_posix.cpp`; Linux daily CI checks PDF and spooler behavior | Implemented; native interactive printing differs from Win32 |
| Read aloud | `src/ng/ReadAloud_linux.cpp` | Implemented; accessibility/UI parity unverified |
| PDF scripts | `src/ng/README.md` documents a GPUI dialog and MuPDF JS child process | Implemented on Linux; needs test |
| Calibrated measurement | `src/ng/README.md` explicitly states `CmdMeasure` is Windows-only | **Missing** |
| Caption overlay | `src/ng/README.md` documents Windows-only `CmdToggleCaption` | Platform-specific gap |
| KDE taskbar identity | `src/ng/linux/sumatrapdf.desktop` exists; live Wayland app ID unverified | Needs runtime test |
| File format associations/icons | `src/ng/linux/sumatrapdf.desktop`, `src/ng/linux/icons/` | Present; new package now includes them |

## Acceptance criteria

- [ ] Build both Linux debug and release binaries on CI.
- [ ] Run base unit tests and render a representative page of each format.
- [ ] Confirm print-to-PDF and spooler tests on Linux.
- [ ] Run GUI tests of Command Palette, Ctrl+Tab, favorites, text search,
      annotation create/edit/delete/save/reopen, session restore and settings.
- [ ] Implement and test calibrated measurement with unit calibration,
      length and rectangle-area modes.
- [ ] Verify the Wayland app ID and taskbar icon under KDE Plasma.
- [ ] Verify Windows 3.6.1 command semantics and settings against the
      exact upstream tag; distinguish UI differences from missing functionality.
- [ ] Test installation on CachyOS and on a clean Arch Linux environment.

## CI scope

`.github/workflows/linux-build.yml` adds non-interactive tests for core units,
rendering and print-to-PDF before packaging. Passing CI does **not** prove
interactive feature parity. The existing `linux-daily.yml` contains additional
Linux printer-spooler checks.

The packaged binary is a build of **SumatraPDF ng at the checked-out commit**,
not the official Windows 3.6.1 binary. Its package version is an independent
local build version, not a claim of full 3.6.1 compatibility.

## Next engineering task

Port `CmdMeasure` using GPUI and shared document geometry. Start by comparing
the Windows measurement handlers and command availability, write focused
geometry/calibration tests, implement the Linux interaction, and validate
against the Windows reference behavior. Do not add a second rendering engine.
