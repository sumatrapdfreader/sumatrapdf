/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SumatraDialogs.h declares every dialog. The win32 dialogs are
// gpui Dialogs here, so each one is a Show / Close / IsVisible / Build
// quadruple instead of a blocking call: Build() is asked once a frame by the
// shell and returns the element (or null when the dialog is not up).

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct WindowTab;

// --- the shell's one entry point into all of them ---------------------------

// the element of whichever dialog is up, or null
gpui::El* DialogsBuild(MainWindow* win, gpui::Ctx* cx);
// Escape: closes the topmost open dialog, true if one was open
bool DialogsOnEscape(MainWindow* win);
// the keys orig's dialog windows answer themselves (closeOnCtrlW, closeOnF1,
// a dialog's own onKeyDown, the message box's Y / N)
bool DialogsOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift, bool alt);
// orig's accelerator table for the window that has the keyboard: none for a
// dialog (FindAcceleratorsForHwnd returns null), the edit table for Properties
enum class DialogAccels {
    All,
    Edit,
    None
};
DialogAccels DialogsAccelTable(MainWindow* win);
// Up / Down (dir -1 / 1) in a dialog whose list they drive, as orig's do
bool DialogsOnArrowKey(MainWindow* win, int dir, bool editFocused);
// Enter in a dialog that has no focused text field (orig's default button)
bool DialogsOnEnter(MainWindow* win);

// --- message boxes ----------------------------------------------------------

// ng: orig calls win32 MessageBoxW through base's MsgBox(). gpui has no modal
// message box, so this is an AlertDialog with the same buttons and an
// `onResult` instead of a return value. The flag and result values are win32's
// so the ported call sites read the same.
constexpr uint MbOk = 0x0;
constexpr uint MbOkCancel = 0x1;
constexpr uint MbYesNoCancel = 0x3;
constexpr uint MbYesNo = 0x4;
constexpr uint MbIconError = 0x10;
constexpr uint MbIconQuestion = 0x20;
constexpr uint MbIconWarning = 0x30;
constexpr uint MbIconInformation = 0x40;

constexpr int MbRetOk = 1;
constexpr int MbRetCancel = 2;
constexpr int MbRetYes = 6;
constexpr int MbRetNo = 7;

void MsgBox(MainWindow* win, Str text, Str caption, uint flags, Func1<int> onResult = {});
// ng: a message box raised while a tool window of `win` is the active window
// is drawn in that window (gui/ToolWindow.cpp asks for it and gives it keys)
struct ToolWindow;
gpui::El* MsgBoxBuildInToolWindow(ToolWindow* tw, gpui::Ctx* cx);
bool MsgBoxOnKeyInToolWindow(ToolWindow* tw, int vk, bool ctrl);

// orig's "Unsaved changes" task dialog: four choices, Cancel the default
enum class SaveChoice {
    Discard,
    SaveNew,
    SaveExisting,
    Cancel,
};
void ShowUnsavedAnnotationsDialog(MainWindow* win, Str fileName, Func1<int> onChoice);
bool IsUnsavedAnnotationsDialogVisible();
gpui::El* UnsavedAnnotationsDialogBuild(MainWindow* win, gpui::Ctx* cx);
void MessageBoxWarning(MainWindow* win, Str msg, Str title = {});

// --- the dialogs ------------------------------------------------------------

// ng: gpui has no save-file prompt. On Windows this is orig's GetSaveFileNameW
// (gui/NativeFileDlg_win.cpp); elsewhere the port's own dialog
struct SavePathArgs {
    MainWindow* win = nullptr;
    Str title;       // owned
    Str initialPath; // owned; the directory it opens in and the suggested name
    Str defExt;      // owned; ".pdf". Also filters the listing
    // pick a file that exists (an open prompt): no default extension, no
    // overwrite question; the listing shows the files of filter's first entry
    bool openExisting = false;
    // orig's OPENFILENAME, for Windows' dialog only
    Str filter;               // owned; lpstrFilter with \1 for \0, empty is "All files"
    int filterIndex = 1;      // in: nFilterIndex; out: the one chosen, 0 if not known
    Str nativeFile;           // owned; lpstrFile when it is not initialPath
    Str nativeDir;            // owned; lpstrInitialDir
    bool noDefExt = false;    // no lpstrDefExt
    bool nativeTitle = false; // lpstrTitle is title; otherwise the system's "Save As"
    // out: the path the user picked, empty if cancelled. Only valid during
    // onDone; the args are deleted afterwards
    Str path;
    Func1<SavePathArgs*> onDone;
};
// takes ownership of args
void ShowSavePathDialog(SavePathArgs* args);
void CloseSavePathDialog();
bool IsSavePathDialogVisible();
// -dbg-control's stand-in for picking a name in the dialog ({} is Cancel)
bool TestFinishSavePathDialog(Str path);
gpui::El* SavePathDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowGoToPageDialog(MainWindow* win);
void CloseGoToPageDialog();
bool IsGoToPageDialogVisible();
gpui::El* GoToPageDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowAddFavoriteDialog(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name);
void CloseAddFavoriteDialog();
bool IsAddFavoriteDialogVisible();
gpui::El* AddFavoriteDialogBuild(MainWindow* win, gpui::Ctx* cx);

// what the user typed in the password dialog; only valid during the callback
struct PasswordDialogResult {
    bool accepted = false;
    Str password;
    bool rememberPassword = false;
    bool showPassword = false;
};

void ShowGetPasswordDialog(MainWindow* win, Str fileName, bool canRemember, Func1<PasswordDialogResult*> onDone);
void CloseGetPasswordDialog();
bool IsGetPasswordDialogVisible();
gpui::El* GetPasswordDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowCustomZoomDialog(MainWindow* win);
bool CustomZoomMoveSelection(int dir);
void CustomZoomOk();
void CloseCustomZoomDialog();
bool IsCustomZoomDialogVisible();
gpui::El* CustomZoomDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowChangeScrollbarDialog(MainWindow* win);
bool ChangeScrollbarMoveSelection(int dir);
void ChangeScrollbarOk();
void CloseChangeScrollbarDialog();
bool IsChangeScrollbarDialogVisible();
gpui::El* ChangeScrollbarDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowChangeLanguageDialog(MainWindow* win);
bool ChangeLanguageMoveSelection(int dir);
void ChangeLanguageOk();
void CloseChangeLanguageDialog();
bool IsChangeLanguageDialogVisible();
gpui::El* ChangeLanguageDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowChangeThemeDialog(MainWindow* win);
void ShowSetDocumentColorsFollowThemeDialog(MainWindow* win);
bool ChangeThemeMoveSelection(int dir);
void ChangeThemeOk();
void CloseChangeThemeDialog();
bool IsChangeThemeDialogVisible();
gpui::El* ChangeThemeDialogBuild(MainWindow* win, gpui::Ctx* cx);
void ChangeThemePreviewTick(int ms);

// generic color picker: picks a color and edits the set of predefined colors
struct ChangeColorsArgs {
    MainWindow* win = nullptr;
    Str title;
    // in: initially picked color, out: the color the user picked
    Color color = kColorUnset;
    // in: predefined colors, out: the set as the user left it
    Vec<Color> colors;
    // false if the user cancelled, in which case color is meaningless
    bool didSelect = false;
    // true if the user added or removed a color
    bool colorsChanged = false;
    // show a slider that sets the color's alpha byte
    bool withOpacity = false;
    // called once, when the dialog closes; args are deleted afterwards
    Func1<ChangeColorsArgs*> onClose;
};
// takes ownership of args
void ShowChangeColorsDialog(ChangeColorsArgs* args);
void ShowChangeBackgroundColorDialog(MainWindow* win);
void ShowSetTabColorDialog(MainWindow* win, WindowTab* tab);
void CloseChangeColorDialog();
bool IsChangeColorDialogVisible();
gpui::El* ChangeColorDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowInverseSearchDialog(MainWindow* win);
void CloseInverseSearchDialog();
bool IsInverseSearchDialogVisible();
gpui::El* InverseSearchDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowEbookSettingsDialog(MainWindow* win);
void CloseEbookSettingsDialog();
bool IsEbookSettingsDialogVisible();
gpui::El* EbookSettingsDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowSettingsDialog(MainWindow* win);
void CloseSettingsDialog();
bool IsSettingsDialogVisible();
gpui::El* SettingsDialogBuild(MainWindow* win, gpui::Ctx* cx);

void ShowAdvancedSettingsDialog(MainWindow* win);
bool AdvancedSettingsMoveSelection(int dir);
bool AdvancedSettingsOnEnter();
void AdvancedSettingsOnEscape();
void CloseAdvancedSettingsDialog();
bool IsAdvancedSettingsDialogVisible();
gpui::El* AdvancedSettingsDialogBuild(MainWindow* win, gpui::Ctx* cx);

// orig's SignDocumentDialog. fieldName preselects a signature field in the
// "where to sign" drop-down (hasField = false leaves the choice alone)
void ShowSignDocumentDialog(MainWindow* win, Str fieldName = {}, bool hasField = false);
void CloseSignDocumentDialog(MainWindow* win);
bool IsSignDocumentDialogVisible();
TempStr SignDocumentPlacementTemp();
TempStr SignDocumentChecksTemp();
gpui::El* SignDocumentDialogBuild(MainWindow* win, gpui::Ctx* cx);
// the dialog hid itself and the next click / drag on the page places the
// signature (issue #5967)
bool IsPlacingSignature(MainWindow* win);
bool CancelPlacingSignature(MainWindow* win);
bool FinishSignaturePlacement(MainWindow* win, int x, int y, bool aborted);

void ShowPageGridDialog(MainWindow* win);
void ClosePageGridDialog();
bool IsPageGridDialogVisible();
gpui::El* PageGridDialogBuild(MainWindow* win, gpui::Ctx* cx);
void ResetPageGridToDefaults();
TempStr PageGridStateTemp();
// ng: orig keeps the grid's session toggle and its painting in Canvas.cpp;
// nothing but this dialog needs them, so they live next to it
bool ShowPageGrid();
void TogglePageGrid();
void SetShowPageGrid(bool);
void RedrawPageGridWindows();
struct DisplayModel;
namespace gpui {
struct PaintCtx;
}
void PaintPageGrid(DisplayModel* dm, gpui::PaintCtx* ctx);

// --- print options ----------------------------------------------------------

// ng: the Advanced page of the print dialog is a win32 property sheet page
// (the print dialog itself is the OS one), so it stays win32; it lives in
// Print.cpp because there is no .rc here and the template is built in memory.

enum class PrintRangeAdv {
    All = 0,
    Even,
    Odd
};
enum class PrintScaleAdv {
    None = 0,
    Shrink,
    Fit,
    Stretch
};
enum class PrintRotationAdv {
    Auto = 0,
    Portrait,
    Landscape
};

struct Print_Advanced_Data {
    PrintRangeAdv range;
    PrintScaleAdv scale;
    PrintRotationAdv rotation;
    bool autoRotate;
    bool centerHorizontally;
    // when true, let the printer pick the input tray whose paper matches the
    // document's page size (DMBIN_FORMSOURCE), independent of page scaling
    bool paperSourceByPageSize;
    // when true, set the paper size to each page's own size before printing it,
    // so mixed page size documents print to the right paper/tray
    bool perPagePaperSize;
    // extra rotation applied to the printout, in degrees (0, 90, 180 or 270),
    // on top of the automatic rotation; lets the user fix wrong orientation
    // (e.g. upside-down output on virtual printers), issue #1246
    int extraRotation;
    // when > 0, the document's resolution to assume instead of what the file
    // says. Set via -print-settings "dpi=<n>" or PrinterDefaults.PrintDpi
    float dpiOverride = 0;

    explicit Print_Advanced_Data(PrintRangeAdv range = PrintRangeAdv::All, PrintScaleAdv scale = PrintScaleAdv::Shrink,
                                 PrintRotationAdv rotation = PrintRotationAdv::Auto, bool autoRotate = true,
                                 bool centerHorizontally = false, bool paperSourceByPageSize = false,
                                 bool perPagePaperSize = false, int extraRotation = 0)
        : range(range),
          scale(scale),
          rotation(rotation),
          autoRotate(autoRotate),
          centerHorizontally(centerHorizontally),
          paperSourceByPageSize(paperSourceByPageSize),
          perPagePaperSize(perPagePaperSize),
          extraRotation(extraRotation) {}
};

#if OS_WIN
// dlgTemplate owns the in-memory DLGTEMPLATE and must outlive the property
// sheet, as orig's RTL template does
HPROPSHEETPAGE CreatePrintAdvancedPropSheet(Print_Advanced_Data* data, ScopedMem<DLGTEMPLATE>& dlgTemplate);
#endif
