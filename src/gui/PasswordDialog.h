/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct PasswordDialogArgs {
    NativeWnd parent = nullptr;
    Str fileName;
    bool canRemember = false;
    bool rememberPassword = false;
    bool showPassword = false;
};

struct PasswordDialogResult {
    Str password;
    bool accepted = false;
    bool rememberPassword = false;
    bool showPassword = false;
};

bool ShowPasswordDialog(const PasswordDialogArgs&, PasswordDialogResult*);

struct DialogPasswordUI : PasswordUI {
    NativeWnd parent = nullptr;
    bool showPassword = false;
    bool canRemember = false;

    explicit DialogPasswordUI(NativeWnd parent);
    Str GetPassword(Str filePath, u8* fileDigest, u8 decryptionKeyOut[32], bool* saveKey) override;
};
