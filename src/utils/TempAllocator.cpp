/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

/*
Temp allocator is meant for allocating temporary values that don't need
to outlive this stack frame.
It's an alternative to using various AutoFree* classes.

It's a very fast bump allocator.

You must periodically call ResetTempAllocator()
to free memory used by allocator.
A safe place to call it is inside message windows loop.
*/

static PoolAllocator* gTempAllocator{nullptr};

// must call before any calls using temp allocator
void InitTempAllocator() {
  CrashIf(gTempAllocator);
  gTempAllocator = new PoolAllocator();
  // this can be large because 64k is nothing and it's used frequently
  gTempAllocator->minBlockSize = 64*1024;
}

void DestroyTempAllocator() {
  delete gTempAllocator;
  gTempAllocator = nullptr;
}

void ResetTempAllocator() {
  gTempAllocator->Reset();
}

TempWstr TempToWstr(const char* s) {
  return TempWstr();
}

TempWstr TempToWstr(std::string_view sv) {
  return TempWstr();
}
