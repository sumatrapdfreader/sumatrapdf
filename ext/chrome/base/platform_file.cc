// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_file.h"

namespace base {

PlatformFileInfo::PlatformFileInfo()
    : size(0),
      is_directory(false),
      is_symbolic_link(false) {
}

PlatformFileInfo::~PlatformFileInfo() {}

}  // namespace base
