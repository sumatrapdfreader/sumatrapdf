@ECHO OFF
SETLOCAL

REM assumes we're being run from top-level directory as:
REM scripts\appveyor-build.bat

REM temporarily build 64-bit releases
GOTO build64

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1
nmake -f makefile.msvc CFG=rel PLATFORM=X86 SumatraPDF Uninstaller

EXIT /B

:build64
CALL "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64
IF ERRORLEVEL 1 EXIT /B 1
nmake -f makefile.msvc CFG=rel PLATFORM=X64 EXTCFLAGS="/D VER_QUALIFIER=x64" STRICT_X64=1 Installer

@rem TODO: build with more platforms, build and run tests etc.
