@ECHO OFF

CALL scripts\vc.bat
IF NOT ERRORLEVEL 1 GOTO VSFOUND
ECHO Visual Studio 2013 doesn't seem to be installed
EXIT /B 1

:VSFOUND

REM add our nasm.exe and StripReloc.exe to the path
SET PATH=%CD%\bin;%PATH%

python -u -B scripts/efi_cmp.py -with-last
