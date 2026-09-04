/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/PlatformWindow.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Accelerators.h"
#include "Translations.h"
#include "KeyboardHelp.h"

struct SumatraKeyboardHelpDataSource : KeyboardHelpDataSource {
    Str Translate(Str s) override { return trans::GetTranslation(s); }

    TempStr CommandDescriptionTemp(int cmdId) override {
        int id = (int)CmdFirst + 1;
        for (Str description = SeqStrFirst(gCommandDescriptions); len(description) > 0;
             description = SeqStrNext(description), id++) {
            if (id == cmdId) {
                return str::DupTemp(trans::GetTranslation(description));
            }
        }
        return {};
    }

    TempStr CommandShortcutTemp(int cmdId, int maxCount) override { return ShortcutsForCmdTemp(cmdId, maxCount); }
};

static SumatraKeyboardHelpDataSource gSumatraKeyboardHelpDataSource;

void ToggleKeyboardHelp(MainWindow* win) {
    if (!win) {
        return;
    }
    KeyboardHelpArgs args;
    args.parent = win->hwndFrame;
    args.parentFullscreen = win->isFullScreen || win->InPresentation();
    args.dataSource = &gSumatraKeyboardHelpDataSource;
    ToggleKeyboardHelp(args);
}
