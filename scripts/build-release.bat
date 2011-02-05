@ECHO OFF

REM assumes we're being run from top-level directory as:
REM scripts\build-release.bat

REM Here you can add the path to your Python installation if it's not already in PATH
REM SET PATH=C:\Python;%PATH%

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

REM add our nasm.exe to the path
SET PATH=%CD%\bin;%PATH%

python scripts\build-release.py %1
