/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "PdfCreator.h"
#include "ImageReader.h"
#include "PngOptimizer.h"
#include "base/Pixmap.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Translations.h"
#include "ExternalViewers.h"
#include "Flags.h"
#include "DisplayModel.h"
#include "Theme.h"

#include "DarkMode_win.h"

extern "C" int pdfbake_main(int argc, char** argv);
extern "C" int pdfclean_main(int argc, char** argv);
extern "C" int muconvert_main(int argc, char** argv);
extern "C" void fz_set_optind(int val);

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

// Every dialog here has the same shape: the source path, a destination edit
// with a browse button, sometimes one more labelled field, and an action +
// Cancel row. PdfToolDialog builds that as one layout tree: the labels and
// buttons are virtual controls, the text fields are native edits, and they sit
// in the same VBox/HBox side by side. Each dialog only adds its rows and
// implements DoIt().
struct PdfToolDialog : WindowBase {
    Str srcPath; // owned
    MainWindow* win = nullptr;
    // what the "..." button's save dialog offers
    WStr browseFilter;
    WStr browseDefExt;

    VBox* mainBox = nullptr;
    VirtText* pathLabel = nullptr;
    Edit* destEdit = nullptr;
    VirtButton* browseBtn = nullptr;
    VirtButton* actionBtn = nullptr;
    VirtButton* cancelBtn = nullptr;
    // the row AddRow() built last, so a dialog can put more in it
    HBox* lastRow = nullptr;
    int rowGap = 0;
    int gap = 0;

    ~PdfToolDialog() override;

    bool CreateToolDialog(MainWindow*, WindowTab*, Str title);
    HBox* AddRow();
    void AddPathRow();
    void AddDestRow(Str destPath, WStr filter, WStr defExt);
    Edit* AddLabeledEdit(Str label, Str text, bool isPassword = false);
    void AddButtonsRow(Str actionText, Str hint = {});
    void FinishDialog(Edit* focusOn);
    VirtButton* NewButton(Str text, bool isDefault);

    void OnBrowse(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
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
    delete dlg;
}

void PdfToolDialog::OnCancel(VirtMouseEvent*) {
    Close();
}

void PdfToolDialog::OnBrowse(VirtMouseEvent*) {
    BrowseForDest(hwnd, destEdit, browseFilter, browseDefExt);
}

VirtButton* PdfToolDialog::NewButton(Str text, bool isDefault) {
    return NewThemedButton(hwnd, text, font, isDefault);
}

bool PdfToolDialog::CreateToolDialog(MainWindow* w, WindowTab* tab, Str title) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    PlatformFont* dialogFont = GetDefaultGuiFont();
    closeOnEsc = true;
    onClose = MkFunc1Void(PdfToolDialogOnClose);

    CreateCustomArgs cargs;
    cargs.title = title;
    cargs.font = dialogFont;
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    cargs.bgColor = DarkModeDialogBgColor();
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    // make the dialog an owned (rather than child) window of the main frame
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    rowGap = DpiScale(6);
    gap = DpiScale(8);
    mainBox = new VBox();
    mainBox->alignCross = CrossAxisAlign::Stretch;
    layout = new Padding(mainBox, DpiScaledInsets(10));
    return true;
}

// a row of the dialog, with the gap that separates it from the row above
HBox* PdfToolDialog::AddRow() {
    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    if (mainBox->ChildrenCount() > 0) {
        mainBox->AddChild(new Padding(row, Insets{rowGap, 0, 0, 0}));
    } else {
        mainBox->AddChild(row);
    }
    lastRow = row;
    return row;
}

// the source path, ellipsized in the middle for long paths
void PdfToolDialog::AddPathRow() {
    pathLabel = NewVirtText({.s = srcPath, .font = font, .isRtl = IsUIRtl(), .ellipsis = true});
    mainBox->AddChild(pathLabel);
}

void PdfToolDialog::AddDestRow(Str destPath, WStr filter, WStr defExt) {
    browseFilter = filter;
    browseDefExt = defExt;
    HBox* row = AddRow();
    row->gap = font->averageCharWidth;

    Edit::CreateArgs args;
    args.parent = hwnd;
    args.withBorder = true;
    args.font = font;
    args.text = destPath;
    args.isRtl = IsUIRtl();
    destEdit = new Edit();
    destEdit->Create(args);
    row->AddChild(destEdit, 1);

    browseBtn = NewButton("...", false);
    browseBtn->onClick = MkMethod1<PdfToolDialog, VirtMouseEvent*, &PdfToolDialog::OnBrowse>(this);
    row->AddChild(browseBtn);

    // align the source path label's text with the destination edit's text,
    // which is inset by the edit's border + internal left margin
    if (pathLabel) {
        pathLabel->padding.left = destEdit->GetLeftTextMargin();
    }
}

Edit* PdfToolDialog::AddLabeledEdit(Str label, Str text, bool isPassword) {
    HBox* row = AddRow();

    row->AddChild(NewVirtText({.s = label, .font = font, .isRtl = IsUIRtl()}));
    row->AddChild(new Spacer(gap, 0));

    Edit::CreateArgs eargs;
    eargs.parent = hwnd;
    eargs.withBorder = true;
    eargs.font = font;
    eargs.text = text;
    eargs.isRtl = IsUIRtl();
    auto* e = new Edit();
    e->Create(eargs);
    if (isPassword) {
        HwndSetWindowStyle(e->hwnd, ES_PASSWORD, true);
    }
    row->AddChild(e, 1);
    return e;
}

void PdfToolDialog::AddButtonsRow(Str actionText, Str hint) {
    HBox* row = AddRow();
    row->gap = font->averageCharWidth;
    if (hint) {
        row->AddChild(NewVirtText({.s = hint, .font = font, .isRtl = IsUIRtl()}));
    }
    // a flexible spacer pushes the buttons to the right
    row->AddChild(new Spacer(0, 0), 1);

    actionBtn = NewButton(actionText, true);
    actionBtn->onClick = MkMethod1<PdfToolDialog, VirtMouseEvent*, &PdfToolDialog::DoIt>(this);
    row->AddChild(actionBtn);

    cancelBtn = NewButton(_TRA("Cancel"), false);
    cancelBtn->onClick = MkMethod1<PdfToolDialog, VirtMouseEvent*, &PdfToolDialog::OnCancel>(this);
    row->AddChild(cancelBtn);
}

void PdfToolDialog::FinishDialog(Edit* focusOn) {
    // size to a width that fits the source path (clamped), let the layout
    // compute the height
    int minClientW = DpiScale(480);
    int clientW = CalcDlgWidth(font, srcPath, minClientW, DpiScale(10));
    Size size = layout->Layout(ExpandHeight(clientW));
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);
    // positions everything and picks up the virtual controls to paint
    DoLayout(size);

    HwndCenterDialog(hwnd, win->hwndFrame);
    DarkModeApplyToWindowAndEraseBg(hwnd);
    SetIsVisible(true);
    if (focusOn) {
        HwndSetFocus(focusOn->hwnd);
    }
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

    logf("PdfBakeDoIt: baking '%s' to '%s'\n", srcPath, destPath);

    // build argv for pdfbake_main: "bake" input output
    char* argv[] = {(char*)"bake", CStrTemp(srcPath), CStrTemp(destPath)};
    int argc = 3;

    fz_set_optind(0);
    int res = pdfbake_main(argc, argv);
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
        MessageBoxWarning(hwnd, "Failed to bake PDF file.", _TRA("Bake PDF"));
    }
}

bool PdfBakeDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, _TRA("Bake PDF"))) {
        return false;
    }
    AddPathRow();
    AddDestRow(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddButtonsRow(_TRA("Bake PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfBakeDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
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

struct PdfExtractTextDialog : PdfToolDialog {
    Edit* pagesEdit = nullptr;
    bool Create(MainWindow* win, WindowTab* tab);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
};

static bool ExtractTextViaEngine(PdfExtractTextDialog* dlg, Str destPath, Str pages) {
    MainWindow* win = dlg->win;
    if (!win || !win->ctrl) {
        return false;
    }
    DisplayModel* dm = win->ctrl->AsFixed();
    if (!dm) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return false;
    }
    int pageCount = engine->PageCount();
    Vec<PageRange> ranges;
    if (!ParsePageRanges(pages, ranges)) {
        return false;
    }
    str::Builder text;
    for (auto& range : ranges) {
        int start = std::max(range.start, 1);
        int end = std::min(range.end, pageCount);
        for (int pageNo = start; pageNo <= end; pageNo++) {
            PageText pt = engine->ExtractPageText(pageNo);
            if (pt.text) {
                text.Append(pt.text.s);
                text.AppendChar('\n');
            }
            FreePageText(&pt);
        }
    }
    return file::WriteFile(destPath, ToStr(text));
}

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
        ok = ExtractTextViaEngine(this, destPath, pages);
    }

    if (ok) {
        logf("PdfExtractTextDoIt: extracted successfully\n");
        TempStr path = str::DupTemp(destPath);
        Close();
        OpenPathInDefaultFileManager(path);
    } else {
        logf("PdfExtractTextDoIt: failed to extract text, isPdf: %d\n", (int)isPdf);
        MessageBoxWarning(hwnd, "Failed to extract text.", _TRA("Extract Text"));
    }
}

bool PdfExtractTextDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, _TRA("Extract Text From PDF"))) {
        return false;
    }
    AddPathRow();
    TempStr noExt = path::GetPathNoExtTemp(srcPath);
    TempStr txtPath = str::JoinTemp(noExt, StrL(".txt"));
    AddDestRow(MakeUniqueFilePathTemp(txtPath), L"Text Files\0*.txt\0All Files\0*.*\0", L"txt");
    int pageCount = w->ctrl ? w->ctrl->PageCount() : 1;
    pagesEdit = AddLabeledEdit(_TRA("Pages:"), fmt("1-%d", pageCount));
    AddButtonsRow(_TRA("Extract Text"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfExtractTextDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
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
        MessageBoxWarning(hwnd, "Failed to compress PDF file.", _TRA("Compress PDF"));
    }
}

bool PdfCompressDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, _TRA("Compress PDF"))) {
        return false;
    }
    AddPathRow();
    AddDestRow(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddButtonsRow(_TRA("Compress PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfCompressDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
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
        MessageBoxWarning(hwnd, "Failed to decompress PDF file.", _TRA("Decompress PDF"));
    }
}

bool PdfDecompressDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, _TRA("Decompress PDF"))) {
        return false;
    }
    AddPathRow();
    AddDestRow(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddButtonsRow(_TRA("Decompress PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfDecompressDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
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

// The dialog's content is a VirtCtrl tree: the labels and buttons are virtual
// controls, while the two text fields stay real HWND edits, hosted in the tree
// so they take part in the same layout.
struct PdfDeletePageDialog : PdfToolDialog {
    bool isExtract = false;
    int pageCount = 0;
    Edit* pagesEdit = nullptr;

    bool Create(MainWindow* win, WindowTab* tab, bool isExtract);
    void DoIt(VirtMouseEvent* ev = nullptr) override;
    void UpdateButton();
};

// Parse delete page ranges like "1,3-8,13-N" where N means last page.
// Returns a sorted list of unique 1-based page numbers to delete.
// Returns false if the syntax is invalid or any page is out of range.
static bool ParseDeletePages(Str s, int pageCount, Vec<int>& pagesToDelete) {
    if (!s) {
        return false;
    }
    StrVec parts;
    Split(&parts, s, ",", true);
    if (len(parts) == 0) {
        return false;
    }
    for (int pi = 0; pi < len(parts); pi++) {
        Str part = parts[pi];
        str::TrimWSInPlace(part, str::TrimOpt::Both);
        if (!part) {
            return false;
        }
        // check for range "A-B" where A/B can be a number or "N"
        Str startStr, endStr;
        if (str::CutChar(part, '-', &startStr, &endStr)) {
            str::TrimWSInPlace(startStr, str::TrimOpt::Both);
            str::TrimWSInPlace(endStr, str::TrimOpt::Both);
            if (!startStr) {
                return false;
            }
            // "8-" means "8-N" (from page 8 to the last page)
            bool endIsEmpty = !endStr;
            int start, end;
            if (str::EqI(startStr, StrL("N"))) {
                start = pageCount;
            } else {
                start = !str::IsNull(str::Parse(startStr, "%d%$", &start)) ? start : -1;
            }
            if (endIsEmpty || str::EqI(endStr, StrL("N"))) {
                end = pageCount;
            } else {
                end = !str::IsNull(str::Parse(endStr, "%d%$", &end)) ? end : -1;
            }
            if (start < 1 || start > pageCount || end < 1 || end > pageCount || start > end) {
                return false;
            }
            for (int i = start; i <= end; i++) {
                pagesToDelete.Append(i);
            }
        } else {
            // single page
            int page;
            if (str::EqI(part, StrL("N"))) {
                page = pageCount;
            } else {
                page = !str::IsNull(str::Parse(part, "%d%$", &page)) ? page : -1;
            }
            if (page < 1 || page > pageCount) {
                return false;
            }
            pagesToDelete.Append(page);
        }
    }
    if (len(pagesToDelete) == 0) {
        return false;
    }
    // sort and deduplicate
    VecSort(pagesToDelete, [](const int* a, const int* b) -> int { return *a - *b; });
    int prev = -1;
    Vec<int> unique;
    for (int p : pagesToDelete) {
        if (p != prev) {
            unique.Append(p);
            prev = p;
        }
    }
    pagesToDelete = unique;
    return true;
}

// Build the page range string of pages to KEEP (complement of pagesToDelete).
static TempStr BuildKeepPagesRangeTemp(int pageCount, const Vec<int>& pagesToDelete) {
    str::Builder s;
    int delIdx = 0;
    int rangeStart = -1;
    int rangeEnd = -1;
    for (int p = 1; p <= pageCount; p++) {
        bool shouldDelete = (delIdx < len(pagesToDelete) && pagesToDelete[delIdx] == p);
        if (shouldDelete) {
            delIdx++;
            if (rangeStart != -1) {
                if (len(s) > 0) {
                    s.AppendChar(',');
                }
                if (rangeStart == rangeEnd) {
                    s.Append(fmt("%d", rangeStart));
                } else {
                    s.Append(fmt("%d-%d", rangeStart, rangeEnd));
                }
                rangeStart = -1;
            }
        } else {
            if (rangeStart == -1) {
                rangeStart = p;
            }
            rangeEnd = p;
        }
    }
    if (rangeStart != -1) {
        if (len(s) > 0) {
            s.AppendChar(',');
        }
        if (rangeStart == rangeEnd) {
            s.Append(fmt("%d", rangeStart));
        } else {
            s.Append(fmt("%d-%d", rangeStart, rangeEnd));
        }
    }
    return ToStrTemp(s);
}

// Format a sorted list of page numbers as a compact range string (e.g. "1-3,5,7-10").
static TempStr FormatPageRangeTemp(const Vec<int>& pages) {
    str::Builder s;
    int i = 0;
    int n = len(pages);
    while (i < n) {
        int start = pages[i];
        int end = start;
        while (i + 1 < n && pages[i + 1] == end + 1) {
            end = pages[++i];
        }
        if (len(s) > 0) {
            s.AppendChar(',');
        }
        if (start == end) {
            s.Append(fmt("%d", start));
        } else {
            s.Append(fmt("%d-%d", start, end));
        }
        i++;
    }
    return ToStrTemp(s);
}

void PdfDeletePageDialog::UpdateButton() {
    TempStr pages = pagesEdit->GetTextTemp();
    Vec<int> parsedPages;
    bool valid = ParseDeletePages(pages, pageCount, parsedPages);
    // for delete mode, can't delete all pages
    if (valid && !isExtract && len(parsedPages) >= pageCount) {
        valid = false;
    }
    if (valid == actionBtn->HasFlag(vwfEnabled)) {
        return;
    }
    actionBtn->SetFlag(vwfEnabled, valid);
    actionBtn->Invalidate();
}

void PdfDeletePageDialog::DoIt(VirtMouseEvent*) {
    TempStr destPath = destEdit->GetTextTemp();
    if (len(destPath) == 0) {
        return;
    }

    TempStr pages = pagesEdit->GetTextTemp();

    Vec<int> parsedPages;
    if (!ParseDeletePages(pages, pageCount, parsedPages)) {
        return;
    }
    if (!isExtract && len(parsedPages) >= pageCount) {
        return;
    }

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

    // equivalent of: clean -gggg -e 100 -f -i -t -Z input.pdf output.pdf <page-range>
    // use the same compression flags as Compress PDF so the result is re-written
    // compactly; otherwise the kept pages drag along the original's full content
    // and the output is nearly as big as the source
    char* argv[] = {(char*)"clean",    (char*)"-gggg",     (char*)"-e",        (char*)"100",
                    (char*)"-f",       (char*)"-i",        (char*)"-t",        (char*)"-Z",
                    CStrTemp(srcPath), CStrTemp(destPath), CStrTemp(pageRange)};
    int argc = 11;

    fz_set_optind(0);
    int res = pdfclean_main(argc, argv);
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
        Str title = isExtract ? _TRA("Extract Pages From PDF") : _TRA("Delete Pages From PDF");
        MessageBoxWarning(hwnd, msg, title);
    }
}

// the buttons are virtual controls, so they are styled here rather than by the
// system: a filled box with a border, brighter on hover

bool PdfDeletePageDialog::Create(MainWindow* w, WindowTab* tab, bool isExtractArg) {
    isExtract = isExtractArg;
    Str title = isExtract ? _TRA("Extract Pages From PDF") : _TRA("Delete Pages From PDF");
    if (!CreateToolDialog(w, tab, title)) {
        return false;
    }
    pageCount = w->ctrl ? w->ctrl->PageCount() : 0;
    AddPathRow();
    AddDestRow(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");

    int currentPage = w->ctrl ? w->ctrl->CurrentPageNo() : 1;
    Str pagesLabel = isExtract ? _TRA("Pages To Extract:") : _TRA("Pages To Delete:");
    pagesEdit = AddLabeledEdit(pagesLabel, fmt("%d", currentPage));
    // "of N" after the edit
    lastRow->AddChild(new Spacer(gap, 0));
    lastRow->AddChild(NewVirtText({.s = fmt("of %d", pageCount), .font = font, .isRtl = IsUIRtl()}));

    Str actionText = isExtract ? _TRA("Extract Pages") : _TRA("Delete Pages");
    AddButtonsRow(actionText, "Syntax: 2,5-7,13-");
    FinishDialog(pagesEdit);

    // attach the change handler only now that actionBtn exists, then set the
    // initial validation state (Edit::Create fires onTextChanged on initial text)
    pagesEdit->onTextChanged = MkMethod0<PdfDeletePageDialog, &PdfDeletePageDialog::UpdateButton>(this);
    UpdateButton();
    return true;
}

static void ShowPdfPageRangeDialog(MainWindow* win, bool isExtract) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
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

void ShowPdfDeletePageDialog(MainWindow* win) {
    ShowPdfPageRangeDialog(win, false);
}

void ShowPdfExtractPagesDialog(MainWindow* win) {
    ShowPdfPageRangeDialog(win, true);
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
    bool valid = len(pwd) > 0;
    if (valid == actionBtn->HasFlag(vwfEnabled)) {
        return;
    }
    actionBtn->SetFlag(vwfEnabled, valid);
    actionBtn->Invalidate();
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
        MessageBoxWarning(hwnd, "Failed to encrypt PDF file.", _TRA("Encrypt PDF"));
    }
}

bool PdfEncryptDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, _TRA("Encrypt PDF"))) {
        return false;
    }
    AddPathRow();
    AddDestRow(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    passwordEdit = AddLabeledEdit(_TRA("Password:"), {}, true);
    AddButtonsRow(_TRA("Encrypt PDF"));
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
    if (!tab || !tab->filePath) {
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
        MessageBoxWarning(hwnd, "Failed to decrypt PDF file.", _TRA("Decrypt PDF"));
    }
}

bool PdfDecryptDialog::Create(MainWindow* w, WindowTab* tab, Str pwd) {
    if (!CreateToolDialog(w, tab, _TRA("Decrypt PDF"))) {
        return false;
    }
    password = str::Dup(pwd);
    AddPathRow();
    AddDestRow(MakeUniqueFilePathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddButtonsRow(_TRA("Decrypt PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowPdfDecryptDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
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

// Default destination: same path with .pdf extension, made unique if the file
// already exists (e.g. comic.cbz → comic.pdf, or comic.1.pdf if taken).
static TempStr DefaultPdfDestPathTemp(Str srcPath) {
    if (!srcPath) {
        return {};
    }
    TempStr noExt = path::GetPathNoExtTemp(srcPath);
    if (!noExt) {
        noExt = str::DupTemp(srcPath);
    }
    TempStr pdfPath = str::JoinTemp(noExt, StrL(".pdf"));
    return MakeUniqueFilePathTemp(pdfPath);
}

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
    if (!engine || !engine->IsImageCollection()) {
        MessageBoxWarning(hwnd, _TRA("Failed to save a file"), _TRA("Convert to PDF"));
        return;
    }

    logf("ConvertToPdf: converting '%s' to '%s'\n", srcPath, destPath);

    TempStr producer = fmt("SumatraPDF %s", currentVersion);
    PdfCreator::SetProducerName(producer);
    // Formats PDF cannot re-wrap (WebP, JXL, HEIC, AVIF, TGA, …): decode via
    // the same codecs we use for viewing, then PNG + zopfli before embed.
    auto toOptimizedPng = [](Str data) -> Str {
        Pixmap* px = PixmapFromData(data);
        if (!px) {
            logf("ConvertToPdf: decode-to-pixmap failed (%d bytes)\n", len(data));
            return {};
        }
        Str png = EncodeAndOptimizePngFromPixmap(px);
        FreePixmap(px);
        return png;
    };
    bool ok = PdfCreator::SaveImageCollectionAsPdf(destPath, engine, toOptimizedPng);
    if (ok) {
        logf("ConvertToPdf: converted successfully\n");
        MainWindow* w = win;
        TempStr path = str::DupTemp(destPath);
        Close();
        LoadArgs args(path, w);
        StartLoadDocument(&args);
    } else {
        logf("ConvertToPdf: SaveImageCollectionAsPdf failed\n");
        MessageBoxWarning(hwnd, _TRA("Failed to save a file"), _TRA("Convert to PDF"));
    }
}

bool ConvertToPdfDialog::Create(MainWindow* w, WindowTab* tab) {
    if (!CreateToolDialog(w, tab, _TRA("Convert to PDF"))) {
        return false;
    }
    AddPathRow();
    AddDestRow(DefaultPdfDestPathTemp(srcPath), L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
    AddButtonsRow(_TRA("Convert to PDF"));
    FinishDialog(destEdit);
    return true;
}

void ShowConvertToPdfDialog(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return;
    }
    EngineBase* engine = tab->GetEngine();
    if (!engine || !engine->IsImageCollection()) {
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
