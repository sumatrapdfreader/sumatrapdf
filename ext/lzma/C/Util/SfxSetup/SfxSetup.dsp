# Microsoft Developer Studio Project File - Name="SfxSetup" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=SfxSetup - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SfxSetup.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SfxSetup.mak" CFG="SfxSetup - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SfxSetup - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "SfxSetup - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SfxSetup - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "SfxSetup - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "SfxSetup - Win32 Release"
# Name "SfxSetup - Win32 Debug"
# Begin Group "Common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\7z.h
# End Source File
# Begin Source File

SOURCE=..\..\7zAlloc.c
# End Source File
# Begin Source File

SOURCE=..\..\7zAlloc.h
# End Source File
# Begin Source File

SOURCE=..\..\7zBuf.c
# End Source File
# Begin Source File

SOURCE=..\..\7zBuf.h
# End Source File
# Begin Source File

SOURCE=..\..\7zCrc.c
# End Source File
# Begin Source File

SOURCE=..\..\7zCrc.h
# End Source File
# Begin Source File

SOURCE=..\..\7zCrcOpt.c
# End Source File
# Begin Source File

SOURCE=..\..\7zDec.c
# End Source File
# Begin Source File

SOURCE=..\..\7zFile.c
# End Source File
# Begin Source File

SOURCE=..\..\7zFile.h
# End Source File
# Begin Source File

SOURCE=..\..\7zIn.c
# End Source File
# Begin Source File

SOURCE=..\..\7zStream.c
# End Source File
# Begin Source File

SOURCE=..\..\Bcj2.c
# End Source File
# Begin Source File

SOURCE=..\..\Bcj2.h
# End Source File
# Begin Source File

SOURCE=..\..\Bra.c
# End Source File
# Begin Source File

SOURCE=..\..\Bra.h
# End Source File
# Begin Source File

SOURCE=..\..\Bra86.c
# End Source File
# Begin Source File

SOURCE=..\..\CpuArch.c
# End Source File
# Begin Source File

SOURCE=..\..\CpuArch.h
# End Source File
# Begin Source File

SOURCE=..\..\Lzma2Dec.c
# End Source File
# Begin Source File

SOURCE=..\..\Lzma2Dec.h
# End Source File
# Begin Source File

SOURCE=..\..\LzmaDec.c
# End Source File
# Begin Source File

SOURCE=..\..\LzmaDec.h
# End Source File
# Begin Source File

SOURCE=..\..\Types.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\SfxSetup.c
# End Source File
# End Target
# End Project
