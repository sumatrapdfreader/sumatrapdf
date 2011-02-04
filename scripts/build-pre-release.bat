@ECHO OFF

REM assumes we're being run from top-level directory as:
REM scripts\build-pre-release.bat

REM Here you can add the path to your Python installation if it's not already in PATH
REM SET PATH=%PATH%;C:\Python

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

python -u scripts\build-pre-release.py
