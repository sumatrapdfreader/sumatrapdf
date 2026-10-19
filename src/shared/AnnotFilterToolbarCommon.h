/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by AnnotFilterToolbarCommon.cpp and each app's AnnotFilterToolbar.cpp ---

// what orig's and ng's annotation filter have in common
struct AnnotFilterToolbarBase {
    MainWindow* win = nullptr;
    // all annotations of the document, and those the filter lets through
    Vec<Annotation*> annotations;
    Vec<Annotation*> visibleAnnots;
};

// implemented by each app
void ShowAnnotFilterWindow(MainWindow* win);
void HideAnnotFilterWindow(MainWindow* win);

WindowTab* FilterTab(AnnotFilterToolbarBase* f);
void LoadAnnotations(AnnotFilterToolbarBase* f);
Annotation* VisibleAnnotAt(AnnotFilterToolbarBase* f, int idx);

// the context menu of a row in the list
enum class AnnotListMenu {
    CopyComment = 1,
    Edit,
    Delete,
};

struct AnnotListMenuItem {
    AnnotListMenu id = AnnotListMenu::CopyComment;
    Str title; // temp
    bool disabled = false;
};

void AnnotListMenuItems(MainWindow* win, Annotation* annot, int nSel, Vec<AnnotListMenuItem>& out);

constexpr int kSelectionDebounceMs = 300;

constexpr int kMaxListLines = 12;

constexpr int kFloatWinPadding = 8;

constexpr int kFloatWinGap = 6;

// the note preview under the list, and where the "?" sends for the filter syntax
constexpr int kAnnotNotePreviewLines = 6;
constexpr const char* kAnnotSearchSyntaxDocUri = "/Editing-annotations#search-syntax";
