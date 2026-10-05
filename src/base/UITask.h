/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace uitask {

enum class Dispatch {
    Native,
    Queue
};

void Initialize(Dispatch dispatch = Dispatch::Native);
void Destroy();

bool IsMainUIThread();

void DrainQueue();

void SetWakeupFn(void (*fn)());

void Post(const Func0& fn, Kind kind = nullptr);
void PostOptimized(const Func0& fn, Kind kind = nullptr);

} // namespace uitask
