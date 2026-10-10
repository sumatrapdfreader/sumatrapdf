/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "ShortcutParse.h"

struct Accel {
    KeyShortcut sc;
    int cmd;
};

struct AccelStroke {
    Str stroke;
    int cmd;
};

void FreeAcceleratorTables();
void CreateSumatraAcceleratorTable();
const Accel* GetAcceleratorTable(int& nOut);
const AccelStroke* GetAcceleratorStrokes(int& nOut);
TempStr AppendAccelKeyToMenuStringTemp(TempStr str, int cmdId);
TempStr ShortcutsForCmdTemp(int cmdId, int maxCount);
int SafeAcceleratorCmd(u16 vk, bool ctrl, bool shift, bool alt, bool cmd = false);
bool IsSafeAccel(const Accel&);
bool IsSafeTreeAccel(const Accel&);

#if OS_WIN
HACCEL* GetAcceleratorTables();
#endif
