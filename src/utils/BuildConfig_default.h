/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
This file should only be included from BaseUtil.h

With msbuild it's not possible to pass additional #define when building
the way we did it in nmake builds.

This file exists to allow that. By default it's empty i.e. doesn't define
anything.

It can be changed by build script before invoking msbuild.

Defines we recognize:

#define SVN_PRE_RELEASE_VER 10175
#define VER_QUALIFIER x64
#define GIT_COMMIT_ID 70cdc024f79167b607f59b77ea0b29dd155925cc

Defines that can be over-written, but shouldn't:

#define CURR_VERSION 3.1
#define CURR_VERISON_COMMA 3,1,0
*/