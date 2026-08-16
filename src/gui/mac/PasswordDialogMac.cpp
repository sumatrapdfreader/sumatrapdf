/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "gui/PlatformWindow.h"
#include "gui/mac/GuiMacBridge.h"
#include "gui/PasswordDialog.h"

bool ShowPasswordDialog(const PasswordDialogArgs& args, PasswordDialogResult* result) {
    if (!result) {
        return false;
    }
    *result = {};
    result->rememberPassword = args.rememberPassword;
    result->showPassword = args.showPassword;
    char* password = nullptr;
    int passwordLen = 0;
    result->accepted = MacGuiShowPasswordDialog(args.parent, args.fileName.s, len(args.fileName), args.canRemember,
                                                args.rememberPassword, args.showPassword, &result->rememberPassword,
                                                &result->showPassword, &password, &passwordLen);
    if (result->accepted) {
        result->password = Str(password, passwordLen);
    } else {
        free(password);
    }
    return result->accepted;
}
