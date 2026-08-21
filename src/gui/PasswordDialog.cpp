/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "gui/UIModels.h"
#include "EngineBase.h"
#include "gui/PlatformWindow.h"
#include "gui/PasswordDialog.h"

DialogPasswordUI::DialogPasswordUI(NativeWnd parent) {
    this->parent = parent;
}

Str DialogPasswordUI::GetPassword(Str filePath, u8*, u8[32], bool* saveKey) {
    *saveKey = false;
    PasswordDialogArgs args;
    args.parent = parent;
    args.fileName = path::GetBaseNameTemp(filePath);
    args.canRemember = canRemember;
    args.showPassword = showPassword;

    PasswordDialogResult result;
    ShowPasswordDialog(args, &result);
    showPassword = result.showPassword;
    if (!result.accepted) {
        str::Free(result.password);
        return {};
    }
    *saveKey = result.rememberPassword;
    return result.password;
}
