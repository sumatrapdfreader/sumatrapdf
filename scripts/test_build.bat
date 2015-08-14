call "%VS140COMNTOOLS%vsvars32.bat"
IF NOT ERRORLEVEL 1 GOTO VSFOUND
ECHO Visual Studio 2015 doesn't seem to be installed
EXIT /B 1

:VSFOUND
@rem http://www.hanselman.com/blog/DevEnvcomSairamasTipOfTheDay.aspx
@rem https://msdn.microsoft.com/en-us/library/abtk353z.aspx
@rem devenv.exe vs2015\SumatraPDF.sln /Rebuild "Release|Win32" /Project vs2015\all.vcxproj

cd vs2015
@rem /p:AdditionalPreprocessorDefinitions="SOMEDEF;ANOTHERDEF"
@rem /p:Configuration=Release;Platform=Win32 /t:proj:Rebuild
@rem https://msdn.microsoft.com/en-us/library/ms171462.aspx
@rem msbuild.exe SumatraPDF.sln /p:Configuration=Release;Platform=Win32

@rem msbuild.exe SumatraPDF.vcxproj /p:Configuration=Release;Platform=Win32

@rem msbuild.exe Installer.vcxproj /p:Configuration=Release;Platform=Win32

@rem msbuild.exe SumatraPDF.sln /t:SumatraPDF-no-MUPDF:Rebuild /p:Configuration=Release;Platform=Win32

msbuild.exe SumatraPDF.sln /t:Installer:Rebuild /p:Configuration=Release;Platform=Win32
