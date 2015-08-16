@ECHO OFF
SETLOCAL

REM TODO: also create SumatraPDF.pdb.lzsa. We can do it here becasue we
REM have MakeLZSA.exe in target directory

REM assumes we're being run from top-level directory as:
REM scripts\appveyor-build-2.bat

REM timing code: http://stackoverflow.com/questions/739606/how-long-does-a-batch-file-take-to-execute

set t0=%time: =0%

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

cd vs2015
msbuild.exe SumatraPDF.sln /t:Installer:Rebuild /p:Configuration=Release;Platform=Win32
msbuild.exe SumatraPDF.sln /t:Installer:Rebuild /p:Configuration=Release;Platform=x64

set t=%time: =0%

set /a h=1%t0:~0,2%-100
set /a m=1%t0:~3,2%-100
set /a s=1%t0:~6,2%-100
set /a c=1%t0:~9,2%-100
set /a starttime = %h% * 360000 + %m% * 6000 + 100 * %s% + %c%

rem make t into a scaler in 100ths of a second
set /a h=1%t:~0,2%-100
set /a m=1%t:~3,2%-100
set /a s=1%t:~6,2%-100
set /a c=1%t:~9,2%-100
set /a endtime = %h% * 360000 + %m% * 6000 + 100 * %s% + %c%

rem runtime in 100ths is now just end - start
set /a runtime = %endtime% - %starttime%
set runtime = %s%.%c%

echo Started at %t0%
echo Ran for %runtime%0 ms
