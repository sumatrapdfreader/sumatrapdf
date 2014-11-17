@ECHO OFF

CALL scripts\vc.bat
IF NOT ERRORLEVEL 1 GOTO VSFOUND
ECHO Visual Studio 2013 doesn't seem to be installed
EXIT /B 1

:VSFOUND

REM add our nasm.exe and StripReloc.exe to the path
SET PATH=%CD%\bin;%PATH%

rem work-around cygwin/msdev issue
set tmp=
set temp=

python -u -B scripts/buildbot.py
