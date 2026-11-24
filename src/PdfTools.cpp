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
#include "SumatraDialogs.h"
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
#include "AIChatTypes.h"
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
#include "Cli.h"

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

// shows a tool's output in the file manager; a -for-testing run must not leave
// file manager windows behind
static void ShowToolOutput(Str path) {
    if (gForTesting) {
        return;
    }
    OpenPathInDefaultFileManager(path);
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

    if ((TakeTestSaveFileName(&ofn) || GetSaveFileNameW(&ofn))) {
        edit->SetText(ToUtf8Temp(dstFileName));
    }
}

// GDI+ encoder data for kConvertImageFormats, by the same index
struct ConvertImageEncoder {
    WStr mime;
    WStr defExt; // OPENFILENAME lpstrDefExt, no dot
};

static const ConvertImageEncoder kConvertImageEncoders[] = {
    {L"image/png", L"png"},
    {L"image/jpeg", L"jpg"},
    {L"image/bmp", L"bmp"},
    {L"image/gif", L"gif"},
};
static_assert(dimof(kConvertImageEncoders) == kConvertImageFormatCount);

// Every dialog here has the same shape, drawn like the palette: the lines
// the user types in (a page range, a password), a hairline, then the files -
// the source and a destination over a hairline, with Browse - then the footer
// with a hint, Cancel and the one filled action button. The labels and
// buttons are virtual controls, the text fields native edits, all in one
// layout tree. Each dialog adds its lines and implements DoIt().
struct PdfToolDialog : WindowBase {
    Str srcPath; // owned
    MainWindow* win = nullptr;
    // what the Browse button's save dialog offers
    WStr browseFilter;
    WStr browseDefExt;

    VBox* mainBox = nullptr;
    Edit* destEdit = nullptr;
    // PNG / JPEG / BMP / GIF next to the destination of the image tools
    DropDown* dropFormat = nullptr;
    bool syncingFormat = false;
    VirtButton* browseBtn = nullptr;
    VirtButton* actionBtn = nullptr;
    VirtButton* cancelBtn = nullptr;
    VirtText* hintText = nullptr;
    // a ghost button after the hint, collapsed until a dialog shows it
    VirtButton* footerBtn = nullptr;
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
    // the tool running on a thread, if any
    PdfToolJob* job = nullptr;

    ~PdfToolDialog() override;

    bool CreateToolDialog(MainWindow*, WindowTab*, Str title);
    WindowTab* SrcTab() const;
    HBox* NewRow();
    VirtText* NewLabel(Str s, PlatformFont* font, Color col, int dy = 0, int dx = 0);
    Edit* NewEdit(Str text, PlatformFont* font, bool isPassword = false);
    HBox* AddLine(Str label);
    Edit* AddLineEdit(Str text, bool isPassword = false);
    VirtText* AddLineMeta();
    void SetMeta(Str s, bool ok);
    void AddFilesBlock(Str destPath, WStr filter, WStr defExt, bool withFormat = false);
    void AddFooter(Str actionText, Str hint = {}, Str footerBtnText = {});
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
    void OnAction(VirtMouseEvent* ev = nullptr);
    void RunJob(PdfToolJob* job);
    void DetachJob();
    // what the action button does; the only thing the dialogs really differ in
    virtual void DoIt(VirtMouseEvent* ev = nullptr) {}
    virtual void JobEnded() { SetActionEnabled(true); }
    virtual void JobProgress(int nDone, int nTotal) {}
};

// the tab the dialog was opened for; the user may have switched tabs since
WindowTab* PdfToolDialog::SrcTab() const {
    return win ? FindTabByFilePath(srcPath, win) : nullptr;
}

PdfToolDialog::~PdfToolDialog() {
    DetachJob();
    str::FreePtr(&srcPath);
    // ~WindowBase deletes `layout`, which owns the whole tree - the virtual
    // controls and the edits alike
}

static void PdfToolDialogOnClose(WindowBase::CloseEvent* ev) {
    auto* dlg = (PdfToolDialog*)ev->e->self;
    dlg->DetachJob();
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

// the dialog goes away before its tool finishes; the output isn't opened
void PdfToolDialog::DetachJob() {
    if (job) {
        job->owner = nullptr;
        job = nullptr;
    }
}

void PdfToolDialog::OnAction(VirtMouseEvent*) {
    if (!job) {
        DoIt();
    }
}

// the action button stays disabled until FinishPdfToolJob
void PdfToolDialog::RunJob(PdfToolJob* j) {
    job = j;
    job->owner = this;
    SetActionEnabled(false);
    StartPdfToolJob(job);
}

void FinishPdfToolJob(PdfToolJob* job) {
    auto* dlg = (PdfToolDialog*)job->owner;
    if (!dlg) {
        return;
    }
    dlg->job = nullptr;
    dlg->JobEnded();
    if (job->res != 0 && job->fail == PdfToolFail::Notification) {
        ShowWarningNotification(dlg->win, job->failMsg, kNotif5SecsTimeOut);
        return;
    }
    if (job->res != 0) {
        MessageBoxWarning(dlg->hwnd, job->failMsg, job->failTitle);
        return;
    }
    MainWindow* w = dlg->win;
    TempStr path = str::DupTemp(job->destPath);
    dlg->Close();
    if (job->done == PdfToolDone::ShowDest) {
        ShowToolOutput(path);
        return;
    }
    if (IsMainWindowValidAndNotClosing(w)) {
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    }
}

void PdfToolJobProgress(PdfToolJob* job) {
    auto* dlg = (PdfToolDialog*)job->owner;
    if (dlg) {
        dlg->JobProgress(AtomicIntGet(&job->nDone), job->nTotal);
    }
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
    colMuted = ThemeWindowDarkerTextColor();
    closeOnEsc = true;
    onClose = MkFunc1Void(PdfToolDialogOnClose);

    CreateCustomArgs cargs;
    cargs.title = title;
    cargs.font = GetDefaultGuiFont();
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    cargs.visible = false;
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

HBox* PdfToolDialog::NewRow() {
    return NewDlgRow();
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

// PNG / JPEG / BMP / GIF, as the Save Image dialog has them
static DropDown* NewFormatDropDown(HWND parent, PlatformFont* font) {
    auto* dd = new DropDown();
    DropDown::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.isRtl = IsUIRtl();
    dd->Create(args);
    StrVec items;
    for (int i = 0; i < kConvertImageFormatCount; i++) {
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
void PdfToolDialog::AddFooter(Str actionText, Str hint, Str footerBtnText) {
    HBox* row = NewRow();
    if (hint) {
        hintText = NewLabel(hint, fontMeta, colMuted);
        row->AddChild(hintText);
    }
    if (footerBtnText) {
        footerBtn = NewDlgButton(hwnd, footerBtnText, fontMeta, DlgButtonStyle::Ghost, false);
        footerBtn->SetIsVisible(false);
        row->AddChild(footerBtn);
    }
    row->AddChild(new Spacer(0, 0), 1);

    cancelBtn = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    cancelBtn->onClick = MkMethod1<PdfToolDialog, VirtMouseEvent*, &PdfToolDialog::OnCancel>(this);
    row->AddChild(cancelBtn);

    actionBtn = NewDlgButton(hwnd, actionText, fontText, DlgButtonStyle::Filled, true);
    actionBtn->onClick = MkMethod1<PdfToolDialog, VirtMouseEvent*, &PdfToolDialog::OnAction>(this);
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
    HwndResizeClientSize(hwnd, size.dx, size.dy);
    windowDy = HwndWindowRect(hwnd).dy;
    // positions everything and picks up the virtual controls to paint
    DoLayout(size);

    HwndCenterDialog(hwnd, win->hwndFrame);
    UpdateTheme();
    SetIsVisible(true);
    EditSetFocus(focusOn);
}

// stays disabled while the tool runs
void PdfToolDialog::SetActionEnabled(bool enabled) {
    if (actionBtn) {
        SetDlgButtonEnabled(actionBtn, DlgButtonStyle::Filled, enabled && !job);
    }
}

int PdfToolDialog::SelectedFormatIdx() const {
    if (!dropFormat) {
        return 0;
    }
    int idx = CbGetCurrentSelection(dropFormat);
    if (idx < 0 || idx >= kConvertImageFormatCount) {
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
    WindowTab* tab = SrcTab();
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    // pdfbake_main re-opens the file from disk, so unsaved session
    // annotations would be missing unless we write them out first (issue #5977).
    if (engine && EngineHasUnsavedAnnotations(engine)) {
        tmpPath = GetTempFilePathTemp(StrL("bake"));
        if (len(tmpPath) == 0 || !EngineMupdfSaveCopy(engine, tmpPath)) {
            MessageBoxWarning(hwnd, Tr("Failed to bake PDF file."), Tr("Bake PDF"));
            return;
        }
        inputPath = tmpPath;
        logf("PdfBakeDoIt: unsaved annotations, baking from temp '%s'\n", tmpPath);
    }

    logf("PdfBakeDoIt: baking '%s' to '%s'\n", inputPath, destPath);

    // build argv for pdfbake_main: "bake" input output
    char* argv[] = {(char*)"bake", (char*)"-p", PdfToolPasswordTemp(win, srcPath), CStrTemp(inputPath),
                    CStrTemp(destPath)};
    auto* j =
        NewPdfToolJob(pdfbake_main, StrL("PdfBakeDoIt"), destPath, Tr("Failed to bake PDF file."), Tr("Bake PDF"));
    SetPdfToolJobArgs(j, argv, dimofi(argv), tmpPath);
    RunJob(j);
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

    WindowTab* tab = SrcTab();
    bool isPdf = tab && IsPdfDoc(tab);
    if (isPdf) {
        // use muconvert for PDF
        char* argv[] = {(char*)"convert", (char*)"-p",        PdfToolPasswordTemp(win, srcPath),
                        (char*)"-o",      CStrTemp(destPath), CStrTemp(srcPath),
                        CStrTemp(pages)};
        auto* j = NewPdfToolJob(muconvert_main, StrL("PdfExtractTextDoIt"), destPath, Tr("Failed to extract text."),
                                Tr("Extract Text"));
        SetPdfToolJobArgs(j, argv, dimofi(argv), {});
        j->done = PdfToolDone::ShowDest;
        RunJob(j);
        return;
    }
    // use engine text extraction for other formats (DjVu, etc.)
    bool ok = ExtractTextViaEngine(tab, destPath, pages);

    if (ok) {
        logf("PdfExtractTextDoIt: extracted successfully\n");
        TempStr path = str::DupTemp(destPath);
        Close();
        ShowToolOutput(path);
    } else {
        logf("PdfExtractTextDoIt: failed to extract text, isPdf: %d\n", (int)isPdf);
        MessageBoxWarning(hwnd, Tr("Failed to extract text."), Tr("Extract Text"));
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

// --- Export as Markdown dialog ---

struct PdfExportMarkdownDialog : PdfPagesDialog {
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

// the pages picked as Markdown next to the document, as `cli markdown` writes
// them (pictures in <name>_images)
void PdfExportMarkdownDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    Vec<int> pages;
    if (len(destPath) == 0 || !PageRangeOk(pagesEdit->GetTextTemp(), pageCount, true, pages)) {
        return;
    }
    WindowTab* tab = SrcTab();
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    logf("PdfExportMarkdownDoIt: %d pages of '%s' to '%s'\n", len(pages), srcPath, destPath);
    TempStr md = DocumentToMarkdownTemp(engine, pages, destPath, MarkdownOpts{}, nullptr);
    if (!file::WriteFile(destPath, md)) {
        logf("PdfExportMarkdownDoIt: failed to write '%s'\n", destPath);
        MessageBoxWarning(hwnd, Tr("Failed to save a file"), Tr("Export as Markdown"));
        return;
    }
    TempStr path = str::DupTemp(destPath);
    Close();
    ShowToolOutput(path);
}

bool PdfExportMarkdownDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Export as Markdown"))) {
        return false;
    }
    pageCount = w->ctrl ? w->ctrl->PageCount() : 1;
    AddPagesLine(fmt("1-%d", pageCount));
    TempStr mdPath = str::JoinTemp(path::GetPathNoExtTemp(srcPath), StrL(".md"));
    AddFilesBlock(MakeUniqueFilePathTemp(mdPath), L"Markdown Files\0*.md\0All Files\0*.*\0", L"md");
    AddFooter(Tr("Export"), PageRangeHint());
    FinishDialog(pagesEdit);
    pagesEdit->onTextChanged = MkMethod0<PdfPagesDialog, &PdfPagesDialog::UpdatePages>(this);
    UpdatePages();
    return true;
}

void ShowExportMarkdownDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }
    logf("ShowExportMarkdownDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new PdfExportMarkdownDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Compress PDF dialog ---

struct PdfCompressDialog : PdfToolDialog {
    DropDown* dropQuality = nullptr;

    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void PdfCompressDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    WindowTab* tab = SrcTab();
    TempStr tmpPath;
    TempStr inputPath = PdfToolInputTemp(tab ? tab->GetEngine() : nullptr, srcPath, &tmpPath);
    if (len(inputPath) == 0) {
        MessageBoxWarning(hwnd, Tr("Failed to compress PDF file."), Tr("Compress PDF"));
        return;
    }
    logf("PdfCompressDoIt: compressing '%s' to '%s'\n", inputPath, destPath);

    // equivalent of: clean -gggg -e 100 -f -i -t -Z [picture options] input output
    auto quality = dropQuality ? (PdfCompressQuality)CbGetCurrentSelection(dropQuality) : PdfCompressQuality::Same;
    Vec<char*> argv;
    VecAppend(argv, (char*)"clean");
    VecAppend(argv, (char*)"-p");
    VecAppend(argv, PdfToolPasswordTemp(win, srcPath));
    AppendPdfCompressArgs(argv, quality);
    VecAppend(argv, CStrTemp(inputPath));
    VecAppend(argv, CStrTemp(destPath));

    auto* j = NewPdfToolJob(pdfclean_main, StrL("PdfCompressDoIt"), destPath, Tr("Failed to compress PDF file."),
                            Tr("Compress PDF"));
    SetPdfToolJobArgs(j, argv.els, len(argv), tmpPath);
    RunJob(j);
}

bool PdfCompressDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Compress PDF"))) {
        return false;
    }
    AddLine(Tr("Pictures"));
    auto* dd = new DropDown();
    DropDown::CreateArgs args;
    args.parent = hwnd;
    args.font = fontText;
    args.isRtl = IsUIRtl();
    dd->Create(args);
    StrVec items;
    PdfCompressQualityLabels(items);
    dd->SetItems(items);
    CbSetCurrentSelection(dd, (int)PdfCompressQuality::Same);
    dd->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    dropQuality = dd;
    lastLine->AddChild(dropQuality);
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

    WindowTab* tab = SrcTab();
    TempStr tmpPath;
    TempStr inputPath = PdfToolInputTemp(tab ? tab->GetEngine() : nullptr, srcPath, &tmpPath);
    if (len(inputPath) == 0) {
        MessageBoxWarning(hwnd, Tr("Failed to decompress PDF file."), Tr("Decompress PDF"));
        return;
    }
    logf("PdfDecompressDoIt: decompressing '%s' to '%s'\n", inputPath, destPath);

    // equivalent of: clean -d input output
    char* argv[] = {(char*)"clean", (char*)"-p",         PdfToolPasswordTemp(win, srcPath),
                    (char*)"-d",    CStrTemp(inputPath), CStrTemp(destPath)};
    auto* j = NewPdfToolJob(pdfclean_main, StrL("PdfDecompressDoIt"), destPath, Tr("Failed to decompress PDF file."),
                            Tr("Decompress PDF"));
    SetPdfToolJobArgs(j, argv, dimofi(argv), tmpPath);
    RunJob(j);
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

    // XPS, EPUB, ...: MuPDF draws the pages of the open document into a new PDF
    WindowTab* docTab = FindTabByFilePath(srcPath, win);
    if (isExtract && IsMupdfNonPdfDoc(docTab)) {
        HCURSOR prev = SetCursor(LoadCursor(nullptr, IDC_WAIT));
        bool ok = ExtractPagesToNewPdf(docTab, parsedPages, destPath);
        SetCursor(prev);
        if (!ok) {
            MessageBoxWarning(hwnd, Tr("Failed to save a file"), Tr("Extract Pages"));
            return;
        }
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        LoadArgs args(path, w);
        StartLoadDocument(&args);
        return;
    }

    WindowTab* sourceTab = FindTabByFilePath(srcPath, win);
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
    if (sourceEngine && EngineHasUnsavedAnnotations(sourceEngine)) {
        tmpPath = GetTempFilePathTemp(StrL("extract-pages"));
        if (len(tmpPath) == 0 || !EngineMupdfSaveCopy(sourceEngine, tmpPath)) {
            Str msg =
                isExtract ? Tr("Failed to extract pages from PDF file.") : Tr("Failed to delete pages from PDF file.");
            MessageBoxWarning(hwnd, msg, isExtract ? Tr("Extract Pages From PDF") : Tr("Delete Pages From PDF"));
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
    char* argv[] = {(char*)"clean",     (char*)"-p",         PdfToolPasswordTemp(win, srcPath),
                    (char*)"-gggg",     (char*)"-e",         (char*)"100",
                    (char*)"-f",        (char*)"-i",         (char*)"-t",
                    (char*)"-Z",        CStrTemp(inputPath), CStrTemp(destPath),
                    CStrTemp(pageRange)};
    Str msg = isExtract ? Tr("Failed to extract pages from PDF file.") : Tr("Failed to delete pages from PDF file.");
    Str title = isExtract ? Tr("Extract Pages From PDF") : Tr("Delete Pages From PDF");
    auto* j = NewPdfToolJob(pdfclean_main, StrL("PdfDeletePageDoIt"), destPath, msg, title);
    SetPdfToolJobArgs(j, argv, dimofi(argv), tmpPath);
    RunJob(j);
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
    // from XPS, EPUB, ... the pages go to a new PDF next to the document
    bool toNewPdf = isExtract && IsMupdfNonPdfDoc(tab);
    Str title = isExtract ? Tr("Extract Pages") : Tr("Delete Pages From PDF");
    if (!CreateToolDialog(w, tab, title)) {
        return false;
    }
    pageCount = w->ctrl ? w->ctrl->PageCount() : 0;
    int currentPage = w->ctrl ? w->ctrl->CurrentPageNo() : 1;
    AddPagesLine(fmt("%d", currentPage));
    TempStr destPath = toNewPdf ? DefaultPdfDestPathTemp(srcPath) : MakeUniqueFilePathTemp(srcPath);
    AddFilesBlock(destPath, L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");

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
    // extract also works for XPS, EPUB, ...; delete rewrites the PDF itself
    bool canExtract = isExtract && IsMupdfNonPdfDoc(tab);
    if (!IsPdfDoc(tab) && !canExtract) {
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

    WindowTab* tab = SrcTab();
    TempStr tmpPath;
    TempStr inputPath = PdfToolInputTemp(tab ? tab->GetEngine() : nullptr, srcPath, &tmpPath);
    if (len(inputPath) == 0) {
        MessageBoxWarning(hwnd, Tr("Failed to encrypt PDF file."), Tr("Encrypt PDF"));
        return;
    }
    logf("PdfEncryptDoIt: encrypting '%s' to '%s' with AES-256\n", inputPath, destPath);

    // equivalent of: clean -E aes-256 -U <pwd> -O <pwd> input output
    char* pwdZ = CStrTemp(pwd);
    char* argv[] = {(char*)"clean", (char*)"-E", (char*)"aes-256",    (char*)"-U",       pwdZ,
                    (char*)"-O",    pwdZ,        CStrTemp(inputPath), CStrTemp(destPath)};

    auto* j = NewPdfToolJob(pdfclean_main, StrL("PdfEncryptDoIt"), destPath, Tr("Failed to encrypt PDF file."),
                            Tr("Encrypt PDF"));
    SetPdfToolJobArgs(j, argv, dimofi(argv), tmpPath);
    RunJob(j);
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

// --- Add Watermark / Add Page Numbers dialogs ---

// text on the pages of a copy of the PDF: a watermark across them, or page
// numbers ({n}, {total}) at a place picked
struct PdfStampDialog : PdfPagesDialog {
    bool watermark = true;
    Edit* textEdit = nullptr;
    DropDown* dropPosition = nullptr;

    bool Create(MainWindow* win, WindowTab* tab, bool watermark);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
    void JobEnded() override { UpdateButton(); }
    void UpdateButton();
};

void PdfStampDialog::UpdateButton() {
    TempStr pages = pagesEdit->GetTextTemp();
    Vec<int> parsed;
    bool ok = ParseDeletePages(pages, pageCount, parsed);
    SetMeta(PageRangeCountTemp(pages, pageCount, true), ok);
    SetActionEnabled(ok && len(textEdit->GetTextTemp()) > 0);
}

void PdfStampDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    Vec<int> pages;
    if (len(destPath) == 0 || !ParseDeletePages(pagesEdit->GetTextTemp(), pageCount, pages)) {
        return;
    }
    WindowTab* tab = SrcTab();
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    int posIdx = dropPosition ? CbGetCurrentSelection(dropPosition) : 0;
    logf("PdfStampDialog: %d pages of '%s' to '%s'\n", len(pages), srcPath, destPath);
    RunJob(NewStampJob(engine, watermark, textEdit->GetTextTemp(), pages, posIdx, destPath));
}

bool PdfStampDialog::Create(MainWindow* w, WindowTab* tab, bool watermarkArg) {
    watermark = watermarkArg;
    Str title = watermark ? Tr("Add Watermark") : Tr("Add Page Numbers");
    if (!CreateToolDialog(w, tab, title)) {
        return false;
    }
    pageCount = w->ctrl ? w->ctrl->PageCount() : 0;
    AddLine(Tr("Text"));
    textEdit = AddLineEdit(watermark ? StrL("DRAFT") : StrL("{n}"));
    if (!watermark) {
        AddLine(Tr("Position"));
        auto* dd = new DropDown();
        DropDown::CreateArgs args;
        args.parent = hwnd;
        args.font = fontText;
        args.isRtl = IsUIRtl();
        dd->Create(args);
        StrVec items;
        PageNumberPositionLabels(items);
        dd->SetItems(items);
        CbSetCurrentSelection(dd, 0);
        dd->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
        dropPosition = dd;
        lastLine->AddChild(dropPosition);
    }
    AddPagesLine(fmt("1-%d", pageCount));
    AddFilesBlock(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    Str hint = watermark ? Str{} : Tr("{n} is the page number, {total} the last one");
    AddFooter(title, hint);
    FinishDialog(textEdit);
    // the change handlers only now that actionBtn exists (Edit::Create fires
    // onTextChanged on the initial text)
    textEdit->onTextChanged = MkMethod0<PdfStampDialog, &PdfStampDialog::UpdateButton>(this);
    pagesEdit->onTextChanged = MkMethod0<PdfStampDialog, &PdfStampDialog::UpdateButton>(this);
    UpdateButton();
    return true;
}

static void ShowPdfStampDialog(MainWindow* win, bool watermark) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0 || !IsPdfDoc(tab)) {
        return;
    }
    logf("ShowPdfStampDialog: opening for '%s'\n", tab->filePath);
    auto* dlg = new PdfStampDialog();
    if (!dlg->Create(win, tab, watermark)) {
        delete dlg;
    }
}

void ShowPdfWatermarkDialog(MainWindow* win) {
    ShowPdfStampDialog(win, true);
}

void ShowPdfPageNumbersDialog(MainWindow* win) {
    ShowPdfStampDialog(win, false);
}

// --- Decrypt PDF dialog ---

struct PdfDecryptDialog : PdfToolDialog {
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void PdfDecryptDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    WindowTab* tab = SrcTab();
    TempStr tmpPath;
    TempStr inputPath = PdfToolInputTemp(tab ? tab->GetEngine() : nullptr, srcPath, &tmpPath);
    if (len(inputPath) == 0) {
        ShowWarningNotification(win, Tr("Failed to decrypt PDF file."), kNotif5SecsTimeOut);
        return;
    }
    logf("PdfDecryptDoIt: decrypting '%s' to '%s'\n", inputPath, destPath);

    // equivalent of: clean -p <key> -D input output
    char* argv[] = {(char*)"clean", (char*)"-p",         PdfToolPasswordTemp(win, srcPath),
                    (char*)"-D",    CStrTemp(inputPath), CStrTemp(destPath)};

    auto* j = NewPdfToolJob(pdfclean_main, StrL("PdfDecryptDoIt"), destPath, Tr("Failed to decrypt PDF file."),
                            Tr("Decrypt PDF"));
    SetPdfToolJobArgs(j, argv, dimofi(argv), tmpPath);
    j->fail = PdfToolFail::Notification;
    RunJob(j);
}

bool PdfDecryptDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Decrypt PDF"))) {
        return false;
    }
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
    logf("ShowPdfDecryptDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new PdfDecryptDialog();
    if (!dlg->Create(win, tab)) {
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
    WindowTab* tab = SrcTab();
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm) {
        return;
    }
    EngineBase* engine = dm->GetEngine();
    bool isOfd = EngineMupdfIsOfd(engine);
    if (!engine || !(engine->isImageCollection || isOfd)) {
        MessageBoxWarning(hwnd, Tr("Failed to save a file"), Tr("Convert to PDF"));
        return;
    }

    logf("ConvertToPdf: converting '%s' to '%s'\n", srcPath, destPath);
    RunJob(NewConvertToPdfJob(engine, destPath));
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
    if (!engine || !(engine->isImageCollection || EngineMupdfIsOfd(engine))) {
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

// --- Convert EPUB to PDF dialog ---

// the page sizes of kEbookPdfPageSizes, A5 first
static DropDown* NewPageSizeDropDown(HWND parent, PlatformFont* font) {
    auto* dd = new DropDown();
    DropDown::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.isRtl = IsUIRtl();
    dd->Create(args);
    StrVec items;
    for (const EbookPdfPageSize& ps : kEbookPdfPageSizes) {
        items.Append(ps.label);
    }
    dd->SetItems(items);
    CbSetCurrentSelection(dd, 0);
    dd->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    return dd;
}

// editable combo listing kEbookPdfFontSizes; any other size can be typed in
static DropDown* NewFontSizeCombo(HWND parent, PlatformFont* font) {
    DropDown::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.isRtl = IsUIRtl();
    args.isEditable = true;
    auto* dd = new DropDown();
    dd->Create(args);
    StrVec items;
    for (int size : kEbookPdfFontSizes) {
        items.Append(fmt("%d", size));
    }
    dd->SetItems(items);
    dd->SetText(fmt("%d", kEbookPdfDefaultFontSize));
    dd->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    return dd;
}

// lays the book out into pages of the chosen size and writes them as a PDF
// that keeps the text, then opens it
struct ConvertEpubToPdfDialog : PdfToolDialog {
    DropDown* dropPageSize = nullptr;
    DropDown* dropFontSize = nullptr;

    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void ConvertEpubToPdfDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }
    int fontSize = EbookPdfFontSizeFromText(dropFontSize->GetTextTemp());
    if (fontSize == 0) {
        MessageBoxWarning(hwnd, Tr("Invalid font size"), Tr("Convert EPUB to PDF"));
        return;
    }
    int idx = CbGetCurrentSelection(dropPageSize);
    if (idx < 0 || idx >= dimofi(kEbookPdfPageSizes)) {
        idx = 0;
    }
    const EbookPdfPageSize& ps = kEbookPdfPageSizes[idx];
    logf("ConvertEpubToPdf: '%s' -> '%s', %s, %d pt\n", srcPath, destPath, ps.label, fontSize);
    RunJob(NewEpubToPdfJob(srcPath, destPath, ps, fontSize));
}

bool ConvertEpubToPdfDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Convert EPUB to PDF"))) {
        return false;
    }
    AddLine(Tr("Page size"));
    dropPageSize = NewPageSizeDropDown(hwnd, fontText);
    lastLine->AddChild(dropPageSize);

    AddLine(Tr("Font size"));
    dropFontSize = NewFontSizeCombo(hwnd, fontText);
    lastLine->AddChild(dropFontSize);

    AddFilesBlock(DefaultPdfDestPathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddFooter(Tr("Convert to PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowConvertEpubToPdfDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!IsEpubDoc(tab)) {
        return;
    }
    logf("ShowConvertEpubToPdfDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new ConvertEpubToPdfDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// runs caj2pdf on the open CAJ file, then opens the PDF
struct ConvertCajToPdfDialog : PdfToolDialog {
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

void ConvertCajToPdfDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }
    logf("ConvertCajToPdf: '%s' -> '%s'\n", srcPath, destPath);
    RunJob(NewCajToPdfJob(srcPath, destPath));
}

bool ConvertCajToPdfDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Convert CAJ to PDF"))) {
        return false;
    }
    AddFilesBlock(DefaultPdfDestPathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddFooter(Tr("Convert to PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowConvertCajToPdfDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!IsCajDoc(tab)) {
        return;
    }
    logf("ShowConvertCajToPdfDialog: opening for '%s'\n", tab->filePath);

    auto* dlg = new ConvertCajToPdfDialog();
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
    CLSID enc = GetGdiPlusEncoderClsid(kConvertImageEncoders[idx].mime);
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
    void JobEnded() override { UpdateButton(); }
    void JobProgress(int nDone, int nTotal) override;
    void OnBrowseTemplate(VirtMouseEvent* ev = nullptr);
    void UpdateButton();
    bool SelectedPages(Vec<int>& pages);
    void UpdateExisting(bool pagesOk);
    void OnUseNewNames(VirtMouseEvent* ev = nullptr);
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
    UpdateExisting(destOk && pagesOk);
    SetActionEnabled(destOk && pagesOk && dpi > 0);
}

// the pages the mode picks; false for a custom range that doesn't parse
bool ConvertPdfToImagesDialog::SelectedPages(Vec<int>& pages) {
    VecReset(pages);
    if (radioCurrent && radioCurrent->IsChecked()) {
        WindowTab* tab = SrcTab();
        DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
        int cur = dm ? dm->CurrentPageNo() : 0;
        if (cur < 1 || cur > pageCount) {
            return false;
        }
        VecAppend(pages, cur);
        return true;
    }
    if (radioAll && radioAll->IsChecked()) {
        CollectAllPages(pageCount, pages);
        return true;
    }
    TempStr custom = pagesEdit ? pagesEdit->GetTextTemp() : TempStr{};
    return ParseDeletePages(custom, pageCount, pages);
}

// files the conversion would overwrite: the hint says how many, the action
// becomes Overwrite and Use New Names picks free names (#6439)
void ConvertPdfToImagesDialog::UpdateExisting(bool pagesOk) {
    int nExisting = 0;
    Vec<int> pages;
    if (pagesOk && SelectedPages(pages)) {
        nExisting = CountExistingImages(destEdit->GetTextTemp(), pages);
    }
    if (nExisting > 0) {
        hintText->SetText(ExistingImagesTextTemp(nExisting));
        hintText->SetColor(kColText, colTxt);
        actionBtn->SetText(Tr("Overwrite"));
    } else {
        hintText->SetText(Tr("Use <N> for the page number"));
        hintText->SetColor(kColText, colMuted);
        actionBtn->SetText(Tr("Convert"));
    }
    footerBtn->SetIsVisible(nExisting > 0);
    if (layout) {
        DoLayout(); // the texts' widths changed
    }
    HwndInvalidate(hwnd);
}

void ConvertPdfToImagesDialog::OnUseNewNames(VirtMouseEvent*) {
    Vec<int> pages;
    if (!SelectedPages(pages)) {
        return;
    }
    destEdit->SetText(UniqueImagesTemplateTemp(destEdit->GetTextTemp(), pages));
    UpdateButton();
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
    ofn.lpstrFilter = L"PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0BMP\0*.bmp\0GIF\0*.gif\0All Files\0*.*\0";
    ofn.nFilterIndex = (DWORD)(fmtIdx + 1);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = kConvertImageEncoders[fmtIdx].defExt.s;

    if (!(TakeTestSaveFileName(&ofn) || GetSaveFileNameW(&ofn))) {
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
    if (filterIdx >= 0 && filterIdx < kConvertImageFormatCount) {
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
    WindowTab* tab = SrcTab();
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        MessageBoxWarning(hwnd, Tr("Failed to convert to images."), Tr("Convert to Images"));
        return;
    }

    Vec<int> pages;
    if (!SelectedPages(pages)) {
        return;
    }

    // the box may have been filled without a change notification
    dpi = DpiFromComboText(dropDpi);
    if (dpi < 1) {
        return;
    }
    if (!PagesFitDpi(engine, pages, dpi)) {
        MessageBoxWarning(hwnd, Tr("Too large for this DPI"), Tr("Convert to Images"));
        return;
    }

    logf("ConvertPdfToImages: '%s' -> '%s', %d page(s), %d DPI\n", srcPath, destPath, len(pages), dpi);
    RunJob(NewImagesJob(engine, dm->GetRotation(), destPath, pages, dpi));
}

// the hint says which page is being converted
void ConvertPdfToImagesDialog::JobProgress(int nDone, int nTotal) {
    hintText->SetText(fmt(Tr("Page %d of %d").s, nDone + 1, nTotal));
    hintText->SetColor(kColText, colMuted);
    footerBtn->SetIsVisible(false);
    DoLayout();
    HwndInvalidate(hwnd);
}

bool ConvertPdfToImagesDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, Tr("Convert to Images"))) {
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
    Vec<int> allPages;
    CollectAllPages(pageCount, allPages);
    pngPath = UniqueImagesTemplateTemp(pngPath, allPages);
    AddFilesBlock(pngPath, {}, {}, true);
    browseBtn->onClick =
        MkMethod1<ConvertPdfToImagesDialog, VirtMouseEvent*, &ConvertPdfToImagesDialog::OnBrowseTemplate>(this);

    AddFooter(Tr("Convert"), Tr("Use <N> for the page number"), Tr("Use New Names"));
    footerBtn->onClick =
        MkMethod1<ConvertPdfToImagesDialog, VirtMouseEvent*, &ConvertPdfToImagesDialog::OnUseNewNames>(this);
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
    if (!tab->AsFixed()) {
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
        MessageBoxWarning(parent, Tr("Failed to save the selection as an image."), Tr("Save Selection As Image"));
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
    WindowTab* tab = SrcTab();
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
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
    ofn.lpstrFilter = L"PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0BMP\0*.bmp\0GIF\0*.gif\0All Files\0*.*\0";
    ofn.nFilterIndex = (DWORD)(fmtIdx + 1);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = kConvertImageEncoders[fmtIdx].defExt.s;

    if (!(TakeTestSaveFileName(&ofn) || GetSaveFileNameW(&ofn))) {
        return;
    }
    TempStr picked = ToUtf8Temp(dstFileName);
    destEdit->SetText(picked);
    int filterIdx = (int)ofn.nFilterIndex - 1;
    if (filterIdx >= 0 && filterIdx < kConvertImageFormatCount) {
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
    WindowTab* tab = SrcTab();
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    TempStr withExt = WithDefaultImageExtTemp(destPath);
    if (len(withExt) == 0) {
        MessageBoxWarning(hwnd, Tr("Unsupported image format."), Tr("Save Selection As Image"));
        return;
    }
    // the box may have been filled without a change notification
    dpi = DpiFromComboText(dropDpi);
    if (dpi < 1) {
        return;
    }
    Pixmap* px = RenderSelectionPixmap(engine, dm->GetRotation(), pageNo, selRect, (float)dpi);
    if (!px) {
        MessageBoxWarning(hwnd, Tr("Failed to save the selection as an image."), Tr("Save Selection As Image"));
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

// the images picked in a multi-select Open dialog; false if cancelled
static bool PickImageFiles(HWND owner, StrVec& paths) {
    // a multi-select buffer: the folder, then the file names, then an empty string
    constexpr int kBufSize = 64 * 1024;
    Vec<WCHAR> buf;
    VecAppendBlanks(buf, kBufSize);
    // the filter's separators become the NULs OPENFILENAME wants
    TempWStr filter = ToWStrTemp(ImagesToPdfFilter());
    for (int i = 0; i < len(filter); i++) {
        if (filter.s[i] == 1) {
            filter.s[i] = 0;
        }
    }
    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buf.els;
    ofn.nMaxFile = kBufSize;
    ofn.lpstrFilter = filter.s;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }
    WCHAR* first = buf.els;
    WCHAR* name = first + wcslen(first) + 1;
    if (*name == 0) {
        // one file: the buffer is its full path
        paths.Append(ToUtf8Temp(first));
    }
    for (; *name; name += wcslen(name) + 1) {
        paths.Append(path::JoinTemp(ToUtf8Temp(first), ToUtf8Temp(name)));
    }
    return len(paths) > 0;
}

// Make PDF From Images (CmdImagesToPdf): picks images, then where the PDF goes,
// makes it with a page per image (in name order: scan1, scan2, ...) and opens it
void MakePdfFromImages(MainWindow* win) {
    StrVec paths;
    if (!TakeTestOpenFileNames(paths) && !PickImageFiles(win->hwndFrame, paths)) {
        return;
    }
    SortNatural(&paths);

    WCHAR dstFileName[MAX_PATH + 1]{};
    wstr::BufSet(WStr(dstFileName, MAX_PATH), ToWStrTemp(ImagesToPdfDestTemp(paths[0])));
    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = L"PDF documents\0*.pdf\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"pdf";
    if (!(TakeTestSaveFileName(&ofn) || GetSaveFileNameW(&ofn))) {
        return;
    }
    TempStr dest = ToUtf8Temp(dstFileName);
    TempStr err = MakePdfFromImagesTemp(paths, dest);
    if (len(err) > 0) {
        ShowWarningNotification(win, err, kNotif5SecsTimeOut);
        return;
    }
    LoadArgs args(dest, win);
    StartLoadDocument(&args);
}

// Export Annotations (CmdExportAnnotations): the PDF's annotations, unsaved
// ones too, as Markdown or CSV, to a file the Save As dialog picks
void ExportAnnotations(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    if (!engine || len(tab->filePath) == 0) {
        return;
    }
    WCHAR dstFileName[MAX_PATH + 1]{};
    wstr::BufSet(WStr(dstFileName, MAX_PATH), ToWStrTemp(AnnotationsExportDestTemp(tab->filePath)));
    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = win->hwndFrame;
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);
    ofn.lpstrFilter = L"Markdown\0*.md\0CSV\0*.csv\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"md";
    if (!(TakeTestSaveFileName(&ofn) || GetSaveFileNameW(&ofn))) {
        return;
    }
    TempStr dest = ToUtf8Temp(dstFileName);
    TempStr err = ExportAnnotationsTemp(engine, tab->filePath, dest);
    if (len(err) > 0) {
        ShowWarningNotification(win, err, kNotif5SecsTimeOut);
        return;
    }
    ShowTemporaryNotification(win, fmt(Tr("Saved annotations to '%s'").s, dest));
}
