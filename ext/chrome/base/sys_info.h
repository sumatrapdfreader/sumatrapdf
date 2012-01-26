// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYS_INFO_H_
#define BASE_SYS_INFO_H_
#pragma once

#include "base/base_export.h"
#include "base/basictypes.h"

#include <string>

class FilePath;

namespace base {

class BASE_EXPORT SysInfo {
 public:
  // Return the number of logical processors/cores on the current machine.
  static int NumberOfProcessors();

  // Return the number of bytes of physical memory on the current machine.
  static int64 AmountOfPhysicalMemory();

  // Return the number of megabytes of physical memory on the current machine.
  static int AmountOfPhysicalMemoryMB() {
    return static_cast<int>(AmountOfPhysicalMemory() / 1024 / 1024);
  }

  // Return the available disk space in bytes on the volume containing |path|,
  // or -1 on failure.
  static int64 AmountOfFreeDiskSpace(const FilePath& path);

  // Returns the name of the host operating system.
  static std::string OperatingSystemName();

  // Returns the version of the host operating system.
  static std::string OperatingSystemVersion();

  // Retrieves detailed numeric values for the OS version.
  // TODO(port): Implement a Linux version of this method and enable the
  // corresponding unit test.
  // DON'T USE THIS ON THE MAC OR WINDOWS to determine the current OS release
  // for OS version-specific feature checks and workarounds. If you must use
  // an OS version check instead of a feature check, use the base::mac::IsOS*
  // family from base/mac/mac_util.h, or base::win::GetVersion from
  // base/win/windows_version.h.
  static void OperatingSystemVersionNumbers(int32* major_version,
                                            int32* minor_version,
                                            int32* bugfix_version);

  // Returns the CPU architecture of the system. Exact return value may differ
  // across platforms.
  static std::string CPUArchitecture();

  // Return the smallest amount of memory (in bytes) which the VM system will
  // allocate.
  static size_t VMAllocationGranularity();

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Returns the maximum SysV shared memory segment size.
  static size_t MaxSharedMemorySize();
#endif

#if defined(OS_CHROMEOS)
  // Returns the name of the version entry we wish to look up in the
  // Linux Standard Base release information file.
  static std::string GetLinuxStandardBaseVersionKey();

  // Parses /etc/lsb-release to get version information for Google Chrome OS.
  // Declared here so it can be exposed for unit testing.
  static void ParseLsbRelease(const std::string& lsb_release,
                              int32* major_version,
                              int32* minor_version,
                              int32* bugfix_version);
#endif
};

}  // namespace base

#endif  // BASE_SYS_INFO_H_
