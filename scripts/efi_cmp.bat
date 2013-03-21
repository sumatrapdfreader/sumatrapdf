@ECHO OFF

:TRY_VS10
CALL "%VS100COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO VSFOUND

:TRY_VS12
CALL "%VS110COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO VSFOUND

ECHO Visual Studio 2010 or 2012 doesn't seem to be installed
EXIT /B 1

:VSFOUND

REM add our nasm.exe and StripReloc.exe to the path
SET PATH=%CD%\bin;%PATH%

python -u -B scripts/efi_cmp.py %1 %2 %3 %4
