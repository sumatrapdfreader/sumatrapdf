/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Version_h
#define Version_h

// CURR_VERSION can be over-written externally (via makefile)
#ifndef CURR_VERSION
#define CURR_VERSION 1.9
#endif
#ifndef CURR_VERSION_COMMA
#define CURR_VERSION_COMMA 1,9,0
#endif

#define APP_NAME_STR       _T("SumatraPDF")

// #define SVN_PRE_RELEASE_VER 2295

#define _QUOTEME(x) #x
#define _QUOTEME3(x, y, z) _QUOTEME(x##y##z)
#define QM(x) _QUOTEME(x)
#define QM3(x, y, z) _QUOTEME3(x, y, z)

// version as displayed in UI and included in resources
#ifndef SVN_PRE_RELEASE_VER
 #ifndef DEBUG
  #define CURR_VERSION_STR _T(QM(CURR_VERSION))
 #else
  // hack: adds " (dbg)" after the version
  #define CURR_VERSION_STR _T(QM3(CURR_VERSION, \x20, (dbg)))
 #endif
 #define VER_RESOURCE      CURR_VERSION_COMMA,0
 #define VER_RESOURCE_STR  QM3(CURR_VERSION, .0., 0)
 #define UPDATE_CHECK_VER  _T(QM(CURR_VERSION))
#else
 #define CURR_VERSION_STR  _T(QM3(CURR_VERSION, ., SVN_PRE_RELEASE_VER))
 #define VER_RESOURCE      CURR_VERSION_COMMA,SVN_PRE_RELEASE_VER
 #define VER_RESOURCE_STR  QM3(CURR_VERSION, .0., SVN_PRE_RELEASE_VER)
 #define UPDATE_CHECK_VER  _T(QM(SVN_PRE_RELEASE_VER))
#endif

#define COPYRIGHT_STR      "Copyright 2006-2011 all authors (GPLv3)"
#define PUBLISHER_STR      "Krzysztof Kowalczyk"

#endif
