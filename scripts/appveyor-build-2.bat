@ECHO OFF
SETLOCAL

REM assumes we're being run from top-level directory as:
REM scripts\appveyor-build-2.bat

REM timing code: http://stackoverflow.com/questions/739606/how-long-does-a-batch-file-take-to-execute

set t0=%time: =0%

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

msbuild.exe "vs2015\SumatraPDF.sln" "/t:Installer;SumatraPDF;test_util" "/p:Configuration=Release;Platform=Win32" /m
msbuild.exe "vs2015\SumatraPDF.sln" "/t:Installer;SumatraPDF;test_util" "/p:Configuration=Release;Platform=x64" /m

cd rel
.\MakeLZSA.exe SumatraPDF.pdb.lzsa libmupdf.pdb:libmupdf.pdb Installer.pdb:Installer.pdb SumatraPDF-no-MuPDF.pdb:SumatraPDF-no-MuPDF.pdb SumatraPDF.pdb:SumatraPDF.pdb
cd ..\rel64
.\MakeLZSA.exe SumatraPDF.pdb.lzsa libmupdf.pdb:libmupdf.pdb Installer.pdb:Installer.pdb SumatraPDF-no-MuPDF.pdb:SumatraPDF-no-MuPDF.pdb SumatraPDF.pdb:SumatraPDF.pdb

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
