/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

HACCEL* CreateSumatraAcceleratorTable();
HACCEL* GetSafeAcceleratorTable();
void AppendAccelKeyToMenuString(str::Str& str, const ACCEL& a);
bool GetAccelByCmd(int cmdId, ACCEL& accelOut);
