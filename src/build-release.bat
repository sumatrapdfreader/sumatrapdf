@set PATH=%PATH%;%ProgramFiles%\Microsoft Visual Studio 8\Common7\IDE
@set PATH=%PATH%;%ProgramFiles%\NSIS
@set FASTDL_PATH=C:\kjk\src\web\fastdl\www

@pushd .
@set VERSION=%1
@IF NOT DEFINED VERSION GOTO VERSION_NEEDED

@rem check if makensis exists
@makensis /version
@IF ERRORLEVEL 1 goto NSIS_NEEDED

devenv ..\sumatrapdf.sln /Rebuild "Release|Win32"
@IF ERRORLEVEL 1 goto BUILD_FAILED
echo Compilation ok!
copy ..\obj-rel\SumatraPDF.exe ..\obj-rel\SumatraPDF-uncomp.exe
@rem upx --best ..\obj-rel\SumatraPDF.exe
upx --ultra-brute ..\obj-rel\SumatraPDF.exe
@IF ERRORLEVEL 1 goto PACK_FAILED

@makensis installer
@IF ERRORLEVEL 1 goto INSTALLER_FAILED

move SumatraPDF-install.exe ..\obj-rel\SumatraPDF-%VERSION%-install.exe
copy ..\obj-rel\SumatraPDF-%VERSION%-install.exe %FASTDL_PATH%\SumatraPDF-%VERSION%-install.exe

@cd ..\obj-rel
@rem don't bother compressing since our *.exe has already been packed
zip -0 SumatraPDF-%VERSION%.zip SumatraPDF.exe
copy SumatraPDF-%VERSION%.zip %FASTDL_PATH%\SumatraPDF-%VERSION%.zip
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

:END
@popd
