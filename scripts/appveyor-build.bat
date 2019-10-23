@echo on
SETLOCAL

REM assumes we're being run from top-level directory as:
REM scripts\appveyor-build.bat

@rem this is local for testing
@rem call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

@rem this is on GitHub Actions
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64

@rem don't build 32-bit version for now as it uses 141_xp toolset
@rem msbuild.exe "vs2019\SumatraPDF.sln" "/t:all;Installer" "/p:Configuration=Release;Platform=Win32" /m
@rem IF ERRORLEVEL 1 EXIT /B 1

msbuild.exe "vs2019\SumatraPDF.sln" "/t:SumatraPDF;Installer;test_util" "/p:Configuration=Release;Platform=x64" /m
IF ERRORLEVEL 1 EXIT /B 1

@rem rel\test_util.exe
@rem IF ERRORLEVEL 1 EXIT /B 1

rel64\test_util.exe
IF ERRORLEVEL 1 EXIT /B 1

@rem cd rel
@rem ..\bin\MakeLZSA.exe SumatraPDF.pdb.lzsa libmupdf.pdb:libmupdf.pdb Installer.pdb:Installer.pdb SumatraPDF-no-MuPDF.pdb:SumatraPDF-no-MuPDF.pdb SumatraPDF.pdb:SumatraPDF.pdb
@rem IF ERRORLEVEL 1 EXIT /B 1

cd rel64
..\bin\MakeLZSA.exe SumatraPDF.pdb.lzsa libmupdf.pdb:libmupdf.pdb Installer.pdb:Installer.pdb SumatraPDF-no-MuPDF.pdb:SumatraPDF-no-MuPDF.pdb SumatraPDF.pdb:SumatraPDF.pdb
IF ERRORLEVEL 1 EXIT /B 1

cd ..
