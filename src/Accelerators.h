/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void FreeAcceleratorTables();
void CreateSumatraAcceleratorTable();
HACCEL* GetAcceleratorTables();
TempStr AppendAccelKeyToMenuStringTemp(TempStr str, int cmdId);
TempStr ShortcutsForCmdTemp(int cmdId, int maxCount);

int SafeAcceleratorCmd(u16 vk, bool ctrl, bool shift, bool alt);
