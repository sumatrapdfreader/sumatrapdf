/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

struct KeyboardHelpDataSource {
    virtual ~KeyboardHelpDataSource() = default;
    virtual Str Translate(Str) = 0;
    virtual TempStr CommandDescriptionTemp(int cmdId) = 0;
    virtual TempStr CommandShortcutTemp(int cmdId, int maxCount) = 0;
};

struct KeyboardHelpArgs {
    void* parent = nullptr;
    bool parentFullscreen = false;
    KeyboardHelpDataSource* dataSource = nullptr;
};

KeyboardHelpDataSource* GetDefaultKeyboardHelpDataSource();
void ToggleKeyboardHelp(const KeyboardHelpArgs&);
bool IsKeyboardHelpVisible();

#if OS_WIN
void ToggleKeyboardHelp(MainWindow*);
#endif
