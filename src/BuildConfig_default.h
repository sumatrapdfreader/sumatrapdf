/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
Template / documentation for src/BuildConfig.h (empty by default).

With msbuild it's not possible to pass additional #define when building
the way we did it in nmake builds.

BuildConfig.h exists to allow that. CI and local customization write defines
into src/BuildConfig.h before invoking msbuild. Include it from Version.h
(and any translation unit that needs PRE_RELEASE_VER etc. without Version.h).

Defines we recognize:

#define PRE_RELEASE_VER 10175
#define VER_QUALIFIER x64
#define GIT_COMMIT_ID 70cdc024f79167b607f59b77ea0b29dd155925cc
#define BUILT_ON 2026-01-01

Defines that can be over-written, but shouldn't:

#define CURR_VERSION 3.1
#define CURR_VERSION_COMMA 3,1,0
*/