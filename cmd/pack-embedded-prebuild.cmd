@echo off
REM VS / ninja prebuild helper: stages everything SumatraPDF.exe embeds under
REM out\<cfg>\ and packs it into one LzSA archive linked as IDR_EMBEDDED_PAK
REM (SumatraPDF.rc gets its path via the EMBEDDED_PAK resdefine, see premake5.lua).
REM
REM   pack-embedded-prebuild.cmd <staging dir> <archive> [<file>[:<in-archive name>] ...]
REM
REM   SumatraPDF-static:  out\<cfg>\embedded-static  ->  out\<cfg>\embedded-static.lzsa
REM   SumatraPDF:         out\<cfg>\embedded         ->  out\<cfg>\embedded.lzsa
REM                       plus libsumatrapdf.dll & co (the installer payload)
REM                       passed as extra files
REM
REM Staged (in-archive names are relative to the staging dir):
REM   .work\translations.txt  (created empty when trans-dl.ts never ran)
REM   ext\marked.min.js, ext\mermaid.min.js
REM   .work\docs\**           (in-app manual from gen-docs.ts; skipped when missing)
REM
REM Only cmd + MakeLZSA so MSBuild need not have bun on PATH. The archive is kept
REM between runs: MakeLZSA reuses unchanged entries, which keeps rebuilds fast.
REM
REM Do NOT permanently change the caller's cwd (MSBuild chains further
REM prebuild lines that assume the project directory).
setlocal
if "%~2"=="" (
  echo usage: pack-embedded-prebuild.cmd ^<staging dir^> ^<archive^> [^<file^>[:^<in-archive name^>] ...]
  exit /b 1
)
REM %~dp0 ends with a backslash, so strip it before appending "..": chopping the
REM last character off "<repo>\cmd\.." instead leaves "<repo>\cmd\" and every
REM path below then points inside cmd\ (MakeLZSA.exe "missing", the archive
REM never built, and the .rc fails with "file not found: ...\embedded.lzsa")
set CMDDIR=%~dp0
set ROOT=%CMDDIR:~0,-1%\..
set WORK=%ROOT%\.work
REM expand to full paths with backslashes (cmd expands %% even in REM lines, so
REM the modifier isn't spelled out here); the callers pass mixed vs2022-relative
REM slashes that rmdir / robocopy don't always accept.
set STAGING=%~f1
set ARCHIVE=%~f2

set EXTRA=
shift
:next_extra
shift
if "%~1"=="" goto extra_done
set EXTRA=%EXTRA% "%~1"
goto next_extra
:extra_done

if not exist "%ROOT%\bin\MakeLZSA.exe" (
  echo MakeLZSA.exe missing
  exit /b 1
)
if not exist "%WORK%" mkdir "%WORK%"
if not exist "%WORK%\translations.txt" type nul > "%WORK%\translations.txt"

REM mirror the manual first (/MIR also drops files no longer in .work\docs);
REM robocopy exit codes below 8 are success
if exist "%WORK%\docs\" (
  robocopy "%WORK%\docs" "%STAGING%" /MIR /NFL /NDL /NJH /NJS /NP >nul
  if errorlevel 8 (
    echo robocopy "%WORK%\docs" "%STAGING%" failed
    exit /b 1
  )
) else (
  echo note: %WORK%\docs missing, packing without the manual ^(run: bun cmd\gen-docs.ts^)
  if exist "%STAGING%\" rmdir /s /q "%STAGING%"
)
if not exist "%STAGING%\" mkdir "%STAGING%"
copy /y "%WORK%\translations.txt" "%STAGING%\translations.txt" >nul || exit /b 1
copy /y "%ROOT%\ext\marked.min.js" "%STAGING%\marked.min.js" >nul || exit /b 1
copy /y "%ROOT%\ext\mermaid.min.js" "%STAGING%\mermaid.min.js" >nul || exit /b 1

"%ROOT%\bin\MakeLZSA.exe" "%ARCHIVE%" "%STAGING%" %EXTRA%
exit /b %ERRORLEVEL%
