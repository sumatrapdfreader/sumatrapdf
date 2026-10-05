/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef IS_PERF_LOG
#define IS_PERF_LOG 0
#endif

void InitPerfLog();
void StartPerfLog();
void StopPerfLog();
void SetPerfLogPath(Str path);
void SavePerfLog();
void DestroyPerfLog();
