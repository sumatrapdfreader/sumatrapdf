/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace uitask {

void Initialize();
void Destroy();

bool IsMainUIThread();

void DrainQueue();

void Post(const Func0& fn, Kind kind = nullptr);
void PostOptimized(const Func0& fn, Kind kind = nullptr);

} // namespace uitask
