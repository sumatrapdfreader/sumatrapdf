@ECHO OFF
SETLOCAL

REM You can add Python to PATH if it's not already there
REM SET PATH=C:\Python;%PATH%

CALL vc.bat
IF ERRORLEVEL 1 EXIT /B 1

lib /def:lib\msvcrt.def /OUT:lib\msvcrt_dll.lib
lib /def:lib\ntdll.def /OUT:lib\ntdll_dll.lib