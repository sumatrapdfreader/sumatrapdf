call "C:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"
@set PATH=%PATH%;%ProgramFiles%\NSIS
@set OUT_PATH=C:\kjk\src\sumatrapdf\builds

@rem create OUT_PATH if doesn't exist
@IF EXIST %OUT_PATH% GOTO DONT_CREATE_OUT_PATH
mkdir %OUT_PATH%
:DONT_CREATE_OUT_PATH

@pushd .
@set VERSION=%1
@IF NOT DEFINED VERSION GOTO VERSION_NEEDED

@rem check if makensis exists
@makensis /version >nul
@IF ERRORLEVEL 1 goto NSIS_NEEDED

@rem check if zip exists
@zip >nul
@IF ERRORLEVEL 1 goto ZIP_NEEDED

start /low /b /wait devenv ..\sumatrapdf-vc2008.sln /Rebuild "Release|Win32"
@IF ERRORLEVEL 1 goto BUILD_FAILED
echo Compilation ok!
copy ..\obj-rel\SumatraPDF.exe ..\obj-rel\SumatraPDF-uncomp.exe
copy ..\obj-rel\SumatraPDF.pdb %OUT_PATH%\SumatraPDF-%VERSION%.pdb
@rem upx --best --compress-icons=0 ..\obj-rel\SumatraPDF.exe
start /low /b /wait upx --ultra-brute --compress-icons=0 ..\obj-rel\SumatraPDF.exe
@IF ERRORLEVEL 1 goto PACK_FAILED

@makensis installer
@IF ERRORLEVEL 1 goto INSTALLER_FAILED

move SumatraPDF-install.exe ..\obj-rel\SumatraPDF-%VERSION%-install.exe
copy ..\obj-rel\SumatraPDF-%VERSION%-install.exe %OUT_PATH%\SumatraPDF-%VERSION%-install.exe

@cd ..\obj-rel
@rem don't bother compressing since our *.exe has already been packed
zip -0 SumatraPDF-%VERSION%.zip SumatraPDF.exe
copy SumatraPDF-%VERSION%.zip %OUT_PATH%\SumatraPDF-%VERSION%.zip
@goto END

:INSTALLER_FAILED
echo Installer script failed
@goto END

:PACK_FAILED
echo Failed to pack executable with upx. Do you have upx installed?
@goto END

:BUILD_FAILED
echo Build failed!
@goto END

:VERSION_NEEDED
echo Need to provide version number e.g. build-release.bat 1.0
@goto END

:NSIS_NEEDED
echo NSIS doesn't seem to be installed. Get it from http://nsis.sourceforge.net/Download
@goto END

:ZIP_NEEDED
echo zip.exe doesn't seem to be available in PATH
@goto END

:END
@popd
