@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
msbuild vs2017\SumatraPDF.sln /t:SumatraPDF-mupdf-dll /p:Configuration=Debug;Platform=Win32 /m

@REM "/p:Configuration=Release;Platform=Win32", "/m")
