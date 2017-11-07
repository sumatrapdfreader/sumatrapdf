@ECHO OFF
SETLOCAL

REM assumes we're being run from top-level directory as:
REM scripts\appveyor-build-2.bat

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

REM TODO: appveyor complains about lack of v141_xp toolset
REM msbuild.exe "vs2017\SumatraPDF.sln" "/t:SumatraPDF;Installer;test_util" "/p:Configuration=Release;Platform=Win32" /m
REM IF ERRORLEVEL 1 EXIT /B 1

msbuild.exe "vs2017\SumatraPDF.sln" "/t:SumatraPDF;Installer;test_util" "/p:Configuration=Release;Platform=x64" /m
IF ERRORLEVEL 1 EXIT /B 1

REM rel\test_util.exe
REM IF ERRORLEVEL 1 EXIT /B 1

rel64\test_util.exe
IF ERRORLEVEL 1 EXIT /B 1

@REM cd rel
@REM ..\bin\MakeLZSA.exe SumatraPDF.pdb.lzsa libmupdf.pdb:libmupdf.pdb Installer.pdb:Installer.pdb @REM SumatraPDF-no-MuPDF.pdb:SumatraPDF-no-MuPDF.pdb SumatraPDF.pdb:SumatraPDF.pdb
@REM IF ERRORLEVEL 1 EXIT /B 1

cd ..\rel64
..\bin\MakeLZSA.exe SumatraPDF.pdb.lzsa libmupdf.pdb:libmupdf.pdb Installer.pdb:Installer.pdb SumatraPDF-no-MuPDF.pdb:SumatraPDF-no-MuPDF.pdb SumatraPDF.pdb:SumatraPDF.pdb
IF ERRORLEVEL 1 EXIT /B 1
