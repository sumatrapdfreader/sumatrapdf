/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct CommandPaletteEntry {
    int commandId = 0;
};

struct CommandPaletteModel {
    StrVecWithData<CommandPaletteEntry> commands;
    StrVecWithData<CommandPaletteEntry> filtered;
    StrVec filterWords;

    void SetCommands(const int* commandIds, int count);
    void Filter(Str query);
    int Count() const;
    Str ItemText(int index) const;
    int ItemCommandId(int index) const;
};

#if defined(DEBUG)
void CommandPaletteModel_UnitTests();
#endif
