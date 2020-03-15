# Microsoft Developer Studio Project File - Name="lice" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=lice - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "lice.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "lice.mak" CFG="lice - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "lice - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "lice - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "lice - Win32 Release Profile" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "lice - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../zlib" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "PNG_USE_PNGVCRD" /D "PNG_LIBPNG_SPECIALBUILD" /D "__MMX__" /D "PNG_HAVE_MMX_COMBINE_ROW" /D "PNG_HAVE_MMX_READ_INTERLACE" /D "PNG_HAVE_MMX_READ_FILTER_ROW" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../zlib" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "PNG_USE_PNGVCRD" /D "PNG_DEBUG" /D "PNG_LIBPNG_SPECIALBUILD" /D "__MMX__" /D "PNG_HAVE_MMX_COMBINE_ROW" /D "PNG_HAVE_MMX_READ_INTERLACE" /D "PNG_HAVE_MMX_READ_FILTER_ROW" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "lice___Win32_Release_Profile"
# PROP BASE Intermediate_Dir "lice___Win32_Release_Profile"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_Profile"
# PROP Intermediate_Dir "Release_Profile"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I "../zlib" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "PNG_USE_PNGVCRD" /D "PNG_LIBPNG_SPECIALBUILD" /D "__MMX__" /D "PNG_HAVE_MMX_COMBINE_ROW" /D "PNG_HAVE_MMX_READ_INTERLACE" /D "PNG_HAVE_MMX_READ_FILTER_ROW" /D "PNG_WRITE_SUPPORTED" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /Zi /O2 /I "../zlib" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "PNG_USE_PNGVCRD" /D "PNG_LIBPNG_SPECIALBUILD" /D "__MMX__" /D "PNG_HAVE_MMX_COMBINE_ROW" /D "PNG_HAVE_MMX_READ_INTERLACE" /D "PNG_HAVE_MMX_READ_FILTER_ROW" /D "PNG_WRITE_SUPPORTED" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "lice - Win32 Release"
# Name "lice - Win32 Debug"
# Name "lice - Win32 Release Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "libpng"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\libpng\png.c
# End Source File
# Begin Source File

SOURCE=..\libpng\png.h
# End Source File
# Begin Source File

SOURCE=..\libpng\pngconf.h
# End Source File
# Begin Source File

SOURCE=..\libpng\pngerror.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngget.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngmem.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngpread.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngread.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngrio.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngrtran.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngrutil.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngset.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngtrans.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngwio.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngwrite.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngwtran.c
# End Source File
# Begin Source File

SOURCE=..\libpng\pngwutil.c
# End Source File
# End Group
# Begin Group "zlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\zlib\adler32.c
# End Source File
# Begin Source File

SOURCE=..\zlib\compress.c
# End Source File
# Begin Source File

SOURCE=..\zlib\crc32.c
# End Source File
# Begin Source File

SOURCE=..\zlib\crc32.h
# End Source File
# Begin Source File

SOURCE=..\zlib\deflate.c
# End Source File
# Begin Source File

SOURCE=..\zlib\infback.c
# End Source File
# Begin Source File

SOURCE=..\zlib\inffast.c
# End Source File
# Begin Source File

SOURCE=..\zlib\inffast.h
# End Source File
# Begin Source File

SOURCE=..\zlib\inffixed.h
# End Source File
# Begin Source File

SOURCE=..\zlib\inflate.c
# End Source File
# Begin Source File

SOURCE=..\zlib\inflate.h
# End Source File
# Begin Source File

SOURCE=..\zlib\inftrees.c
# End Source File
# Begin Source File

SOURCE=..\zlib\inftrees.h
# End Source File
# Begin Source File

SOURCE=..\zlib\trees.c
# End Source File
# Begin Source File

SOURCE=..\zlib\trees.h
# End Source File
# Begin Source File

SOURCE=..\zlib\uncompr.c
# End Source File
# Begin Source File

SOURCE=..\zlib\zconf.h
# End Source File
# Begin Source File

SOURCE=..\zlib\zlib.h
# End Source File
# Begin Source File

SOURCE=..\zlib\zutil.c
# End Source File
# Begin Source File

SOURCE=..\zlib\zutil.h
# End Source File
# End Group
# Begin Group "jpeglib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\jpeglib\jcapimin.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcapistd.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jccoefct.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jccolor.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcdctmgr.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jchuff.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcinit.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcmainct.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcmarker.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcmaster.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcomapi.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcparam.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcphuff.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcprepct.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jcsample.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jctrans.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdapimin.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdapistd.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdatadst.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdatasrc.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdcoefct.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdcolor.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jddctmgr.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdhuff.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdinput.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdmainct.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdmarker.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdmaster.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdmerge.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdphuff.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdpostct.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdsample.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jdtrans.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jerror.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jfdctflt.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jfdctfst.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jfdctint.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jidctflt.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jidctfst.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jidctint.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jidctred.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jmemmgr.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jmemnobs.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jquant1.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jquant2.c
# End Source File
# Begin Source File

SOURCE=..\jpeglib\jutils.c
# End Source File
# End Group
# Begin Group "giflib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\giflib\dgif_lib.c

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

# ADD CPP /I "../giflib" /D "HAVE_CONFIG_H"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"
# ADD CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\giflib\egif_lib.c

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

# ADD CPP /I "../giflib" /D "HAVE_CONFIG_H"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"
# ADD CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\giflib\gif_hash.c

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

# ADD CPP /I "../giflib" /D "HAVE_CONFIG_H"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"
# ADD CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\giflib\gif_hash.h
# End Source File
# Begin Source File

SOURCE=..\giflib\gif_lib.h
# End Source File
# Begin Source File

SOURCE=..\giflib\gif_lib_private.h
# End Source File
# Begin Source File

SOURCE=..\giflib\gifalloc.c

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

# ADD CPP /I "../giflib" /D "HAVE_CONFIG_H"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"
# ADD CPP /MT /I "../giflib" /D "HAVE_CONFIG_H"

!ENDIF 

# End Source File
# End Group
# Begin Group "tinyxml"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\tinyxml\svgtiny_colors.c
# End Source File
# Begin Source File

SOURCE=..\tinyxml\tinystr.cpp
# End Source File
# Begin Source File

SOURCE=..\tinyxml\tinyxml.cpp
# End Source File
# Begin Source File

SOURCE=..\tinyxml\tinyxml.h
# End Source File
# Begin Source File

SOURCE=..\tinyxml\tinyxmlerror.cpp
# End Source File
# Begin Source File

SOURCE=..\tinyxml\tinyxmlparser.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\lice.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_arc.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_bmp.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_colorspace.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_gif.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_gl_ctx.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_glbitmap.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_ico.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_image.cpp
# End Source File
# Begin Source File

SOURCE=.\lice_jpg.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_jpg_write.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_line.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_palette.cpp
# End Source File
# Begin Source File

SOURCE=.\lice_pcx.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_png.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_png_write.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_svg.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_texgen.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_text.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lice_textnew.cpp

!IF  "$(CFG)" == "lice - Win32 Release"

# ADD CPP /D "USE_ICC"

!ELSEIF  "$(CFG)" == "lice - Win32 Debug"

!ELSEIF  "$(CFG)" == "lice - Win32 Release Profile"

# ADD BASE CPP /D "USE_ICC"
# ADD CPP /D "USE_ICC"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\lice.h
# End Source File
# Begin Source File

SOURCE=.\lice_combine.h
# End Source File
# Begin Source File

SOURCE=.\lice_extended.h
# End Source File
# Begin Source File

SOURCE=.\lice_gl_ctx.h
# End Source File
# Begin Source File

SOURCE=.\lice_glbitmap.h
# End Source File
# Begin Source File

SOURCE=.\lice_text.h
# End Source File
# End Group
# End Target
# End Project
