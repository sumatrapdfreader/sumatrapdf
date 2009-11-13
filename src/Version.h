#ifndef VERSION_H__
#define VERSION_H__

// CURR_VERSION can be over-written externally (via makefile)
#ifndef CURR_VERSION
#define CURR_VERSION "1.0"
#endif

// version as included in resources
#define VER_RESOURCE      1,0,0,0
#define VER_RESOURCE_STR  "1.0.0.0\0"

// #define SVN_PRE_RELEASE_VER 994

#define _QUOTEME(x) #x
#define QM(x) _QUOTEME(x)

#ifdef SVN_PRE_RELEASE_VER
#define UPDATE_CHECK_VER _T(QM(SVN_PRE_RELEASE_VER))
#else
#define UPDATE_CHECK_VER _T(CURR_VERSION)
#endif

#endif

