// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_LEAK_ANNOTATIONS_H_
#define BASE_DEBUG_LEAK_ANNOTATIONS_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX) && defined(USE_HEAPCHECKER)

#include "third_party/tcmalloc/chromium/src/google/heap-checker.h"

// Annotate a program scope as having memory leaks. Tcmalloc's heap leak
// checker will ignore them. Note that these annotations may mask real bugs
// and should not be used in the production code.
#define ANNOTATE_SCOPED_MEMORY_LEAK \
    HeapLeakChecker::Disabler heap_leak_checker_disabler

// Annotate an object pointer as referencing a leaky object. This object and all
// the heap objects referenced by it will be ignored by the heap checker.
//
// X should be referencing an active allocated object. If it is not, the
// annotation will be ignored.
// No object should be annotated with ANNOTATE_SCOPED_MEMORY_LEAK twice.
// Once an object is annotated with ANNOTATE_SCOPED_MEMORY_LEAK, it cannot be
// deleted.
#define ANNOTATE_LEAKING_OBJECT_PTR(X) \
    HeapLeakChecker::IgnoreObject(X)

#else

// If tcmalloc is not used, the annotations should be no-ops.
#define ANNOTATE_SCOPED_MEMORY_LEAK ((void)0)
#define ANNOTATE_LEAKING_OBJECT_PTR(X) ((void)0)

#endif

#endif  // BASE_DEBUG_LEAK_ANNOTATIONS_H_
