@rem this is for Visual Studio 2005 aka vs8
@rem call "%ProgramFiles%\Microsoft Visual Studio 8\Common7\Tools\vsvars32.bat"

@rem this is for Visual Studio 2008 aka vs9
call "%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"

nmake -f makefile.msvc EXTLIBSDIR=..\ CFG=dbg
nmake -f makefile.msvc EXTLIBSDIR=..\ CFG=rel
