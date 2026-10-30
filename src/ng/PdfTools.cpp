/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The PDF tools: bake, extract text, compress, decompress, delete / extract
// pages, encrypt, decrypt, convert an image collection to PDF, convert PDF
// pages to images and save the selection as an image. The work is mupdf's
// command-line tools (pdfbake / pdfclean / muconvert) and the engines.
// ng: orig builds each dialog as a window of virtual controls plus real HWND
// edits. Here they are one gpui dialog with the same rows in the same order -
// the source path, the destination with its "..." browse, the extra field a
// tool needs, and the action + Cancel row - and orig's UpdateButton() is the
// enabled state computed while the frame is built.

#include "gui/GpuiBridge.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/UIModels.h"
#include "VirtKeys.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "PdfCreator.h"
#include "ImageReader.h"
#include "PngOptimizer.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "Notifications.h"
#include "Flags.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "Commands.h"
#include "Menu.h"
#include "gui/Dpi.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/PlatformFont.h"
#include "gui/ToolWindow.h"
#include "gui/DocCanvas.h"
#include "gui/WasmBridge.h"
#include "ImageSaveCropResize.h"
#include "SumatraDialogs.h"
#include "PdfTools.h"
#include "PdfToolsCommon.h"

#include "SumatraLog.h"

// --- the page range syntax the page dialogs share ---------------------------

// --- the image formats the two image dialogs offer --------------------------

// PNG / JPEG / BMP / GIF, same core formats as the Save Image dialog
// ng: orig also carries the GDI+ mime type and the OPENFILENAME default
// extension; the encoders here are the portable ones of the image editor
struct ConvertImageFormat {
    Str label;
    Str ext; // including the dot, e.g. ".png"
};

static const ConvertImageFormat kConvertImageFormats[] = {
    {StrL("PNG"), StrL(".png")},
    {StrL("JPEG"), StrL(".jpg")},
    {StrL("BMP"), StrL(".bmp")},
    {StrL("GIF"), StrL(".gif")},
};

static int ConvertImageFormatCount() {
    return dimofi(kConvertImageFormats);
}

// ng: orig encodes with GDI+; the image editor's host already writes PNG (via
// zopfli), JPEG (mupdf) and BMP on every platform, so use that
bool SavePixmapAsImageFile(Pixmap* px, Str path) {
    if (!px || len(path) == 0) {
        return false;
    }
    if (!IsSupportedConvertImageExt(path)) {
        return false;
    }
    InitImageEditHost();
    if (!gImageEditHost.SavePixmapAsImage) {
        return false;
    }
    // an engine can render into a palette DIB nothing but the platform bitmap
    // can read; copy it into a 32bpp one first, as orig does before GDI+
    Pixmap* converted = nullptr;
    Pixmap* use = px;
    if (px->format == PixmapFormat::Native) {
#if OS_WIN
        converted = PixmapCopyAs32bppDIB(px);
#endif
        if (!converted) {
            return false;
        }
        use = converted;
    }
    Str ext = kConvertImageFormats[ConvertImageFormatIdxFromPath(path)].ext;
    bool ok = gImageEditHost.SavePixmapAsImage(use, path, ext);
    FreePixmap(converted);
    if (!ok) {
        file::Delete(path);
    }
    return ok;
}

// --- saving a page rectangle as an image ------------------------------------

void FinishSaveSelImg(SaveSelImgWork* d) {
    bool winOk = IsMainWindowValidAndNotClosing(d->win);
    if (winOk) {
        RemoveNotificationsForGroup(d->win, kNotifSaveSelectionAsImage);
    }
    if (d->ok) {
        logf("SaveSelectionAsImage: wrote '%s'\n", d->destPath);
        if (winOk) {
            LoadDocument(d->win, d->destPath);
        }
    } else {
        MessageBoxWarning(winOk ? d->win : nullptr, StrL("Failed to save the selection as an image."),
                          Tr("Save Selection As Image"));
    }
    str::Free(d->destPath);
    delete d;
}

// --- converting an image collection to PDF ----------------------------------

// --- the scripted entry points (orig's SumatraControl commands) -------------

// SumatraControl.cpp drives these. They are the model half of the dialogs
// below.

// --- the dialog -------------------------------------------------------------

enum class PdfToolKind {
    Bake,
    ExtractText,
    Compress,
    Decompress,
    DeletePages,
    ExtractPages,
    Encrypt,
    Decrypt,
    ConvertToPdf,
    ConvertToImages,
    SaveSelectionAsImage,
    Merge,
    ConvertEpubToPdf,
};

// orig's three "Pages:" radios of the Convert PDF to Images dialog
enum class PagesMode {
    Current,
    All,
    Custom,
};

// orig's kThumbDx / kThumbDy / kGap and the rest of MergeGrid's constants (dips)
constexpr int kMergeThumbDx = 140;
constexpr int kMergeThumbDy = 198;
constexpr int kMergeGap = 28;
constexpr int kMergeDialogDx = 1100;
constexpr int kMergeScrollbarDx = 10;
// the page number (and its file's dot) under a page
constexpr int kMergeLabelDy = 26;
constexpr float kMergeDotDx = 7;
// the selection and hover frame: around a page and its label
constexpr float kMergeFramePad = 8;
constexpr float kMergeFrameRadius = 6;
constexpr int kMergeSelShift = 26;
constexpr int kMergeHoverShift = 9;
constexpr int kMergeFocusRingShift = 45;
constexpr int kMergePageEdgeShift = 22;
// the remove / restore button in a page's top-right corner
constexpr int kMergeCornerBtnDx = 22;
constexpr int kMergeCornerBtnInset = 6;
constexpr float kMergeCornerGlyphPx = 13;
constexpr int kMergeDropBarDx = 2;
constexpr int kMergeAutoScrollMs = 40;
constexpr u8 kMergeRemovedVeilAlpha = 200;
constexpr u8 kMergeCornerBtnAlpha = 225;
constexpr u8 kMergeBandAlpha = 18;
// ng: orig's SM_CXDRAG
constexpr float kMergeDragThreshold = 4;

// what the left button went down on in the page grid
enum class MergePress {
    None,
    Page,
    CornerBtn,
    Band,
};

// the files' colors on their pages, in the order they were added
static const Color kSourceColors[] = {
    MkRgb(0x1f, 0x77, 0xb4), MkRgb(0xff, 0x7f, 0x0e), MkRgb(0x2c, 0xa0, 0x2c),
    MkRgb(0xd6, 0x27, 0x28), MkRgb(0x94, 0x67, 0xbd), MkRgb(0x8c, 0x56, 0x4b),
    MkRgb(0xe3, 0x77, 0xc2), MkRgb(0xbc, 0xbd, 0x22), MkRgb(0x17, 0xbe, 0xcf),
};

// 0, 90, 180, 270: each has its own thumbnail
constexpr int kMergeRotations = 4;

struct MergeThumb {
    Pixmap* bitmap = nullptr;
    gp::RenderImage* image = nullptr;
    bool failed = false;
};

struct MergeThumbCache {
    Vec<MergeThumb> thumbs;
    MainWindow* win = nullptr;
    AtomicInt cancel = 0;
    bool workerRunning = false;
    bool deleteWhenWorkerFinishes = false;
};

struct MergeSourceDlg {
    Str path;
    Str password;
    int pageCount = 0;
    int thumbStart = 0;
    Color color = 0;
    EngineBase* engine = nullptr;
};

struct MergePageDlg {
    int sourceNo = 0;
    int pageNo = 0;
    // degrees clockwise: 0, 90, 180 or 270
    int rotation = 0;
    bool removed = false;
    bool selected = false;
};

// orig's InsertPosWnd: where Add PDF... puts the pages
enum class MergeInsertAt {
    End,
    Beginning,
    AfterPage,
};

struct PdfToolDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    PdfToolKind kind = PdfToolKind::Bake;
    Str srcPath;   // owned
    Str browseExt; // owned; the default extension of the "..." save dialog
    // ExistingImageCount() for the inputs in existKey (owned): a frame doesn't
    // touch the disk
    Str existKey;
    int existCount = 0;
    int pageCount = 0;
    PagesMode pagesMode = PagesMode::All;
    // Save Selection As Image
    int pageNo = 0;
    RectF selRect;
    DialogSelect ddFormat;
    // orig's NewDpiCombo: the list of kImageDpiChoices under a field that
    // takes any other DPI
    DialogSelect ddDpi;
    gpui::InputState* dpiEdit = nullptr;
    gpui::InputState* destEdit = nullptr;
    gpui::InputState* pagesEdit = nullptr;
    gpui::InputState* passwordEdit = nullptr;
    // Merge PDF as orig's window, where the platform can have one
    // (gui/ToolWindow.h); null: a dialog in the frame
    ToolWindow* tw = nullptr;
    // orig's window for a single-purpose tool (Bake, Compress, ...), where
    // the platform can have one (DlgWindowOpen): modeless, owned by the main
    // window; null: a dialog in the frame
    ToolWindow* dlgTw = nullptr;
    bool wantFocus = false;
    // orig's FinishDialog(focusOn): the page dialogs open on the pages field
    bool focusPages = false;
    // orig's CalcDlgWidth: the client width (dips) of the tool's own window
    float winDx = 0;
    Vec<MergeSourceDlg> mergeSources;
    Vec<MergePageDlg> mergePages;
    MergeThumbCache* mergeThumbs = nullptr;
    // more pages than fit in the dialog
    float mergeScrollY = 0;
    // Shift + click selects from the anchor to the clicked page
    int mergeAnchorIdx = 0;
    int mergeFocusIdx = 0;
    // the grid's bounds in the window, as of the last frame, and what the
    // layout of that frame was computed for
    gp::Bounds mergeView{};
    // the files' legend and its entries, as of the last frame
    gp::Bounds mergeLegendView{};
    Vec<gp::Bounds> mergeLegendItems;
    float mergeLayoutDx = 0;
    float mergeLayoutDy = 0;
    // width the columns were laid out for; bounds are filled in only while painting
    float mergeGeomDx = 0;
    int mergeCols = 1;
    // scroll the keyboard's page into view once the grid has a size
    bool mergeEnsureFocus = false;
    int mergeHoverIdx = -1;
    // the mouse is on that page's corner button
    bool mergeHoverBtn = false;
    MergePress mergePress = MergePress::None;
    // the grid's context menu and the box it is placed in
    gp::Entity<gp::PopupMenuState> mergePopup;
    gpui::Bounds mergePagesBounds{};
    int mergePressIdx = -1;
    PointF mergePressPt; // window coords
    bool mergeDragging = false;
    // while dragging pages: they'd go in front of this item
    // (len(items): at the end); -1 when not over the grid
    int mergeDropBefore = -1;
    // the rubber band, in content coords (y + scrollY)
    PointF mergeBandFrom;
    PointF mergeBandTo;
    // the selection the band started from (Ctrl adds to it)
    Vec<u8> mergeBandBase;
    // where the mouse was last, local coords; auto scroll goes on from it
    PointF mergeLast;
    int mergeAutoScrollMs = 0;
    // the "Add PDF" question, up while mergeAskPaths has the files to add
    StrVec mergeAskPaths;
    MergeInsertAt mergeAskAt = MergeInsertAt::End;
    gp::InputState* mergeAskEdit = nullptr;
    bool mergeAskFocus = false;
    // the pages last copied or cut; pasted while the system clipboard still
    // has the text copying them put there (orig: its sequence number)
    Vec<MergePageDlg> mergeClip;
    Str mergeClipText; // owned
};

// Merge PDF (orig has one such window), and the single-purpose tool that is
// a dialog in the frame, where there is room for one
static PdfToolDlg gMainTool;

// ng: orig's tool dialogs are objects of their own, so several can be up at
// once. Here a tool in a window of its own has a PdfToolDlg of its own
// (gWinTools), and the code below works on "the current tool", gTool: the
// frame's and Merge PDF's unless a ToolScope says otherwise. Everything that
// can run for a tool window - its build and key callbacks, the listeners of
// its controls, what comes back from its file dialog - starts with a scope
static PdfToolDlg* gCurTool = &gMainTool;
#define gTool (*gCurTool)

static Vec<PdfToolDlg*> gWinTools;
// closed ones; freed when the next tool opens, by when nothing uses them
static Vec<PdfToolDlg*> gDeadTools;

struct ToolScope {
    PdfToolDlg* prev;

    explicit ToolScope(PdfToolDlg* tool) : prev(gCurTool) { gCurTool = tool; }
    ToolScope() : prev(gCurTool) {}
    ~ToolScope() { gCurTool = prev; }
    ToolScope(const ToolScope&) = delete;
    ToolScope& operator=(const ToolScope&) = delete;
};

// the tool whose window `cx` is; the frame's (and Merge PDF's) otherwise
static PdfToolDlg* ToolForCx(gp::Ctx* cx) {
    for (PdfToolDlg* t : gWinTools) {
        if (t->dlgTw && cx->win && ToolWindowGpui(t->dlgTw) == cx->win) {
            return t;
        }
    }
    return &gMainTool;
}

// the tool of the dialog window whose callback is running
static PdfToolDlg* ToolForDlgWindow() {
    ToolWindow* tw = DlgWindowCurrent();
    for (PdfToolDlg* t : gWinTools) {
        if (tw && t->dlgTw == tw) {
            return t;
        }
    }
    return nullptr;
}

static bool IsWinTool(PdfToolDlg* tool) {
    return VecContains(gWinTools, tool);
}

struct PdfToolView {
    static void OnAction(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnBrowse(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnUseNewNames(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnPagesMode(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t mode);
    static void OnMergeAdd(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnMergeScroll(PdfToolView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnMergeGridDown(PdfToolView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnMergeGridMove(PdfToolView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
    static void OnMergeGridDrag(PdfToolView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
    static void OnMergeGridUp(PdfToolView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev);
    static void OnMergeSetRemoved(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t removed);
    static void OnMergeMenu(PdfToolView* self, gp::Ctx* cx, const gp::ActionEvent* ev);
    static void OnMergeSaveAs(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnMergeAskAt(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t at);
    static void OnMergeAskInput(PdfToolView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnMergeAskOk(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnMergeAskCancel(PdfToolView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(PdfToolView* self, gp::Ctx* cx, const gp::InputEvent* ev);
};

static gp::Entity<PdfToolView> gToolView;

static bool IsPageRangeTool() {
    return gTool.kind == PdfToolKind::DeletePages || gTool.kind == PdfToolKind::ExtractPages;
}

static bool HasFormatDropDown() {
    return gTool.kind == PdfToolKind::ConvertToImages || gTool.kind == PdfToolKind::SaveSelectionAsImage;
}

static Str ToolTitleFor(PdfToolKind kind) {
    switch (kind) {
        case PdfToolKind::Bake:
            return Tr("Bake PDF");
        case PdfToolKind::ExtractText:
            return Tr("Extract Text From PDF");
        case PdfToolKind::Compress:
            return Tr("Compress PDF");
        case PdfToolKind::Decompress:
            return Tr("Decompress PDF");
        case PdfToolKind::DeletePages:
            return Tr("Delete Pages From PDF");
        case PdfToolKind::ExtractPages:
            return Tr("Extract Pages From PDF");
        case PdfToolKind::Encrypt:
            return Tr("Encrypt PDF");
        case PdfToolKind::Decrypt:
            return Tr("Decrypt PDF");
        case PdfToolKind::ConvertToPdf:
            return Tr("Convert to PDF");
        case PdfToolKind::ConvertToImages:
            return Tr("Convert PDF to Images");
        case PdfToolKind::SaveSelectionAsImage:
            return Tr("Save Selection As Image");
        case PdfToolKind::Merge:
            return Tr("Merge PDF");
        case PdfToolKind::ConvertEpubToPdf:
            return Tr("Convert EPUB to PDF");
    }
    return {};
}

static Str ToolTitle() {
    return ToolTitleFor(gTool.kind);
}

// ng: a window's caption is read with no tool current, so each kind has a
// function of its own
template <PdfToolKind kind>
static Str ToolTitleOf() {
    return ToolTitleFor(kind);
}

static Str (*ToolTitleFn(PdfToolKind kind))() {
    switch (kind) {
        case PdfToolKind::Bake:
            return ToolTitleOf<PdfToolKind::Bake>;
        case PdfToolKind::ExtractText:
            return ToolTitleOf<PdfToolKind::ExtractText>;
        case PdfToolKind::Compress:
            return ToolTitleOf<PdfToolKind::Compress>;
        case PdfToolKind::Decompress:
            return ToolTitleOf<PdfToolKind::Decompress>;
        case PdfToolKind::DeletePages:
            return ToolTitleOf<PdfToolKind::DeletePages>;
        case PdfToolKind::ExtractPages:
            return ToolTitleOf<PdfToolKind::ExtractPages>;
        case PdfToolKind::Encrypt:
            return ToolTitleOf<PdfToolKind::Encrypt>;
        case PdfToolKind::Decrypt:
            return ToolTitleOf<PdfToolKind::Decrypt>;
        case PdfToolKind::ConvertToPdf:
            return ToolTitleOf<PdfToolKind::ConvertToPdf>;
        case PdfToolKind::ConvertToImages:
            return ToolTitleOf<PdfToolKind::ConvertToImages>;
        case PdfToolKind::SaveSelectionAsImage:
            return ToolTitleOf<PdfToolKind::SaveSelectionAsImage>;
        case PdfToolKind::ConvertEpubToPdf:
            return ToolTitleOf<PdfToolKind::ConvertEpubToPdf>;
        case PdfToolKind::Merge:
            break;
    }
    return ToolTitleOf<PdfToolKind::Merge>;
}

static int ExistingImageCount();

static Str ToolActionText() {
    switch (gTool.kind) {
        case PdfToolKind::ExtractText:
            return Tr("Extract Text");
        case PdfToolKind::DeletePages:
            return Tr("Delete Pages");
        case PdfToolKind::ExtractPages:
            return Tr("Extract Pages");
        case PdfToolKind::ConvertToImages:
            return ExistingImageCount() > 0 ? Tr("Overwrite") : Tr("Convert");
        case PdfToolKind::SaveSelectionAsImage:
            return Tr("Save");
        case PdfToolKind::Merge:
            return Tr("Save");
        case PdfToolKind::ConvertEpubToPdf:
            return Tr("Convert to PDF");
        default:
            return ToolTitle();
    }
}

static TempStr InputTextTemp(gp::InputState* s) {
    if (!s) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(s)));
}

static TempStr DestPathTemp() {
    return InputTextTemp(gTool.destEdit);
}

static int SelectedFormatIdx() {
    int idx = gTool.ddFormat.sel;
    if (idx < 0 || idx >= ConvertImageFormatCount()) {
        return 0;
    }
    return idx;
}

static bool HasDpiCombo() {
    return HasFormatDropDown() || gTool.kind == PdfToolKind::ConvertEpubToPdf;
}

// orig's DpiFromComboText: 0 if what is typed is not a usable DPI
static int SelectedDpi() {
    TempStr s = InputTextTemp(gTool.dpiEdit);
    str::TrimWSInPlace(s, str::TrimOpt::Both);
    int dpi = ParseInt(s);
    if (dpi < 1 || dpi > kMaxImageDpi) {
        return 0;
    }
    return dpi;
}

static void SetDestExtFromFormat() {
    if (!gTool.destEdit) {
        return;
    }
    TempStr dest = DestPathTemp();
    if (len(dest) == 0) {
        return;
    }
    Str ext = kConvertImageFormats[SelectedFormatIdx()].ext;
    TempStr noExt = path::GetPathNoExtTemp(dest);
    TempStr newDest = str::JoinTemp(noExt, ext);
    if (str::EqI(dest, newDest)) {
        return;
    }
    gp::InputSetValue(gTool.destEdit, ToGpui(newDest));
}

static void SyncFormatFromPath(gp::App* app, Str path) {
    int idx = ConvertImageFormatIdxFromPath(path);
    if (idx == gTool.ddFormat.sel) {
        return;
    }
    gTool.ddFormat.SetSel(app, idx);
}

static void DeleteMergeThumbs(MergeThumbCache* cache) {
    for (MergeThumb& thumb : cache->thumbs) {
        FreePixmap(thumb.bitmap);
        if (thumb.image) {
            gp::RenderImageRelease(thumb.image);
        }
    }
    VecReset(cache->thumbs);
    delete cache;
}

static void DetachMergeThumbs() {
    MergeThumbCache* cache = gTool.mergeThumbs;
    gTool.mergeThumbs = nullptr;
    if (!cache) {
        return;
    }
    AtomicIntSet(&cache->cancel, 1);
    if (cache->workerRunning) {
        cache->deleteWhenWorkerFinishes = true;
    } else {
        DeleteMergeThumbs(cache);
    }
}

// `toolWin`: the tool's own window, which is closed a moment later
static void FreeToolState(gp::Window* toolWin) {
    MainWindow* win = gTool.win;
    // whichever of them has the focus: the window would keep a pointer to a
    // freed field (the page range of Delete Pages was left focused)
    gp::InputState* edits[] = {gTool.destEdit, gTool.pagesEdit, gTool.passwordEdit, gTool.mergeAskEdit, gTool.dpiEdit};
    for (gp::InputState* e : edits) {
        if (e && win && win->gpuiWin && win->gpuiWin->input == e) {
            gp::InputBlur(e, win->gpuiWin->app, win->gpuiWin);
        }
        if (e && toolWin && toolWin->input == e) {
            gp::InputBlur(e, toolWin->app, toolWin);
        }
    }
    delete gTool.destEdit;
    delete gTool.pagesEdit;
    delete gTool.passwordEdit;
    delete gTool.mergeAskEdit;
    delete gTool.dpiEdit;
    gTool.dpiEdit = nullptr;
    gTool.mergeAskEdit = nullptr;
    gTool.mergeAskPaths.Reset();
    gTool.destEdit = nullptr;
    gTool.pagesEdit = nullptr;
    gTool.passwordEdit = nullptr;
    gTool.ddFormat.Free();
    gTool.ddDpi.Free();
    DetachMergeThumbs();
    for (MergeSourceDlg& source : gTool.mergeSources) {
        str::Free(source.path);
        str::Free(source.password);
        SafeEngineRelease(&source.engine);
    }
    VecReset(gTool.mergeSources);
    VecReset(gTool.mergePages);
    VecReset(gTool.mergeClip);
    str::FreePtr(&gTool.mergeClipText);
    str::Free(gTool.srcPath);
    gTool.srcPath = {};
    str::Free(gTool.browseExt);
    gTool.browseExt = {};
    str::Free(gTool.existKey);
    gTool.existKey = {};
}

// the dialog in the frame; a tool in a window of its own is not the frame's
// business
bool IsPdfToolDialogVisible() {
    return gMainTool.visible && !gMainTool.tw && !gMainTool.dlgTw;
}

void ClosePdfToolDialog() {
    if (!gTool.visible) {
        return;
    }
    gTool.visible = false;
    // its window can outlive the main window by a moment (the layer tells
    // it later): nothing of a closed main window is touched
    if (!IsMainWindowValid(gTool.win)) {
        gTool.win = nullptr;
    }
    MainWindow* win = gTool.win;
    if (gTool.tw) {
        ToolWindowClose(gTool.tw);
        gTool.tw = nullptr;
    }
    gp::Window* toolWin = ToolWindowGpui(gTool.dlgTw);
    FreeToolState(toolWin);
    DlgWindowClose(&gTool.dlgTw);
    AppShellInvalidate(win);
    // a tool window's state goes with it; the caller may still read it
    PdfToolDlg* tool = gCurTool;
    if (IsWinTool(tool)) {
        VecRemove(gWinTools, tool);
        VecAppend(gDeadTools, tool);
    }
}

// --- what the action button does --------------------------------------------

// orig closes the dialog, then loads what the tool wrote into the window it
// was opened from
static void CloseAndLoad(Str destPath) {
    MainWindow* win = gTool.win;
    TempStr path = str::DupTemp(destPath);
    ClosePdfToolDialog();
    if (IsMainWindowValidAndNotClosing(win)) {
        LoadDocument(win, path);
    }
}

static void PdfBakeDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    MainWindow* win = gTool.win;
    Str inputPath = gTool.srcPath;
    TempStr tmpPath;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    // pdfbake_main re-opens the file from disk, so unsaved session
    // annotations would be missing unless we write them out first (issue #5977).
    if (engine && EngineHasUnsavedAnnotations(engine)) {
        tmpPath = GetTempFilePathTemp(StrL("bake"));
        if (len(tmpPath) == 0 || !EngineMupdfSaveCopy(engine, tmpPath)) {
            MessageBoxWarning(win, StrL("Failed to bake PDF file."), Tr("Bake PDF"));
            return;
        }
        inputPath = tmpPath;
        logf("PdfBakeDoIt: unsaved annotations, baking from temp '%s'\n", tmpPath);
    }

    logf("PdfBakeDoIt: baking '%s' to '%s'\n", inputPath, destPath);

    // build argv for pdfbake_main: "bake" input output
    char* argv[] = {(char*)"bake", CStrTemp(inputPath), CStrTemp(destPath)};
    int argc = 3;

    fz_set_optind(0);
    int res = pdfbake_main(argc, argv);
    if (tmpPath) {
        file::Delete(tmpPath);
    }
    if (res == 0) {
        logf("PdfBakeDoIt: baked successfully\n");
        CloseAndLoad(destPath);
        return;
    }
    logf("PdfBakeDoIt: pdfbake_main failed with %d\n", res);
    MessageBoxWarning(win, StrL("Failed to bake PDF file."), Tr("Bake PDF"));
}

static void PdfExtractTextDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    TempStr pages = InputTextTemp(gTool.pagesEdit);
    if (len(pages) == 0) {
        return;
    }
    MainWindow* win = gTool.win;

    logf("PdfExtractTextDoIt: extracting text from '%s' to '%s', pages: %s\n", gTool.srcPath, destPath, pages);

    bool ok = false;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    bool isPdf = tab && IsPdfDoc(tab);
    if (isPdf) {
        // use muconvert for PDF
        char* argv[] = {(char*)"convert", (char*)"-o", CStrTemp(destPath), CStrTemp(gTool.srcPath), CStrTemp(pages)};
        int argc = 5;
        fz_set_optind(0);
        ok = muconvert_main(argc, argv) == 0;
    } else {
        // use engine text extraction for other formats (DjVu, etc.)
        ok = ExtractTextViaEngine(gTool.win, destPath, pages);
    }

    if (ok) {
        logf("PdfExtractTextDoIt: extracted successfully\n");
        TempStr path = str::DupTemp(destPath);
        ClosePdfToolDialog();
        ShowFileInFolder(win, path);
        return;
    }
    logf("PdfExtractTextDoIt: failed to extract text, isPdf: %d\n", (int)isPdf);
    MessageBoxWarning(win, StrL("Failed to extract text."), Tr("Extract Text"));
}

static void PdfCleanDoIt(bool decompress) {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    MainWindow* win = gTool.win;
    Str what = decompress ? StrL("decompress") : StrL("compress");
    logf("PdfCleanDoIt: %s '%s' to '%s'\n", what, gTool.srcPath, destPath);

    // compress is: clean -gggg -e 100 -f -i -t -Z input output
    // decompress is: clean -d input output
    char* compressArgv[] = {
        (char*)"clean", (char*)"-gggg",          (char*)"-e",       (char*)"100", (char*)"-f", (char*)"-i", (char*)"-t",
        (char*)"-Z",    CStrTemp(gTool.srcPath), CStrTemp(destPath)};
    char* decompressArgv[] = {(char*)"clean", (char*)"-d", CStrTemp(gTool.srcPath), CStrTemp(destPath)};
    char** argv = decompress ? decompressArgv : compressArgv;
    int argc = decompress ? 4 : 10;

    fz_set_optind(0);
    int res = pdfclean_main(argc, argv);
    if (res == 0) {
        logf("PdfCleanDoIt: %sed successfully\n", what);
        CloseAndLoad(destPath);
        return;
    }
    logf("PdfCleanDoIt: pdfclean_main failed with %d\n", res);
    Str msg = decompress ? StrL("Failed to decompress PDF file.") : StrL("Failed to compress PDF file.");
    MessageBoxWarning(win, msg, decompress ? Tr("Decompress PDF") : Tr("Compress PDF"));
}

static void PdfDeletePageDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    MainWindow* win = gTool.win;
    bool isExtract = gTool.kind == PdfToolKind::ExtractPages;
    TempStr pages = InputTextTemp(gTool.pagesEdit);

    Vec<int> parsedPages;
    if (!ParseDeletePages(pages, gTool.pageCount, parsedPages)) {
        return;
    }
    if (!isExtract && len(parsedPages) >= gTool.pageCount) {
        return;
    }

    WindowTab* sourceTab = isExtract ? FindTabByFilePath(gTool.srcPath) : nullptr;
    EngineBase* sourceEngine = sourceTab ? sourceTab->GetEngine() : nullptr;

    TempStr pageRange;
    if (isExtract) {
        // for extract: pass the specified pages directly to pdfclean
        pageRange = FormatPageRangeTemp(parsedPages);
    } else {
        // for delete: pass the complement (pages to keep) to pdfclean
        pageRange = BuildKeepPagesRangeTemp(gTool.pageCount, parsedPages);
    }

    Str op = isExtract ? StrL("extract") : StrL("delete");
    logf("PdfDeletePageDoIt: %s pages '%s' from '%s' to '%s', range for pdfclean: %s\n", op, pages, gTool.srcPath,
         destPath, pageRange);

    Str inputPath = gTool.srcPath;
    TempStr tmpPath;
    if (isExtract && sourceEngine && EngineHasUnsavedAnnotations(sourceEngine)) {
        tmpPath = GetTempFilePathTemp(StrL("extract-pages"));
        if (len(tmpPath) == 0 || !EngineMupdfSaveCopy(sourceEngine, tmpPath)) {
            MessageBoxWarning(win, StrL("Failed to extract pages from PDF file."), Tr("Extract Pages From PDF"));
            return;
        }
        inputPath = tmpPath;
    }

    // the destination folder may not exist yet (issue #6424)
    dir::CreateForFile(destPath);

    // equivalent of: clean -gggg -e 100 -f -i -t -Z input.pdf output.pdf <page-range>
    // use the same compression flags as Compress PDF so the result is re-written
    // compactly; otherwise the kept pages drag along the original's full content
    // and the output is nearly as big as the source
    char* argv[] = {(char*)"clean",      (char*)"-gggg",     (char*)"-e",        (char*)"100",
                    (char*)"-f",         (char*)"-i",        (char*)"-t",        (char*)"-Z",
                    CStrTemp(inputPath), CStrTemp(destPath), CStrTemp(pageRange)};
    int argc = 11;

    fz_set_optind(0);
    int res = pdfclean_main(argc, argv);
    if (tmpPath) {
        file::Delete(tmpPath);
    }
    if (res == 0) {
        logf("PdfDeletePageDoIt: %s pages successfully\n", op);
        CloseAndLoad(destPath);
        return;
    }
    logf("PdfDeletePageDoIt: pdfclean_main failed with %d for %s\n", res, op);
    Str msg =
        isExtract ? StrL("Failed to extract pages from PDF file.") : StrL("Failed to delete pages from PDF file.");
    Str title = isExtract ? Tr("Extract Pages From PDF") : Tr("Delete Pages From PDF");
    MessageBoxWarning(win, msg, title);
}

static void PdfEncryptDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    TempStr pwd = InputTextTemp(gTool.passwordEdit);
    if (len(pwd) == 0) {
        return;
    }
    MainWindow* win = gTool.win;

    logf("PdfEncryptDoIt: encrypting '%s' to '%s' with AES-256\n", gTool.srcPath, destPath);

    // equivalent of: clean -E aes-256 -U <pwd> -O <pwd> input output
    char* pwdZ = CStrTemp(pwd);
    char* argv[] = {(char*)"clean", (char*)"-E", (char*)"aes-256",        (char*)"-U",       pwdZ,
                    (char*)"-O",    pwdZ,        CStrTemp(gTool.srcPath), CStrTemp(destPath)};
    int argc = 9;

    fz_set_optind(0);
    int res = pdfclean_main(argc, argv);
    if (res == 0) {
        logf("PdfEncryptDoIt: encrypted successfully\n");
        CloseAndLoad(destPath);
        return;
    }
    logf("PdfEncryptDoIt: pdfclean_main failed with %d\n", res);
    MessageBoxWarning(win, StrL("Failed to encrypt PDF file."), Tr("Encrypt PDF"));
}

static void PdfDecryptDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    MainWindow* win = gTool.win;

    logf("PdfDecryptDoIt: decrypting '%s' to '%s'\n", gTool.srcPath, destPath);

    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (EngineMupdfSaveDecrypted(tab ? tab->GetEngine() : nullptr, gTool.srcPath, destPath)) {
        logf("PdfDecryptDoIt: decrypted successfully\n");
        CloseAndLoad(destPath);
        return;
    }
    logf("PdfDecryptDoIt: failed, src: '%s'\n", gTool.srcPath);
    ShowWarningNotification(win, Tr("Failed to decrypt PDF file."), kNotif5SecsTimeOut);
}

// orig's ConvertEpubToPdfDialog::DoIt: ddFormat holds the page size and the
// DPI combo the font size
static void ConvertEpubToPdfDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    MainWindow* win = gTool.win;
    int fontSize = EbookPdfFontSizeFromText(InputTextTemp(gTool.dpiEdit));
    if (fontSize == 0) {
        MessageBoxWarning(win, Tr("Invalid font size"), Tr("Convert EPUB to PDF"));
        return;
    }
    int idx = gTool.ddFormat.sel;
    if (idx < 0 || idx >= dimofi(kEbookPdfPageSizes)) {
        idx = 0;
    }
    const EbookPdfPageSize& ps = kEbookPdfPageSizes[idx];
    logf("ConvertEpubToPdf: '%s' -> '%s', %s, %d pt\n", gTool.srcPath, destPath, ps.label, fontSize);
    int nPages = EngineMupdfEbookToPdf(gTool.srcPath, destPath, ps.dx, ps.dy, (float)fontSize);
    if (nPages == 0) {
        MessageBoxWarning(win, Tr("Failed to save a file"), Tr("Convert EPUB to PDF"));
        return;
    }
    logf("ConvertEpubToPdf: wrote %d pages\n", nPages);
    CloseAndLoad(destPath);
}

static void ConvertToPdfDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    MainWindow* win = gTool.win;
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !engine->isImageCollection) {
        MessageBoxWarning(win, Tr("Failed to save a file"), Tr("Convert to PDF"));
        return;
    }

    logf("ConvertToPdf: converting '%s' to '%s'\n", gTool.srcPath, destPath);
    bool ok = ConvertImageCollectionToPdf(engine, destPath);
    if (ok) {
        logf("ConvertToPdf: converted successfully\n");
        CloseAndLoad(destPath);
        return;
    }
    logf("ConvertToPdf: SaveImageCollectionAsPdf failed\n");
    MessageBoxWarning(win, Tr("Failed to save a file"), Tr("Convert to PDF"));
}

// the pages the mode picks; false for a custom range that doesn't parse
static bool SelectedImagePages(Vec<int>& pages) {
    VecReset(pages);
    if (gTool.pagesMode == PagesMode::Current) {
        DisplayModel* dm = gTool.win ? gTool.win->AsFixed() : nullptr;
        int cur = dm ? dm->CurrentPageNo() : 0;
        if (cur < 1 || cur > gTool.pageCount) {
            return false;
        }
        VecAppend(pages, cur);
        return true;
    }
    if (gTool.pagesMode == PagesMode::All) {
        CollectAllPages(gTool.pageCount, pages);
        return true;
    }
    return ParseDeletePages(InputTextTemp(gTool.pagesEdit), gTool.pageCount, pages);
}

// how many files Convert would overwrite (#6439)
static int ExistingImageCount() {
    if (gTool.kind != PdfToolKind::ConvertToImages) {
        return 0;
    }
    DisplayModel* dm = gTool.win ? gTool.win->AsFixed() : nullptr;
    int cur = dm ? dm->CurrentPageNo() : 0;
    TempStr key = fmt("%d %d %s|%s", (int)gTool.pagesMode, cur, InputTextTemp(gTool.pagesEdit), DestPathTemp());
    if (str::Eq(key, gTool.existKey)) {
        return gTool.existCount;
    }
    str::ReplaceWithCopy(&gTool.existKey, key);
    Vec<int> pages;
    gTool.existCount = SelectedImagePages(pages) ? CountExistingImages(DestPathTemp(), pages) : 0;
    return gTool.existCount;
}

static void ConvertPdfToImagesDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    MainWindow* win = gTool.win;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        MessageBoxWarning(win, StrL("Failed to convert PDF to images."), Tr("Convert PDF to Images"));
        return;
    }

    Vec<int> pages;
    if (!SelectedImagePages(pages)) {
        return;
    }

    int dpi = SelectedDpi();
    if (dpi < 1) {
        return;
    }
    if (!PagesFitDpi(engine, pages, dpi)) {
        MessageBoxWarning(win, Tr("Too large for this DPI"), Tr("Convert PDF to Images"));
        return;
    }

    logf("ConvertPdfToImages: '%s' -> '%s', %d page(s), %d DPI\n", gTool.srcPath, destPath, len(pages), dpi);

    Str firstPath;
    int nOk = ConvertPagesToImages(engine, dm->GetRotation(), destPath, pages, dpi, &firstPath);

    if (nOk == 0) {
        str::Free(firstPath);
        MessageBoxWarning(win, StrL("Failed to convert PDF to images."), Tr("Convert PDF to Images"));
        return;
    }
    logf("ConvertPdfToImages: wrote %d of %d file(s)\n", nOk, len(pages));
    TempStr openPath = str::DupTemp(firstPath);
    str::Free(firstPath);
    ClosePdfToolDialog();
    ShowFileInFolder(win, openPath);
}

static void SaveSelectionAsImageDoIt() {
    TempStr destPath = DestPathTemp();
    if (len(destPath) == 0) {
        return;
    }
    MainWindow* win = gTool.win;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    TempStr withExt = WithDefaultImageExtTemp(destPath);
    if (len(withExt) == 0) {
        MessageBoxWarning(win, StrL("Unsupported image format."), Tr("Save Selection As Image"));
        return;
    }
    int dpi = SelectedDpi();
    if (dpi < 1) {
        return;
    }
    Pixmap* px = RenderSelectionPixmap(engine, dm->GetRotation(), gTool.pageNo, gTool.selRect, (float)dpi);
    if (!px) {
        MessageBoxWarning(win, StrL("Failed to save the selection as an image."), Tr("Save Selection As Image"));
        return;
    }

    auto* work = new SaveSelImgWork();
    work->px = px;
    work->destPath = str::Dup(withExt);
    work->win = win;

    NotificationCreateArgs nargs;
    nargs.win = win;
    nargs.msg = Tr("Saving image...");
    nargs.groupId = kNotifSaveSelectionAsImage;
    nargs.timeoutMs = kNotifNoTimeout;
    ShowNotification(nargs);

    ClosePdfToolDialog();
    RunAsync(MkFunc0(SaveSelImgThread, work), StrL("SaveSelImg"));
}

struct MergeThumbWorkItem {
    int sourceNo = 0;
    int pageNo = 0;
    int rotation = 0;
    int thumbIdx = 0;
};

struct MergeThumbWorker {
    MergeThumbCache* cache = nullptr;
    Vec<EngineBase*> engines;
    Vec<MergeThumbWorkItem> items;
};

struct MergeThumbResult {
    MergeThumbCache* cache = nullptr;
    int thumbIdx = 0;
    Pixmap* bitmap = nullptr;
};

static int MergeThumbIdx(const MergePageDlg& page) {
    const MergeSourceDlg& source = gTool.mergeSources[page.sourceNo];
    return source.thumbStart + ((page.pageNo - 1) * kMergeRotations) + (page.rotation / 90);
}

static Pixmap* RenderMergeThumb(EngineBase* engine, int pageNo, int rotation) {
    RectF pageRect = engine->PageMediabox(pageNo);
    if (pageRect.IsEmpty()) {
        return nullptr;
    }
    pageRect = engine->Transform(pageRect, pageNo, 1.0f, rotation);
    float zoom = std::min((float)kMergeThumbDx / pageRect.dx, (float)kMergeThumbDy / pageRect.dy);
    RenderPageArgs args(pageNo, zoom, rotation, nullptr, RenderTarget::View);
    // kept as opaque 24bpp, like the other thumbnails
    return PixmapToBgr(engine->RenderPage(args));
}

static void FinishMergeThumb(MergeThumbResult* result) {
    MergeThumbCache* cache = result->cache;
    if (!cache->deleteWhenWorkerFinishes && result->thumbIdx >= 0 && result->thumbIdx < len(cache->thumbs)) {
        MergeThumb& thumb = cache->thumbs[result->thumbIdx];
        thumb.bitmap = result->bitmap;
        result->bitmap = nullptr;
        thumb.failed = !thumb.bitmap;
        AppShellInvalidate(cache->win);
    }
    FreePixmap(result->bitmap);
    delete result;
}

static void StartMergeThumbs();

static void FinishMergeThumbWorker(MergeThumbCache* cache) {
    cache->workerRunning = false;
    if (cache->deleteWhenWorkerFinishes) {
        DeleteMergeThumbs(cache);
        return;
    }
    AppShellInvalidate(cache->win);
    // pages rotated meanwhile
    StartMergeThumbs();
}

static void RenderMergeThumbs(MergeThumbWorker* worker) {
    Vec<EngineBase*> engines;
    for (EngineBase* engine : worker->engines) {
        VecAppend(engines, engine->Clone());
        engine->Release();
    }
    for (const MergeThumbWorkItem& item : worker->items) {
        if (AtomicIntGet(&worker->cache->cancel) != 0) {
            break;
        }
        auto* result = new MergeThumbResult;
        result->cache = worker->cache;
        result->thumbIdx = item.thumbIdx;
        EngineBase* engine = engines[item.sourceNo];
        result->bitmap = engine ? RenderMergeThumb(engine, item.pageNo, item.rotation) : nullptr;
        uitask::Post(MkFunc0<MergeThumbResult>(FinishMergeThumb, result));
    }
    for (EngineBase* engine : engines) {
        SafeEngineRelease(&engine);
    }
    uitask::Post(MkFunc0<MergeThumbCache>(FinishMergeThumbWorker, worker->cache));
    delete worker;
}

static void StartMergeThumbs() {
    MergeThumbCache* cache = gTool.mergeThumbs;
    if (!cache || cache->workerRunning) {
        return;
    }
    auto* worker = new MergeThumbWorker;
    worker->cache = cache;
    for (MergeSourceDlg& source : gTool.mergeSources) {
        source.engine->AddRef();
        VecAppend(worker->engines, source.engine);
    }
    // only the rotations the pages have; each page is in the grid once
    for (const MergePageDlg& page : gTool.mergePages) {
        int idx = MergeThumbIdx(page);
        const MergeThumb& thumb = cache->thumbs[idx];
        if (!thumb.bitmap && !thumb.failed) {
            VecAppend(worker->items, MergeThumbWorkItem{page.sourceNo, page.pageNo, page.rotation, idx});
        }
    }
    if (len(worker->items) == 0) {
        for (EngineBase* engine : worker->engines) {
            engine->Release();
        }
        delete worker;
        return;
    }
    cache->workerRunning = true;
    RunAsync(MkFunc0<MergeThumbWorker>(RenderMergeThumbs, worker), StrL("MergeThumbnailRender"));
}

static gp::ImageLoadState MergeThumbLoad(gp::PaintApp* pa, void* user, gp::RenderImage** imageOut) {
    auto* thumb = (MergeThumb*)user;
    if (!thumb->image && thumb->bitmap) {
        thumb->image = RenderImageFromPixmap(pa, thumb->bitmap);
    }
    *imageOut = thumb->image;
    return thumb->image ? gp::ImageLoadState::Ready : gp::ImageLoadState::Loading;
}

static void MergeSelectOnly(int idx);
static void MergeMoveSelected(int before);

// --- orig's MergeGrid: geometry, hit testing, press / drag / rubber band -------
// Coordinates are dips local to the grid's view (gTool.mergeView, the bounds
// gpui reported for the last frame); "content" coords are local y + scrollY.

static float MergePad() {
    return (float)(kMergeGap * 3 / 4);
}

static float MergeItemDy() {
    return (float)(kMergeThumbDy + kMergeLabelDy + kMergeGap);
}

static int MergeRows() {
    int cols = std::max(1, gTool.mergeCols);
    return (len(gTool.mergePages) + cols - 1) / cols;
}

static float MergeViewportDy() {
    return std::max(0.f, gTool.mergeView.h - 2 * MergePad());
}

static float MergeMaxScrollY() {
    return std::max(0.f, (float)MergeRows() * MergeItemDy() - MergeViewportDy());
}

// orig's SetBounds: as many columns as fit beside the scrollbar
static int MergeColsForDx(float dx) {
    float availableDx = dx - 2 * MergePad() - (float)kMergeScrollbarDx;
    return std::max(1, (int)((availableDx + (float)kMergeGap) / (float)(kMergeThumbDx + kMergeGap)));
}

// x of the first column, local coords: the grid is centered
static float MergeGridLeft() {
    float sbDx = MergeMaxScrollY() > 0 ? (float)kMergeScrollbarDx : 0;
    float w = gTool.mergeGeomDx > 1.f ? gTool.mergeGeomDx : gTool.mergeView.w;
    float itemsDx = w - 2 * MergePad() - sbDx;
    float gridDx = (float)(gTool.mergeCols * kMergeThumbDx + (gTool.mergeCols - 1) * kMergeGap);
    return MergePad() + std::max(0.f, (itemsDx - gridDx) / 2);
}

// the thumbnail of item idx, local coords
static RectF MergeCellRect(int idx) {
    int row = idx / gTool.mergeCols;
    int col = idx % gTool.mergeCols;
    return {MergeGridLeft() + (float)(col * (kMergeThumbDx + kMergeGap)),
            MergePad() + (float)row * MergeItemDy() - gTool.mergeScrollY, (float)kMergeThumbDx, (float)kMergeThumbDy};
}

static RectF MergeCornerBtnRect(int idx) {
    RectF r = MergeCellRect(idx);
    float dx = (float)kMergeCornerBtnDx;
    float inset = (float)kMergeCornerBtnInset;
    return {r.x + r.dx - dx - inset, r.y + inset, dx, dx};
}

// -1 when not on a thumbnail
static int MergeItemAt(float x, float y) {
    if (y < MergePad() || y >= MergePad() + MergeViewportDy()) {
        return -1;
    }
    float left = MergeGridLeft();
    if (x < left) {
        return -1;
    }
    int row = (int)((y - MergePad() + gTool.mergeScrollY) / MergeItemDy());
    int col = (int)((x - left) / (float)(kMergeThumbDx + kMergeGap));
    if (col >= gTool.mergeCols) {
        return -1;
    }
    int idx = (row * gTool.mergeCols) + col;
    if (idx < 0 || idx >= len(gTool.mergePages) || !MergeCellRect(idx).Contains(PointF{x, y})) {
        return -1;
    }
    return idx;
}

// Where pages dropped at the point go: in front of the returned item, at the
// gap nearest to the point (len(items): after the last)
static int MergeDropPositionAt(float x, float y) {
    int n = len(gTool.mergePages);
    if (x < 0 || y < 0 || x >= gTool.mergeView.w || y >= gTool.mergeView.h) {
        return -1;
    }
    float cy = y - MergePad() + gTool.mergeScrollY;
    int row = cy < 0 ? 0 : (int)(cy / MergeItemDy());
    if (row >= MergeRows()) {
        return n;
    }
    // left of a thumbnail's middle: in front of it
    float step = (float)(kMergeThumbDx + kMergeGap);
    float cx = x - MergeGridLeft() - (float)(kMergeThumbDx / 2);
    int slot = cx < 0 ? 0 : limitValue((int)(cx / step) + 1, 0, gTool.mergeCols);
    return std::min((row * gTool.mergeCols) + slot, n);
}

static bool MergeScrollTo(float y) {
    y = limitValue(y, 0.f, MergeMaxScrollY());
    if (y == gTool.mergeScrollY) {
        return false;
    }
    gTool.mergeScrollY = y;
    return true;
}

// orig's VirtListBox::EnsureVisible for a row of the grid
static void MergeEnsureVisible(int row) {
    float top = (float)row * MergeItemDy();
    float viewDy = MergeViewportDy();
    if (viewDy <= 0) {
        gTool.mergeEnsureFocus = true;
        return;
    }
    float y = gTool.mergeScrollY;
    if (top < y) {
        y = top;
    } else if (top + MergeItemDy() > y + viewDy) {
        y = top + MergeItemDy() - viewDy;
    }
    MergeScrollTo(y);
}

// selects what the band touches, on top of what was selected when it started
static void MergeUpdateBand() {
    RectF band{
        std::min(gTool.mergeBandFrom.x, gTool.mergeBandTo.x), std::min(gTool.mergeBandFrom.y, gTool.mergeBandTo.y),
        std::abs(gTool.mergeBandTo.x - gTool.mergeBandFrom.x), std::abs(gTool.mergeBandTo.y - gTool.mergeBandFrom.y)};
    band.Offset(0, -gTool.mergeScrollY);
    // a click without a move still deselects; an empty rect intersects nothing
    for (int i = 0; i < len(gTool.mergePages); i++) {
        bool base = i < len(gTool.mergeBandBase) && gTool.mergeBandBase[i];
        gTool.mergePages[i].selected = base || !band.Intersect(MergeCellRect(i)).IsEmpty();
    }
}

static void MergeEndPress() {
    gTool.mergePress = MergePress::None;
    gTool.mergePressIdx = -1;
    gTool.mergeDragging = false;
    VecReset(gTool.mergeBandBase);
    gTool.mergeDropBefore = -1;
    gTool.mergeAutoScrollMs = 0;
}

// near the top or bottom edge a drag scrolls, for as long as the mouse stays
// there (orig's AutoScrollStep, off the shell's tick instead of a WM_TIMER)
void PdfToolDialogTick(MainWindow* win, int ms) {
    if (!gTool.visible || gTool.win != win || gTool.kind != PdfToolKind::Merge) {
        return;
    }
    bool active = gTool.mergeDragging || gTool.mergePress == MergePress::Band;
    if (!active) {
        return;
    }
    gTool.mergeAutoScrollMs += ms;
    if (gTool.mergeAutoScrollMs < kMergeAutoScrollMs) {
        return;
    }
    gTool.mergeAutoScrollMs = 0;
    float edge = (float)(kMergeThumbDy / 4);
    float y = gTool.mergeLast.y;
    float dy = 0;
    if (y < edge) {
        dy = -(edge - y);
    } else if (y > gTool.mergeView.h - edge) {
        dy = y - (gTool.mergeView.h - edge);
    }
    if (dy == 0 || !MergeScrollTo(gTool.mergeScrollY + dy)) {
        return;
    }
    if (gTool.mergeDragging) {
        gTool.mergeDropBefore = MergeDropPositionAt(gTool.mergeLast.x, gTool.mergeLast.y);
    } else {
        gTool.mergeBandTo = {gTool.mergeLast.x, gTool.mergeLast.y + gTool.mergeScrollY};
        MergeUpdateBand();
    }
    AppShellInvalidate(win);
}

static int MergeSelectedCount() {
    int n = 0;
    for (const MergePageDlg& it : gTool.mergePages) {
        n += it.selected ? 1 : 0;
    }
    return n;
}

static int MergeSelectedRemovedCount() {
    int n = 0;
    for (const MergePageDlg& it : gTool.mergePages) {
        n += (it.selected && it.removed) ? 1 : 0;
    }
    return n;
}

// where added pages go: after the last selected page, else at the end
static int MergeInsertPosition() {
    for (int i = len(gTool.mergePages) - 1; i >= 0; i--) {
        if (gTool.mergePages[i].selected) {
            return i + 1;
        }
    }
    return len(gTool.mergePages);
}

static void MergeSelectOnly(int idx) {
    for (int i = 0; i < len(gTool.mergePages); i++) {
        gTool.mergePages[i].selected = i == idx;
    }
    gTool.mergeAnchorIdx = idx;
    gTool.mergeFocusIdx = idx;
}

static void MergeSelectAll() {
    for (MergePageDlg& it : gTool.mergePages) {
        it.selected = true;
    }
}

// the selected pages go together in front of item `before` and stay selected
static void MergeMoveSelected(int before) {
    Vec<MergePageDlg>& items = gTool.mergePages;
    Vec<MergePageDlg> moved;
    Vec<MergePageDlg> rest;
    int at = before;
    for (int i = 0; i < len(items); i++) {
        if (!items[i].selected) {
            VecAppend(rest, items[i]);
            continue;
        }
        VecAppend(moved, items[i]);
        if (i < before) {
            at--;
        }
    }
    if (len(moved) == 0) {
        return;
    }
    at = limitValue(at, 0, len(rest));
    VecReset(items);
    for (int i = 0; i < at; i++) {
        VecAppend(items, rest[i]);
    }
    for (const MergePageDlg& it : moved) {
        VecAppend(items, it);
    }
    for (int i = at; i < len(rest); i++) {
        VecAppend(items, rest[i]);
    }
    gTool.mergeFocusIdx = at;
    gTool.mergeAnchorIdx = at;
}

static void MergeSetRemoved(bool removed) {
    for (MergePageDlg& it : gTool.mergePages) {
        if (it.selected) {
            it.removed = removed;
        }
    }
}

// degrees: 90 turns the selected pages clockwise, -90 counterclockwise
static void MergeRotate(int degrees) {
    for (MergePageDlg& it : gTool.mergePages) {
        if (it.selected) {
            it.rotation = (((it.rotation + degrees) % 360) + 360) % 360;
        }
    }
    StartMergeThumbs();
}

// every page of the source, selected, in front of item `at`
static void MergeInsertPages(int sourceNo, int at) {
    Vec<MergePageDlg>& items = gTool.mergePages;
    for (MergePageDlg& it : items) {
        it.selected = false;
    }
    at = limitValue(at, 0, len(items));
    int pageCount = gTool.mergeSources[sourceNo].pageCount;
    for (int i = 0; i < pageCount; i++) {
        MergePageDlg it;
        it.sourceNo = sourceNo;
        it.pageNo = i + 1;
        it.selected = true;
        VecInsertAt(items, at + i, it);
    }
    gTool.mergeFocusIdx = at;
    gTool.mergeAnchorIdx = at;
}

// orig's MergeGrid::CopySelected: the selected pages to our clipboard, their
// names to the system one
static void MergeCopySelected() {
    VecReset(gTool.mergeClip);
    str::Builder text;
    for (const MergePageDlg& it : gTool.mergePages) {
        if (!it.selected) {
            continue;
        }
        VecAppend(gTool.mergeClip, it);
        Str path = gTool.mergeSources[it.sourceNo].path;
        text.Append(fmt(Tr("%s, page %d").s, path::GetBaseNameTemp(path), it.pageNo));
        text.AppendChar('\n');
    }
    if (len(gTool.mergeClip) == 0 || !gTool.win) {
        return;
    }
    str::FreePtr(&gTool.mergeClipText);
    gTool.mergeClipText = str::Dup(ToStrTemp(text));
    gp::ClipboardSetText(gTool.win->gpuiWin, ToGpui(gTool.mergeClipText));
}

// copies the selected pages and takes them out; nothing is selected after
static void MergeCutSelected() {
    MergeCopySelected();
    if (len(gTool.mergeClip) == 0) {
        return;
    }
    Vec<MergePageDlg>& items = gTool.mergePages;
    Vec<MergePageDlg> rest;
    int first = -1;
    for (int i = 0; i < len(items); i++) {
        if (!items[i].selected) {
            VecAppend(rest, items[i]);
        } else if (first < 0) {
            first = i;
        }
    }
    VecReset(items);
    for (const MergePageDlg& it : rest) {
        VecAppend(items, it);
    }
    gTool.mergeFocusIdx = limitValue(first, 0, std::max(0, len(items) - 1));
    gTool.mergeAnchorIdx = gTool.mergeFocusIdx;
}

// copies of the pages last copied or cut, selected, in front of item `at`
static void MergePastePages(int at) {
    Vec<MergePageDlg>& items = gTool.mergePages;
    for (MergePageDlg& it : items) {
        it.selected = false;
    }
    at = limitValue(at, 0, len(items));
    for (int i = 0; i < len(gTool.mergeClip); i++) {
        MergePageDlg it = gTool.mergeClip[i];
        it.selected = true;
        VecInsertAt(items, at + i, it);
    }
    gTool.mergeFocusIdx = at;
    gTool.mergeAnchorIdx = at;
    MergeEnsureVisible(at / std::max(1, gTool.mergeCols));
    StartMergeThumbs();
}

// the platform's line ends may differ from what we put there
static bool IsMergeClipText(Str text) {
    if (len(gTool.mergeClip) == 0 || len(text) == 0) {
        return false;
    }
    return str::Eq(str::ReplaceTemp(text, StrL("\r\n"), StrL("\n")), gTool.mergeClipText);
}

// PDF files copied in a file manager; ng: also a text of such paths, one a
// line, where gpui has no file list (Linux, macOS)
static void MergePdfPathsFromClipboard(const gp::ClipboardItem& item, StrVec& out) {
    Str all = FromGpui(item.HasExternalPaths() ? item.externalPaths : item.text);
    StrVec lines;
    Split(&lines, str::ReplaceTemp(all, StrL("\r"), StrL("")), StrL("\n"), true);
    for (Str line : lines) {
        if (str::EndsWithI(line, StrL(".pdf")) && file::Exists(line)) {
            out.Append(line);
        }
    }
}

static void AddMergeSourcesAt(const StrVec& paths, int at);

// orig's MergePdfWnd::Paste: the pages last copied or cut, else PDF files
// copied in a file manager; after the last selected page
static void MergePaste() {
    if (!gTool.win) {
        return;
    }
    int at = MergeInsertPosition();
    gp::ClipboardItem item = gp::ClipboardGetItem(gp::GetTempArena(), gTool.win->gpuiWin);
    if (IsMergeClipText(FromGpui(item.text))) {
        MergePastePages(at);
        return;
    }
    StrVec paths;
    MergePdfPathsFromClipboard(item, paths);
    AddMergeSourcesAt(paths, at);
}

// arrows move through the pages; with Shift they extend the selection
static void MergeMoveFocus(int idx, bool shift) {
    Vec<MergePageDlg>& items = gTool.mergePages;
    if (len(items) == 0) {
        return;
    }
    idx = limitValue(idx, 0, len(items) - 1);
    if (!shift) {
        MergeSelectOnly(idx);
    } else {
        gTool.mergeFocusIdx = idx;
        int first = std::min(gTool.mergeAnchorIdx, idx);
        int last = std::max(gTool.mergeAnchorIdx, idx);
        for (int i = 0; i < len(items); i++) {
            items[i].selected = i >= first && i <= last;
        }
    }
    MergeEnsureVisible(idx / std::max(1, gTool.mergeCols));
}

static bool ActionEnabled();

// orig's MergePdfWnd::AddPdf: its pages go in front of item `at`
static bool AddMergeSource(Str filePath, int at) {
    if (len(filePath) == 0) {
        return false;
    }
    EngineBase* engine = CreateEngineFromFile(filePath, nullptr, true);
    if (!engine || !EngineMupdfIsPdf(engine) || engine->PageCount() <= 0) {
        SafeEngineRelease(&engine);
        return false;
    }
    MergeSourceDlg source;
    source.path = str::Dup(filePath);
    source.password = str::Dup(EngineMupdfGetPassword(engine));
    source.pageCount = engine->PageCount();
    source.thumbStart = len(gTool.mergeThumbs->thumbs);
    source.color = kSourceColors[len(gTool.mergeSources) % dimofi(kSourceColors)];
    source.engine = engine;
    for (int i = 0; i < source.pageCount * kMergeRotations; i++) {
        VecAppend(gTool.mergeThumbs->thumbs, MergeThumb{});
    }
    VecAppend(gTool.mergeSources, source);
    MergeInsertPages(len(gTool.mergeSources) - 1, at);
    return true;
}

// adds the files one after another from `at` on, saying which could not be opened
static void AddMergeSourcesAt(const StrVec& paths, int at) {
    for (Str path : paths) {
        int n = len(gTool.mergePages);
        if (!AddMergeSource(path, at)) {
            MessageBoxWarning(gTool.win, fmt(Tr("Couldn't open '%s'").s, path::GetBaseNameTemp(path)), Tr("Merge PDF"));
            continue;
        }
        at += len(gTool.mergePages) - n;
    }
}

//   Insert 'b.pdf':
//   ( ) At the end
//   ( ) At the beginning
//   (o) After page [ 3 ] (of 12)
//                     [OK] [Cancel]
// ng: orig's AskInsertPosition runs a modal window and returns the answer;
// here the question replaces the dialog until OK / Cancel
static void MergeAskInsertPosition(const StrVec& paths) {
    if (len(paths) == 0 || !gTool.win) {
        return;
    }
    int afterPage = MergeSelectedCount() > 0 ? MergeInsertPosition() : 0;
    bool hasPage = afterPage > 0;
    gTool.mergeAskPaths.Reset();
    for (Str path : paths) {
        gTool.mergeAskPaths.Append(path);
    }
    gTool.mergeAskAt = hasPage ? MergeInsertAt::AfterPage : MergeInsertAt::End;
    if (!gTool.mergeAskEdit) {
        gTool.mergeAskEdit = new gp::InputState();
        gTool.mergeAskEdit->focus = gp::FocusHandleNew(gTool.win->gpuiWin ? gTool.win->gpuiWin->app : nullptr);
    }
    gp::InputSetValue(gTool.mergeAskEdit, ToGpui(fmt("%d", hasPage ? afterPage : len(gTool.mergePages))));
    gTool.mergeAskFocus = true;
    AppShellInvalidate(gTool.win);
}

static void MergeAskDone(bool ok) {
    if (len(gTool.mergeAskPaths) == 0) {
        return;
    }
    StrVec paths;
    for (Str path : gTool.mergeAskPaths) {
        paths.Append(path);
    }
    gTool.mergeAskPaths.Reset();
    if (ok) {
        int nItems = len(gTool.mergePages);
        int at = nItems;
        if (gTool.mergeAskAt == MergeInsertAt::Beginning) {
            at = 0;
        } else if (gTool.mergeAskAt == MergeInsertAt::AfterPage) {
            at = limitValue(ParseInt(InputTextTemp(gTool.mergeAskEdit)), 0, nItems);
        }
        AddMergeSourcesAt(paths, at);
    }
    AppShellInvalidate(gTool.win);
}

// PDFs dropped on the window while the dialog is up go into it, in front of
// the page nearest to the drop point (orig's MergeDropTarget). `pt` is in
// window dips, null when the platform doesn't say where the drop was: then
// they go where Add PDF... would put them
static bool IsMergeUp(MainWindow* win) {
    return gTool.visible && gTool.win == win && gTool.kind == PdfToolKind::Merge;
}

// Merge PDF is up and is drawn in that kind of window
static bool IsMergeUpIn(MainWindow* win, DropHost host) {
    return IsMergeUp(win) && (gTool.tw != nullptr) == (host == DropHost::ToolWindow);
}

bool PdfToolDialogIsMerge(MainWindow* win) {
    return IsMergeUpIn(win, DropHost::Frame);
}

bool PdfToolDialogOnDragOver(MainWindow* win, const PointF* pt, bool hasPdf, bool* accept, DropHost host) {
    *accept = false;
    if (!IsMergeUpIn(win, host)) {
        return false;
    }
    int before = -1;
    if (pt && hasPdf && len(gTool.mergeAskPaths) == 0) {
        before = MergeDropPositionAt(pt->x - gTool.mergeView.x, pt->y - gTool.mergeView.y);
    }
    *accept = before >= 0;
    // the pages' own drag owns the bar while it lasts
    if (!gTool.mergeDragging && gTool.mergeDropBefore != before) {
        gTool.mergeDropBefore = before;
        AppShellInvalidate(win);
    }
    return true;
}

bool PdfToolDialogOnDropFiles(MainWindow* win, const Str* paths, int n, const PointF* pt, DropHost host) {
    if (!IsMergeUpIn(win, host)) {
        return false;
    }
    if (!gTool.mergeDragging) {
        gTool.mergeDropBefore = -1;
    }
    StrVec pdfs;
    for (int i = 0; i < n; i++) {
        if (str::EndsWithI(paths[i], StrL(".pdf"))) {
            pdfs.Append(paths[i]);
        }
    }
    if (len(pdfs) > 0 && len(gTool.mergeAskPaths) == 0) {
        int before = pt ? MergeDropPositionAt(pt->x - gTool.mergeView.x, pt->y - gTool.mergeView.y) : -1;
        if (before < 0) {
            before = MergeInsertPosition();
        }
        logf("MergePdf: %d dropped file(s) go in front of item %d\n", len(pdfs), before);
        AddMergeSourcesAt(pdfs, before);
    }
    AppShellInvalidate(win);
    return true;
}

// Writes the pages not removed to destPath, which may be one of the sources:
// the result goes to a temp file first
static bool MergeSaveTo(Str destPath) {
    Vec<PdfMergeSource> sources;
    Vec<PdfMergePage> pages;
    for (const MergePageDlg& page : gTool.mergePages) {
        if (!page.removed) {
            VecAppend(pages, PdfMergePage{page.sourceNo, page.pageNo, page.rotation});
        }
    }
    if (len(destPath) == 0 || len(pages) == 0) {
        return false;
    }
    for (const MergeSourceDlg& source : gTool.mergeSources) {
        VecAppend(sources, PdfMergeSource{source.path, source.password});
    }

    // unsaved changes of the document (annotations) are merged from a copy
    TempStr sourceCopy;
    EngineBase* docEngine = gTool.mergeSources[0].engine;
    if (docEngine && EngineHasUnsavedAnnotations(docEngine)) {
        sourceCopy = GetTempFilePathTemp(StrL("merge-source"));
        if (len(sourceCopy) == 0 || !EngineMupdfSaveCopy(docEngine, sourceCopy)) {
            return false;
        }
        sources[0].path = sourceCopy;
    }

    TempStr tmpPath = GetTempFilePathTemp(StrL("merge"));
    bool ok = len(tmpPath) > 0 && EngineMupdfMergePdfs(sources, pages, tmpPath);
    if (ok) {
        WindowTab* tab = FindTabByFilePath(destPath);
        if (tab) {
            // it's reloaded below; the file watcher needn't
            tab->ignoreNextAutoReload = true;
        }
        Str data = file::ReadFile(tmpPath);
        ok = len(data) > 0 && file::WriteFile(destPath, data);
        str::Free(data);
    }
    if (len(tmpPath) > 0) {
        file::Delete(tmpPath);
    }
    if (len(sourceCopy) > 0) {
        file::Delete(sourceCopy);
    }
    logf("MergeSaveTo: %d pages from %d files to '%s', ok: %d\n", len(pages), len(sources), destPath, (int)ok);
    return ok;
}

// closes the dialog and shows the saved file: reloaded where it's open, else in a new tab
static void MergeAfterSave(Str path) {
    MainWindow* win = gTool.win;
    TempStr savedPath = str::DupTemp(path);
    ClosePdfToolDialog();
    WindowTab* tab = FindTabByFilePath(savedPath);
    if (tab) {
        SelectTabInWindow(tab);
        ReloadDocument(tab->win, false);
        return;
    }
    if (IsMainWindowValidAndNotClosing(win)) {
        LoadDocument(win, savedPath);
    }
}

static void MergeSaveAndClose(Str destPath) {
    TempStr path = str::DupTemp(destPath);
    if (!MergeSaveTo(path)) {
        MessageBoxWarning(gTool.win, fmt(Tr("Couldn't save '%s'").s, path), Tr("Merge PDF"));
        return;
    }
    MergeAfterSave(path);
}

// Save: over the document itself
static void MergePdfDoIt() {
    MergeSaveAndClose(gTool.srcPath);
}

static void OnMergeSaveAsPicked(SavePathArgs* args) {
    // the path is the dialog's (a temp string): copied, never freed
    TempStr picked = str::DupTemp(args->path);
    if (!gTool.visible || gTool.kind != PdfToolKind::Merge || len(picked) == 0) {
        return;
    }
    MergeSaveAndClose(picked);
}

// Drives the Merge PDF dialog and reports it. action: "open", "add" (arg: a
// PDF path, n: in front of that item, -1: where Add PDF... puts it), "askpos"
// (arg: a PDF path; asks where to add it, like Add PDF...), "addprompt" (ng:
// the Add PDF... button), "answer" (ng: n: 1 OK, 0 Cancel of that
// question), "asktext" (the After page field), "move" (arg: 0-based
// items like "0,2", n: in front of that item), "remove" / "restore" (arg: items),
// "rotate" (arg: items, n: degrees), "select" (ng: arg: items), "save" /
// "saveas" (arg: the path), "close" or "" (report only). Reports the items as
// src:page[@rotation] (r: removed, s: selected).
// ng: for the platforms whose picker answers later (AppShellPickFileAsync)
static void OnMergePicked(MainWindow* win, Str path) {
    if (!gTool.visible || gTool.win != win || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    StrVec paths;
    paths.Append(path);
    MergeAskInsertPosition(paths);
}

// orig's MergePdfWnd::OnAdd. ng: the prompt runs a message loop, so it is
// shown from the ui task queue, after gpui has unwound from the click
static void MergeAddPromptNow(MainWindow* win) {
    if (!gTool.visible || gTool.win != win || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (AppShellPickFileAsync(win, Tr("Open"), StrL("PDF documents\1*.pdf\1"), MkFunc1(OnMergePicked, win))) {
        return;
    }
    StrVec paths;
    if (AppShellPromptForFiles(win, StrL("PDF documents\1*.pdf\1"), &paths) && gTool.visible) {
        MergeAskInsertPosition(paths);
    }
    AppShellInvalidate(win);
}

static void MergeAddPrompt() {
    uitask::Post(MkFunc0(MergeAddPromptNow, gTool.win), "MergeAddPdf");
}

static TempStr MergeInfoTemp();

// the legend's entries that fit in it, as of the last frame; reported to tests
static int MergeLegendShownCount() {
    if (len(gTool.mergeSources) < 2) {
        return 0;
    }
    const gp::Bounds& box = gTool.mergeLegendView;
    int n = 0;
    for (const gp::Bounds& b : gTool.mergeLegendItems) {
        bool fits = b.w > 0 && b.x >= box.x && b.Right() <= box.Right() + 0.5f && b.Bottom() <= box.Bottom() + 0.5f;
        n += fits ? 1 : 0;
    }
    return n;
}

TempStr MergePdfResultTemp(Str action, Str arg, int n, int* exitCodeOut) {
    auto finish = [exitCodeOut](int code, TempStr res) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return res;
    };
    if (str::Eq(action, StrL("open"))) {
        MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
        ShowMergePdfDialog(win);
    }
    if (!gTool.visible || gTool.kind != PdfToolKind::Merge) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-dialog")));
    }
    Vec<MergePageDlg>& items = gTool.mergePages;
    auto selectItems = [&]() {
        StrVec parts;
        Split(&parts, arg, StrL(","), true);
        for (MergePageDlg& it : items) {
            it.selected = false;
        }
        for (Str part : parts) {
            int idx = ParseInt(part);
            if (idx >= 0 && idx < len(items)) {
                items[idx].selected = true;
            }
        }
    };
    str::Builder out;
    if (str::Eq(action, StrL("add"))) {
        out.Append(fmt("added=%d ", (int)AddMergeSource(arg, n < 0 ? MergeInsertPosition() : n)));
    } else if (str::Eq(action, StrL("addprompt"))) {
        MergeAddPrompt();
    } else if (str::Eq(action, StrL("askpos"))) {
        StrVec paths;
        paths.Append(arg);
        MergeAskInsertPosition(paths);
    } else if (str::Eq(action, StrL("answer"))) {
        MergeAskDone(n != 0);
    } else if (str::Eq(action, StrL("asktext"))) {
        // the page number typed into Add PDF's "After page" field
        if (gTool.mergeAskEdit) {
            gp::InputSetValue(gTool.mergeAskEdit, ToGpui(arg));
            gTool.mergeAskAt = MergeInsertAt::AfterPage;
        }
    } else if (str::Eq(action, StrL("move"))) {
        selectItems();
        MergeMoveSelected(n);
    } else if (str::Eq(action, StrL("select"))) {
        selectItems();
    } else if (str::Eq(action, StrL("remove")) || str::Eq(action, StrL("restore"))) {
        selectItems();
        MergeSetRemoved(str::Eq(action, StrL("remove")));
    } else if (str::Eq(action, StrL("rotate"))) {
        selectItems();
        MergeRotate(n);
    } else if (str::Eq(action, StrL("copy"))) {
        selectItems();
        MergeCopySelected();
    } else if (str::Eq(action, StrL("cut"))) {
        selectItems();
        MergeCutSelected();
    } else if (str::Eq(action, StrL("paste"))) {
        selectItems();
        MergePaste();
    } else if (str::Eq(action, StrL("save")) || str::Eq(action, StrL("saveas"))) {
        TempStr path = str::DupTemp(str::Eq(action, StrL("save")) ? gTool.srcPath : arg);
        bool ok = MergeSaveTo(path);
        TempStr res = fmt("OK saved=%d", (int)ok);
        if (ok) {
            MergeAfterSave(path);
        }
        return finish(0, res);
    } else if (str::Eq(action, StrL("close"))) {
        ClosePdfToolDialog();
        return finish(0, str::DupTemp(StrL("OK closed")));
    } else if (str::Eq(action, StrL("key"))) {
        // a key in the grid: n is the virtual key, arg "shift" / "ctrl"
        PdfToolDialogOnKeyDown(gTool.win, n, str::Eq(arg, StrL("ctrl")), str::Eq(arg, StrL("shift")));
    } else if (str::Eq(action, StrL("drop"))) {
        // a file dropped at a point of the grid: arg is "path*x*y" (grid dips)
        StrVec parts;
        Split(&parts, arg, StrL("*"));
        if (len(parts) == 3) {
            Str path = parts[0];
            PointF pt{gTool.mergeView.x + (float)ParseInt(parts[1]), gTool.mergeView.y + (float)ParseInt(parts[2])};
            PdfToolDialogOnDropFiles(gTool.win, &path, 1, &pt);
        }
    }
    AppShellInvalidate(gTool.win);
    int hwnd = 0;
    int focused = 0;
#if OS_WIN
    HWND native = ToolWindowHwnd(gTool.tw);
    hwnd = (int)(intptr_t)native;
    int dpi = native ? (int)GetDpiForWindow(native) : 96;
    focused = (native && GetFocus() == native) ? 1 : 0;
#else
    int dpi = AppShellWindowDpi(gTool.win);
#endif
    if (dpi < 96) {
        dpi = 96;
    }
    // orig's line, plus the Add PDF question (it is not a window of its own)
    out.Append(fmt("hwnd=%d focused=%d items=", hwnd, focused));
    for (int i = 0; i < len(items); i++) {
        const MergePageDlg& it = items[i];
        out.Append(fmt(i == 0 ? "%d:%d" : ",%d:%d", it.sourceNo, it.pageNo));
        if (it.rotation != 0) {
            out.Append(fmt("@%d", it.rotation));
        }
        if (it.removed) {
            out.AppendChar('r');
        }
        if (it.selected) {
            out.AppendChar('s');
        }
    }
    out.Append(fmt(" canSave=%d rects=", (int)ActionEnabled()));
    if (gTool.mergeView.w > 1.f && gTool.mergeView.h > 1.f) {
        auto toPx = [&](float dip) { return MulDiv((int)lroundf(dip), dpi, 96); };
        float ox = gTool.mergeView.x;
        float oy = gTool.mergeView.y;
        for (int i = 0; i < len(items); i++) {
            RectF r = MergeCellRect(i);
            if (r.y < 0 || r.y + r.dy > gTool.mergeView.h) {
                continue;
            }
            RectF b = MergeCornerBtnRect(i);
            out.Append(fmt("%d:%d,%d,%d,%d:%d,%d,%d,%d;", i, toPx(ox + r.x), toPx(oy + r.y), toPx(r.dx), toPx(r.dy),
                           toPx(ox + b.x), toPx(oy + b.y), toPx(b.dx), toPx(b.dy)));
        }
    }
    Str ask = StrL("");
    if (len(gTool.mergeAskPaths) > 0 && gTool.mergeAskEdit) {
        ask = InputTextTemp(gTool.mergeAskEdit);
    }
    out.Append(fmt(" asking=%d ask=%s", (int)(len(gTool.mergeAskPaths) > 0), ask));
    out.Append(fmt(" info=\"%s\"", MergeInfoTemp()));
    out.Append(
        fmt(" legend=%d/%d", MergeLegendShownCount(), len(gTool.mergeSources) > 1 ? len(gTool.mergeSources) : 0));
    return finish(0, fmt("OK %s", ToStrTemp(out)));
}

// orig's UpdateButton(): what makes the action button clickable
static bool ActionEnabled() {
    if (len(DestPathTemp()) == 0) {
        return false;
    }
    if (IsPageRangeTool() || gTool.kind == PdfToolKind::ExtractText) {
        Vec<int> parsed;
        bool isExtract = gTool.kind != PdfToolKind::DeletePages;
        return PageRangeOk(InputTextTemp(gTool.pagesEdit), gTool.pageCount, isExtract, parsed);
    }
    if (gTool.kind == PdfToolKind::Encrypt) {
        // there is nothing to encrypt with until a password is typed
        return len(InputTextTemp(gTool.passwordEdit)) > 0;
    }
    if (gTool.kind == PdfToolKind::ConvertEpubToPdf) {
        return EbookPdfFontSizeFromText(InputTextTemp(gTool.dpiEdit)) > 0;
    }
    if (gTool.kind == PdfToolKind::ConvertToImages) {
        if (SelectedDpi() < 1) {
            return false;
        }
        if (gTool.pagesMode != PagesMode::Custom) {
            return true;
        }
        Vec<int> parsed;
        return ParseDeletePages(InputTextTemp(gTool.pagesEdit), gTool.pageCount, parsed);
    }
    if (gTool.kind == PdfToolKind::SaveSelectionAsImage) {
        DisplayModel* dm = gTool.win ? gTool.win->AsFixed() : nullptr;
        EngineBase* engine = dm ? dm->GetEngine() : nullptr;
        if (!engine) {
            return false;
        }
        int dpi = SelectedDpi();
        if (dpi < 1) {
            return false;
        }
        int w = 0;
        int h = 0;
        EstimateSelectionPx(gTool.selRect, SaveSelectionZoom(engine, (float)dpi), w, h);
        return SaveSelectionSizeOk(w, h);
    }
    if (gTool.kind == PdfToolKind::Merge) {
        for (const MergePageDlg& page : gTool.mergePages) {
            if (!page.removed) {
                return true;
            }
        }
        return false;
    }
    return true;
}

static void PdfToolDoIt() {
    if (!gTool.visible || !ActionEnabled()) {
        return;
    }
    switch (gTool.kind) {
        case PdfToolKind::Bake:
            PdfBakeDoIt();
            break;
        case PdfToolKind::ExtractText:
            PdfExtractTextDoIt();
            break;
        case PdfToolKind::Compress:
            PdfCleanDoIt(false);
            break;
        case PdfToolKind::Decompress:
            PdfCleanDoIt(true);
            break;
        case PdfToolKind::DeletePages:
        case PdfToolKind::ExtractPages:
            PdfDeletePageDoIt();
            break;
        case PdfToolKind::Encrypt:
            PdfEncryptDoIt();
            break;
        case PdfToolKind::Decrypt:
            PdfDecryptDoIt();
            break;
        case PdfToolKind::ConvertToPdf:
            ConvertToPdfDoIt();
            break;
        case PdfToolKind::ConvertEpubToPdf:
            ConvertEpubToPdfDoIt();
            break;
        case PdfToolKind::ConvertToImages:
            ConvertPdfToImagesDoIt();
            break;
        case PdfToolKind::SaveSelectionAsImage:
            SaveSelectionAsImageDoIt();
            break;
        case PdfToolKind::Merge:
            MergePdfDoIt();
            break;
    }
}

static void MergeSaveAsPrompt();

static void ToolOnEnter() {
    // orig's Merge PDF window: Save As... is the default button
    if (gTool.kind == PdfToolKind::Merge) {
        if (ActionEnabled()) {
            MergeSaveAsPrompt();
        }
        return;
    }
    PdfToolDoIt();
}

bool PdfToolDialogOnEnter() {
    if (!IsPdfToolDialogVisible()) {
        return false;
    }
    ToolOnEnter();
    return true;
}

// --- events -----------------------------------------------------------------

void PdfToolView::OnAction(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ToolScope scope(ToolForCx(cx));
    gp::Notify(cx);
    PdfToolDoIt();
}

void PdfToolView::OnCancel(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ToolScope scope(ToolForCx(cx));
    ClosePdfToolDialog();
    gp::Notify(cx);
}

void PdfToolView::OnInput(PdfToolView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    ToolScope scope(ToolForCx(cx));
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    gp::Notify(cx);
    PdfToolDoIt();
}

void PdfToolView::OnPagesMode(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t mode) {
    ToolScope scope(ToolForCx(cx));
    gTool.pagesMode = (PagesMode)mode;
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

void PdfToolView::OnMergeAdd(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*) {
    MergeAddPrompt();
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

void PdfToolView::OnMergeScroll(PdfToolView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    if (MergeScrollTo(ev->offsetY)) {
        gTool.mergeHoverIdx = -1;
    }
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

// orig's MergeGrid::OnGridMouseDown
void PdfToolView::OnMergeGridDown(PdfToolView*, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    float x = ev->x - ev->el.x;
    float y = ev->y - ev->el.y;
    gTool.mergeLast = {x, y};
    // the scrollbar strip is gpui's own
    bool onScrollbar = MergeMaxScrollY() > 0 && x >= gTool.mergeView.w - gp::kScrollbarBandW;
    if (onScrollbar) {
        return;
    }
    Vec<MergePageDlg>& items = gTool.mergePages;
    int idx = MergeItemAt(x, y);
    if (ev->button == gp::MouseButton::Right) {
        // right button: the menu acts on the selection, which includes the page
        if (idx >= 0 && !items[idx].selected) {
            MergeSelectOnly(idx);
            gp::Notify(cx);
            AppShellInvalidate(gTool.win);
        }
        return;
    }
    if (ev->button != gp::MouseButton::Left) {
        return;
    }
    bool isCtrl = ev->modifiers.control || ev->modifiers.platform;
    bool isShift = ev->modifiers.shift;
    gTool.mergePressPt = {ev->x, ev->y};
    if (idx >= 0 && MergeCornerBtnRect(idx).Contains(PointF{x, y})) {
        gTool.mergePress = MergePress::CornerBtn;
        gTool.mergePressIdx = idx;
        return;
    }
    if (idx < 0) {
        // a rubber band; Ctrl adds what it touches to the selection
        gTool.mergePress = MergePress::Band;
        gTool.mergeBandFrom = {x, y + gTool.mergeScrollY};
        gTool.mergeBandTo = gTool.mergeBandFrom;
        VecReset(gTool.mergeBandBase);
        for (MergePageDlg& it : items) {
            VecAppend(gTool.mergeBandBase, (u8)(isCtrl && it.selected ? 1 : 0));
        }
        MergeUpdateBand();
        gp::Notify(cx);
        AppShellInvalidate(gTool.win);
        return;
    }
    gTool.mergePress = MergePress::Page;
    gTool.mergePressIdx = idx;
    gTool.mergeFocusIdx = idx;
    if (isShift) {
        int first = std::min(gTool.mergeAnchorIdx, idx);
        int last = std::max(gTool.mergeAnchorIdx, idx);
        for (int i = 0; i < len(items); i++) {
            bool inRange = i >= first && i <= last;
            items[i].selected = inRange || (isCtrl && items[i].selected);
        }
    } else if (isCtrl) {
        items[idx].selected = !items[idx].selected;
        gTool.mergeAnchorIdx = idx;
    } else if (!items[idx].selected) {
        // a plain click on a selected page may start dragging all of them, so
        // only on release does it select just that page
        MergeSelectOnly(idx);
    }
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

// orig's MergeGrid::OnGridMouseMove, the part without a button down. ng: gpui
// gives a move to the topmost element under the mouse only, so nothing inside
// the grid may be a hit target of its own
void PdfToolView::OnMergeGridMove(PdfToolView*, gp::Ctx* cx, const gp::MouseMoveEvent* ev) {
    float x = ev->x - ev->el.x;
    float y = ev->y - ev->el.y;
    if (gTool.mergePress != MergePress::None) {
        // ng: not ev->Dragging(), which gpui's Windows backend never sets
        if (cx->win && cx->win->mouseDown) {
            return;
        }
        // the button went up where we could not see it
        MergeEndPress();
        gp::Notify(cx);
        AppShellInvalidate(gTool.win);
    }
    int idx = MergeItemAt(x, y);
    bool onBtn = idx >= 0 && MergeCornerBtnRect(idx).Contains(PointF{x, y});
    if (idx == gTool.mergeHoverIdx && onBtn == gTool.mergeHoverBtn) {
        return;
    }
    gTool.mergeHoverIdx = idx;
    gTool.mergeHoverBtn = onBtn;
    // orig's MergeGrid::OnGridTooltip
    if (idx < 0) {
        HoverTooltipHide(cx);
    } else {
        const MergePageDlg& item = gTool.mergePages[idx];
        Str tip = item.removed ? Tr("Restore page") : Tr("Remove page");
        if (!onBtn) {
            tip = fmt(Tr("%s, page %d").s, path::GetBaseNameTemp(gTool.mergeSources[item.sourceNo].path), item.pageNo);
        }
        RectF r = onBtn ? MergeCornerBtnRect(idx) : MergeCellRect(idx);
        gp::Bounds at{gTool.mergeView.x + r.x, gTool.mergeView.y + r.y, r.dx, r.dy};
        HoverTooltipShow(cx, tip, at);
    }
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

// the part with the left button down: gpui keeps sending these to the element
// the press started on wherever the mouse goes, which is orig's capture
void PdfToolView::OnMergeGridDrag(PdfToolView*, gp::Ctx* cx, const gp::DragMoveEvent* dragEv) {
    const gp::MouseMoveEvent* ev = &dragEv->event;
    float x = ev->x - gTool.mergeView.x;
    float y = ev->y - gTool.mergeView.y;
    // orig: SetCapture synthesizes a move at the real cursor: not a gesture
    if (gTool.mergePress == MergePress::None || !ev->Dragging()) {
        return;
    }
    gTool.mergeLast = {x, y};
    if (gTool.mergePress == MergePress::Band) {
        gTool.mergeBandTo = {x, y + gTool.mergeScrollY};
        MergeUpdateBand();
        gp::Notify(cx);
        AppShellInvalidate(gTool.win);
        return;
    }
    if (gTool.mergePress != MergePress::Page) {
        return;
    }
    if (!gTool.mergeDragging) {
        float dx = std::abs(ev->x - gTool.mergePressPt.x);
        float dy = std::abs(ev->y - gTool.mergePressPt.y);
        if (dx <= kMergeDragThreshold && dy <= kMergeDragThreshold) {
            return;
        }
        gTool.mergeDragging = true;
        if (!gTool.mergePages[gTool.mergePressIdx].selected) {
            MergeSelectOnly(gTool.mergePressIdx);
        }
    }
    gTool.mergeDropBefore = MergeDropPositionAt(x, y);
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

// orig's MergeGrid::OnGridMouseUp; also when the button goes up outside the grid
void PdfToolView::OnMergeGridUp(PdfToolView*, gp::Ctx* cx, const gp::MouseUpEvent* ev) {
    if (ev->button != gp::MouseButton::Left || gTool.mergePress == MergePress::None) {
        return;
    }
    float x = ev->x - gTool.mergeView.x;
    float y = ev->y - gTool.mergeView.y;
    MergePress what = gTool.mergePress;
    int idx = gTool.mergePressIdx;
    bool wasDragging = gTool.mergeDragging;
    int before = gTool.mergeDropBefore;
    bool modifiers = ev->modifiers.control || ev->modifiers.platform || ev->modifiers.shift;
    MergeEndPress();
    Vec<MergePageDlg>& items = gTool.mergePages;
    if (what == MergePress::CornerBtn) {
        // only if released over the button it went down on
        if (MergeItemAt(x, y) == idx && MergeCornerBtnRect(idx).Contains(PointF{x, y})) {
            items[idx].removed = !items[idx].removed;
        }
    } else if (what == MergePress::Page) {
        if (wasDragging) {
            if (before >= 0) {
                MergeMoveSelected(before);
            }
        } else if (!modifiers && idx >= 0 && idx < len(items)) {
            MergeSelectOnly(idx);
        }
    }
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

void PdfToolView::OnMergeSetRemoved(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t removed) {
    MergeSetRemoved(removed != 0);
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

enum {
    kMenuRemove = 1,
    kMenuRestore,
    kMenuRotateLeft,
    kMenuRotateRight,
    kMenuSelectAll,
    kMenuCut,
    kMenuCopy,
    kMenuPaste,
};

static uint32_t ActMergeMenu() {
    static uint32_t act = gp::ActionOf(GStrL("sumatra::MergeMenu"));
    return act;
}

void PdfToolView::OnMergeMenu(PdfToolView*, gp::Ctx* cx, const gp::ActionEvent* ev) {
    int cmd = (int)ev->arg;
    if (cmd == kMenuRemove || cmd == kMenuRestore) {
        MergeSetRemoved(cmd == kMenuRemove);
    } else if (cmd == kMenuRotateLeft || cmd == kMenuRotateRight) {
        MergeRotate(cmd == kMenuRotateRight ? 90 : -90);
    } else if (cmd == kMenuSelectAll) {
        MergeSelectAll();
    } else if (cmd == kMenuCut) {
        MergeCutSelected();
    } else if (cmd == kMenuCopy) {
        MergeCopySelected();
    } else if (cmd == kMenuPaste) {
        MergePaste();
    }
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

void PdfToolView::OnMergeSaveAs(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*) {
    gp::Notify(cx);
    MergeSaveAsPrompt();
}

static void MergeSaveAsPrompt() {
    auto* args = new SavePathArgs();
    args->win = gTool.win;
    args->title = str::Dup(Tr("Save As"));
    args->initialPath = str::Dup(MakeUniqueFilePathTemp(gTool.srcPath));
    args->defExt = str::Dup(StrL(".pdf"));
    args->filter = str::Dup(StrL("PDF documents\1*.pdf\1"));
    args->onDone = MkFunc1Void<SavePathArgs*>(OnMergeSaveAsPicked);
    ShowSavePathDialog(args);
}

void PdfToolView::OnMergeAskAt(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t at) {
    gTool.mergeAskAt = (MergeInsertAt)at;
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

// typing a page number picks "After page"
void PdfToolView::OnMergeAskInput(PdfToolView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind == gp::InputEventKind::PressEnter) {
        MergeAskDone(true);
    } else if (ev->kind == gp::InputEventKind::Change) {
        gTool.mergeAskAt = MergeInsertAt::AfterPage;
    }
    gp::Notify(cx);
    AppShellInvalidate(gTool.win);
}

void PdfToolView::OnMergeAskOk(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*) {
    MergeAskDone(true);
    gp::Notify(cx);
}

void PdfToolView::OnMergeAskCancel(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*) {
    MergeAskDone(false);
    gp::Notify(cx);
}

static bool MergeContextMenuFromKey(MainWindow* win, gp::Ctx* cx);

// the frame's Apps key
bool PdfToolDialogContextMenuFromKey(MainWindow* win, gp::Ctx* cx) {
    return !gTool.tw && MergeContextMenuFromKey(win, cx);
}

// orig's OnGridKeyDown, VK_APPS: ShowContextMenu(), which opens at the cursor
static bool MergeContextMenuFromKey(MainWindow* win, gp::Ctx* cx) {
    if (!gTool.visible || gTool.win != win || gTool.kind != PdfToolKind::Merge || !cx->win) {
        return false;
    }
    if (len(gTool.mergeAskPaths) > 0 || len(gTool.mergePages) == 0) {
        return false;
    }
    const gpui::Bounds& b = gTool.mergePagesBounds;
    float x = limitValue(cx->win->mouseX - b.x, 0.f, std::max(0.f, b.w));
    float y = limitValue(cx->win->mouseY - b.y, 0.f, std::max(0.f, b.h));
    OpenPopupMenuAt(cx, gTool.mergePopup, x, y);
    return true;
}

static bool MergeOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift);

bool PdfToolDialogIsPagesKind() {
    return gTool.visible && (gTool.kind == PdfToolKind::DeletePages || gTool.kind == PdfToolKind::ExtractPages);
}

bool PdfToolDialogEndDrag() {
    if (!gTool.visible || gTool.kind != PdfToolKind::Merge || gTool.mergePress == MergePress::None) {
        return false;
    }
    MergeEndPress();
    AppShellInvalidate(gTool.win);
    return true;
}

// the frame's keys
bool PdfToolDialogOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift) {
    return !gTool.tw && MergeOnKeyDown(win, vk, ctrl, shift);
}

// arrows move through the pages, Delete removes (or restores) the selected
// ones, Ctrl + A selects all (orig's MergeGrid::OnGridKeyDown)
static bool MergeOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift) {
    if (!gTool.visible || gTool.win != win || gTool.kind != PdfToolKind::Merge) {
        return false;
    }
    if (len(gTool.mergeAskPaths) > 0) {
        if (vk == VK_ESCAPE) {
            MergeAskDone(false);
            return true;
        }
        return false;
    }
    // pasting works without pages too
    if (ctrl && vk == 'V') {
        MergePaste();
        AppShellInvalidate(win);
        return true;
    }
    int n = len(gTool.mergePages);
    if (n == 0) {
        return false;
    }
    int cols = std::max(1, gTool.mergeCols);
    int pageStep = std::max(1, (int)(MergeViewportDy() / MergeItemDy())) * cols;
    switch (vk) {
        case VK_LEFT:
            MergeMoveFocus(gTool.mergeFocusIdx - 1, shift);
            break;
        case VK_RIGHT:
            MergeMoveFocus(gTool.mergeFocusIdx + 1, shift);
            break;
        case VK_UP:
            MergeMoveFocus(gTool.mergeFocusIdx - cols, shift);
            break;
        case VK_DOWN:
            MergeMoveFocus(gTool.mergeFocusIdx + cols, shift);
            break;
        case VK_PRIOR:
            MergeMoveFocus(gTool.mergeFocusIdx - pageStep, shift);
            break;
        case VK_NEXT:
            MergeMoveFocus(gTool.mergeFocusIdx + pageStep, shift);
            break;
        case VK_HOME:
            MergeMoveFocus(0, shift);
            break;
        case VK_END:
            MergeMoveFocus(n - 1, shift);
            break;
        case VK_DELETE:
            // all removed already: restore them
            MergeSetRemoved(MergeSelectedRemovedCount() < MergeSelectedCount());
            break;
        // [ and ], as in the document window
        case VK_OEM_4:
            MergeRotate(-90);
            break;
        case VK_OEM_6:
            MergeRotate(90);
            break;
        case 'A':
            if (!ctrl) {
                return false;
            }
            MergeSelectAll();
            break;
        case 'C':
            if (!ctrl) {
                return false;
            }
            MergeCopySelected();
            break;
        case 'X':
            if (!ctrl) {
                return false;
            }
            MergeCutSelected();
            break;
        default:
            return false;
    }
    AppShellInvalidate(win);
    return true;
}

// the picked path comes back here; orig writes it straight into the edit.
// `tool`: the one whose "..." asked, which may be gone by now
static void OnDestPathPicked(PdfToolDlg* tool, SavePathArgs* args) {
    if (tool != &gMainTool && !IsWinTool(tool)) {
        return;
    }
    ToolScope scope(tool);
    if (!gTool.visible || !gTool.destEdit || len(args->path) == 0) {
        return;
    }
    // the path is the dialog's (a temp string): copied, never freed
    TempStr picked = str::DupTemp(args->path);
    if (gTool.kind == PdfToolKind::ConvertToImages) {
        if (str::Contains(picked, StrL("{N}"))) {
            picked = str::ReplaceTemp(picked, StrL("{N}"), StrL("<N>"));
        } else if (!PathHasPagePlaceholder(picked)) {
            picked = EnsurePagePlaceholderTemp(picked, true);
        }
    }
    gp::InputSetValue(gTool.destEdit, ToGpui(picked));
    if (HasFormatDropDown() && gTool.win && gTool.win->gpuiWin) {
        // Windows' dialog says which filter was chosen
        int filterIdx = args->filterIndex - 1;
        if (filterIdx >= 0 && filterIdx < ConvertImageFormatCount()) {
            gTool.ddFormat.SetSel(gTool.win->gpuiWin->app, filterIdx);
            SetDestExtFromFormat();
        } else {
            SyncFormatFromPath(gTool.win->gpuiWin->app, picked);
        }
    }
    AppShellInvalidate(gTool.win);
    ToolWindowInvalidate(gTool.dlgTw);
}

// orig's OnUseNewNames: free names for the files Convert would overwrite
void PdfToolView::OnUseNewNames(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ToolScope scope(ToolForCx(cx));
    gp::Notify(cx);
    Vec<int> pages;
    if (!gTool.destEdit || !SelectedImagePages(pages)) {
        return;
    }
    gp::InputSetValue(gTool.destEdit, ToGpui(UniqueImagesTemplateTemp(DestPathTemp(), pages)));
    AppShellInvalidate(gTool.win);
    ToolWindowInvalidate(gTool.dlgTw);
}

void PdfToolView::OnBrowse(PdfToolView*, gp::Ctx* cx, const gp::ClickEvent*) {
    ToolScope scope(ToolForCx(cx));
    gp::Notify(cx);
    Str ext = gTool.browseExt;
    if (HasFormatDropDown()) {
        ext = kConvertImageFormats[SelectedFormatIdx()].ext;
    }
    auto* args = new SavePathArgs();
    args->win = gTool.win;
    args->title = str::Dup(Tr("Save As"));
    args->initialPath = str::Dup(DestPathTemp());
    args->defExt = str::Dup(ext);
    if (HasFormatDropDown()) {
        args->filter = str::Dup(StrL("PNG\1*.png\1JPEG\1*.jpg;*.jpeg\1BMP\1*.bmp\1GIF\1*.gif\1All Files\1*.*\1"));
        args->filterIndex = SelectedFormatIdx() + 1;
        // < and > are illegal in Windows filenames; show {N} in the save dialog
        TempStr shown = str::ReplaceTemp(DestPathTemp(), StrL("<N>"), StrL("{N}"));
        if (str::Contains(shown, StrL("<n>"))) {
            shown = str::ReplaceTemp(shown, StrL("<n>"), StrL("{N}"));
        }
        args->nativeFile = str::Dup(shown);
    } else if (gTool.kind == PdfToolKind::ExtractText) {
        args->filter = str::Dup(StrL("Text Files\1*.txt\1All Files\1*.*\1"));
    } else {
        args->filter = str::Dup(StrL("PDF Files\1*.pdf\1All Files\1*.*\1"));
    }
    args->onDone = MkFunc1(OnDestPathPicked, gCurTool);
    ShowSavePathDialog(args);
}

// --- showing ----------------------------------------------------------------

static gp::InputState* NewInput(MainWindow* win, Str text) {
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    if (len(text) > 0) {
        gp::InputSetValue(s, ToGpui(text));
    }
    return s;
}

// orig's PdfDeletePageDialog::PreTranslate: Page Up / Page Down turn the
// document's pages while the page-range dialog has the keyboard
static bool ToolDlgWindowOnKey(MainWindow* win, int vk, bool ctrl, bool shift, bool alt) {
    PdfToolDlg* tool = ToolForDlgWindow();
    if (!tool) {
        return false;
    }
    ToolScope scope(tool);
    if (ctrl || shift || alt || !IsPageRangeTool() || (vk != VK_NEXT && vk != VK_PRIOR)) {
        return false;
    }
    ExecuteCmd(win, vk == VK_NEXT ? (int)CmdGoToNextPage : (int)CmdGoToPrevPage);
    return true;
}

// the width (dips) the user last dragged a tool dialog to; the next one
// opens that wide, as Advanced Settings does
static float gToolLastDx = 0;

static void ToolDlgOnResized(float clientDx) {
    gToolLastDx = clientDx;
}

// orig's CalcDlgWidth: wide enough for the source path, at least kToolDlgMinDx
// and at most 80% of the screen (a longer path is cut with an ellipsis)
static float ToolWinClientDx(MainWindow* win, Str srcPath) {
    constexpr float kMinDx = kToolDlgMinDx;
    constexpr float kPathExtraDx = 2 * kToolDlgPadDx + 32;
    int dpi = std::max(AppShellWindowDpi(win), 96);
    float pathDx = (float)PlatformFontMeasureText(GetDefaultGuiFont(), srcPath).dx * 96.f / (float)dpi;
    float dx = std::max(pathDx + kPathExtraDx, kMinDx);
    Rect mon = AppShellMonitorRect(win);
    if (mon.dx > 0) {
        float screenDx = (float)mon.dx * 96.f / (float)dpi;
        dx = std::min(dx, (float)(int)(screenDx * 80 / 100));
    }
    return dx;
}

// Esc, the close box, the main window going
static void ToolDlgWindowClose() {
    PdfToolDlg* tool = ToolForDlgWindow();
    if (!tool) {
        return;
    }
    ToolScope scope(tool);
    ClosePdfToolDialog();
}

static void ToolOnEnter();

static void ToolDlgWindowOnEnter() {
    PdfToolDlg* tool = ToolForDlgWindow();
    if (!tool) {
        return;
    }
    ToolScope scope(tool);
    ToolOnEnter();
}

// how TestToolWindow names a tool's window
static const char* ToolWindowName(PdfToolKind kind) {
    switch (kind) {
        case PdfToolKind::Bake:
            return "pdfbake";
        case PdfToolKind::ExtractText:
            return "pdfextracttext";
        case PdfToolKind::Compress:
            return "pdfcompress";
        case PdfToolKind::Decompress:
            return "pdfdecompress";
        case PdfToolKind::DeletePages:
            return "pdfdeletepages";
        case PdfToolKind::ExtractPages:
            return "pdfextractpages";
        case PdfToolKind::Encrypt:
            return "pdfencrypt";
        case PdfToolKind::Decrypt:
            return "pdfdecrypt";
        case PdfToolKind::ConvertToPdf:
            return "converttopdf";
        case PdfToolKind::ConvertEpubToPdf:
            return "convertepubtopdf";
        case PdfToolKind::ConvertToImages:
            return "converttoimages";
        case PdfToolKind::SaveSelectionAsImage:
            return "saveselectionasimage";
        case PdfToolKind::Merge:
            break;
    }
    return "pdftool";
}

// orig's CreateToolDialog + AddPathRow + AddDestRow: every tool dialog starts
// with the source path and a destination seeded with a unique file name
static bool StartToolDialog(MainWindow* win, PdfToolKind kind, Str destPath, Str defExt) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!tab) {
        return false;
    }
    // a tool in a window of its own gets a state of its own; the caller's
    // ToolScope puts the current tool back
    bool ownWindow = kind != PdfToolKind::Merge && ToolWindowsAvailable();
    if (gCurTool == &gMainTool) {
        for (PdfToolDlg* dead : gDeadTools) {
            delete dead;
        }
        VecReset(gDeadTools);
    }
    if (ownWindow) {
        gCurTool = new PdfToolDlg();
    } else {
        gCurTool = &gMainTool;
        ClosePdfToolDialog();
    }
    gTool.win = win;
    gTool.kind = kind;
    gTool.srcPath = str::Dup(tab->filePath);
    gTool.browseExt = str::Dup(defExt);
    gTool.pageCount = win->ctrl ? win->ctrl->PageCount() : 0;
    gTool.pagesMode = PagesMode::All;
    gTool.destEdit = NewInput(win, destPath);
    gTool.pagesEdit = nullptr;
    gTool.passwordEdit = nullptr;
    gTool.visible = true;
    gTool.wantFocus = true;
    gTool.focusPages = false;
    if (!ownWindow) {
        return true;
    }
    DlgWindowSpec spec;
    spec.name = ToolWindowName(kind);
    spec.title = ToolTitleFn(kind);
    // orig's are modeless and owned by the main window
    spec.modal = false;
    spec.owned = true;
    spec.build = PdfToolDialogBuild;
    spec.close = ToolDlgWindowClose;
    spec.onEnter = ToolDlgWindowOnEnter;
    spec.onKey = ToolDlgWindowOnKey;
    gTool.winDx = gToolLastDx > 0 ? std::max(gToolLastDx, (float)kToolDlgMinDx) : ToolWinClientDx(win, gTool.srcPath);
    spec.clientDx = gTool.winDx;
    spec.resizable = true;
    spec.minClientDx = kToolDlgMinDx;
    spec.onResized = ToolDlgOnResized;
    gTool.dlgTw = DlgWindowOpen(spec, win);
    VecAppend(gWinTools, gCurTool);
    if (!gTool.dlgTw) {
        ClosePdfToolDialog();
        return false;
    }
    return true;
}

static bool CanShowToolDialog(MainWindow* win, bool needsPdf) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return false;
    }
    return !needsPdf || IsPdfDoc(tab);
}

void ShowPdfBakeDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, true)) {
        return;
    }
    logf("ShowPdfBakeDialog: opening for '%s'\n", win->CurrentTab()->filePath);
    if (!StartToolDialog(win, PdfToolKind::Bake, MakeUniqueFilePathTemp(win->CurrentTab()->filePath), StrL(".pdf"))) {
        return;
    }
    AppShellInvalidate(win);
}

void ShowPdfExtractTextDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, false)) {
        return;
    }
    Str srcPath = win->CurrentTab()->filePath;
    logf("ShowPdfExtractTextDialog: opening for '%s'\n", srcPath);
    TempStr txtPath = str::JoinTemp(path::GetPathNoExtTemp(srcPath), StrL(".txt"));
    if (!StartToolDialog(win, PdfToolKind::ExtractText, MakeUniqueFilePathTemp(txtPath), StrL(".txt"))) {
        return;
    }
    int pageCount = win->ctrl ? win->ctrl->PageCount() : 1;
    gTool.pagesEdit = NewInput(win, fmt("1-%d", pageCount));
    gTool.focusPages = true;
    AppShellInvalidate(win);
}

void ShowPdfCompressDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, true)) {
        return;
    }
    logf("ShowPdfCompressDialog: opening for '%s'\n", win->CurrentTab()->filePath);
    if (!StartToolDialog(win, PdfToolKind::Compress, MakeUniqueFilePathTemp(win->CurrentTab()->filePath),
                         StrL(".pdf"))) {
        return;
    }
    AppShellInvalidate(win);
}

void ShowPdfDecompressDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, true)) {
        return;
    }
    logf("ShowPdfDecompressDialog: opening for '%s'\n", win->CurrentTab()->filePath);
    if (!StartToolDialog(win, PdfToolKind::Decompress, MakeUniqueFilePathTemp(win->CurrentTab()->filePath),
                         StrL(".pdf"))) {
        return;
    }
    AppShellInvalidate(win);
}

void ShowPdfPageRangeDialog(MainWindow* win, bool isExtract) {
    ToolScope scope;
    if (!CanShowToolDialog(win, true)) {
        return;
    }
    int pageCount = win->ctrl ? win->ctrl->PageCount() : 0;
    if (pageCount < 2) {
        return;
    }
    Str srcPath = win->CurrentTab()->filePath;
    logf("ShowPdfPageRangeDialog: opening %s dialog for '%s', %d pages\n", Str(isExtract ? "extract" : "delete"),
         srcPath, pageCount);
    PdfToolKind kind = isExtract ? PdfToolKind::ExtractPages : PdfToolKind::DeletePages;
    if (!StartToolDialog(win, kind, MakeUniqueFilePathTemp(srcPath), StrL(".pdf"))) {
        return;
    }
    int currentPage = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    gTool.pagesEdit = NewInput(win, fmt("%d", currentPage));
    gTool.focusPages = true;
    AppShellInvalidate(win);
}

static void MergeOpenToolWindow(MainWindow* win);

void ShowMergePdfDialog(MainWindow* win) {
    if (!CanShowToolDialog(win, true)) {
        return;
    }
    // orig has one Merge PDF window: BringWindowToTop when it is up
    if (gMainTool.visible && gMainTool.tw) {
        ToolWindowActivate(gMainTool.tw);
        return;
    }
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    if (!StartToolDialog(win, PdfToolKind::Merge, tab->filePath, StrL(".pdf"))) {
        return;
    }
    gTool.mergeThumbs = new MergeThumbCache;
    gTool.mergeThumbs->win = win;
    gTool.mergeScrollY = 0;
    gTool.mergeAnchorIdx = 0;
    gTool.mergeFocusIdx = 0;
    gTool.mergeView = {};
    gTool.mergeLayoutDx = 0;
    gTool.mergeLayoutDy = 0;
    gTool.mergeGeomDx = 0;
    gTool.mergeCols = 1;
    gTool.mergeHoverIdx = -1;
    MergeEndPress();
    gTool.mergeAskPaths.Reset();
    MergeSourceDlg source;
    source.color = kSourceColors[0];
    source.path = str::Dup(tab->filePath);
    source.password = str::Dup(EngineMupdfGetPassword(engine));
    source.pageCount = engine->PageCount();
    source.engine = engine;
    engine->AddRef();
    for (int i = 0; i < source.pageCount * kMergeRotations; i++) {
        VecAppend(gTool.mergeThumbs->thumbs, MergeThumb{});
    }
    VecAppend(gTool.mergeSources, source);
    for (int pageNo = 1; pageNo <= source.pageCount; pageNo++) {
        VecAppend(gTool.mergePages, MergePageDlg{0, pageNo});
    }
    // orig: the page the document is on is selected and in view
    DocController* ctrl = tab->ctrl;
    int currIdx = limitValue((ctrl ? ctrl->CurrentPageNo() : 1) - 1, 0, std::max(len(gTool.mergePages) - 1, 0));
    MergeSelectOnly(currIdx);
    MergeOpenToolWindow(win);
    gTool.mergeEnsureFocus = true;
    AppShellInvalidate(win);
}

void ShowPdfEncryptDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, true)) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = tab->GetEngine();
    if (EngineMupdfIsEncrypted(engine)) {
        logf("ShowPdfEncryptDialog: '%s' is already encrypted, skipping\n", tab->filePath);
        return;
    }
    logf("ShowPdfEncryptDialog: opening for '%s'\n", tab->filePath);
    if (!StartToolDialog(win, PdfToolKind::Encrypt, MakeUniqueFilePathTemp(tab->filePath), StrL(".pdf"))) {
        return;
    }
    gTool.passwordEdit = NewInput(win, {});
    AppShellInvalidate(win);
}

void ShowPdfDecryptDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, true)) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = tab->GetEngine();
    if (!EngineMupdfIsEncrypted(engine)) {
        logf("ShowPdfDecryptDialog: '%s' is not encrypted, skipping\n", tab->filePath);
        return;
    }
    logf("ShowPdfDecryptDialog: opening for '%s'\n", tab->filePath);
    if (!StartToolDialog(win, PdfToolKind::Decrypt, MakeUniqueFilePathTemp(tab->filePath), StrL(".pdf"))) {
        return;
    }
    AppShellInvalidate(win);
}

void ShowConvertToPdfDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, false)) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = tab->GetEngine();
    if (!engine || !engine->isImageCollection) {
        return;
    }
    if (!engine->AllowsPrinting()) {
        return;
    }
    logf("ShowConvertToPdfDialog: opening for '%s'\n", tab->filePath);
    if (!StartToolDialog(win, PdfToolKind::ConvertToPdf, DefaultPdfDestPathTemp(tab->filePath), StrL(".pdf"))) {
        return;
    }
    AppShellInvalidate(win);
}

static void InitPageSizeDropDown(MainWindow* win);
static void InitFontSizeCombo(MainWindow* win);

void ShowConvertEpubToPdfDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, false)) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!IsEpubDoc(tab)) {
        return;
    }
    logf("ShowConvertEpubToPdfDialog: opening for '%s'\n", tab->filePath);
    if (!StartToolDialog(win, PdfToolKind::ConvertEpubToPdf, DefaultPdfDestPathTemp(tab->filePath), StrL(".pdf"))) {
        return;
    }
    InitPageSizeDropDown(win);
    InitFontSizeCombo(win);
    AppShellInvalidate(win);
}

// orig's NewDpiCombo
static void InitDpiCombo(MainWindow* win, int dpi) {
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gTool.ddDpi.Init(app);
    StrVec dpis;
    int selIdx = -1;
    for (int i = 0; i < dimofi(kImageDpiChoices); i++) {
        dpis.Append(fmt("%d", kImageDpiChoices[i]));
        if (kImageDpiChoices[i] == dpi) {
            selIdx = i;
        }
    }
    gTool.ddDpi.SetItems(dpis, selIdx);
    gTool.dpiEdit = NewInput(win, fmt("%d", dpi));
}

// orig's NewPageSizeDropDown, in ddFormat
static void InitPageSizeDropDown(MainWindow* win) {
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gTool.ddFormat.Init(app);
    StrVec items;
    for (const EbookPdfPageSize& ps : kEbookPdfPageSizes) {
        items.Append(ps.label);
    }
    gTool.ddFormat.SetItems(items, 0);
}

// orig's NewFontSizeCombo, in the DPI combo's fields
static void InitFontSizeCombo(MainWindow* win) {
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gTool.ddDpi.Init(app);
    StrVec sizes;
    int selIdx = -1;
    for (int i = 0; i < dimofi(kEbookPdfFontSizes); i++) {
        sizes.Append(fmt("%d", kEbookPdfFontSizes[i]));
        if (kEbookPdfFontSizes[i] == kEbookPdfDefaultFontSize) {
            selIdx = i;
        }
    }
    gTool.ddDpi.SetItems(sizes, selIdx);
    gTool.dpiEdit = NewInput(win, fmt("%d", kEbookPdfDefaultFontSize));
}

static void InitFormatDropDown(MainWindow* win, int sel) {
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gTool.ddFormat.Init(app);
    StrVec items;
    for (int i = 0; i < ConvertImageFormatCount(); i++) {
        items.Append(kConvertImageFormats[i].label);
    }
    gTool.ddFormat.SetItems(items, sel);
}

void ShowConvertPdfToImagesDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, true)) {
        return;
    }
    Str srcPath = win->CurrentTab()->filePath;
    logf("ShowConvertPdfToImagesDialog: opening for '%s'\n", srcPath);
    int pageCount = win->ctrl ? win->ctrl->PageCount() : 1;
    TempStr pngPath = str::JoinTemp(path::GetPathNoExtTemp(srcPath), StrL("-<N>.png"));
    Vec<int> allPages;
    CollectAllPages(pageCount, allPages);
    pngPath = UniqueImagesTemplateTemp(pngPath, allPages);
    if (!StartToolDialog(win, PdfToolKind::ConvertToImages, pngPath, StrL(".png"))) {
        return;
    }
    gTool.pagesEdit = NewInput(win, fmt("1-%d", pageCount));
    gTool.pagesMode = PagesMode::All;
    InitFormatDropDown(win, 0);
    InitDpiCombo(win, kConvertPdfToImagesDpi);
    AppShellInvalidate(win);
}

void ShowSaveSelectionAsImageDialog(MainWindow* win) {
    ToolScope scope;
    if (!CanShowToolDialog(win, false)) {
        return;
    }
    if (!HasPermission(Perm::CopySelection) || !CanAccessDisk()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!IsRectangularSelection(win)) {
        return;
    }
    if (!tab->selectionOnPage || len(*tab->selectionOnPage) == 0) {
        return;
    }
    SelectionOnPage& sel = (*tab->selectionOnPage)[0];
    if (sel.rect.IsEmpty()) {
        return;
    }
    logf("ShowSaveSelectionAsImageDialog: opening for '%s'\n", tab->filePath);
    TempStr pngPath = MakeUniqueFilePathTemp(str::JoinTemp(path::GetPathNoExtTemp(tab->filePath), StrL("-sel.png")));
    if (!StartToolDialog(win, PdfToolKind::SaveSelectionAsImage, pngPath, StrL(".png"))) {
        return;
    }
    gTool.pageNo = sel.pageNo;
    gTool.selRect = sel.rect;
    InitFormatDropDown(win, 0);
    InitDpiCombo(win, kSaveSelectionDefaultDpi);
    AppShellInvalidate(win);
}

// the open dialog as one line: what a scripted session checks instead of
// reading pixels out of a screenshot
TempStr PdfToolStateTemp() {
    if (!gTool.visible) {
        return str::DupTemp(StrL("no pdf tool dialog"));
    }
    str::Builder s;
    s.Append(fmt("'%s' src '%s' dest '%s'", ToolTitle(), gTool.srcPath, DestPathTemp()));
    if (gTool.pagesEdit) {
        s.Append(fmt(" pages '%s' of %d", InputTextTemp(gTool.pagesEdit), gTool.pageCount));
    }
    if (gTool.kind == PdfToolKind::ConvertToImages) {
        s.Append(fmt(" mode %d format %s dpi %d", (int)gTool.pagesMode, gTool.ddFormat.SelText(), SelectedDpi()));
    }
    if (gTool.kind == PdfToolKind::SaveSelectionAsImage) {
        s.Append(fmt(" format %s dpi %d", gTool.ddFormat.SelText(), SelectedDpi()));
    }
    s.Append(fmt(" enabled %d", (int)ActionEnabled()));
    return ToStrTemp(s);
}

// --- building ---------------------------------------------------------------

// orig's MergeGrid::DrawCell / Paint as gpui elements. Only the rows in view
// are built; everything is positioned absolutely in a box as tall as the grid.
static void MergeInvalidateLater() {
    if (gTool.visible && gTool.kind == PdfToolKind::Merge) {
        AppShellInvalidate(gTool.win);
    }
}

static gp::El* MergeGridEl(gp::Ctx* cx, float viewDx, float viewDy) {
    // Last frame's bounds, or the width the caller already knows: gpui fills
    // bounds while painting, and this window may not be built again.
    float dx = gTool.mergeView.w > 1.f ? gTool.mergeView.w : viewDx;
    if (gTool.mergeView.w != gTool.mergeLayoutDx || gTool.mergeView.h != gTool.mergeLayoutDy) {
        gTool.mergeLayoutDx = gTool.mergeView.w;
        gTool.mergeLayoutDy = gTool.mergeView.h;
        uitask::Post(MkFunc0Void(MergeInvalidateLater), "MergeGridLayout");
    }
    if (dx > 1.f) {
        gTool.mergeGeomDx = dx;
        gTool.mergeCols = MergeColsForDx(dx);
        if (gTool.mergeEnsureFocus) {
            gTool.mergeEnsureFocus = false;
            MergeEnsureVisible(gTool.mergeFocusIdx / gTool.mergeCols);
        }
    }
    MergeScrollTo(gTool.mergeScrollY);

    Vec<MergePageDlg>& items = gTool.mergePages;
    int n = len(items);
    int cols = gTool.mergeCols;
    float scrollY = gTool.mergeScrollY;
    float contentDy = (float)MergeRows() * MergeItemDy() + 2 * MergePad();
    gp::El* content = gp::Div(cx->a)->W(gp::kFill)->H(contentDy)->Shrink0();

    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    auto withAlpha = [](Color c, u8 a) {
        gp::Rgba res = ToGpui(c);
        res.a = a;
        return res;
    };
    bool multipleSources = len(gTool.mergeSources) > 1;
    int firstRow = std::max(0, (int)(scrollY / MergeItemDy()) - 1);
    int lastRow = (int)((scrollY + viewDy) / MergeItemDy()) + 1;
    int first = firstRow * cols;
    int last = std::min(n - 1, (lastRow + 1) * cols - 1);
    float labelDy = (float)kMergeLabelDy;
    for (int i = first; i <= last; i++) {
        const MergePageDlg& page = items[i];
        const MergeSourceDlg& source = gTool.mergeSources[page.sourceNo];
        MergeThumb& thumb = gTool.mergeThumbs->thumbs[MergeThumbIdx(page)];
        RectF r = MergeCellRect(i);
        r.y += scrollY;
        // the page is the thumbnail's size once rendered: rotated, it's landscape
        RectF pr = r;
        if (thumb.bitmap) {
            pr.dx = std::min((float)thumb.bitmap->width, r.dx);
            pr.dy = std::min((float)thumb.bitmap->height, r.dy);
            pr.x = r.x + ((r.dx - pr.dx) / 2);
            pr.y = r.y + ((r.dy - pr.dy) / 2);
        }

        // the selection and hover: a rounded frame around page and number;
        // the keyboard's page a ring when it isn't selected
        bool hovered = i == gTool.mergeHoverIdx && gTool.mergePress == MergePress::None;
        float pad = kMergeFramePad;
        gp::El* frame = gp::Div(cx->a)
                            ->Absolute()
                            ->Left(r.x - pad)
                            ->Top(r.y - pad)
                            ->W(r.dx + 2 * pad)
                            ->H(r.dy + labelDy + pad)
                            ->Radius(kMergeFrameRadius);
        Color colFrame = kColorUnset;
        if (page.selected) {
            colFrame = AccentColor(colBg, kMergeSelShift);
        } else if (hovered) {
            colFrame = AccentColor(colBg, kMergeHoverShift);
        }
        if (colFrame != kColorUnset) {
            content->Child(frame->Bg(ToGpui(colFrame)));
        } else if (i == gTool.mergeFocusIdx) {
            content->Child(frame->Border(1, ToGpui(AccentColor(colBg, kMergeFocusRingShift))));
        }

        // white pages on a white grid need an edge
        gp::El* cell = gp::Div(cx->a)
                           ->Absolute()
                           ->Left(pr.x)
                           ->Top(pr.y)
                           ->W(pr.dx)
                           ->H(pr.dy)
                           ->ItemsCenter()
                           ->JustifyCenter()
                           ->Bg(gp::Rgba{0xff, 0xff, 0xff, 0xff})
                           ->Border(1, ToGpui(AccentColor(colBg, kMergePageEdgeShift)));
        if (thumb.bitmap) {
            gp::ImageSource image = gp::ImageSource::FromCustom(MergeThumbLoad, &thumb);
            cell->Child(gp::ImageEl(cx->a, image, GStrL(""))->SizeFull()->ObjectFitMode(gp::ObjectFit::Contain));
        }
        if (page.removed) {
            Color veil = colFrame != kColorUnset ? colFrame : colBg;
            cell->Child(gp::Div(cx->a)->Absolute()->Left(0)->Top(0)->W(pr.dx)->H(pr.dy)->Bg(
                withAlpha(veil, kMergeRemovedVeilAlpha)));
        }

        // the page number under the page; its file's dot when there are more files
        gp::El* label = gp::Div(cx->a)
                            ->Absolute()
                            ->Left(r.x)
                            ->Top(r.y + r.dy)
                            ->W(r.dx)
                            ->H(labelDy)
                            ->FlexRow()
                            ->ItemsCenter()
                            ->JustifyCenter()
                            ->Gap(kMergeDotDx);
        if (multipleSources) {
            u8 alpha = page.removed ? kMergeBandAlpha * 4 : 255;
            label->Child(gp::Div(cx->a)
                             ->W(kMergeDotDx)
                             ->H(kMergeDotDx)
                             ->Shrink0()
                             ->Radius(kMergeDotDx / 2)
                             ->Bg(withAlpha(source.color, alpha)));
        }
        Color colLabel = page.selected && !page.removed ? colTxt : colMuted;
        label->Child(
            gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d", page.pageNo)))->Font(kToolDlgMetaFontPx)->Fg(ToGpui(colLabel)));
        content->Child(label);

        // remove on the page under the mouse, restore on every removed page
        if (page.removed || (i == gTool.mergeHoverIdx && !gTool.mergeDragging)) {
            float btnDx = (float)kMergeCornerBtnDx;
            float inset = (float)kMergeCornerBtnInset;
            Str glyph = page.removed ? StrL("\xe2\x86\xba") : StrL("\xc3\x97");
            cell->Child(
                gp::Div(cx->a)
                    ->Absolute()
                    ->Right(inset)
                    ->Top(inset)
                    ->W(btnDx)
                    ->H(btnDx)
                    ->Radius(btnDx / 2)
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->Bg(withAlpha(colTxt, kMergeCornerBtnAlpha))
                    ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, glyph))->Font(kMergeCornerGlyphPx)->Fg(ToGpui(colBg))));
        }
        content->Child(cell);
    }

    // where dragged pages would go, and the rubber band
    if (gTool.mergeDropBefore >= 0 && n > 0) {
        bool atEnd = gTool.mergeDropBefore >= n;
        RectF r = MergeCellRect(atEnd ? n - 1 : gTool.mergeDropBefore);
        r.y += scrollY;
        float barDx = (float)kMergeDropBarDx;
        float x = atEnd ? r.x + r.dx + (float)(kMergeGap / 2) : r.x - (float)(kMergeGap / 2);
        content->Child(gp::Div(cx->a)
                           ->Absolute()
                           ->Left(x - barDx / 2)
                           ->Top(r.y)
                           ->W(barDx)
                           ->H(r.dy)
                           ->Radius(barDx / 2)
                           ->Bg(ToGpui(colTxt)));
    }
    if (gTool.mergePress == MergePress::Band) {
        float x0 = std::min(gTool.mergeBandFrom.x, gTool.mergeBandTo.x);
        float y0 = std::min(gTool.mergeBandFrom.y, gTool.mergeBandTo.y);
        content->Child(gp::Div(cx->a)
                           ->Absolute()
                           ->Left(x0)
                           ->Top(y0)
                           ->W(std::abs(gTool.mergeBandTo.x - gTool.mergeBandFrom.x))
                           ->H(std::abs(gTool.mergeBandTo.y - gTool.mergeBandFrom.y))
                           ->Bg(withAlpha(colTxt, kMergeBandAlpha)));
    }

    return gp::Div(cx->a)
        ->Id(GStrL("merge-pages"))
        ->Click(gp::HashClickId(GStrL("merge-pages")))
        ->W(gp::kFill)
        ->H(viewDy)
        ->Bg(ToGpui(colBg))
        ->ScrollY(scrollY)
        ->ScrollFromPath()
        ->BoundsOut(&gTool.mergeView)
        ->OnScroll(gp::ListenTo(gToolView, &PdfToolView::OnMergeScroll))
        ->OnMouseDown(gp::ListenTo(gToolView, &PdfToolView::OnMergeGridDown))
        ->OnMouseMove(gp::ListenTo(gToolView, &PdfToolView::OnMergeGridMove))
        ->OnDragMove(gp::ListenTo(gToolView, &PdfToolView::OnMergeGridDrag))
        ->OnMouseUp(gp::ListenTo(gToolView, &PdfToolView::OnMergeGridUp))
        ->OnMouseUpOut(gp::ListenTo(gToolView, &PdfToolView::OnMergeGridUp))
        ->Child(content);
}

// orig's UpdateSizeLabel: what the selection comes to at the DPI in the box
static TempStr SelectionSizeTextTemp(MainWindow* win) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    int dpi = SelectedDpi();
    if (!engine || dpi < 1) {
        return {};
    }
    int w = 0;
    int h = 0;
    EstimateSelectionPx(gTool.selRect, SaveSelectionZoom(engine, (float)dpi), w, h);
    if (!SaveSelectionSizeOk(w, h)) {
        return str::DupTemp(Tr("Too large for this DPI"));
    }
    return fmt("%d x %d px", w, h);
}

static gp::El* RadioEl(gp::Ctx* cx, Str id, Str label, PagesMode mode) {
    return gpc::Radio::New(cx, GpuiDup(cx->a, id))
        ->Label(GpuiDup(cx->a, label))
        ->Checked(gTool.pagesMode == mode)
        ->WithSize(gp::UiSize::Small)
        ->OnClick(gp::ListenTo(gToolView, &PdfToolView::OnPagesMode, (intptr_t)mode))
        ->IntoEl();
}

static gp::El* MergeAskRadioEl(gp::Ctx* cx, Str id, Str label, MergeInsertAt at) {
    return DlgAccelEl(
        cx, gpc::Radio::New(cx, GpuiDup(cx->a, id))->Checked(gTool.mergeAskAt == at)->WithSize(gp::UiSize::Small),
        label, gp::ListenTo(gToolView, &PdfToolView::OnMergeAskAt, (intptr_t)at));
}

// orig's InsertPosWnd: the file in the larger font, the choices, a bare
// underlined page field, Cancel and the filled Add
constexpr float kMergeAskDx = 340;
constexpr float kMergeAskPageDx = 48;

static gp::El* MergeAskDialogBuild(gp::Ctx* cx) {
    Color colTxt = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    int nItems = len(gTool.mergePages);
    Str what = path::GetBaseNameTemp(gTool.mergeAskPaths[0]);
    if (len(gTool.mergeAskPaths) > 1) {
        what = fmt(Tr("%d files").s, len(gTool.mergeAskPaths));
    }
    gTool.mergeAskEdit->onChange = gp::ListenTo(gToolView, &PdfToolView::OnMergeAskInput);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Bg(ToGpui(ThemeWindowControlBackgroundColor()));
    col->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->W(gp::kFill)
                   ->H(kToolDlgLineDy)
                   ->Shrink0()
                   ->PadX(kToolDlgPadDx)
                   ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt(Tr("Insert '%s':").s, what)))
                               ->Font(kToolDlgQueryFontPx)
                               ->Fg(ToGpui(colTxt))
                               ->Truncate()
                               ->MinW(0)));
    col->Child(DlgHairline(cx));

    gp::El* choices = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0()->PadX(kToolDlgPadDx)->PadY(kToolDlgFilesPadDy);
    auto row = [&]() {
        return gp::Div(cx->a)->FlexRow()->ItemsCenter()->H(kToolDlgRowDy)->Shrink0()->Gap(kToolDlgGapDx);
    };
    choices->Child(row()->Child(MergeAskRadioEl(cx, StrL("merge-ask-end"), Tr("At the &end"), MergeInsertAt::End)));
    choices->Child(
        row()->Child(MergeAskRadioEl(cx, StrL("merge-ask-start"), Tr("At the &beginning"), MergeInsertAt::Beginning)));
    gp::El* after = row();
    after->Child(MergeAskRadioEl(cx, StrL("merge-ask-after"), Tr("&After page"), MergeInsertAt::AfterPage));
    after->Child(gp::Div(cx->a)
                     ->W(kMergeAskPageDx)
                     ->Shrink0()
                     ->Child(DlgUnderlineInput(cx, GStrL("merge-ask-page"), gTool.mergeAskEdit, kToolDlgTextFontPx,
                                               ThemeEdgeColor())));
    after->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt(Tr("(of %d)").s, nItems)))
                     ->Font(kToolDlgMetaFontPx)
                     ->Fg(ToGpui(colMuted)));
    choices->Child(after);
    col->Child(choices);

    // the card's own footer row, so it pads the buttons as it pads the rest
    gp::El* footer = gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->Shrink0()->Gap(kToolDlgGapDx);
    footer->Child(gp::Div(cx->a)->Flex1());
    footer->Child(gpc::Button::New(cx, GStrL("merge-ask-cancel"))
                      ->Label(ToGpui(Tr("Cancel")))
                      ->Outline()
                      ->WithSize(gp::UiSize::Small)
                      ->OnClick(gp::ListenTo(gToolView, &PdfToolView::OnMergeAskCancel))
                      ->IntoEl());
    footer->Child(
        DlgFilledButton(cx, GStrL("merge-ask-ok"), Tr("Add"), gp::ListenTo(gToolView, &PdfToolView::OnMergeAskOk)));

    gp::El* dlg = gpc::Dialog::New(cx)
                      ->Open(true)
                      ->Title(ToGpui(Tr("Add PDF")))
                      ->Body(col)
                      ->Footer(footer)
                      ->W(kMergeAskDx)
                      ->OnClose(gp::ListenTo(gToolView, &PdfToolView::OnMergeAskCancel))
                      ->IntoEl(gp::WindowSize(cx->win));
    if (gTool.mergeAskFocus) {
        gTool.mergeAskFocus = false;
        gp::InputFocus(gTool.mergeAskEdit, cx->app, cx->win);
        gp::InputSelectAll(gTool.mergeAskEdit, cx->app, cx->win);
    }
    return dlg;
}

// the page grid with its context menu
static gp::El* MergePagesEl(gp::Ctx* cx, float viewDx, float viewDy) {
    gp::El* pages = MergeGridEl(cx, viewDx, viewDy);
    int nSel = MergeSelectedCount();
    int nSelRemoved = MergeSelectedRemovedCount();
    gpc::PopupMenu* menu = gpc::PopupMenu::New(cx, GStrL("merge-ctx-menu"))->MinW(200);
    menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(Tr("&Remove")).display), ActMergeMenu(),
                         (intptr_t)kMenuRemove);
    menu->Disabled(nSel <= nSelRemoved);
    menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(Tr("R&estore")).display), ActMergeMenu(),
                         (intptr_t)kMenuRestore);
    menu->Disabled(nSelRemoved == 0);
    menu->Separator();
    menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(Tr("Rotate &Left")).display), ActMergeMenu(),
                         (intptr_t)kMenuRotateLeft);
    menu->Disabled(nSel == 0);
    menu->Kbd(GStrL("["));
    menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(Tr("Rotate &Right")).display), ActMergeMenu(),
                         (intptr_t)kMenuRotateRight);
    menu->Disabled(nSel == 0);
    menu->Kbd(GStrL("]"));
    menu->Separator();
    menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(Tr("Cu&t")).display), ActMergeMenu(),
                         (intptr_t)kMenuCut);
    menu->Disabled(nSel == 0);
    menu->Kbd(GStrL("Ctrl+X"));
    menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(Tr("&Copy")).display), ActMergeMenu(),
                         (intptr_t)kMenuCopy);
    menu->Disabled(nSel == 0);
    menu->Kbd(GStrL("Ctrl+C"));
    // ng: always enabled; reading the clipboard every frame is a round trip on X11
    menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(Tr("&Paste")).display), ActMergeMenu(),
                         (intptr_t)kMenuPaste);
    menu->Kbd(GStrL("Ctrl+V"));
    menu->Separator();
    menu->MenuWithAction(GpuiDup(cx->a, ParseMenuAccelTextTemp(Tr("Select &All")).display), ActMergeMenu(),
                         (intptr_t)kMenuSelectAll);
    menu->Kbd(GStrL("Ctrl+A"));
    // ContextMenu::IntoEl() puts its own click path and mouse-down on the
    // child it is given, so the grid goes inside a wrapper
    gp::El* pagesWrap = gp::Div(cx->a)->W(gp::kFill)->BoundsOut(&gTool.mergePagesBounds)->Child(pages);
    gTool.mergePopup = menu->state;
    gp::El* pagesWithMenu =
        gpc::ContextMenu::New(cx, GStrL("merge-ctx"))->Child(pagesWrap)->Menu(TrackPopup(cx, menu))->IntoEl();
    pagesWithMenu->OnAction(ActMergeMenu(), gp::ListenTo(gToolView, &PdfToolView::OnMergeMenu));
    return pagesWithMenu;
}

// --- Merge PDF in a window of its own (Windows) ------------------------------

// orig's PaintMergeInfo: how many pages the result has
static TempStr MergeInfoTemp() {
    int nRemoved = 0;
    for (const MergePageDlg& page : gTool.mergePages) {
        nRemoved += page.removed ? 1 : 0;
    }
    TempStr s = nRemoved > 0 ? fmt(Tr("%d pages, %d removed").s, len(gTool.mergePages) - nRemoved, nRemoved)
                             : fmt(Tr("%d pages").s, len(gTool.mergePages));
    int nSel = MergeSelectedCount();
    if (nSel > 0) {
        s = fmt("%s, %s", s, fmt(Tr("%d selected").s, nSel));
    }
    return s;
}

constexpr int kMergeWinMinDx = 480;
constexpr int kMergeWinMinDy = 320;
// the frame's card: the grid gets what the frame leaves after its title and
// the footer
constexpr float kMergeCardChromeDy = 200;
constexpr float kMergeCardMinViewDy = 240;
// between the files of the legend
constexpr float kMergeInfoGap = 24;

static Str MergeToolTitle() {
    return fmt("%s - %s", Tr("Merge PDF"), path::GetBaseNameTemp(gTool.srcPath));
}

static gp::El* MergeText(gp::Ctx* cx, Str s, Color col) {
    return gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(kToolDlgMetaFontPx)->Fg(ToGpui(col))->Shrink0();
}

// orig's MergePdfWnd::PaintInfo: the page count
static gp::El* MergeInfoEl(gp::Ctx* cx) {
    Color colMuted = ThemeWindowTextDisabledColor();
    gp::El* info = gp::Div(cx->a)->FlexRow()->Flex1()->MinW(0)->ItemsCenter()->ClipX();
    info->Child(MergeText(cx, MergeInfoTemp(), colMuted));
    return info;
}

// orig's MergePdfWnd::LegendRects: each file's dot and name, in rows that wrap
// at dx, so every file is named (issue #6397). Relative to the legend
static void MergeLegendRects(gp::Ctx* cx, float dx, Vec<RectF>& out) {
    VecReset(out);
    if (len(gTool.mergeSources) < 2) {
        return;
    }
    gp::PaintCtx* paint = cx->win ? &cx->win->paint : nullptr;
    float lineDy = ceilf(gp::MeasureText(paint, GStrL("M"), kToolDlgMetaFontPx, 0).h);
    float x = 0;
    float y = kToolDlgFootPadDy;
    for (const MergeSourceDlg& source : gTool.mergeSources) {
        Str name = path::GetBaseNameTemp(source.path);
        float nameDx = ceilf(gp::MeasureText(paint, ToGpui(name), kToolDlgMetaFontPx, 0).w);
        RectF r{0, 0, (2 * kMergeDotDx) + nameDx, lineDy};
        if (x > 0 && x + r.dx > dx) {
            x = 0;
            y += lineDy + (lineDy / 2);
        }
        r.x = x;
        r.y = y;
        x += r.dx + kMergeInfoGap;
        VecAppend(out, r);
    }
}

// the legend's width: the column's, inside its padding. The card's column is
// as wide as the grid was the last frame
static float MergeLegendDx(float viewDx) {
    float colDx = viewDx > 0 ? viewDx : gTool.mergeView.w;
    if (colDx <= 1.f) {
        colDx = (float)kMergeDialogDx;
    }
    return std::max(0.f, colDx - (2 * kToolDlgPadDx));
}

static float MergeLegendDy(gp::Ctx* cx, float dx) {
    Vec<RectF> rects;
    MergeLegendRects(cx, dx, rects);
    return len(rects) > 0 ? VecLast(rects).Bottom() : 0;
}

// orig's MergeLegend: above the footer, as tall as its rows of files
static gp::El* MergeLegendEl(gp::Ctx* cx, float dx) {
    Color colMuted = ThemeWindowTextDisabledColor();
    Vec<RectF> rects;
    MergeLegendRects(cx, dx, rects);
    VecResize(gTool.mergeLegendItems, len(rects));
    float dy = len(rects) > 0 ? VecLast(rects).Bottom() : 0;
    gp::El* box = gp::Div(cx->a)->W(gp::kFill)->H(dy)->BoundsOut(&gTool.mergeLegendView);
    for (int i = 0; i < len(rects); i++) {
        const MergeSourceDlg& source = gTool.mergeSources[i];
        RectF r = rects[i];
        gp::El* file = gp::Div(cx->a)
                           ->Absolute()
                           ->Left(r.x)
                           ->Top(r.y)
                           ->W(r.dx)
                           ->H(r.dy)
                           ->FlexRow()
                           ->ItemsCenter()
                           ->Gap(kMergeDotDx)
                           ->BoundsOut(&gTool.mergeLegendItems[i]);
        file->Child(gp::Div(cx->a)->W(kMergeDotDx)->H(kMergeDotDx)->Radius(kMergeDotDx / 2)->Bg(ToGpui(source.color)));
        file->Child(MergeText(cx, path::GetBaseNameTemp(source.path), colMuted));
        box->Child(file);
    }
    return gp::Div(cx->a)->W(gp::kFill)->Shrink0()->PadX(kToolDlgPadDx)->Child(box);
}

// orig's footer: the quiet page actions and the status on the left, Cancel,
// Save and the one filled Save As... on the right
static gp::El* MergeFooterEl(gp::Ctx* cx) {
    int nSel = MergeSelectedCount();
    int nSelRemoved = MergeSelectedRemovedCount();
    bool canSave = ActionEnabled();
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->W(gp::kFill)
                      ->Shrink0()
                      ->PadX(kToolDlgPadDx)
                      ->PadY(kToolDlgFootPadDy)
                      ->Gap(kToolDlgGapDx);
    row->Child(
        DlgGhostButton(cx, GStrL("merge-add"), Tr("Add PDF..."), gp::ListenTo(gToolView, &PdfToolView::OnMergeAdd)));
    row->Child(DlgGhostButton(cx, GStrL("merge-remove"), Tr("Remove"),
                              gp::ListenTo(gToolView, &PdfToolView::OnMergeSetRemoved, (intptr_t)1),
                              nSel <= nSelRemoved));
    row->Child(DlgGhostButton(cx, GStrL("merge-restore"), Tr("Restore"),
                              gp::ListenTo(gToolView, &PdfToolView::OnMergeSetRemoved, (intptr_t)0), nSelRemoved == 0));
    row->Child(gp::Div(cx->a)->W(kToolDlgGapDx)->Shrink0());
    row->Child(MergeInfoEl(cx));
    auto outline = [&](gp::Str id, Str label, gp::Listener onClick, bool enabled) {
        return gpc::Button::New(cx, id)
            ->Label(ToGpui(label))
            ->Outline()
            ->WithSize(gp::UiSize::Small)
            ->Disabled(!enabled)
            ->OnClick(onClick)
            ->IntoEl();
    };
    row->Child(outline(GStrL("pdftool-cancel"), Tr("Cancel"), gp::ListenTo(gToolView, &PdfToolView::OnCancel), true));
    row->Child(outline(GStrL("pdftool-action"), Tr("Save"), gp::ListenTo(gToolView, &PdfToolView::OnAction), canSave));
    row->Child(DlgFilledButton(cx, GStrL("merge-saveas"), Tr("Save As..."),
                               gp::ListenTo(gToolView, &PdfToolView::OnMergeSaveAs), !canSave));
    return row;
}

// the pages edge to edge, then the legend and the footer
static gp::El* MergeColumn(gp::Ctx* cx, float viewDx, float viewDy) {
    StartMergeThumbs();
    return gp::Div(cx->a)
        ->FlexCol()
        ->W(gp::kFill)
        ->Bg(ToGpui(ThemeWindowControlBackgroundColor()))
        ->Child(MergePagesEl(cx, viewDx, viewDy))
        ->Child(MergeLegendEl(cx, MergeLegendDx(viewDx)))
        ->Child(MergeFooterEl(cx));
}

static float MergeFooterDy() {
    return 2 * kToolDlgFootPadDy + kDlgBtnDy;
}

static gp::El* MergeToolBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gTool.visible || !gTool.tw || gTool.win != win || gTool.kind != PdfToolKind::Merge) {
        return nullptr;
    }
    if (!gToolView.IsValid()) {
        gToolView = gp::EntityNewState<PdfToolView>(cx->app);
    }
    gp::WinSize ws = gp::WindowSize(cx->win);
    float viewDy = std::max(100.f, ws.dipH - MergeFooterDy() - MergeLegendDy(cx, MergeLegendDx(ws.dipW)));
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->Child(MergeColumn(cx, ws.dipW, viewDy));
    if (len(gTool.mergeAskPaths) == 0) {
        return col;
    }
    // orig's InsertPosWnd is a dialog owned by this window
    return col->Child(MergeAskDialogBuild(cx));
}

// orig's WindowBase::PreTranslateMessage for this window: the grid has the
// focus, so its keys (MergeGrid::OnGridKeyDown), MergePdfWnd::OnKey (Esc ends
// a drag), closeOnEsc and Enter for the default button
static bool MergeToolOnKey(MainWindow* win, gp::Ctx* cx, const gp::KeyEvent* ev) {
    if (!gTool.visible || gTool.win != win || gTool.kind != PdfToolKind::Merge) {
        return false;
    }
    bool editFocused = cx->win->input && cx->win->input->focused;
    if (len(gTool.mergeAskPaths) > 0) {
        if (ev->vk == VK_ESCAPE) {
            MergeAskDone(false);
            return true;
        }
        if (ev->vk == VK_RETURN && !editFocused) {
            MergeAskDone(true);
            return true;
        }
        return false;
    }
    if (ev->vk == VK_ESCAPE) {
        if (!PdfToolDialogEndDrag()) {
            ClosePdfToolDialog();
        }
        return true;
    }
    if (ev->vk == VK_RETURN) {
        ToolOnEnter();
        return true;
    }
    if (ev->vk == VK_APPS) {
        return MergeContextMenuFromKey(win, cx);
    }
    // Cmd on macOS
    return MergeOnKeyDown(win, ev->vk, ev->ctrl || ev->platform, ev->shift);
}

// the caption's close box, or the frame went away
static void MergeToolOnClosed(MainWindow*) {
    gTool.tw = nullptr;
    ClosePdfToolDialog();
}

static ToolWindowDesc MergeToolDesc() {
    // orig: WS_OVERLAPPEDWINDOW owned by the frame, a drop target for PDFs
    ToolWindowDesc desc;
    desc.name = "mergepdf";
    desc.title = MergeToolTitle;
    desc.frame = ToolWinFrame::Overlapped;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::Owned;
    desc.minClient = Size(kMergeWinMinDx, kMergeWinMinDy);
    desc.dropFiles = true;
    desc.build = MergeToolBuild;
    desc.onKey = MergeToolOnKey;
    desc.onClosed = MergeToolOnClosed;
    return desc;
}

// orig: most of the work area of the document's monitor (kDialogDx wide, 85%
// of its height), centered on the frame, kept on screen
static Rect MergeToolRect(MainWindow* win) {
    Rect frame = AppShellWindowScreenRect(win);
    Rect work = AppShellWorkArea(win);
    int dpi = std::max(AppShellWindowDpi(win), 96);
    int dx = std::min(work.dx, MulDiv(kMergeDialogDx, dpi, 96));
    int dy = work.dy * 85 / 100;
    Rect r{frame.x + (frame.dx - dx) / 2, frame.y + (frame.dy - dy) / 2, dx, dy};
    return AppShellShiftToWorkArea(r, win, true);
}

static void MergeOpenToolWindow(MainWindow* win) {
    if (gTool.tw || !ToolWindowsAvailable()) {
        return;
    }
    gTool.tw = ToolWindowOpen(MergeToolDesc(), win, MergeToolRect(win));
}

// --- the dialog, drawn like the palette -------------------------------------

// what a bare gpui input insets its text by; the source path lines up with it
constexpr float kToolDlgInputPadDx = 8;
// The tool's lines (a page range, a password, the DPI), a hairline, the files
// - the source and the destination over a hairline, with Browse - and the
// footer with a hint, Cancel and the one filled action button. The same
// column goes in a window of its own and in the frame.

static gp::El* ToolText(gp::Ctx* cx, Str s, float font, Color col) {
    return gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(font)->Fg(ToGpui(col));
}

static gp::El* ToolLabel(gp::Ctx* cx, Str label) {
    return gp::Div(cx->a)
        ->MinW(kToolDlgLabelDx)
        ->Shrink0()
        ->Child(ToolText(cx, label, kToolDlgTextFontPx, ThemeWindowTextDisabledColor()));
}

// a line of the top section: its label, then what the caller adds
static gp::El* ToolLine(gp::Ctx* cx, Str label) {
    return gp::Div(cx->a)
        ->FlexRow()
        ->ItemsCenter()
        ->W(gp::kFill)
        ->H(kToolDlgLineDy)
        ->Shrink0()
        ->PadX(kToolDlgPadDx)
        ->Gap(kToolDlgGapDx)
        ->Child(ToolLabel(cx, label));
}

// a bare field in the larger font: what the dialog is about
static gp::El* ToolLineInput(gp::Ctx* cx, gp::Str id, gp::InputState* edit, bool masked = false,
                             bool disabled = false) {
    gpc::Input* in = gpc::Input::New(cx, id, edit)
                         ->Appearance(false)
                         ->WithSize(gp::UiSize::Small)
                         ->Masked(masked)
                         ->Disabled(disabled)
                         ->W(gp::kFill);
    return gp::Div(cx->a)->Flex1()->MinW(0)->Child(in->IntoEl()->Font(kToolDlgQueryFontPx));
}

// the text at a line's right end, lighter while what it describes is not usable
static gp::El* ToolMeta(gp::Ctx* cx, Str s, bool ok) {
    Color col = ThemeWindowTextDisabledColor();
    if (!ok) {
        col = AccentColor(col, kToolDlgFaintShift);
    }
    return gp::Div(cx->a)->Shrink0()->Child(ToolText(cx, s, kToolDlgMetaFontPx, col));
}

static gp::El* ToolDpiCombo(gp::Ctx* cx) {
    return gp::Div(cx->a)
        ->W(kToolDlgComboDx)
        ->Shrink0()
        ->Child(gTool.ddDpi.BuildCombo(cx, StrL("pdftool-dpi"), gTool.dpiEdit, kToolDlgComboDx));
}

static gp::El* ToolPagesLine(gp::Ctx* cx, bool isExtract) {
    gp::El* line = ToolLine(cx, Tr("Pages"));
    line->Child(ToolLineInput(cx, GStrL("pdftool-pages"), gTool.pagesEdit));
    TempStr pages = InputTextTemp(gTool.pagesEdit);
    Vec<int> parsed;
    bool ok = PageRangeOk(pages, gTool.pageCount, isExtract, parsed);
    line->Child(ToolMeta(cx, PageRangeCountTemp(pages, gTool.pageCount, isExtract), ok));
    return line;
}

// Convert PDF to Images: the mode, the custom range, and the count the mode
// comes to
static gp::El* ToolPagesModeLine(gp::Ctx* cx) {
    gp::El* line = ToolLine(cx, Tr("Pages"));
    line->Child(RadioEl(cx, StrL("pdftool-cur"), Tr("Current"), PagesMode::Current));
    line->Child(RadioEl(cx, StrL("pdftool-all"), Tr("All"), PagesMode::All));
    line->Child(RadioEl(cx, StrL("pdftool-custom"), Tr("Custom"), PagesMode::Custom));
    bool custom = gTool.pagesMode == PagesMode::Custom;
    line->Child(ToolLineInput(cx, GStrL("pdftool-pages"), gTool.pagesEdit, false, !custom));
    if (custom) {
        TempStr pages = InputTextTemp(gTool.pagesEdit);
        Vec<int> parsed;
        bool ok = ParseDeletePages(pages, gTool.pageCount, parsed);
        line->Child(ToolMeta(cx, PageRangeCountTemp(pages, gTool.pageCount, true), ok));
    } else {
        int n = gTool.pagesMode == PagesMode::All ? gTool.pageCount : 1;
        line->Child(ToolMeta(cx, PageCountTextTemp(n, gTool.pageCount), true));
    }
    return line;
}

// "From" the source - the folder muted, the file name in the text color -
// and "Save as" the destination over a hairline, with the format drop-down
// of the image tools and Browse after it
static gp::El* ToolFilesBlock(gp::Ctx* cx) {
    Color colText = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    gp::El* box = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0()->PadX(kToolDlgPadDx)->PadY(kToolDlgFilesPadDy);
    auto row = [&](Str label) {
        gp::El* r = gp::Div(cx->a)
                        ->FlexRow()
                        ->ItemsCenter()
                        ->W(gp::kFill)
                        ->H(kToolDlgRowDy)
                        ->Shrink0()
                        ->Gap(kToolDlgGapDx)
                        ->Child(ToolLabel(cx, label));
        box->Child(r);
        return r;
    };

    gp::El* from = row(Tr("From"));
    Str name = path::GetBaseNameTemp(gTool.srcPath);
    Str dir = Str(gTool.srcPath.s, len(gTool.srcPath) - len(name));
    gp::El* path = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Flex1()->MinW(0)->PadL(kToolDlgInputPadDx);
    path->Child(ToolText(cx, dir, kToolDlgTextFontPx, colMuted)->Truncate()->MinW(0));
    path->Child(ToolText(cx, name, kToolDlgTextFontPx, colText)->Shrink0());
    from->Child(path);

    gp::El* save = row(Tr("Save as"));
    save->Child(DlgUnderlineInput(cx, GStrL("pdftool-dest"), gTool.destEdit, kToolDlgTextFontPx, ThemeEdgeColor())
                    ->Flex1()
                    ->MinW(0));
    if (HasFormatDropDown()) {
        save->Child(gp::Div(cx->a)
                        ->W(kToolDlgComboDx)
                        ->Shrink0()
                        ->Child(gTool.ddFormat.Build(cx, StrL("pdftool-format"), kToolDlgComboDx)));
    }
    save->Child(
        DlgGhostButton(cx, GStrL("pdftool-browse"), Tr("Browse..."), gp::ListenTo(gToolView, &PdfToolView::OnBrowse)));
    return box;
}

// the hint on the left, Cancel and the filled action button on the right
static gp::El* ToolDlgFooter(gp::Ctx* cx, Str hint) {
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->W(gp::kFill)
                      ->Shrink0()
                      ->PadX(kToolDlgPadDx)
                      ->PadY(kToolDlgFootPadDy)
                      ->Gap(kToolDlgGapDx);
    // files Convert would overwrite replace the hint (#6439)
    int nExisting = ExistingImageCount();
    if (nExisting > 0) {
        row->Child(ToolText(cx, ExistingImagesTextTemp(nExisting), kToolDlgMetaFontPx, ThemeWindowTextColor()));
        row->Child(DlgGhostButton(cx, GStrL("pdftool-newnames"), Tr("Use New Names"),
                                  gp::ListenTo(gToolView, &PdfToolView::OnUseNewNames)));
    } else if (len(hint) > 0) {
        row->Child(ToolText(cx, hint, kToolDlgMetaFontPx, ThemeWindowTextDisabledColor()));
    }
    row->Child(gp::Div(cx->a)->Flex1());
    row->Child(gpc::Button::New(cx, GStrL("pdftool-cancel"))
                   ->Label(ToGpui(Tr("Cancel")))
                   ->Outline()
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gToolView, &PdfToolView::OnCancel))
                   ->IntoEl());
    row->Child(DlgFilledButton(cx, GStrL("pdftool-action"), ToolActionText(),
                               gp::ListenTo(gToolView, &PdfToolView::OnAction), !ActionEnabled()));
    return row;
}

static gp::El* PdfToolColumn(MainWindow* win, gp::Ctx* cx) {
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0();
    Str hint;
    int nLines = 0;
    switch (gTool.kind) {
        case PdfToolKind::ExtractText:
        case PdfToolKind::DeletePages:
        case PdfToolKind::ExtractPages:
            col->Child(ToolPagesLine(cx, gTool.kind != PdfToolKind::DeletePages));
            nLines++;
            hint = PageRangeHint();
            break;
        case PdfToolKind::Encrypt:
            col->Child(
                ToolLine(cx, Tr("Password"))->Child(ToolLineInput(cx, GStrL("pdftool-pwd"), gTool.passwordEdit, true)));
            nLines++;
            break;
        case PdfToolKind::ConvertToImages:
            col->Child(ToolPagesModeLine(cx));
            col->Child(ToolLine(cx, Tr("Resolution (DPI)"))->Child(ToolDpiCombo(cx)));
            nLines += 2;
            hint = Tr("Use <N> for the page number");
            break;
        case PdfToolKind::ConvertEpubToPdf:
            col->Child(ToolLine(cx, Tr("Page size"))
                           ->Child(gp::Div(cx->a)
                                       ->W(kToolDlgComboDx)
                                       ->Shrink0()
                                       ->Child(gTool.ddFormat.Build(cx, StrL("pdftool-pagesize"), kToolDlgComboDx))));
            col->Child(ToolLine(cx, Tr("Font size"))->Child(ToolDpiCombo(cx)));
            nLines += 2;
            break;
        case PdfToolKind::SaveSelectionAsImage:
            col->Child(ToolLine(cx, Tr("Resolution (DPI)"))
                           ->Child(ToolDpiCombo(cx))
                           ->Child(gp::Div(cx->a)->Flex1())
                           ->Child(ToolMeta(cx, SelectionSizeTextTemp(win), ActionEnabled())));
            nLines++;
            break;
        default:
            break;
    }
    if (nLines > 0) {
        col->Child(DlgHairline(cx));
    }
    col->Child(ToolFilesBlock(cx));
    col->Child(ToolDlgFooter(cx, hint));
    return col;
}

// orig's FinishDialog(focusOn)
static void PdfToolFocusFirst(gp::Ctx* cx) {
    if (!gTool.wantFocus) {
        return;
    }
    gTool.wantFocus = false;
    gp::InputState* focusOn = gTool.focusPages && gTool.pagesEdit ? gTool.pagesEdit : gTool.destEdit;
    if (gTool.kind == PdfToolKind::Encrypt) {
        focusOn = gTool.passwordEdit;
    }
    gp::InputFocus(focusOn, cx->app, cx->win);
    gp::InputSelectAll(focusOn, cx->app, cx->win);
    logf("PdfTool: %s\n", PdfToolStateTemp());
}

static gp::El* PdfToolBuildCurrent(MainWindow* win, gp::Ctx* cx) {
    if (!gTool.visible || gTool.win != win || gTool.tw) {
        return nullptr;
    }
    if (gTool.dlgTw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gToolView.IsValid()) {
        gToolView = gp::EntityNewState<PdfToolView>(cx->app);
    }
    if (gTool.kind == PdfToolKind::Merge && len(gTool.mergeAskPaths) > 0) {
        return MergeAskDialogBuild(cx);
    }
    if (HasFormatDropDown() && gTool.ddFormat.PollChanged(cx->app)) {
        SetDestExtFromFormat();
    }
    if (gTool.kind == PdfToolKind::ConvertEpubToPdf) {
        gTool.ddFormat.PollChanged(cx->app);
    }
    if (HasDpiCombo()) {
        gTool.dpiEdit->onChange = gp::ListenTo(gToolView, &PdfToolView::OnInput);
        gTool.ddDpi.TakeComboPicked();
    }
    for (gp::InputState* e : {gTool.destEdit, gTool.pagesEdit, gTool.passwordEdit}) {
        if (e) {
            e->onChange = gp::ListenTo(gToolView, &PdfToolView::OnInput);
        }
    }
    if (DlgWindowIsHost(cx)) {
        gp::El* content = DlgWinContent(cx, PdfToolColumn(win, cx));
        PdfToolFocusFirst(cx);
        return content;
    }

    gp::El* body;
    gp::El* footer = nullptr;
    float dialogDx = 600;
    if (gTool.kind == PdfToolKind::Merge) {
        float chromeDy = kMergeCardChromeDy + MergeLegendDy(cx, MergeLegendDx(0));
        float viewDy = std::max(kMergeCardMinViewDy, gp::WindowSize(cx->win).dipH - chromeDy);
        body = MergeColumn(cx, 0, viewDy);
        footer = gp::Div(cx->a)->H(0);
        // orig's kDialogDx, as far as the window allows
        dialogDx = std::max(dialogDx, std::min((float)kMergeDialogDx, gp::WindowSize(cx->win).dipW - 80));
    } else {
        body = PdfToolColumn(win, cx);
    }
    gpc::Dialog* d = gpc::Dialog::New(cx)
                         ->Open(true)
                         ->Title(GpuiDup(cx->a, ToolTitle()))
                         ->Body(body)
                         ->W(dialogDx)
                         ->OnClose(gp::ListenTo(gToolView, &PdfToolView::OnCancel));
    if (footer) {
        d->Footer(footer);
    }
    gp::El* dlg = DlgIntoEl(cx, d);

    PdfToolFocusFirst(cx);
    return dlg;
}

gp::El* PdfToolDialogBuild(MainWindow* win, gp::Ctx* cx) {
    PdfToolDlg* tool = ToolForCx(cx);
    // a tool's window whose state is gone has nothing to draw
    if (tool == &gMainTool && DlgWindowIsHost(cx)) {
        return nullptr;
    }
    ToolScope scope(tool);
    return PdfToolBuildCurrent(win, cx);
}
