@echo on
SETLOCAL

@REM assumes we're being run from top-level directory as:
@REM .\scripts\appveyor-build.bat

IF EXIST  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
) ELSE (
    @rem this is on GitHub Actions
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

msbuild.exe "vs2019\SumatraPDF.sln" "/t:all;Installer" "/p:Configuration=Release;Platform=Win32" /m
IF ERRORLEVEL 1 EXIT /B 1

msbuild.exe "vs2019\SumatraPDF.sln" "/t:SumatraPDF;Installer;test_util" "/p:Configuration=Release;Platform=x64" /m
IF ERRORLEVEL 1 EXIT /B 1

out\rel32\test_util.exe
IF ERRORLEVEL 1 EXIT /B 1

out\rel64\test_util.exe
IF ERRORLEVEL 1 EXIT /B 1

cd out\rel32
..\bin\MakeLZSA.exe SumatraPDF.pdb.lzsa libmupdf.pdb:libmupdf.pdb Installer.pdb:Installer.pdb SumatraPDF-mupdf-dll.pdb:SumatraPDF-mupdf-dll.pdb SumatraPDF.pdb:SumatraPDF.pdb
IF ERRORLEVEL 1 EXIT /B 1

cd ..\rel64
..\bin\MakeLZSA.exe SumatraPDF.pdb.lzsa libmupdf.pdb:libmupdf.pdb Installer.pdb:Installer.pdb SumatraPDF-mupdf-dll.pdb:SumatraPDF-mupdf-dll.pdb SumatraPDF.pdb:SumatraPDF.pdb
IF ERRORLEVEL 1 EXIT /B 1

cd ..
