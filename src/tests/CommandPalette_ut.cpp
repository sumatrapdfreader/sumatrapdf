/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "FilterUtil.h"
#include "Commands.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

// A pure model of the command palette's list + filtering, with no window
// attached, so the filtering can be tested without a UI.
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

void CommandPaletteModel::SetCommands(const int* commandIds, int count) {
    commands.Reset();
    filtered.Reset();
    filterWords.Reset();
    for (int i = 0; i < count; i++) {
        int commandId = commandIds[i];
        Str description = GetCommandDescription(commandId);
        if (len(description) == 0) {
            continue;
        }
        commands.Append(description, {commandId});
    }
    SortNoCase(&commands);
    Filter({});
}

void CommandPaletteModel::Filter(Str query) {
    filtered.Reset();
    filterWords.Reset();
    SplitFilterToWords(query, filterWords);
    for (int i = 0; i < len(commands); i++) {
        if (FilterMatches(commands[i], filterWords)) {
            filtered.AppendFrom(&commands, i);
        }
    }
}

int CommandPaletteModel::Count() const {
    return len(filtered);
}

Str CommandPaletteModel::ItemText(int index) const {
    return index >= 0 && index < len(filtered) ? filtered[index] : Str{};
}

int CommandPaletteModel::ItemCommandId(int index) const {
    CommandPaletteEntry* entry = index >= 0 && index < len(filtered) ? filtered.AtData(index) : nullptr;
    return entry ? entry->commandId : 0;
}

void CommandPaletteModel_UnitTests() {
    const int commands[] = {CmdOpenFile, CmdRotateLeft, CmdRotateRight, CmdZoomFitWidth};
    CommandPaletteModel model;
    model.SetCommands(commands, dimofi(commands));
    utassert(model.Count() == dimofi(commands));
    utassert(model.ItemCommandId(0) == CmdOpenFile);

    model.Filter(StrL("rotate right"));
    utassert(model.Count() == 1);
    utassert(model.ItemCommandId(0) == CmdRotateRight);

    model.Filter(StrL("FIT width"));
    utassert(model.Count() == 1);
    utassert(model.ItemCommandId(0) == CmdZoomFitWidth);

    model.Filter(StrL("missing"));
    utassert(model.Count() == 0);
}
