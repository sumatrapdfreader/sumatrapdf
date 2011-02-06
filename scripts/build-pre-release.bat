@ECHO OFF

REM assumes we're being run from top-level directory as:
REM scripts\build-pre-release.bat

REM You can add Python to PATH if it's not already there
REM SET PATH=C:\Python;%PATH%

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

REM add our nasm.exe to the path
SET PATH=%CD%\bin;%PATH%

python -u -B scripts\build-pre-release.py
