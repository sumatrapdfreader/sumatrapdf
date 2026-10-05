/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

struct KeyboardHelpDataSource {
    virtual ~KeyboardHelpDataSource() = default;
    virtual Str Translate(Str) = 0;
    virtual TempStr CommandDescriptionTemp(int cmdId) = 0;
    virtual TempStr CommandShortcutTemp(int cmdId, int maxCount) = 0;
};

struct KeyboardHelpArgs {
    MainWindow* win = nullptr;
    bool parentFullscreen = false;
    KeyboardHelpDataSource* dataSource = nullptr;
};

KeyboardHelpDataSource* GetDefaultKeyboardHelpDataSource();
void ToggleKeyboardHelp(const KeyboardHelpArgs&);
void ToggleKeyboardHelp(MainWindow*);
void CloseKeyboardHelp();
bool IsKeyboardHelpVisible();
bool KeyboardHelpOnKeyDown(int vk);
gpui::El* KeyboardHelpBuild(MainWindow* win, gpui::Ctx* cx);
