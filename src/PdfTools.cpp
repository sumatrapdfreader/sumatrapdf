/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "Annotation.h"
#include "PdfCreator.h"
#include "ImageReader.h"
#include "PngOptimizer.h"
#include "base/Pixmap.h"
#include "base/GdiPlusUtil.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Translations.h"
#include "ExternalViewers.h"
#include "Flags.h"
#include "DisplayModel.h"
#include "Selection.h"
#include "Theme.h"
#include "Notifications.h"

#include "DarkMode.h"
#include "Commands.h"
#include "PdfTools.h"
#include "PdfToolsCommon.h"

// compute a dialog client width that fits the source path text, clamped to a
// minimum and to 80% of the screen width (long paths get ellipsized instead)
static int CalcDlgWidth(PlatformFont* font, Str path, int minW, int padding) {
    Size size = PlatformFontMeasureText(font, path);
    int dlgW = size.dx + (2 * padding) + DpiScale(32);
    dlgW = std::max(dlgW, minW);
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    dlgW = std::min(dlgW, screenW * 80 / 100);
    return dlgW;
}

// the client width the user last dragged a tool dialog to, kept while the
// process runs so the next one opens that wide, as Advanced Settings does
static int gToolDlgLastClientDx = 0;

// the Sumatra app icon, shown in each tool dialog's title bar / taskbar
static HICON GetAppIcon() {
    return LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
}

// shared "Save As" browse used by the layout-based PDF tool dialogs below.
// Seeds the dialog with the edit's current text and writes the chosen path back.
static void BrowseForDest(HWND owner, Edit* edit, WStr filter, WStr defExt) {
    WCHAR dstFileName[MAX_PATH + 1]{};
    TempWStr currentFileName = ToWStrTemp(HwndGetTextTemp(edit->hwnd));
    wstr::BufSet(WStr(dstFileName, MAX_PATH), currentFileName);

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = filter.s;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = defExt.s;

    if (GetSaveFileNameW(&ofn)) {
        edit->SetText(ToUtf8Temp(dstFileName));
    }
}

// PNG / JPEG / BMP, same core formats as the Save Image dialog
struct ConvertImageFormat {
    Str label;
    Str ext; // including the dot, e.g. ".png"
    WStr mime;
    WStr defExt; // OPENFILENAME lpstrDefExt, no dot
};

static const ConvertImageFormat kConvertImageFormats[] = {
    {StrL("PNG"), StrL(".png"), L"image/png", L"png"},
    {StrL("JPEG"), StrL(".jpg"), L"image/jpeg", L"jpg"},
    {StrL("BMP"), StrL(".bmp"), L"image/bmp", L"bmp"},
};

static int ConvertImageFormatCount() {
    return dimofi(kConvertImageFormats);
}

// Every dialog here has the same shape, drawn like the palette: the lines
// the user types in (a page range, a password), then the files - the source
// and a destination over a hairline, with Browse - then the footer with a
// hint, Cancel and the one filled action button, hairlines between the
// three. The labels and buttons are virtual controls, the text fields
// native edits, all in one layout tree. Each dialog adds its lines and
// implements DoIt().
struct PdfToolDialog : WindowBase {
    Str srcPath; // owned
    MainWindow* win = nullptr;
    // what the Browse button's save dialog offers
    WStr browseFilter;
    WStr browseDefExt;

    VBox* mainBox = nullptr;
    Edit* destEdit = nullptr;
    // PNG / JPEG / BMP next to the destination of the image tools
    DropDown* dropFormat = nullptr;
    bool syncingFormat = false;
    VirtButton* browseBtn = nullptr;
    VirtButton* actionBtn = nullptr;
    VirtButton* cancelBtn = nullptr;
    // the line AddLine() built last, so a dialog can put more in it
    HBox* lastLine = nullptr;
    // the text at a line's right end: the pages a range picks, an image's size
    VirtText* metaText = nullptr;
    int nLines = 0;
    Color colTxt = kColorUnset;
    Color colBg = kColorUnset;
    Color colMuted = kColorUnset;
    PlatformFont* fontText = nullptr;
    PlatformFont* fontQuery = nullptr;
    PlatformFont* fontMeta = nullptr;
    // the window's height once laid out; the frame can only be dragged wider
    int windowDy = 0;

    ~PdfToolDialog() override;

    bool CreateToolDialog(MainWindow*, WindowTab*, Str title);
    HBox* NewRow();
    VirtText* NewLabel(Str s, PlatformFont* font, Color col, int dy = 0, int dx = 0);
    Edit* NewEdit(Str text, PlatformFont* font, bool isPassword = false);
    HBox* AddLine(Str label);
    Edit* AddLineEdit(Str text, bool isPassword = false);
    VirtText* AddLineMeta();
    void SetMeta(Str s, bool ok);
    void AddFilesBlock(Str destPath, WStr filter, WStr defExt, bool withFormat = false);
    void AddFooter(Str actionText, Str hint = {});
    void FinishDialog(Edit* focusOn);
    void SetActionEnabled(bool enabled);

    int SelectedFormatIdx() const;
    void SetDestExtFromFormat();
    void SyncFormatFromPath(Str path);
    void OnFormatChanged();
    void OnBrowse(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev);
    void OnSize(WindowBase::SizeEvent* ev);
    // what the action button does; the only thing the dialogs really differ in
    virtual void DoIt(VirtMouseEvent* ev = nullptr) {}
};

PdfToolDialog::~PdfToolDialog() {
    str::FreePtr(&srcPath);
    // ~WindowBase deletes `layout`, which owns the whole tree - the virtual
    // controls and the edits alike
}

static void PdfToolDialogOnClose(WindowBase::CloseEvent* ev) {
    auto* dlg = (PdfToolDialog*)ev->e->self;
    // hand activation back to the main window ourselves: after the browse
    // dialog has been up, letting DestroyWindow pick the next window can drop
    // the frame behind whatever else is on screen
    if (dlg->win && IsWindow(dlg->win->hwndFrame)) {
        SetActiveWindow(dlg->win->hwndFrame);
    }
    // must not `delete dlg` here: we're inside the window's own WM_CLOSE
    dlg->ScheduleDelete();
}

void PdfToolDialog::OnCancel(VirtMouseEvent*) {
    Close();
}

// no narrower than the layout's minimum, never taller than the layout
void PdfToolDialog::OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev) {
    if (!ev->mmi || windowDy == 0) {
        return;
    }
    int chromeDx = HwndWindowRect(hwnd).dx - HwndClientRect(hwnd).dx;
    ev->mmi->ptMinTrackSize.x = DpiScale(kToolDlgMinDx) + chromeDx;
    ev->mmi->ptMinTrackSize.y = windowDy;
    ev->mmi->ptMaxTrackSize.y = windowDy;
}

void PdfToolDialog::OnSize(WindowBase::SizeEvent* ev) {
    if (windowDy > 0 && ev->size.dx > 0) {
        gToolDlgLastClientDx = ev->size.dx;
    }
}

void PdfToolDialog::OnBrowse(VirtMouseEvent*) {
    BrowseForDest(hwnd, destEdit, browseFilter, browseDefExt);
}

bool PdfToolDialog::CreateToolDialog(MainWindow* w, WindowTab* tab, Str title) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    colTxt = ThemeWindowTextColor();
    colBg = ThemeWindowControlBackgroundColor();
    colMuted = ThemeWindowTextDisabledColor();
    closeOnEsc = true;
    onClose = MkFunc1Void(PdfToolDialogOnClose);

    CreateCustomArgs cargs;
    cargs.title = title;
    cargs.font = GetDefaultGuiFont();
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    cargs.bgColor = colBg;
    onGetMinMaxInfo = MkMethod1<PdfToolDialog, WindowBase::GetMinMaxInfoEvent*, &PdfToolDialog::OnGetMinMaxInfo>(this);
    onSize = MkMethod1<PdfToolDialog, WindowBase::SizeEvent*, &PdfToolDialog::OnSize>(this);
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    // make the dialog an owned (rather than child) window of the main frame
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);
    SetColors(colTxt, colBg);

    fontText = DlgUiFont(kToolDlgTextFontPx);
    fontQuery = DlgUiFont(kToolDlgQueryFontPx);
    fontMeta = DlgUiFont(kToolDlgMetaFontPx);
    mainBox = new VBox();
    mainBox->alignCross = CrossAxisAlign::Stretch;
    layout = mainBox;
    return true;
}

// a row with its content centered and the dialog's gap between the controls
HBox* PdfToolDialog::NewRow() {
    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->rtl = IsUIRtl();
    row->gap = DpiScale(kToolDlgGapDx);
    return row;
}

// `dy`: the label pads itself to that height, and so sets its row's;
// `dx`: the label column's width
VirtText* PdfToolDialog::NewLabel(Str s, PlatformFont* font, Color col, int dy, int dx) {
    VirtText* t = NewVirtText({.s = s, .font = font, .textColor = col, .isRtl = IsUIRtl()});
    if (dy > 0) {
        int lineDy = PlatformFontLineHeight(font);
        t->padding.top = std::max(0, (DpiScale(dy) - lineDy) / 2);
        t->padding.bottom = std::max(0, DpiScale(dy) - lineDy - t->padding.top);
    }
    if (dx > 0) {
        int textDx = PlatformFontMeasureText(font, s).dx;
        t->padding.right = std::max(0, DpiScale(dx) - textDx);
    }
    return t;
}

// a bare edit, in the dialog's colors
Edit* PdfToolDialog::NewEdit(Str text, PlatformFont* font, bool isPassword) {
    Edit::CreateArgs args;
    args.parent = hwnd;
    args.withBorder = false;
    args.font = font;
    args.text = text;
    args.isRtl = IsUIRtl();
    auto* e = new Edit();
    e->SetColors(colTxt, colBg);
    e->Create(args);
    if (isPassword) {
        HwndSetWindowStyle(e->hwnd, ES_PASSWORD, true);
    }
    return e;
}

// a line of the top section: its label, then what the caller adds
HBox* PdfToolDialog::AddLine(Str label) {
    HBox* row = NewRow();
    row->AddChild(NewLabel(label, fontText, colMuted, kToolDlgLineDy, kToolDlgLabelDx));
    mainBox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
    lastLine = row;
    nLines++;
    return row;
}

// the line's field, in the larger font: what the dialog is about
Edit* PdfToolDialog::AddLineEdit(Str text, bool isPassword) {
    Edit* e = NewEdit(text, fontQuery, isPassword);
    lastLine->AddChild(e, 1);
    return e;
}

VirtText* PdfToolDialog::AddLineMeta() {
    metaText = NewLabel({}, fontMeta, colMuted);
    lastLine->AddChild(metaText);
    return metaText;
}

// the meta text fades when what it describes is not usable
void PdfToolDialog::SetMeta(Str s, bool ok) {
    if (!metaText) {
        return;
    }
    metaText->SetText(s);
    metaText->SetColor(kColText, ok ? colMuted : AccentColor(colMuted, kToolDlgFaintShift));
    if (layout) {
        DoLayout(); // the text's width changed
    }
    HwndInvalidate(hwnd);
}

// the source path: the folder muted, the file name in the text color, the
// folder cut in the middle when the row is too narrow for both
static void PaintSrcPath(PdfToolDialog* dlg, VirtPaintCtx* ctx) {
    PlatformFont* font = dlg->fontText;
    Str name = path::GetBaseNameTemp(dlg->srcPath);
    Str dir = Str(dlg->srcPath.s, len(dlg->srcPath) - len(name));
    Rect r = ctx->bounds;
    int nameDx = ctx->gfx->MeasureText(name, font).dx;
    int dirDx = std::min(ctx->gfx->MeasureText(dir, font).dx, r.dx - nameDx);
    u32 flags = gfxTextVCenter;
    if (dirDx > 0) {
        ctx->gfx->DrawText(dir, {r.x, r.y, dirDx, r.dy}, flags | gfxTextPathEllipsis, font, dlg->colMuted);
    }
    dirDx = std::max(dirDx, 0);
    ctx->gfx->DrawText(name, {r.x + dirDx, r.y, r.dx - dirDx, r.dy}, flags | gfxTextEllipsis, font, dlg->colTxt);
}

// PNG / JPEG / BMP, as the Save Image dialog has them
static DropDown* NewFormatDropDown(HWND parent, PlatformFont* font) {
    auto* dd = new DropDown();
    DropDown::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.isRtl = IsUIRtl();
    dd->Create(args);
    StrVec items;
    for (int i = 0; i < ConvertImageFormatCount(); i++) {
        items.Append(kConvertImageFormats[i].label);
    }
    dd->SetItems(items);
    CbSetCurrentSelection(dd, 0);
    dd->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    return dd;
}

// "From" the source, "Save as" the destination over a hairline, with the
// format drop-down of the image tools and Browse after it
void PdfToolDialog::AddFilesBlock(Str destPath, WStr filter, WStr defExt, bool withFormat) {
    browseFilter = filter;
    browseDefExt = defExt;
    if (nLines > 0) {
        mainBox->AddChild(new VirtLine());
    }
    auto* box = new VBox();
    box->alignCross = CrossAxisAlign::Stretch;

    HBox* from = NewRow();
    from->AddChild(NewLabel(Tr("From"), fontText, colMuted, kToolDlgRowDy, kToolDlgLabelDx));
    auto* path = new VirtCustom();
    path->idealSize = {PlatformFontMeasureText(fontText, srcPath).dx, PlatformFontLineHeight(fontText)};
    path->onPaint = MkFunc1(PaintSrcPath, this);
    path->SetFlag(vwfNoHitTest, true);
    box->AddChild(from);

    HBox* save = NewRow();
    save->AddChild(NewLabel(Tr("Save as"), fontText, colMuted, kToolDlgRowDy, kToolDlgLabelDx));
    destEdit = NewEdit(destPath, fontText);
    // the source path lines up with the destination's text
    from->AddChild(new Padding(path, Insets{0, 0, 0, destEdit->GetLeftTextMargin()}), 1);
    auto* underlined = new VBox();
    underlined->alignCross = CrossAxisAlign::Stretch;
    underlined->AddChild(destEdit);
    underlined->AddChild(new VirtLine());
    save->AddChild(underlined, 1);
    if (withFormat) {
        dropFormat = NewFormatDropDown(hwnd, fontText);
        dropFormat->onSelectionChanged = MkMethod0<PdfToolDialog, &PdfToolDialog::OnFormatChanged>(this);
        save->AddChild(dropFormat);
    }
    browseBtn = NewDlgButton(hwnd, Tr("Browse..."), fontMeta, DlgButtonStyle::Ghost, false);
    browseBtn->onClick = MkMethod1<PdfToolDialog, VirtMouseEvent*, &PdfToolDialog::OnBrowse>(this);
    save->AddChild(browseBtn);
    box->AddChild(save);
    lastLine = save;

    mainBox->AddChild(new Padding(box, DpiScaledInsets(kToolDlgFilesPadDy, kToolDlgPadDx)));
}

// the hint on the left, Cancel and the action button (the default) on the right
void PdfToolDialog::AddFooter(Str actionText, Str hint) {
    mainBox->AddChild(new VirtLine());
    HBox* row = NewRow();
    if (hint) {
        row->AddChild(NewLabel(hint, fontMeta, colMuted));
    }
    row->AddChild(new Spacer(0, 0), 1);

    cancelBtn = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    cancelBtn->onClick = MkMethod1<PdfToolDialog, VirtMouseEvent*, &PdfToolDialog::OnCancel>(this);
    row->AddChild(cancelBtn);

    actionBtn = NewDlgButton(hwnd, actionText, fontText, DlgButtonStyle::Filled, true);
    actionBtn->onClick = MkMethod1<PdfToolDialog, VirtMouseEvent*, &PdfToolDialog::DoIt>(this);
    row->AddChild(actionBtn);
    mainBox->AddChild(new Padding(row, DpiScaledInsets(kToolDlgFootPadDy, kToolDlgPadDx)));
}

void PdfToolDialog::FinishDialog(Edit* focusOn) {
    // size to a width that fits the source path (clamped), let the layout
    // compute the height
    int clientW = CalcDlgWidth(fontText, srcPath, DpiScale(kToolDlgMinDx), DpiScale(kToolDlgPadDx));
    if (gToolDlgLastClientDx > 0) {
        clientW = std::max(gToolDlgLastClientDx, DpiScale(kToolDlgMinDx));
    }
    Size size = layout->Layout(ExpandHeight(clientW));
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);
    windowDy = HwndWindowRect(hwnd).dy;
    // positions everything and picks up the virtual controls to paint
    DoLayout(size);

    HwndCenterDialog(hwnd, win->hwndFrame);
    UpdateTheme();
    SetIsVisible(true);
    EditSetFocus(focusOn);
}

void PdfToolDialog::SetActionEnabled(bool enabled) {
    if (actionBtn) {
        SetDlgButtonEnabled(actionBtn, DlgButtonStyle::Filled, enabled);
    }
}

int PdfToolDialog::SelectedFormatIdx() const {
    if (!dropFormat) {
        return 0;
    }
    int idx = CbGetCurrentSelection(dropFormat);
    if (idx < 0 || idx >= ConvertImageFormatCount()) {
        return 0;
    }
    return idx;
}

void PdfToolDialog::SetDestExtFromFormat() {
    if (!destEdit) {
        return;
    }
    TempStr dest = destEdit->GetTextTemp();
    if (len(dest) == 0) {
        return;
    }
    Str ext = kConvertImageFormats[SelectedFormatIdx()].ext;
    TempStr noExt = path::GetPathNoExtTemp(dest);
    TempStr newDest = str::JoinTemp(noExt, ext);
    if (str::EqI(dest, newDest)) {
        return;
    }
    destEdit->SetText(newDest);
}

void PdfToolDialog::SyncFormatFromPath(Str path) {
    if (!dropFormat) {
        return;
    }
    int idx = ConvertImageFormatIdxFromPath(path);
    if (idx == CbGetCurrentSelection(dropFormat)) {
        return;
    }
    syncingFormat = true;
    CbSetCurrentSelection(dropFormat, idx);
    syncingFormat = false;
}

void PdfToolDialog::OnFormatChanged() {
    if (syncingFormat) {
        return;
    }
    SetDestExtFromFormat();
}

// a dialog with a page range: the line counts the pages the range picks and
// the action waits for a usable one
struct PdfPagesDialog : PdfToolDialog {
    Edit* pagesEdit = nullptr;
    int pageCount = 0;
    bool isExtract = true;

    void AddPagesLine(Str pages);
    void UpdatePages();
};

void PdfPagesDialog::AddPagesLine(Str pages) {
    AddLine(Tr("Pages"));
    pagesEdit = AddLineEdit(pages);
    AddLineMeta();
}

void PdfPagesDialog::UpdatePages() {
    TempStr pages = pagesEdit->GetTextTemp();
    Vec<int> parsed;
    bool ok = PageRangeOk(pages, pageCount, isExtract, parsed);
    SetMeta(PageRangeCountTemp(pages, pageCount, isExtract), ok);
    SetActionEnabled(ok);
}

struct PdfBakeDialog : PdfToolDialog {
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void PdfBakeDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    Str inputPath = srcPath;
    TempStr tmpPath;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    // pdfbake_main re-opens the file from disk, so unsaved session
    // annotations would be missing unless we write them out first (issue #5977).
    if (engine && EngineHasUnsavedAnnotations(engine)) {
        tmpPath = GetTempFilePathTemp(StrL("bake"));
        if (len(tmpPath) == 0 || !EngineMupdfSaveCopy(engine, tmpPath)) {
            MessageBoxWarning(hwnd, StrL("Failed to bake PDF file."), Tr("Bake PDF"));
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
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        // open the baked file
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    } else {
        logf("PdfBakeDoIt: pdfbake_main failed with %d\n", res);
        MessageBoxWarning(hwnd, StrL("Failed to bake PDF file."), Tr("Bake PDF"));
    }
}

bool PdfBakeDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Bake PDF"))) {
        return false;
    }
    AddFilesBlock(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddFooter(Tr("Bake PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfBakeDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    if (!IsPdfDoc(tab)) {
        return;
    }
    logf("ShowPdfBakeDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new PdfBakeDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Extract PDF Text dialog ---

struct PdfExtractTextDialog : PdfPagesDialog {
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void PdfExtractTextDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    TempStr pages = pagesEdit->GetTextTemp();
    if (len(pages) == 0) {
        return;
    }

    logf("PdfExtractTextDoIt: extracting text from '%s' to '%s', pages: %s\n", srcPath, destPath, pages);

    bool ok = false;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    bool isPdf = tab && IsPdfDoc(tab);
    if (isPdf) {
        // use muconvert for PDF
        char* argv[] = {(char*)"convert", (char*)"-o", CStrTemp(destPath), CStrTemp(srcPath), CStrTemp(pages)};
        int argc = 5;
        fz_set_optind(0);
        ok = muconvert_main(argc, argv) == 0;
    } else {
        // use engine text extraction for other formats (DjVu, etc.)
        ok = ExtractTextViaEngine(win, destPath, pages);
    }

    if (ok) {
        logf("PdfExtractTextDoIt: extracted successfully\n");
        TempStr path = str::DupTemp(destPath);
        Close();
        OpenPathInDefaultFileManager(path);
    } else {
        logf("PdfExtractTextDoIt: failed to extract text, isPdf: %d\n", (int)isPdf);
        MessageBoxWarning(hwnd, StrL("Failed to extract text."), Tr("Extract Text"));
    }
}

bool PdfExtractTextDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Extract Text From PDF"))) {
        return false;
    }
    pageCount = w->ctrl ? w->ctrl->PageCount() : 1;
    AddPagesLine(fmt("1-%d", pageCount));
    TempStr noExt = path::GetPathNoExtTemp(srcPath);
    TempStr txtPath = str::JoinTemp(noExt, StrL(".txt"));
    AddFilesBlock(MakeUniqueFilePathTemp(txtPath), L"Text Files\0*.txt\0All Files\0*.*\0", L"txt");
    AddFooter(Tr("Extract Text"), PageRangeHint());
    FinishDialog(pagesEdit);
    pagesEdit->onTextChanged = MkMethod0<PdfPagesDialog, &PdfPagesDialog::UpdatePages>(this);
    UpdatePages();
    return true;
}

void ShowPdfExtractTextDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    logf("ShowPdfExtractTextDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new PdfExtractTextDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Compress PDF dialog ---

struct PdfCompressDialog : PdfToolDialog {
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void PdfCompressDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    logf("PdfCompressDoIt: compressing '%s' to '%s'\n", srcPath, destPath);

    // equivalent of: clean -gggg -e 100 -f -i -t -Z input output
    char* argv[] = {(char*)"clean", (char*)"-gggg", (char*)"-e", (char*)"100",      (char*)"-f",
                    (char*)"-i",    (char*)"-t",    (char*)"-Z", CStrTemp(srcPath), CStrTemp(destPath)};
    int argc = 10;

    fz_set_optind(0);
    int res = pdfclean_main(argc, argv);
    if (res == 0) {
        logf("PdfCompressDoIt: compressed successfully\n");
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    } else {
        logf("PdfCompressDoIt: pdfclean_main failed with %d\n", res);
        MessageBoxWarning(hwnd, StrL("Failed to compress PDF file."), Tr("Compress PDF"));
    }
}

bool PdfCompressDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Compress PDF"))) {
        return false;
    }
    AddFilesBlock(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddFooter(Tr("Compress PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfCompressDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    if (!IsPdfDoc(tab)) {
        return;
    }
    logf("ShowPdfCompressDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new PdfCompressDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Decompress PDF dialog ---

struct PdfDecompressDialog : PdfToolDialog {
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void PdfDecompressDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    logf("PdfDecompressDoIt: decompressing '%s' to '%s'\n", srcPath, destPath);

    // equivalent of: clean -d input output
    char* argv[] = {(char*)"clean", (char*)"-d", CStrTemp(srcPath), CStrTemp(destPath)};
    int argc = 4;

    fz_set_optind(0);
    int res = pdfclean_main(argc, argv);
    if (res == 0) {
        logf("PdfDecompressDoIt: decompressed successfully\n");
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    } else {
        logf("PdfDecompressDoIt: pdfclean_main failed with %d\n", res);
        MessageBoxWarning(hwnd, StrL("Failed to decompress PDF file."), Tr("Decompress PDF"));
    }
}

bool PdfDecompressDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Decompress PDF"))) {
        return false;
    }
    AddFilesBlock(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddFooter(Tr("Decompress PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfDecompressDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    if (!IsPdfDoc(tab)) {
        return;
    }
    logf("ShowPdfDecompressDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new PdfDecompressDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Delete Pages From PDF dialog ---

struct PdfDeletePageDialog : PdfPagesDialog {
    bool Create(MainWindow* win, WindowTab* tab, bool isExtract);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
    void PreTranslate(WindowBase::PreTranslateEvent* ev);
};

void PdfDeletePageDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    TempStr pages = pagesEdit->GetTextTemp();

    Vec<int> parsedPages;
    if (!PageRangeOk(pages, pageCount, isExtract, parsedPages)) {
        return;
    }

    WindowTab* sourceTab = isExtract ? FindTabByFile(srcPath, win) : nullptr;
    EngineBase* sourceEngine = sourceTab ? sourceTab->GetEngine() : nullptr;

    TempStr pageRange;
    if (isExtract) {
        // for extract: pass the specified pages directly to pdfclean
        pageRange = FormatPageRangeTemp(parsedPages);
    } else {
        // for delete: pass the complement (pages to keep) to pdfclean
        pageRange = BuildKeepPagesRangeTemp(pageCount, parsedPages);
    }

    Str op = isExtract ? StrL("extract") : StrL("delete");
    logf("PdfDeletePageDoIt: %s pages '%s' from '%s' to '%s', range for pdfclean: %s\n", op, pages, srcPath, destPath,
         pageRange);

    Str inputPath = srcPath;
    TempStr tmpPath;
    if (isExtract && sourceEngine && EngineHasUnsavedAnnotations(sourceEngine)) {
        tmpPath = GetTempFilePathTemp(StrL("extract-pages"));
        if (len(tmpPath) == 0 || !EngineMupdfSaveCopy(sourceEngine, tmpPath)) {
            MessageBoxWarning(hwnd, StrL("Failed to extract pages from PDF file."), Tr("Extract Pages From PDF"));
            return;
        }
        inputPath = tmpPath;
    }

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
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    } else {
        logf("PdfDeletePageDoIt: pdfclean_main failed with %d for %s\n", res, op);
        Str msg =
            isExtract ? StrL("Failed to extract pages from PDF file.") : StrL("Failed to delete pages from PDF file.");
        Str title = isExtract ? Tr("Extract Pages From PDF") : Tr("Delete Pages From PDF");
        MessageBoxWarning(hwnd, msg, title);
    }
}

// Page Up / Page Down skip the edit accelerator table; send next/prev page.
void PdfDeletePageDialog::PreTranslate(WindowBase::PreTranslateEvent* ev) {
    MSG& msg = *ev->msg;
    if (msg.message != WM_KEYDOWN) {
        return;
    }
    if (IsCtrlPressed() || IsAltPressed() || IsShiftPressed()) {
        return;
    }
    int cmdId = 0;
    if (msg.wParam == VK_NEXT) {
        cmdId = CmdGoToNextPage;
    } else if (msg.wParam == VK_PRIOR) {
        cmdId = CmdGoToPrevPage;
    } else {
        return;
    }
    if (!win || !win->hwndFrame) {
        return;
    }
    HwndSendCommand(win->hwndFrame, cmdId);
    ev->didHandle = true;
}

bool PdfDeletePageDialog::Create(MainWindow* w, WindowTab* tab, bool isExtractArg) {
    isExtract = isExtractArg;
    Str title = isExtract ? Tr("Extract Pages From PDF") : Tr("Delete Pages From PDF");
    if (!CreateToolDialog(w, tab, title)) {
        return false;
    }
    pageCount = w->ctrl ? w->ctrl->PageCount() : 0;
    int currentPage = w->ctrl ? w->ctrl->CurrentPageNo() : 1;
    AddPagesLine(fmt("%d", currentPage));
    AddFilesBlock(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");

    Str actionText = isExtract ? Tr("Extract Pages") : Tr("Delete Pages");
    AddFooter(actionText, PageRangeHint());
    onPreTranslate =
        MkMethod1<PdfDeletePageDialog, WindowBase::PreTranslateEvent*, &PdfDeletePageDialog::PreTranslate>(this);
    FinishDialog(pagesEdit);

    // attach the change handler only now that actionBtn exists, then set the
    // initial validation state (Edit::Create fires onTextChanged on initial text)
    pagesEdit->onTextChanged = MkMethod0<PdfPagesDialog, &PdfPagesDialog::UpdatePages>(this);
    UpdatePages();
    return true;
}

void ShowPdfPageRangeDialog(MainWindow* win, bool isExtract) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    if (!IsPdfDoc(tab)) {
        return;
    }

    int pageCount = win->ctrl ? win->ctrl->PageCount() : 0;
    if (pageCount < 2) {
        return;
    }
    logf("ShowPdfPageRangeDialog: opening %s dialog for '%s', %d pages\n", Str(isExtract ? "extract" : "delete"),
         tab->filePath, pageCount);

    auto* dlg = new PdfDeletePageDialog();
    if (!dlg->Create(win, tab, isExtract)) {
        delete dlg;
    }
}

// --- Encrypt PDF dialog ---

struct PdfEncryptDialog : PdfToolDialog {
    Edit* passwordEdit = nullptr;
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
    void UpdateButton();
};

// there is nothing to encrypt with until a password is typed
void PdfEncryptDialog::UpdateButton() {
    TempStr pwd = passwordEdit->GetTextTemp();
    SetActionEnabled(len(pwd) > 0);
}

void PdfEncryptDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    TempStr pwd = passwordEdit->GetTextTemp();
    if (len(pwd) == 0) {
        return;
    }

    logf("PdfEncryptDoIt: encrypting '%s' to '%s' with AES-256\n", srcPath, destPath);

    // equivalent of: clean -E aes-256 -U <pwd> -O <pwd> input output
    char* pwdZ = CStrTemp(pwd);
    char* argv[] = {(char*)"clean", (char*)"-E", (char*)"aes-256",  (char*)"-U",       pwdZ,
                    (char*)"-O",    pwdZ,        CStrTemp(srcPath), CStrTemp(destPath)};
    int argc = 9;

    fz_set_optind(0);
    int res = pdfclean_main(argc, argv);
    if (res == 0) {
        logf("PdfEncryptDoIt: encrypted successfully\n");
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    } else {
        logf("PdfEncryptDoIt: pdfclean_main failed with %d\n", res);
        MessageBoxWarning(hwnd, StrL("Failed to encrypt PDF file."), Tr("Encrypt PDF"));
    }
}

bool PdfEncryptDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Encrypt PDF"))) {
        return false;
    }
    AddLine(Tr("Password"));
    passwordEdit = AddLineEdit({}, true);
    AddFilesBlock(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddFooter(Tr("Encrypt PDF"));
    FinishDialog(passwordEdit);
    passwordEdit->onTextChanged = MkMethod0<PdfEncryptDialog, &PdfEncryptDialog::UpdateButton>(this);
    UpdateButton();
    return true;
}

void ShowPdfEncryptDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    if (!IsPdfDoc(tab)) {
        return;
    }
    EngineBase* engine = tab->GetEngine();
    if (EngineMupdfIsEncrypted(engine)) {
        logf("ShowPdfEncryptDialog: '%s' is already encrypted, skipping\n", tab->filePath);
        return;
    }
    logf("ShowPdfEncryptDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new PdfEncryptDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Decrypt PDF dialog ---

struct PdfDecryptDialog : PdfToolDialog {
    // the password the document was opened with
    Str password;

    ~PdfDecryptDialog() override;
    bool Create(MainWindow* win, WindowTab* tab, Str pwd);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

PdfDecryptDialog::~PdfDecryptDialog() {
    str::FreePtr(&password);
}

void PdfDecryptDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    logf("PdfDecryptDoIt: decrypting '%s' to '%s', password len: %d\n", srcPath, destPath, len(password));

    // equivalent of: clean -p <pwd> -D input output
    // -p provides the password to open the encrypted input, -D removes encryption from output
    char* argv[] = {(char*)"clean", (char*)"-p",       CStrTemp(password),
                    (char*)"-D",    CStrTemp(srcPath), CStrTemp(destPath)};
    int argc = 6;

    fz_set_optind(0);
    int res = pdfclean_main(argc, argv);
    if (res == 0) {
        logf("PdfDecryptDoIt: decrypted successfully\n");
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    } else {
        logf("PdfDecryptDoIt: pdfclean_main failed with %d, src: '%s', password len: %d\n", res, srcPath,
             len(password));
        MessageBoxWarning(hwnd, StrL("Failed to decrypt PDF file."), Tr("Decrypt PDF"));
    }
}

bool PdfDecryptDialog::Create(MainWindow* w, WindowTab* tab, Str pwd) {
    if (!CreateToolDialog(w, tab, Tr("Decrypt PDF"))) {
        return false;
    }
    password = str::Dup(pwd);
    AddFilesBlock(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddFooter(Tr("Decrypt PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfDecryptDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    if (!IsPdfDoc(tab)) {
        return;
    }
    EngineBase* engine = tab->GetEngine();
    if (!EngineMupdfIsEncrypted(engine)) {
        logf("ShowPdfDecryptDialog: '%s' is not encrypted, skipping\n", tab->filePath);
        return;
    }
    Str pwd = EngineMupdfGetPassword(engine);
    if (len(pwd) == 0) {
        logf("ShowPdfDecryptDialog: '%s' is encrypted but no password available\n", tab->filePath);
        return;
    }
    logf("ShowPdfDecryptDialog: opening for '%s', password len: %d\n", tab->filePath, len(pwd));

    auto* dlg = new PdfDecryptDialog();
    if (!dlg->Create(win, tab, pwd)) {
        delete dlg;
    }
}

// --- Convert to PDF dialog (comics, image folders, single images; issue #4118) ---

struct ConvertToPdfDialog : PdfToolDialog {
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void ConvertToPdfDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine || !engine->isImageCollection) {
        MessageBoxWarning(hwnd, Tr("Failed to save a file"), Tr("Convert to PDF"));
        return;
    }

    logf("ConvertToPdf: converting '%s' to '%s'\n", srcPath, destPath);
    bool ok = ConvertImageCollectionToPdf(engine, destPath);
    if (ok) {
        logf("ConvertToPdf: converted successfully\n");
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    } else {
        logf("ConvertToPdf: SaveImageCollectionAsPdf failed\n");
        MessageBoxWarning(hwnd, Tr("Failed to save a file"), Tr("Convert to PDF"));
    }
}

bool ConvertToPdfDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Convert to PDF"))) {
        return false;
    }
    AddFilesBlock(DefaultPdfDestPathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddFooter(Tr("Convert to PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowConvertToPdfDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    EngineBase* engine = tab->GetEngine();
    if (!engine || !engine->isImageCollection) {
        return;
    }
    if (!engine->AllowsPrinting()) {
        return;
    }
    logf("ShowConvertToPdfDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new ConvertToPdfDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Convert PDF to Images dialog (issue #5991) ---

// editable combo listing kImageDpiChoices; any other DPI can be typed in
static DropDown* NewDpiCombo(HWND parent, PlatformFont* font, int dpi) {
    DropDown::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.isRtl = IsUIRtl();
    args.isEditable = true;
    auto* dd = new DropDown();
    dd->Create(args);

    StrVec items;
    for (int choice : kImageDpiChoices) {
        items.Append(fmt("%d", choice));
    }
    dd->SetItems(items);
    dd->SetText(fmt("%d", dpi));
    dd->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    return dd;
}

// 0 if not a usable DPI
static int DpiFromComboText(DropDown* dd) {
    Str s = dd->GetTextTemp();
    str::TrimWsBoth(s);
    int dpi = ParseInt(s);
    if (dpi < 1 || dpi > kMaxImageDpi) {
        return 0;
    }
    return dpi;
}

// on CBN_SELCHANGE the edit still has the previous text, so read the list
static int DpiFromComboSel(DropDown* dd) {
    int idx = CbGetCurrentSelection(dd);
    if (idx < 0 || idx >= dimofi(kImageDpiChoices)) {
        return DpiFromComboText(dd);
    }
    return kImageDpiChoices[idx];
}

static bool GetImageEncoderForPath(Str path, CLSID* encOut) {
    if (!IsSupportedConvertImageExt(path)) {
        return false;
    }
    int idx = ConvertImageFormatIdxFromPath(path);
    CLSID enc = GetGdiPlusEncoderClsid(kConvertImageFormats[idx].mime);
    if (enc.Data1 == 0 && enc.Data2 == 0 && enc.Data3 == 0) {
        return false;
    }
    *encOut = enc;
    return true;
}

bool SavePixmapAsImageFile(Pixmap* px, Str path) {
    if (!px || len(path) == 0) {
        return false;
    }
    CLSID enc{};
    if (!GetImageEncoderForPath(path, &enc)) {
        return false;
    }
    Pixmap* converted = nullptr;
    Pixmap* use = px;
    if (px->format == PixmapFormat::Native) {
        converted = PixmapCopyAs32bppDIB(px);
        if (!converted) {
            return false;
        }
        use = converted;
    }
    Gdiplus::Bitmap* bmp = WrapPixmapGdiplus(use);
    bool ok = false;
    if (bmp) {
        bmp->SetResolution(use->xres, use->yres);
        WCHAR* pathW = CWStrTemp(path);
        ok = bmp->Save(pathW, &enc, nullptr) == Gdiplus::Ok;
        delete bmp;
    } else if (use->hbmp) {
        Gdiplus::Bitmap gbmp(use->hbmp, nullptr);
        gbmp.SetResolution(use->xres, use->yres);
        WCHAR* pathW = CWStrTemp(path);
        ok = gbmp.Save(pathW, &enc, nullptr) == Gdiplus::Ok;
    }
    FreePixmap(converted);
    if (!ok) {
        file::Delete(path);
    }
    return ok;
}

struct ConvertPdfToImagesDialog : PdfToolDialog {
    int pageCount = 0;
    Checkbox* radioCurrent = nullptr;
    Checkbox* radioAll = nullptr;
    Checkbox* radioCustom = nullptr;
    Edit* pagesEdit = nullptr;
    DropDown* dropDpi = nullptr;
    int dpi = kConvertPdfToImagesDpi; // 0 if what's typed is not a DPI

    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
    void OnBrowseTemplate(VirtMouseEvent* ev = nullptr);
    void UpdateButton();
    void OnPagesModeChanged();
    void OnDpiPicked();
    void OnDpiTyped();
};

static Checkbox* NewPagesRadio(HWND parent, PlatformFont* font, Str text, bool groupStart, bool checked) {
    Checkbox::CreateArgs args;
    args.parent = parent;
    args.text = text;
    args.font = font;
    args.isRtl = IsUIRtl();
    args.isRadio = true;
    args.isGroupStart = groupStart;
    args.initialState = checked ? Checkbox::State::Checked : Checkbox::State::Unchecked;
    auto* c = new Checkbox();
    c->Create(args);
    c->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    return c;
}

void ConvertPdfToImagesDialog::OnDpiPicked() {
    dpi = DpiFromComboSel(dropDpi);
    UpdateButton();
}

void ConvertPdfToImagesDialog::OnDpiTyped() {
    dpi = DpiFromComboText(dropDpi);
    UpdateButton();
}

void ConvertPdfToImagesDialog::OnPagesModeChanged() {
    bool custom = radioCustom && radioCustom->IsChecked();
    if (pagesEdit) {
        pagesEdit->SetIsEnabled(custom);
    }
    UpdateButton();
}

// the line's count follows the mode: the one page, all of them, or what the
// custom range picks
void ConvertPdfToImagesDialog::UpdateButton() {
    if (!actionBtn) {
        return;
    }
    TempStr dest = destEdit ? destEdit->GetTextTemp() : TempStr{};
    bool destOk = len(dest) > 0;
    bool pagesOk = true;
    if (radioCustom && radioCustom->IsChecked()) {
        TempStr pages = pagesEdit ? pagesEdit->GetTextTemp() : TempStr{};
        Vec<int> parsed;
        pagesOk = ParseDeletePages(pages, pageCount, parsed);
        SetMeta(PageRangeCountTemp(pages, pageCount, true), pagesOk);
    } else {
        int n = radioAll && radioAll->IsChecked() ? pageCount : 1;
        SetMeta(PageCountTextTemp(n, pageCount), true);
    }
    SetActionEnabled(destOk && pagesOk && dpi > 0);
}

void ConvertPdfToImagesDialog::OnBrowseTemplate(VirtMouseEvent*) {
    WCHAR dstFileName[MAX_PATH + 1]{};
    TempStr current = destEdit->GetTextTemp();
    // < and > are illegal in Windows filenames; show {N} in the save dialog
    TempStr shown = str::ReplaceTemp(current, StrL("<N>"), StrL("{N}"));
    if (str::Contains(shown, StrL("<n>"))) {
        shown = str::ReplaceTemp(shown, StrL("<n>"), StrL("{N}"));
    }
    wstr::BufSet(WStr(dstFileName, MAX_PATH), ToWStrTemp(shown));

    int fmtIdx = SelectedFormatIdx();
    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = L"PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0BMP\0*.bmp\0All Files\0*.*\0";
    ofn.nFilterIndex = (DWORD)(fmtIdx + 1);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = kConvertImageFormats[fmtIdx].defExt.s;

    if (!GetSaveFileNameW(&ofn)) {
        return;
    }
    TempStr picked = ToUtf8Temp(dstFileName);
    if (str::Contains(picked, StrL("{N}"))) {
        picked = str::ReplaceTemp(picked, StrL("{N}"), StrL("<N>"));
    } else if (!PathHasPagePlaceholder(picked)) {
        picked = EnsurePagePlaceholderTemp(picked, true);
    }
    destEdit->SetText(picked);
    int filterIdx = (int)ofn.nFilterIndex - 1;
    if (filterIdx >= 0 && filterIdx < ConvertImageFormatCount()) {
        syncingFormat = true;
        CbSetCurrentSelection(dropFormat, filterIdx);
        syncingFormat = false;
        SetDestExtFromFormat();
    } else {
        SyncFormatFromPath(picked);
    }
}

void ConvertPdfToImagesDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        MessageBoxWarning(hwnd, StrL("Failed to convert PDF to images."), Tr("Convert PDF to Images"));
        return;
    }

    Vec<int> pages;
    if (radioCurrent && radioCurrent->IsChecked()) {
        int cur = dm->CurrentPageNo();
        if (cur < 1 || cur > pageCount) {
            return;
        }
        VecAppend(pages, cur);
    } else if (radioAll && radioAll->IsChecked()) {
        CollectAllPages(pageCount, pages);
    } else {
        TempStr custom = pagesEdit ? pagesEdit->GetTextTemp() : TempStr{};
        if (!ParseDeletePages(custom, pageCount, pages)) {
            return;
        }
    }

    // the box may have been filled without a change notification
    dpi = DpiFromComboText(dropDpi);
    if (dpi < 1) {
        return;
    }
    if (!PagesFitDpi(engine, pages, dpi)) {
        MessageBoxWarning(hwnd, Tr("Too large for this DPI"), Tr("Convert PDF to Images"));
        return;
    }

    logf("ConvertPdfToImages: '%s' -> '%s', %d page(s), %d DPI\n", srcPath, destPath, len(pages), dpi);

    HCURSOR prev = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    Str firstPath;
    int nOk = ConvertPagesToImages(engine, dm->GetRotation(), destPath, pages, dpi, &firstPath);
    SetCursor(prev);

    if (nOk == 0) {
        str::Free(firstPath);
        MessageBoxWarning(hwnd, StrL("Failed to convert PDF to images."), Tr("Convert PDF to Images"));
        return;
    }
    logf("ConvertPdfToImages: wrote %d of %d file(s)\n", nOk, len(pages));
    TempStr openPath = str::DupTemp(firstPath);
    str::Free(firstPath);
    Close();
    OpenPathInDefaultFileManager(openPath);
}

bool ConvertPdfToImagesDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Convert PDF to Images"))) {
        return false;
    }
    pageCount = w->ctrl ? w->ctrl->PageCount() : 1;

    HBox* row = AddLine(Tr("Pages"));
    radioCurrent = NewPagesRadio(hwnd, fontText, Tr("Current"), true, false);
    radioCurrent->onStateChanged =
        MkMethod0<ConvertPdfToImagesDialog, &ConvertPdfToImagesDialog::OnPagesModeChanged>(this);
    row->AddChild(radioCurrent);
    radioAll = NewPagesRadio(hwnd, fontText, Tr("All"), false, true);
    radioAll->onStateChanged = MkMethod0<ConvertPdfToImagesDialog, &ConvertPdfToImagesDialog::OnPagesModeChanged>(this);
    row->AddChild(radioAll);
    radioCustom = NewPagesRadio(hwnd, fontText, Tr("Custom"), false, false);
    radioCustom->onStateChanged =
        MkMethod0<ConvertPdfToImagesDialog, &ConvertPdfToImagesDialog::OnPagesModeChanged>(this);
    row->AddChild(radioCustom);
    pagesEdit = AddLineEdit(fmt("1-%d", pageCount));
    pagesEdit->SetIsEnabled(false);
    AddLineMeta();

    AddLine(Tr("Resolution (DPI)"));
    dropDpi = NewDpiCombo(hwnd, fontText, dpi);
    dropDpi->onSelectionChanged = MkMethod0<ConvertPdfToImagesDialog, &ConvertPdfToImagesDialog::OnDpiPicked>(this);
    dropDpi->onTextChanged = MkMethod0<ConvertPdfToImagesDialog, &ConvertPdfToImagesDialog::OnDpiTyped>(this);
    lastLine->AddChild(dropDpi);

    TempStr noExt = path::GetPathNoExtTemp(srcPath);
    TempStr pngPath = str::JoinTemp(noExt, StrL("-<N>.png"));
    AddFilesBlock(pngPath, {}, {}, true);
    browseBtn->onClick =
        MkMethod1<ConvertPdfToImagesDialog, VirtMouseEvent*, &ConvertPdfToImagesDialog::OnBrowseTemplate>(this);

    AddFooter(Tr("Convert"), Tr("Use <N> for the page number"));
    FinishDialog(destEdit);

    destEdit->onTextChanged = MkMethod0<ConvertPdfToImagesDialog, &ConvertPdfToImagesDialog::UpdateButton>(this);
    pagesEdit->onTextChanged = MkMethod0<ConvertPdfToImagesDialog, &ConvertPdfToImagesDialog::UpdateButton>(this);
    UpdateButton();
    return true;
}

void ShowConvertPdfToImagesDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    if (!IsPdfDoc(tab)) {
        return;
    }
    logf("ShowConvertPdfToImagesDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new ConvertPdfToImagesDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

void FinishSaveSelImg(SaveSelImgWork* d) {
    if (IsWindow(d->hwndCanvas)) {
        RemoveNotificationsForGroup(d->hwndCanvas, kNotifSaveSelectionAsImage);
    }
    if (d->ok) {
        logf("SaveSelectionAsImage: wrote '%s'\n", d->destPath);
        if (IsMainWindowValidAndNotClosing(d->win)) {
            LoadArgs args(d->destPath, d->win);
            StartLoadDocument(&args);
        }
    } else {
        HWND parent = nullptr;
        if (IsMainWindowValidAndNotClosing(d->win)) {
            parent = d->win->hwndFrame;
        }
        MessageBoxWarning(parent, StrL("Failed to save the selection as an image."), Tr("Save Selection As Image"));
    }
    str::Free(d->destPath);
    delete d;
}

struct SaveSelectionAsImageDialog : PdfToolDialog {
    int pageNo = 0;
    RectF selRect;
    DropDown* dropDpi = nullptr;
    int dpi = kSaveSelectionDefaultDpi; // 0 if what's typed is not a DPI

    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
    void OnBrowseDest(VirtMouseEvent* ev = nullptr);
    void OnDpiPicked();
    void OnDpiTyped();
    void UpdateSizeLabel();
};

// the line's right end: the image's size at this DPI
void SaveSelectionAsImageDialog::UpdateSizeLabel() {
    if (!win) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    float zoom = SaveSelectionZoom(engine, (float)dpi);
    int w = 0;
    int h = 0;
    EstimateSelectionPx(selRect, zoom, w, h);
    if (dpi < 1) {
        SetMeta(StrL(""), false);
        SetActionEnabled(false);
    } else if (!SaveSelectionSizeOk(w, h)) {
        SetMeta(Tr("Too large for this DPI"), false);
        SetActionEnabled(false);
    } else {
        SetMeta(fmt("%d x %d px", w, h), true);
        TempStr dest = destEdit ? destEdit->GetTextTemp() : Str{};
        SetActionEnabled(len(dest) > 0);
    }
}

void SaveSelectionAsImageDialog::OnDpiPicked() {
    dpi = DpiFromComboSel(dropDpi);
    UpdateSizeLabel();
}

void SaveSelectionAsImageDialog::OnDpiTyped() {
    dpi = DpiFromComboText(dropDpi);
    UpdateSizeLabel();
}

void SaveSelectionAsImageDialog::OnBrowseDest(VirtMouseEvent*) {
    WCHAR dstFileName[MAX_PATH + 1]{};
    TempStr current = destEdit->GetTextTemp();
    wstr::BufSet(WStr(dstFileName, MAX_PATH), ToWStrTemp(current));

    int fmtIdx = SelectedFormatIdx();
    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = L"PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0BMP\0*.bmp\0All Files\0*.*\0";
    ofn.nFilterIndex = (DWORD)(fmtIdx + 1);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = kConvertImageFormats[fmtIdx].defExt.s;

    if (!GetSaveFileNameW(&ofn)) {
        return;
    }
    TempStr picked = ToUtf8Temp(dstFileName);
    destEdit->SetText(picked);
    int filterIdx = (int)ofn.nFilterIndex - 1;
    if (filterIdx >= 0 && filterIdx < ConvertImageFormatCount()) {
        syncingFormat = true;
        CbSetCurrentSelection(dropFormat, filterIdx);
        syncingFormat = false;
        SetDestExtFromFormat();
    } else {
        SyncFormatFromPath(picked);
    }
    UpdateSizeLabel();
}

void SaveSelectionAsImageDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    TempStr withExt = WithDefaultImageExtTemp(destPath);
    if (len(withExt) == 0) {
        MessageBoxWarning(hwnd, StrL("Unsupported image format."), Tr("Save Selection As Image"));
        return;
    }
    // the box may have been filled without a change notification
    dpi = DpiFromComboText(dropDpi);
    if (dpi < 1) {
        return;
    }
    Pixmap* px = RenderSelectionPixmap(engine, dm->GetRotation(), pageNo, selRect, (float)dpi);
    if (!px) {
        MessageBoxWarning(hwnd, StrL("Failed to save the selection as an image."), Tr("Save Selection As Image"));
        return;
    }

    auto* work = new SaveSelImgWork();
    work->px = px;
    work->destPath = str::Dup(withExt);
    work->hwndCanvas = win->hwndCanvas;
    work->win = win;

    NotificationCreateArgs nargs;
    nargs.win = win;
    nargs.msg = Tr("Saving image...");
    nargs.groupId = kNotifSaveSelectionAsImage;
    nargs.timeoutMs = kNotifNoTimeout;
    ShowNotification(nargs);

    Close();
    RunAsync(MkFunc0(SaveSelImgThread, work), StrL("SaveSelImg"));
}

bool SaveSelectionAsImageDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!tab || !tab->selectionOnPage || len(*tab->selectionOnPage) == 0) {
        return false;
    }
    SelectionOnPage& sel = (*tab->selectionOnPage)[0];
    if (sel.rect.IsEmpty()) {
        return false;
    }
    pageNo = sel.pageNo;
    selRect = sel.rect;

    if (!CreateToolDialog(w, tab, Tr("Save Selection As Image"))) {
        return false;
    }

    AddLine(Tr("Resolution (DPI)"));
    dropDpi = NewDpiCombo(hwnd, fontText, dpi);
    dropDpi->onSelectionChanged = MkMethod0<SaveSelectionAsImageDialog, &SaveSelectionAsImageDialog::OnDpiPicked>(this);
    dropDpi->onTextChanged = MkMethod0<SaveSelectionAsImageDialog, &SaveSelectionAsImageDialog::OnDpiTyped>(this);
    lastLine->AddChild(dropDpi);
    AddLineMeta();

    TempStr noExt = path::GetPathNoExtTemp(srcPath);
    TempStr pngPath = MakeUniqueFilePathTemp(str::JoinTemp(noExt, StrL("-sel.png")));
    AddFilesBlock(pngPath, {}, {}, true);
    browseBtn->onClick =
        MkMethod1<SaveSelectionAsImageDialog, VirtMouseEvent*, &SaveSelectionAsImageDialog::OnBrowseDest>(this);

    AddFooter(Tr("Save"));
    FinishDialog(destEdit);

    destEdit->onTextChanged = MkMethod0<SaveSelectionAsImageDialog, &SaveSelectionAsImageDialog::UpdateSizeLabel>(this);
    UpdateSizeLabel();
    return true;
}

void ShowSaveSelectionAsImageDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    if (!HasPermission(Perm::CopySelection) || !CanAccessDisk()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !IsRectangularSelection(win)) {
        return;
    }
    logf("ShowSaveSelectionAsImageDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new SaveSelectionAsImageDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}
