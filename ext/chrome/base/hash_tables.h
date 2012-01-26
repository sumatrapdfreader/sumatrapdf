// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Deal with the differences between Microsoft and GNU implemenations
// of hash_map. Allows all platforms to use |base::hash_map| and
// |base::hash_set|.
//  eg:
//   base::hash_map<int> my_map;
//   base::hash_set<int> my_set;
//
// NOTE: It is an explicit non-goal of this class to provide a generic hash
// function for pointers.  If you want to hash a pointers to a particular class,
// please define the template specialization elsewhere (for example, in its
// header file) and keep it specific to just pointers to that class.  This is
// because identity hashes are not desirable for all types that might show up
// in containers as pointers.

#ifndef BASE_HASH_TABLES_H_
#define BASE_HASH_TABLES_H_
#pragma once

#include "build/build_config.h"

#include "base/string16.h"

#if defined(COMPILER_MSVC)
#include <hash_map>
#include <hash_set>

#define BASE_HASH_NAMESPACE stdext

#elif defined(COMPILER_GCC)
#if defined(OS_ANDROID)
#define BASE_HASH_NAMESPACE std
#else
#define BASE_HASH_NAMESPACE __gnu_cxx
#endif

// This is a hack to disable the gcc 4.4 warning about hash_map and hash_set
// being deprecated.  We can get rid of this when we upgrade to VS2008 and we
// can use <tr1/unordered_map> and <tr1/unordered_set>.
#ifdef __DEPRECATED
#define CHROME_OLD__DEPRECATED __DEPRECATED
#undef __DEPRECATED
#endif

#if defined(OS_ANDROID)
#include <hash_map>
#include <hash_set>
#else
#include <ext/hash_map>
#include <ext/hash_set>
#endif

#include <string>

#ifdef CHROME_OLD__DEPRECATED
#define __DEPRECATED CHROME_OLD__DEPRECATED
#undef CHROME_OLD__DEPRECATED
#endif

namespace BASE_HASH_NAMESPACE {

#if !defined(OS_ANDROID)
// The GNU C++ library provides identity hash functions for many integral types,
// but not for |long long|.  This hash function will truncate if |size_t| is
// narrower than |long long|.  This is probably good enough for what we will
// use it for.

#define DEFINE_TRIVIAL_HASH(integral_type) \
    template<> \
    struct hash<integral_type> { \
      std::size_t operator()(integral_type value) const { \
        return static_cast<std::size_t>(value); \
      } \
    }

DEFINE_TRIVIAL_HASH(long long);
DEFINE_TRIVIAL_HASH(unsigned long long);

#undef DEFINE_TRIVIAL_HASH
#endif  // !defined(OS_ANDROID)

// Implement string hash functions so that strings of various flavors can
// be used as keys in STL maps and sets.  The hash algorithm comes from the
// GNU C++ library, in <tr1/functional>.  It is duplicated here because GCC
// versions prior to 4.3.2 are unable to compile <tr1/functional> when RTTI
// is disabled, as it is in our build.

#define DEFINE_STRING_HASH(string_type) \
    template<> \
    struct hash<string_type> { \
      std::size_t operator()(const string_type& s) const { \
        std::size_t result = 0; \
        for (string_type::const_iterator i = s.begin(); i != s.end(); ++i) \
          result = (result * 131) + *i; \
        return result; \
      } \
    }

DEFINE_STRING_HASH(std::string);
DEFINE_STRING_HASH(string16);

#undef DEFINE_STRING_HASH

}  // namespace BASE_HASH_NAMESPACE

#else  // COMPILER
#error define BASE_HASH_NAMESPACE for your compiler
#endif  // COMPILER

namespace base {
using BASE_HASH_NAMESPACE::hash_map;
using BASE_HASH_NAMESPACE::hash_set;
}

#endif  // BASE_HASH_TABLES_H_
