@ECHO OFF
SETLOCAL

REM assumes we're being run from top-level directory as:
REM scripts\appveyor-build.bat

REM temporarily build 64-bit releases
GOTO Build64

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1
nmake -f makefile.msvc CFG=rel PLATFORM=X86 SumatraPDF Installer AllSymbols
IF ERRORLEVEL 1 EXIT /B 1

GOTO RunTests

:Build64
CALL "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64
IF ERRORLEVEL 1 EXIT /B 1
nmake -f makefile.msvc CFG=rel PLATFORM=X64 EXTCFLAGS="/D VER_QUALIFIER=x64" STRICT_X64=1 SumatraPDF Installer AllSymbols
IF ERRORLEVEL 1 EXIT /B 1

GOTO RunTests

:RunTests
devenv /NOLOGO vs\all_tests.sln /build Release && obj-rel\test_util.exe
IF ERRORLEVEL 1 EXIT /B 1

@rem TODO: build with more platforms, build and run tests etc.
