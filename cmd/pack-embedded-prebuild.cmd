@echo off
REM VS / ninja prebuild helper: packs .work\embedded\ (plus optional extra
REM files) into one LzSA archive linked as IDR_EMBEDDED_PAK.
REM
REM   pack-embedded-prebuild.cmd
REM     packs .work\embedded.lzsa (static build / default in SumatraPDF.rc)
REM   pack-embedded-prebuild.cmd <archive> [<file>[:<in-archive name>] ...]
REM     packs <archive> from .work\embedded\ plus the extra files
REM     (dll build: out\<cfg>\embedded.lzsa with libsumatrapdf.dll & co)
REM
REM Do NOT permanently change the caller's cwd (MSBuild chains further
REM prebuild lines that assume the project directory).
setlocal
REM %~dp0 ends with a backslash, so strip it before appending "..": chopping the
REM last character off "<repo>\cmd\.." instead leaves "<repo>\cmd\" and every
REM path below then points inside cmd\ (MakeLZSA.exe "missing", embedded.lzsa
REM never built, and the .rc fails with "file not found: ..\.work\embedded.lzsa")
set CMDDIR=%~dp0
set ROOT=%CMDDIR:~0,-1%\..
set STAGING=%ROOT%\.work\embedded

set ARCHIVE=%~1
if "%ARCHIVE%"=="" set ARCHIVE=%ROOT%\.work\embedded.lzsa

set EXTRA=
:next_extra
shift
if "%~1"=="" goto extra_done
set EXTRA=%EXTRA% "%~1"
goto next_extra
:extra_done

if not exist "%ROOT%\.work" mkdir "%ROOT%\.work"
if not exist "%ROOT%\.work\translations.txt" type nul > "%ROOT%\.work\translations.txt"

REM bootstrap the staging dir when pack-embedded.ts never ran (no manual docs then)
if not exist "%STAGING%\translations.txt" (
  if not exist "%STAGING%" mkdir "%STAGING%"
  copy /y "%ROOT%\.work\translations.txt" "%STAGING%\translations.txt" >nul
  copy /y "%ROOT%\ext\marked.min.js" "%STAGING%\marked.min.js" >nul
  copy /y "%ROOT%\ext\mermaid.min.js" "%STAGING%\mermaid.min.js" >nul
)

if not exist "%ROOT%\bin\MakeLZSA.exe" (
  echo MakeLZSA.exe missing
  exit /b 1
)

"%ROOT%\bin\MakeLZSA.exe" "%ARCHIVE%" "%STAGING%" %EXTRA%
exit /b %ERRORLEVEL%
