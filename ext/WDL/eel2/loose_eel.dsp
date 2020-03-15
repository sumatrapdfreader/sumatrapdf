# Microsoft Developer Studio Project File - Name="loose_eel" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=loose_eel - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "loose_eel.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "loose_eel.mak" CFG="loose_eel - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "loose_eel - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "loose_eel - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "loose_eel - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D WDL_FFT_REALSIZE=8 /D NSEEL_LOOPFUNC_SUPPORT_MAXLEN=0 /D "EEL_LICE_WANT_STANDALONE" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "loose_eel - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D WDL_FFT_REALSIZE=8 /D NSEEL_LOOPFUNC_SUPPORT_MAXLEN=0 /D "EEL_LICE_WANT_STANDALONE" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "loose_eel - Win32 Release"
# Name "loose_eel - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "eel2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=".\ns-eel-addfuncs.h"
# End Source File
# Begin Source File

SOURCE=".\ns-eel-int.h"
# End Source File
# Begin Source File

SOURCE=".\ns-eel.h"
# End Source File
# Begin Source File

SOURCE=".\nseel-caltab.c"
# End Source File
# Begin Source File

SOURCE=".\nseel-cfunc.c"
# End Source File
# Begin Source File

SOURCE=".\nseel-compiler.c"
# End Source File
# Begin Source File

SOURCE=".\nseel-eval.c"
# End Source File
# Begin Source File

SOURCE=".\nseel-lextab.c"
# End Source File
# Begin Source File

SOURCE=".\nseel-ram.c"
# End Source File
# Begin Source File

SOURCE=".\nseel-yylex.c"
# End Source File
# End Group
# Begin Group "lice"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\lice\lice.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_arc.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_bmp.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_ico.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_image.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_line.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_lvg.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_pcx.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_text.cpp
# End Source File
# Begin Source File

SOURCE=..\lice\lice_textnew.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\fft.c
# End Source File
# Begin Source File

SOURCE=.\loose_eel.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
