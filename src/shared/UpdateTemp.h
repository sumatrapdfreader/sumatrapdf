/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool IsUpdateTempFileName(Str name);
void DeleteStaleUpdateTemps(Str dir, Str skip, int minAgeSec);
void NoteTempInstallerRelaunch();
void ScheduleDeleteTempInstaller();
