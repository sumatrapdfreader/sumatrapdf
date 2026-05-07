@echo off
setlocal
set "VSBT=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
call "%VSBT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
where cl
if errorlevel 1 exit /b %errorlevel%
"%VSBT%\MSBuild\Current\Bin\MSBuild.exe" vs2022\SumatraPDF.sln /t:SumatraPDF-dll /property:Configuration=Debug /property:Platform=x64 /m /nologo /v:minimal
exit /b %errorlevel%