@echo off
REM VS prebuild helper. Do NOT permanently change the caller's cwd (MSBuild
REM chains further prebuild lines that assume the project directory).
setlocal
REM %~dp0 ends with a backslash, so strip it before appending "..": chopping the
REM last character off "<repo>\cmd\.." instead leaves "<repo>\cmd\" and every
REM path below then points inside cmd\ (MakeLZSA.exe "missing", embedded.dat
REM never built, and the .rc fails with "file not found: ..\.work\embedded.dat")
set CMDDIR=%~dp0
set ROOT=%CMDDIR:~0,-1%\..

if not exist "%ROOT%\.work" mkdir "%ROOT%\.work"
if not exist "%ROOT%\.work\translations.txt" type nul > "%ROOT%\.work\translations.txt"

if not exist "%ROOT%\.work\embedded-src\translations.txt" (
  if not exist "%ROOT%\.work\embedded-src" mkdir "%ROOT%\.work\embedded-src"
  copy /y "%ROOT%\.work\translations.txt" "%ROOT%\.work\embedded-src\translations.txt" >nul
  copy /y "%ROOT%\ext\marked.min.js" "%ROOT%\.work\embedded-src\marked.min.js" >nul
  copy /y "%ROOT%\ext\mermaid.min.js" "%ROOT%\.work\embedded-src\mermaid.min.js" >nul
)

if not exist "%ROOT%\bin\MakeLZSA.exe" (
  echo MakeLZSA.exe missing
  exit /b 1
)

"%ROOT%\bin\MakeLZSA.exe" "%ROOT%\.work\embedded.dat" "%ROOT%\.work\embedded-src"
exit /b %ERRORLEVEL%
