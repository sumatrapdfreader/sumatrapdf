// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOGGING_H_
#define BASE_LOGGING_H_
#pragma once

#include <assert.h>

#define NOTREACHED() { assert(0); }

#define DCHECK(condition) { assert(condition); }

#define NOTIMPLEMENTED() { char *p = 0; *p = 0; }

#endif  // BASE_LOGGING_H_
