@echo off
setlocal

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

set "PLATFORM=%~2"
if "%PLATFORM%"=="" set "PLATFORM=x64"

set "TARGET=%~3"
if "%TARGET%"=="" set "TARGET=SumatraPDF-dll"

set "VSBT=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
call "%VSBT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%

where cl
if errorlevel 1 exit /b %errorlevel%

"%VSBT%\MSBuild\Current\Bin\MSBuild.exe" vs2022\SumatraPDF.sln /property:GenerateFullPaths=true /t:%TARGET% /property:Configuration=%CONFIG% /property:Platform=%PLATFORM% /m /consoleloggerparameters:NoSummary
exit /b %errorlevel%
