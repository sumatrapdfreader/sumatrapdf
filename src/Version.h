/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// CURR_VERSION can be over-written externally
#ifndef CURR_VERSION
#define CURR_VERSION 3.2
#endif
#ifndef CURR_VERSION_COMMA
#define CURR_VERSION_COMMA 3,2,0
#endif

// VER_QUALIFIER allows people who recompile SumatraPDF to add
// a distinguishing string at the end of the version number
// (e.g. version 2.3.2z or 2.4opt)

#define _QUOTEME(x) #x
#define QM(x) _QUOTEME(x)
#define _QUOTEME2(x, y) _QUOTEME(x##y)
#define QM2(x, y) _QUOTEME2(x, y)
#define _QUOTEME3(x, y, z) _QUOTEME(x##y##z)
#define QM3(x, y, z) _QUOTEME3(x, y, z)
#define _QUOTEME4(x, y, z, u) _QUOTEME(x##y##z##u)
#define QM4(x, y, z, u) _QUOTEME4(x, y, z, u)

// version as displayed in UI and included in resources
#ifndef PRE_RELEASE_VER
 #ifndef VER_QUALIFIER
  #define CURR_VERSION_STRA QM(CURR_VERSION)
 #else
  #define CURR_VERSION_STRA QM2(CURR_VERSION, VER_QUALIFIER)
 #endif
 #define VER_RESOURCE_STR  CURR_VERSION_STRA
 #define VER_RESOURCE      CURR_VERSION_COMMA,0
 #define UPDATE_CHECK_VER  TEXT(QM(CURR_VERSION))
#else
 #ifndef VER_QUALIFIER
   #define CURR_VERSION_STRA QM3(CURR_VERSION, ., PRE_RELEASE_VER)
   #define VER_RESOURCE_STR  QM3(CURR_VERSION, .0., PRE_RELEASE_VER)
 #else
   #define CURR_VERSION_STRA QM4(CURR_VERSION, ., PRE_RELEASE_VER, VER_QUALIFIER)
   #define VER_RESOURCE_STR  QM4(CURR_VERSION, .0., PRE_RELEASE_VER, VER_QUALIFIER)
 #endif
 #define VER_RESOURCE      CURR_VERSION_COMMA,PRE_RELEASE_VER
 #define UPDATE_CHECK_VER  TEXT(QM(PRE_RELEASE_VER))
#endif
#define CURR_VERSION_STR TEXT(CURR_VERSION_STRA)

#define COPYRIGHT_STR      "Copyright 2006-2020 all authors (GPLv3)"
#if defined(RAMICRO)
#define PUBLISHER_STR      "RA-MICRO"
#else
#define PUBLISHER_STR      "Krzysztof Kowalczyk"
#endif

#if defined(RAMICRO)
#define APP_NAME_STR        "RA-MICRO PDF Viewer"
#else
#define APP_NAME_STR        "SumatraPDF"
#endif
