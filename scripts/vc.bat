@ECHO OFF

REM append ..\bin to PATH to make nasm.exe available
FOR %%p IN (nasm.exe) DO IF "%%~$PATH:p" == "" SET PATH=%PATH%;%~dp0..\bin

IF EXIST C:\Python27\python.exe SET PATH=%PATH%;C:\Python27

REM for an alternative approach, see
REM https://github.com/HaxeFoundation/hxcpp/blob/master/toolchain/msvc-setup.bat

IF "%1" == "2015" GOTO VS_2015
IF "%1" == "2013" GOTO VS_2013
IF EXIST "%VS140COMNTOOLS%\vsvars32.bat" GOTO VS_2015
IF EXIST "%VS120COMNTOOLS%\vsvars32.bat" GOTO VS_2013

ECHO Visual Studio 2013 or 2015 doesn't seem to be installed
EXIT /B 1

:VS_2015
CALL "%VS140COMNTOOLS%\vsvars32.bat"
SET _VS_VERSION=VS 2015
REM defining _USING_V140_SDK71_ is only needed for MFC/ATL headers
GOTO SELECT_PLATFORM

:VS_2013
CALL "%VS120COMNTOOLS%\vsvars32.bat"
SET _VS_VERSION=VS 2013
REM defining _USING_V120_SDK71_ is only needed for MFC/ATL headers
GOTO SELECT_PLATFORM

:SELECT_PLATFORM
IF NOT "%ProgramFiles(x86)%"=="" GOTO XP_WIN64
REM TODO: for /analyze, we shouldn't use XP toolset
GOTO XP_WIN32

:XP_WIN32
ECHO Setting up %_VS_VERSION% with XP toolkit for Win32
SET "INCLUDE=%PROGRAMFILES%\Microsoft SDKs\Windows\7.1A\Include;%INCLUDE%"
SET "PATH=%PROGRAMFILES%\Microsoft SDKs\Windows\7.1A\Bin;%PATH%"
SET "LIB=%PROGRAMFILES%\Microsoft SDKs\Windows\7.1A\Lib;%LIB%"
EXIT /B

:XP_WIN64
ECHO Setting up %_VS_VERSION% with XP toolkit for Win64
SET "INCLUDE=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Include;%INCLUDE%"
SET "PATH=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Bin;%PATH%"
SET "LIB=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Lib;%LIB%"
@REM TODO: should it rather be ...?
@REM SET LIB=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Lib\x64;%LIB%
EXIT /B
