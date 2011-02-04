@ECHO OFF

REM assumes we're being run from top-level directory as:
REM scripts\update_translations.bat

REM Here you can add the path to your python installation if it's not already in PATH
REM SET PATH=%PATH%;C:\Python

PUSHD scripts
python update_translations.py
POPD
