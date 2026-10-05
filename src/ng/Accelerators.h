/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: orig builds three win32 HACCEL tables. We keep the one table of
// (shortcut, command) pairs and hand gpui a (command, stroke) list instead;
// which shortcuts a focused control may swallow is a gpui key context (step 6).
struct Accel {
    KeyShortcut sc;
    int cmd;
};

// what gpui::KeymapBind() takes: a stroke like "ctrl-shift-f5" and a command
struct AccelStroke {
    Str stroke;
    int cmd;
};

void FreeAcceleratorTables();
void CreateSumatraAcceleratorTable();
// builds the table if it isn't built yet
const Accel* GetAcceleratorTable(int& nOut);
// the same table as gpui strokes, shortcuts gpui can't name left out
const AccelStroke* GetAcceleratorStrokes(int& nOut);

TempStr AppendAccelKeyToMenuStringTemp(TempStr str, int cmdId);
TempStr ShortcutsForCmdTemp(int cmdId, int maxCount);

int SafeAcceleratorCmd(u16 vk, bool ctrl, bool shift, bool alt);

// ng: orig keeps these private and bakes them into the edit / tree accelerator
// tables. They are the same rules, asked per shortcut: true when the command
// still runs while an edit control (resp. a tree) has the focus.
bool IsSafeAccel(const Accel&);
bool IsSafeTreeAccel(const Accel&);
