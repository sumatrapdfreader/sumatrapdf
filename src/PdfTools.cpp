/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Log.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SumatraPDF.h"
#include "SumatraConfig.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Translations.h"
#include "ExternalViewers.h"
#include "Flags.h"
#include "DisplayModel.h"
#include "Theme.h"

#include "DarkModeSubclass.h"

extern "C" int pdfbake_main(int argc, char** argv);
extern "C" int pdfclean_main(int argc, char** argv);
extern "C" int muconvert_main(int argc, char** argv);
extern "C" void fz_set_optind(int val);

// compute a dialog client width that fits the source path text, clamped to a
// minimum and to 80% of the screen width (long paths get ellipsized instead)
static int CalcDlgWidth(HWND hwndParent, HFONT font, Str path, int minW, int padding) {
    HDC hdc = GetDC(nullptr);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    int pathCch;
    WCHAR* pathW = CWStrTemp(path, pathCch);
    SIZE sz{};
    GetTextExtentPoint32W(hdc, pathW, pathCch, &sz);
    SelectObject(hdc, oldFont);
    ReleaseDC(nullptr, hdc);
    int dlgW = sz.cx + 2 * padding + DpiScale(hwndParent, 32);
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
    GetWindowTextW(edit->hwnd, dstFileName, MAX_PATH);

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

// create the source-path Static used as the first row of the PDF tool dialogs,
// ellipsized in the middle for long paths
static Static* CreatePathLabel(HWND parent, HFONT font, Str path, bool isRtl) {
    Static::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.text = path;
    args.isRtl = isRtl;
    auto c = new Static();
    c->Create(args);
    DWORD style = (DWORD)GetWindowLongPtrW(c->hwnd, GWL_STYLE);
    SetWindowLongPtrW(c->hwnd, GWL_STYLE, style | SS_PATHELLIPSIS);
    return c;
}

// Bake PDF dialog, built with the wingui layout library (VBox/HBox/Padding)
// instead of manual control positioning.
struct PdfBakeDialog : Wnd {
    HFONT hFont = nullptr;
    Str srcPath;
    MainWindow* win = nullptr;

    ILayout* mainLayout = nullptr;
    Static* pathLabel = nullptr;
    Edit* destEdit = nullptr;
    Button* browseBtn = nullptr;
    Button* bakeBtn = nullptr;
    Button* cancelBtn = nullptr;

    ~PdfBakeDialog() override;

    bool Create(MainWindow* win, WindowTab* tab);
    void OnBrowse();
    void DoBake();
    void OnCancel();
};

PdfBakeDialog::~PdfBakeDialog() {
    str::FreePtr(&srcPath);
    delete mainLayout;
}

void PdfBakeDialog::OnCancel() {
    Close();
}

void PdfBakeDialog::OnBrowse() {
    BrowseForDest(hwnd, destEdit, L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
}

void PdfBakeDialog::DoBake() {
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

static void PdfBakeOnClose(Wnd::CloseEvent* ev) {
    auto dlg = (PdfBakeDialog*)ev->e->self;
    delete dlg;
}

bool PdfBakeDialog::Create(MainWindow* w, WindowTab* tab) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    hFont = GetDefaultGuiFont();
    onClose = MkFunc1Void(PdfBakeOnClose);

    CreateCustomArgs cargs;
    cargs.title = _TRA("Bake PDF");
    cargs.font = hFont;
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    if (UseDarkModeLib() && DarkMode::isEnabled()) {
        cargs.bgColor = ThemeWindowControlBackgroundColor();
    } else {
        cargs.bgColor = MkGray(0xee);
    }
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    // make the dialog an owned (rather than child) window of the main frame
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    bool isRtl = IsUIRtl();
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // row 1: source path label (ellipsized in the middle for long paths)
    pathLabel = CreatePathLabel(hwnd, hFont, srcPath, isRtl);
    vbox->AddChild(pathLabel);

    // row 2: dest edit (flex) + browse button
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.font = hFont;
        args.text = MakeUniqueFilePathTemp(srcPath);
        args.isRtl = isRtl;
        destEdit = new Edit();
        destEdit->Create(args);
        hbox->AddChild(destEdit, 1);

        browseBtn = new Button();
        browseBtn->onClick = MkMethod0<PdfBakeDialog, &PdfBakeDialog::OnBrowse>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = "...";
        bargs.isRtl = isRtl;
        browseBtn->Create(bargs);
        hbox->AddChild(new Padding(browseBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 3: Bake + Cancel buttons (right-aligned), each sized to its label
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        bakeBtn = new Button();
        bakeBtn->isDefault = true;
        bakeBtn->onClick = MkMethod0<PdfBakeDialog, &PdfBakeDialog::DoBake>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = _TRA("Bake PDF");
        bargs.isRtl = isRtl;
        bakeBtn->Create(bargs);
        hbox->AddChild(bakeBtn);

        cancelBtn = new Button();
        cancelBtn->onClick = MkMethod0<PdfBakeDialog, &PdfBakeDialog::OnCancel>(this);
        Button::CreateArgs cargs2;
        cargs2.parent = hwnd;
        cargs2.font = hFont;
        cargs2.text = _TRA("Cancel");
        cargs2.isRtl = isRtl;
        cancelBtn->Create(cargs2);
        hbox->AddChild(new Padding(cancelBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // align the source path label's text with the destination edit's text,
    // which is inset by the edit's border + internal left margin
    pathLabel->insets.left = destEdit->GetLeftTextMargin();
    mainLayout = new Padding(vbox, DpiScaledInsets(hwnd, 10));

    // size to a width that fits the source path (clamped), let the layout
    // compute the height
    int minClientW = DpiScale(hwnd, 480);
    int clientW = CalcDlgWidth(hwnd, hFont, srcPath, minClientW, DpiScale(hwnd, 10));
    Size size = mainLayout->Layout(ExpandHeight(clientW));
    Rect bounds{0, 0, size.dx, size.dy};
    mainLayout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);

    CenterDialog(hwnd, w->hwndFrame);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    SetIsVisible(true);
    HwndSetFocus(destEdit->hwnd);
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
    if (!CouldBePDFDoc(tab)) {
        return;
    }
    logf("ShowPdfBakeDialog: opening for '%s'\n", tab->filePath);

    auto dlg = new PdfBakeDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Extract PDF Text dialog ---

struct PdfExtractTextDialog : Wnd {
    HFONT hFont = nullptr;
    Str srcPath;
    MainWindow* win = nullptr;

    ILayout* mainLayout = nullptr;
    Static* pathLabel = nullptr;
    Edit* destEdit = nullptr;
    Button* browseBtn = nullptr;
    Static* pagesLabel = nullptr;
    Edit* pagesEdit = nullptr;
    Button* extractBtn = nullptr;
    Button* cancelBtn = nullptr;

    ~PdfExtractTextDialog() override;

    bool Create(MainWindow* win, WindowTab* tab);
    void OnBrowse();
    void DoExtract();
    void OnCancel();
};

PdfExtractTextDialog::~PdfExtractTextDialog() {
    str::FreePtr(&srcPath);
    delete mainLayout;
}

void PdfExtractTextDialog::OnCancel() {
    Close();
}

void PdfExtractTextDialog::OnBrowse() {
    BrowseForDest(hwnd, destEdit, L"Text Files\0*.txt\0All Files\0*.*\0", L"txt");
}

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

void PdfExtractTextDialog::DoExtract() {
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
    bool isPdf = tab && CouldBePDFDoc(tab);
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

static void PdfExtractTextOnClose(Wnd::CloseEvent* ev) {
    auto dlg = (PdfExtractTextDialog*)ev->e->self;
    delete dlg;
}

bool PdfExtractTextDialog::Create(MainWindow* w, WindowTab* tab) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    hFont = GetDefaultGuiFont();
    onClose = MkFunc1Void(PdfExtractTextOnClose);

    CreateCustomArgs cargs;
    cargs.title = _TRA("Extract Text");
    cargs.font = hFont;
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    if (UseDarkModeLib() && DarkMode::isEnabled()) {
        cargs.bgColor = ThemeWindowControlBackgroundColor();
    } else {
        cargs.bgColor = MkGray(0xee);
    }
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    bool isRtl = IsUIRtl();
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // row 1: source path label
    pathLabel = CreatePathLabel(hwnd, hFont, srcPath, isRtl);
    vbox->AddChild(pathLabel);

    // row 2: dest edit (flex) + browse button
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        TempStr noExt = path::GetPathNoExtTemp(srcPath);
        TempStr txtPath = str::JoinTemp(noExt, StrL(".txt"));
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.font = hFont;
        args.text = MakeUniqueFilePathTemp(txtPath);
        args.isRtl = isRtl;
        destEdit = new Edit();
        destEdit->Create(args);
        hbox->AddChild(destEdit, 1);

        browseBtn = new Button();
        browseBtn->onClick = MkMethod0<PdfExtractTextDialog, &PdfExtractTextDialog::OnBrowse>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = "...";
        bargs.isRtl = isRtl;
        browseBtn->Create(bargs);
        hbox->AddChild(new Padding(browseBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 3: "Pages:" label + pages edit (flex)
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Static::CreateArgs largs;
        largs.parent = hwnd;
        largs.font = hFont;
        largs.text = _TRA("Pages:");
        largs.isRtl = isRtl;
        pagesLabel = new Static();
        pagesLabel->Create(largs);
        hbox->AddChild(new Padding(pagesLabel, DpiScaledInsets(hwnd, 0, 4, 0, 0)));

        int pageCount = w->ctrl ? w->ctrl->PageCount() : 1;
        Edit::CreateArgs eargs;
        eargs.parent = hwnd;
        eargs.withBorder = true;
        eargs.font = hFont;
        eargs.text = fmt("1-%d", pageCount);
        eargs.isRtl = isRtl;
        pagesEdit = new Edit();
        pagesEdit->Create(eargs);
        hbox->AddChild(pagesEdit, 1);

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 4: Extract Text + Cancel buttons (right-aligned), each sized to its label
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        extractBtn = new Button();
        extractBtn->isDefault = true;
        extractBtn->onClick = MkMethod0<PdfExtractTextDialog, &PdfExtractTextDialog::DoExtract>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = _TRA("Extract Text");
        bargs.isRtl = isRtl;
        extractBtn->Create(bargs);
        hbox->AddChild(extractBtn);

        cancelBtn = new Button();
        cancelBtn->onClick = MkMethod0<PdfExtractTextDialog, &PdfExtractTextDialog::OnCancel>(this);
        Button::CreateArgs cargs2;
        cargs2.parent = hwnd;
        cargs2.font = hFont;
        cargs2.text = _TRA("Cancel");
        cargs2.isRtl = isRtl;
        cancelBtn->Create(cargs2);
        hbox->AddChild(new Padding(cancelBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // align the source path label's text with the destination edit's text,
    // which is inset by the edit's border + internal left margin
    pathLabel->insets.left = destEdit->GetLeftTextMargin();
    mainLayout = new Padding(vbox, DpiScaledInsets(hwnd, 10));

    int minClientW = DpiScale(hwnd, 480);
    int clientW = CalcDlgWidth(hwnd, hFont, srcPath, minClientW, DpiScale(hwnd, 10));
    Size size = mainLayout->Layout(ExpandHeight(clientW));
    Rect bounds{0, 0, size.dx, size.dy};
    mainLayout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);

    CenterDialog(hwnd, w->hwndFrame);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    SetIsVisible(true);
    HwndSetFocus(destEdit->hwnd);
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

    auto dlg = new PdfExtractTextDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Compress PDF dialog ---

struct PdfCompressDialog : Wnd {
    HFONT hFont = nullptr;
    Str srcPath;
    MainWindow* win = nullptr;

    ILayout* mainLayout = nullptr;
    Static* pathLabel = nullptr;
    Edit* destEdit = nullptr;
    Button* browseBtn = nullptr;
    Button* compressBtn = nullptr;
    Button* cancelBtn = nullptr;

    ~PdfCompressDialog() override;

    bool Create(MainWindow* win, WindowTab* tab);
    void OnBrowse();
    void DoCompress();
    void OnCancel();
};

PdfCompressDialog::~PdfCompressDialog() {
    str::FreePtr(&srcPath);
    delete mainLayout;
}

void PdfCompressDialog::OnCancel() {
    Close();
}

void PdfCompressDialog::OnBrowse() {
    BrowseForDest(hwnd, destEdit, L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
}

void PdfCompressDialog::DoCompress() {
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

static void PdfCompressOnClose(Wnd::CloseEvent* ev) {
    auto dlg = (PdfCompressDialog*)ev->e->self;
    delete dlg;
}

bool PdfCompressDialog::Create(MainWindow* w, WindowTab* tab) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    hFont = GetDefaultGuiFont();
    onClose = MkFunc1Void(PdfCompressOnClose);

    CreateCustomArgs cargs;
    cargs.title = _TRA("Compress PDF");
    cargs.font = hFont;
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    if (UseDarkModeLib() && DarkMode::isEnabled()) {
        cargs.bgColor = ThemeWindowControlBackgroundColor();
    } else {
        cargs.bgColor = MkGray(0xee);
    }
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    bool isRtl = IsUIRtl();
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // row 1: source path label
    pathLabel = CreatePathLabel(hwnd, hFont, srcPath, isRtl);
    vbox->AddChild(pathLabel);

    // row 2: dest edit (flex) + browse button
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.font = hFont;
        args.text = MakeUniqueFilePathTemp(srcPath);
        args.isRtl = isRtl;
        destEdit = new Edit();
        destEdit->Create(args);
        hbox->AddChild(destEdit, 1);

        browseBtn = new Button();
        browseBtn->onClick = MkMethod0<PdfCompressDialog, &PdfCompressDialog::OnBrowse>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = "...";
        bargs.isRtl = isRtl;
        browseBtn->Create(bargs);
        hbox->AddChild(new Padding(browseBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 3: Compress + Cancel buttons (right-aligned), each sized to its label
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        compressBtn = new Button();
        compressBtn->isDefault = true;
        compressBtn->onClick = MkMethod0<PdfCompressDialog, &PdfCompressDialog::DoCompress>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = _TRA("Compress PDF");
        bargs.isRtl = isRtl;
        compressBtn->Create(bargs);
        hbox->AddChild(compressBtn);

        cancelBtn = new Button();
        cancelBtn->onClick = MkMethod0<PdfCompressDialog, &PdfCompressDialog::OnCancel>(this);
        Button::CreateArgs cargs2;
        cargs2.parent = hwnd;
        cargs2.font = hFont;
        cargs2.text = _TRA("Cancel");
        cargs2.isRtl = isRtl;
        cancelBtn->Create(cargs2);
        hbox->AddChild(new Padding(cancelBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // align the source path label's text with the destination edit's text,
    // which is inset by the edit's border + internal left margin
    pathLabel->insets.left = destEdit->GetLeftTextMargin();
    mainLayout = new Padding(vbox, DpiScaledInsets(hwnd, 10));

    int minClientW = DpiScale(hwnd, 480);
    int clientW = CalcDlgWidth(hwnd, hFont, srcPath, minClientW, DpiScale(hwnd, 10));
    Size size = mainLayout->Layout(ExpandHeight(clientW));
    Rect bounds{0, 0, size.dx, size.dy};
    mainLayout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);

    CenterDialog(hwnd, w->hwndFrame);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    SetIsVisible(true);
    HwndSetFocus(destEdit->hwnd);
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
    if (!CouldBePDFDoc(tab)) {
        return;
    }
    logf("ShowPdfCompressDialog: opening for '%s'\n", tab->filePath);

    auto dlg = new PdfCompressDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Decompress PDF dialog ---

struct PdfDecompressDialog : Wnd {
    HFONT hFont = nullptr;
    Str srcPath;
    MainWindow* win = nullptr;

    ILayout* mainLayout = nullptr;
    Static* pathLabel = nullptr;
    Edit* destEdit = nullptr;
    Button* browseBtn = nullptr;
    Button* decompressBtn = nullptr;
    Button* cancelBtn = nullptr;

    ~PdfDecompressDialog() override;

    bool Create(MainWindow* win, WindowTab* tab);
    void OnBrowse();
    void DoDecompress();
    void OnCancel();
};

PdfDecompressDialog::~PdfDecompressDialog() {
    str::FreePtr(&srcPath);
    delete mainLayout;
}

void PdfDecompressDialog::OnCancel() {
    Close();
}

void PdfDecompressDialog::OnBrowse() {
    BrowseForDest(hwnd, destEdit, L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
}

void PdfDecompressDialog::DoDecompress() {
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

static void PdfDecompressOnClose(Wnd::CloseEvent* ev) {
    auto dlg = (PdfDecompressDialog*)ev->e->self;
    delete dlg;
}

bool PdfDecompressDialog::Create(MainWindow* w, WindowTab* tab) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    hFont = GetDefaultGuiFont();
    onClose = MkFunc1Void(PdfDecompressOnClose);

    CreateCustomArgs cargs;
    cargs.title = _TRA("Decompress PDF");
    cargs.font = hFont;
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    if (UseDarkModeLib() && DarkMode::isEnabled()) {
        cargs.bgColor = ThemeWindowControlBackgroundColor();
    } else {
        cargs.bgColor = MkGray(0xee);
    }
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    bool isRtl = IsUIRtl();
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // row 1: source path label
    pathLabel = CreatePathLabel(hwnd, hFont, srcPath, isRtl);
    vbox->AddChild(pathLabel);

    // row 2: dest edit (flex) + browse button
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.font = hFont;
        args.text = MakeUniqueFilePathTemp(srcPath);
        args.isRtl = isRtl;
        destEdit = new Edit();
        destEdit->Create(args);
        hbox->AddChild(destEdit, 1);

        browseBtn = new Button();
        browseBtn->onClick = MkMethod0<PdfDecompressDialog, &PdfDecompressDialog::OnBrowse>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = "...";
        bargs.isRtl = isRtl;
        browseBtn->Create(bargs);
        hbox->AddChild(new Padding(browseBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 3: Decompress + Cancel buttons (right-aligned), each sized to its label
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        decompressBtn = new Button();
        decompressBtn->isDefault = true;
        decompressBtn->onClick = MkMethod0<PdfDecompressDialog, &PdfDecompressDialog::DoDecompress>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = _TRA("Decompress PDF");
        bargs.isRtl = isRtl;
        decompressBtn->Create(bargs);
        hbox->AddChild(decompressBtn);

        cancelBtn = new Button();
        cancelBtn->onClick = MkMethod0<PdfDecompressDialog, &PdfDecompressDialog::OnCancel>(this);
        Button::CreateArgs cargs2;
        cargs2.parent = hwnd;
        cargs2.font = hFont;
        cargs2.text = _TRA("Cancel");
        cargs2.isRtl = isRtl;
        cancelBtn->Create(cargs2);
        hbox->AddChild(new Padding(cancelBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // align the source path label's text with the destination edit's text,
    // which is inset by the edit's border + internal left margin
    pathLabel->insets.left = destEdit->GetLeftTextMargin();
    mainLayout = new Padding(vbox, DpiScaledInsets(hwnd, 10));

    int minClientW = DpiScale(hwnd, 480);
    int clientW = CalcDlgWidth(hwnd, hFont, srcPath, minClientW, DpiScale(hwnd, 10));
    Size size = mainLayout->Layout(ExpandHeight(clientW));
    Rect bounds{0, 0, size.dx, size.dy};
    mainLayout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);

    CenterDialog(hwnd, w->hwndFrame);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    SetIsVisible(true);
    HwndSetFocus(destEdit->hwnd);
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
    if (!CouldBePDFDoc(tab)) {
        return;
    }
    logf("ShowPdfDecompressDialog: opening for '%s'\n", tab->filePath);

    auto dlg = new PdfDecompressDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Delete Pages From PDF dialog ---

struct PdfDeletePageDialog : Wnd {
    HFONT hFont = nullptr;
    Str srcPath;
    bool isExtract = false;
    MainWindow* win = nullptr;
    int pageCount = 0;

    ILayout* mainLayout = nullptr;
    Static* pathLabel = nullptr;
    Edit* destEdit = nullptr;
    Button* browseBtn = nullptr;
    Static* pagesLabel = nullptr;
    Edit* pagesEdit = nullptr;
    Static* totalLabel = nullptr;
    Static* syntaxLabel = nullptr;
    Button* actionBtn = nullptr; // "Delete Pages" or "Extract Pages"
    Button* cancelBtn = nullptr;

    ~PdfDeletePageDialog() override;

    bool Create(MainWindow* win, WindowTab* tab, bool isExtract);
    void OnBrowse();
    void DoIt();
    void OnCancel();
    void UpdateButton();
};

PdfDeletePageDialog::~PdfDeletePageDialog() {
    str::FreePtr(&srcPath);
    delete mainLayout;
}

void PdfDeletePageDialog::OnCancel() {
    Close();
}

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
            if (str::EqI(startStr, "N")) {
                start = pageCount;
            } else {
                start = !str::IsNull(str::Parse(startStr, "%d%$", &start)) ? start : -1;
            }
            if (endIsEmpty || str::EqI(endStr, "N")) {
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
            if (str::EqI(part, "N")) {
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
    pagesToDelete.SortTyped([](const int* a, const int* b) -> int { return *a - *b; });
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
    actionBtn->SetIsEnabled(valid);
}

void PdfDeletePageDialog::OnBrowse() {
    BrowseForDest(hwnd, destEdit, L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
}

void PdfDeletePageDialog::DoIt() {
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

static void PdfDeletePageOnClose(Wnd::CloseEvent* ev) {
    auto dlg = (PdfDeletePageDialog*)ev->e->self;
    delete dlg;
}

bool PdfDeletePageDialog::Create(MainWindow* w, WindowTab* tab, bool isExtractArg) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    hFont = GetDefaultGuiFont();
    isExtract = isExtractArg;
    pageCount = w->ctrl ? w->ctrl->PageCount() : 0;
    onClose = MkFunc1Void(PdfDeletePageOnClose);

    CreateCustomArgs cargs;
    cargs.title = isExtract ? _TRA("Extract Pages From PDF") : _TRA("Delete Pages From PDF");
    cargs.font = hFont;
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    if (UseDarkModeLib() && DarkMode::isEnabled()) {
        cargs.bgColor = ThemeWindowControlBackgroundColor();
    } else {
        cargs.bgColor = MkGray(0xee);
    }
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    bool isRtl = IsUIRtl();
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // row 1: source path label
    pathLabel = CreatePathLabel(hwnd, hFont, srcPath, isRtl);
    vbox->AddChild(pathLabel);

    // row 2: dest edit (flex) + browse button
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.font = hFont;
        args.text = MakeUniqueFilePathTemp(srcPath);
        args.isRtl = isRtl;
        destEdit = new Edit();
        destEdit->Create(args);
        hbox->AddChild(destEdit, 1);

        browseBtn = new Button();
        browseBtn->onClick = MkMethod0<PdfDeletePageDialog, &PdfDeletePageDialog::OnBrowse>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = "...";
        bargs.isRtl = isRtl;
        browseBtn->Create(bargs);
        hbox->AddChild(new Padding(browseBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 3: pages label + pages edit (flex) + total pages label
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Static::CreateArgs largs;
        largs.parent = hwnd;
        largs.font = hFont;
        largs.text = isExtract ? _TRA("Pages To Extract:") : _TRA("Pages To Delete:");
        largs.isRtl = isRtl;
        pagesLabel = new Static();
        pagesLabel->Create(largs);
        hbox->AddChild(new Padding(pagesLabel, DpiScaledInsets(hwnd, 0, 8, 0, 0)));

        int currentPage = w->ctrl ? w->ctrl->CurrentPageNo() : 1;
        Edit::CreateArgs eargs;
        eargs.parent = hwnd;
        eargs.withBorder = true;
        eargs.font = hFont;
        eargs.text = fmt("%d", currentPage);
        eargs.isRtl = isRtl;
        pagesEdit = new Edit();
        pagesEdit->Create(eargs);
        hbox->AddChild(pagesEdit, 1);

        Static::CreateArgs targs;
        targs.parent = hwnd;
        targs.font = hFont;
        targs.text = fmt("of %d", pageCount);
        targs.isRtl = isRtl;
        totalLabel = new Static();
        totalLabel->Create(targs);
        hbox->AddChild(new Padding(totalLabel, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 4: syntax hint (left) + action + Cancel buttons (right)
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Static::CreateArgs sargs;
        sargs.parent = hwnd;
        sargs.font = hFont;
        sargs.text = "Syntax: 2,5-7,13-";
        sargs.isRtl = isRtl;
        syntaxLabel = new Static();
        syntaxLabel->Create(sargs);
        hbox->AddChild(syntaxLabel);

        // flexible spacer pushes the buttons to the right
        hbox->AddChild(new Spacer(0, 0), 1);

        actionBtn = new Button();
        actionBtn->isDefault = true;
        actionBtn->onClick = MkMethod0<PdfDeletePageDialog, &PdfDeletePageDialog::DoIt>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = isExtract ? _TRA("Extract Pages") : _TRA("Delete Pages");
        bargs.isRtl = isRtl;
        actionBtn->Create(bargs);
        hbox->AddChild(actionBtn);

        cancelBtn = new Button();
        cancelBtn->onClick = MkMethod0<PdfDeletePageDialog, &PdfDeletePageDialog::OnCancel>(this);
        Button::CreateArgs cargs2;
        cargs2.parent = hwnd;
        cargs2.font = hFont;
        cargs2.text = _TRA("Cancel");
        cargs2.isRtl = isRtl;
        cancelBtn->Create(cargs2);
        hbox->AddChild(new Padding(cancelBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // align the source path label's text with the destination edit's text,
    // which is inset by the edit's border + internal left margin
    pathLabel->insets.left = destEdit->GetLeftTextMargin();
    mainLayout = new Padding(vbox, DpiScaledInsets(hwnd, 10));

    int minClientW = DpiScale(hwnd, 480);
    int clientW = CalcDlgWidth(hwnd, hFont, srcPath, minClientW, DpiScale(hwnd, 10));
    Size size = mainLayout->Layout(ExpandHeight(clientW));
    Rect bounds{0, 0, size.dx, size.dy};
    mainLayout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);

    // attach the change handler only now that actionBtn exists, then set the
    // initial validation state (Edit::Create fires onTextChanged on initial text)
    pagesEdit->onTextChanged = MkMethod0<PdfDeletePageDialog, &PdfDeletePageDialog::UpdateButton>(this);
    UpdateButton();

    CenterDialog(hwnd, w->hwndFrame);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    SetIsVisible(true);
    HwndSetFocus(pagesEdit->hwnd);
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
    if (!CouldBePDFDoc(tab)) {
        return;
    }

    int pageCount = win->ctrl ? win->ctrl->PageCount() : 0;
    if (pageCount < 2) {
        return;
    }
    logf("ShowPdfPageRangeDialog: opening %s dialog for '%s', %d pages\n", Str(isExtract ? "extract" : "delete"),
         tab->filePath, pageCount);

    auto dlg = new PdfDeletePageDialog();
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

struct PdfEncryptDialog : Wnd {
    HFONT hFont = nullptr;
    Str srcPath;
    MainWindow* win = nullptr;

    ILayout* mainLayout = nullptr;
    Static* pathLabel = nullptr;
    Edit* destEdit = nullptr;
    Button* browseBtn = nullptr;
    Static* passwordLabel = nullptr;
    Edit* passwordEdit = nullptr;
    Button* encryptBtn = nullptr;
    Button* cancelBtn = nullptr;

    ~PdfEncryptDialog() override;

    bool Create(MainWindow* win, WindowTab* tab);
    void OnBrowse();
    void DoEncrypt();
    void OnCancel();
    void UpdateButton();
};

PdfEncryptDialog::~PdfEncryptDialog() {
    str::FreePtr(&srcPath);
    delete mainLayout;
}

void PdfEncryptDialog::OnCancel() {
    Close();
}

void PdfEncryptDialog::UpdateButton() {
    TempStr pwd = passwordEdit->GetTextTemp();
    encryptBtn->SetIsEnabled(len(pwd) > 0);
}

void PdfEncryptDialog::OnBrowse() {
    BrowseForDest(hwnd, destEdit, L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
}

void PdfEncryptDialog::DoEncrypt() {
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

static void PdfEncryptOnClose(Wnd::CloseEvent* ev) {
    auto dlg = (PdfEncryptDialog*)ev->e->self;
    delete dlg;
}

bool PdfEncryptDialog::Create(MainWindow* w, WindowTab* tab) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    hFont = GetDefaultGuiFont();
    onClose = MkFunc1Void(PdfEncryptOnClose);

    CreateCustomArgs cargs;
    cargs.title = _TRA("Encrypt PDF");
    cargs.font = hFont;
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    if (UseDarkModeLib() && DarkMode::isEnabled()) {
        cargs.bgColor = ThemeWindowControlBackgroundColor();
    } else {
        cargs.bgColor = MkGray(0xee);
    }
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    bool isRtl = IsUIRtl();
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // row 1: source path label
    pathLabel = CreatePathLabel(hwnd, hFont, srcPath, isRtl);
    vbox->AddChild(pathLabel);

    // row 2: dest edit (flex) + browse button
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.font = hFont;
        args.text = MakeUniqueFilePathTemp(srcPath);
        args.isRtl = isRtl;
        destEdit = new Edit();
        destEdit->Create(args);
        hbox->AddChild(destEdit, 1);

        browseBtn = new Button();
        browseBtn->onClick = MkMethod0<PdfEncryptDialog, &PdfEncryptDialog::OnBrowse>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = "...";
        bargs.isRtl = isRtl;
        browseBtn->Create(bargs);
        hbox->AddChild(new Padding(browseBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 3: "Password:" label + password edit (flex)
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Static::CreateArgs largs;
        largs.parent = hwnd;
        largs.font = hFont;
        largs.text = _TRA("Password:");
        largs.isRtl = isRtl;
        passwordLabel = new Static();
        passwordLabel->Create(largs);
        hbox->AddChild(new Padding(passwordLabel, DpiScaledInsets(hwnd, 0, 4, 0, 0)));

        Edit::CreateArgs eargs;
        eargs.parent = hwnd;
        eargs.withBorder = true;
        eargs.font = hFont;
        eargs.isRtl = isRtl;
        passwordEdit = new Edit();
        passwordEdit->Create(eargs);
        hbox->AddChild(passwordEdit, 1);

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 4: Encrypt PDF + Cancel buttons (right-aligned), each sized to its label
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        encryptBtn = new Button();
        encryptBtn->isDefault = true;
        encryptBtn->onClick = MkMethod0<PdfEncryptDialog, &PdfEncryptDialog::DoEncrypt>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = _TRA("Encrypt PDF");
        bargs.isRtl = isRtl;
        encryptBtn->Create(bargs);
        hbox->AddChild(encryptBtn);

        cancelBtn = new Button();
        cancelBtn->onClick = MkMethod0<PdfEncryptDialog, &PdfEncryptDialog::OnCancel>(this);
        Button::CreateArgs cargs2;
        cargs2.parent = hwnd;
        cargs2.font = hFont;
        cargs2.text = _TRA("Cancel");
        cargs2.isRtl = isRtl;
        cancelBtn->Create(cargs2);
        hbox->AddChild(new Padding(cancelBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // align the source path label's text with the destination edit's text,
    // which is inset by the edit's border + internal left margin
    pathLabel->insets.left = destEdit->GetLeftTextMargin();
    mainLayout = new Padding(vbox, DpiScaledInsets(hwnd, 10));

    int minClientW = DpiScale(hwnd, 480);
    int clientW = CalcDlgWidth(hwnd, hFont, srcPath, minClientW, DpiScale(hwnd, 10));
    Size size = mainLayout->Layout(ExpandHeight(clientW));
    Rect bounds{0, 0, size.dx, size.dy};
    mainLayout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);

    // attach the change handler only now that encryptBtn exists, then disable
    // the button until a password is entered
    passwordEdit->onTextChanged = MkMethod0<PdfEncryptDialog, &PdfEncryptDialog::UpdateButton>(this);
    encryptBtn->SetIsEnabled(false);

    CenterDialog(hwnd, w->hwndFrame);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    SetIsVisible(true);
    HwndSetFocus(passwordEdit->hwnd);
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
    if (!CouldBePDFDoc(tab)) {
        return;
    }
    EngineBase* engine = tab->GetEngine();
    if (EngineMupdfIsEncrypted(engine)) {
        logf("ShowPdfEncryptDialog: '%s' is already encrypted, skipping\n", tab->filePath);
        return;
    }
    logf("ShowPdfEncryptDialog: opening for '%s'\n", tab->filePath);

    auto dlg = new PdfEncryptDialog();
    if (!dlg->Create(win, tab)) {
        delete dlg;
    }
}

// --- Decrypt PDF dialog ---

struct PdfDecryptDialog : Wnd {
    HFONT hFont = nullptr;
    Str srcPath;
    Str password;
    MainWindow* win = nullptr;

    ILayout* mainLayout = nullptr;
    Static* pathLabel = nullptr;
    Edit* destEdit = nullptr;
    Button* browseBtn = nullptr;
    Button* decryptBtn = nullptr;
    Button* cancelBtn = nullptr;

    ~PdfDecryptDialog() override;

    bool Create(MainWindow* win, WindowTab* tab, Str pwd);
    void OnBrowse();
    void DoDecrypt();
    void OnCancel();
};

PdfDecryptDialog::~PdfDecryptDialog() {
    str::FreePtr(&srcPath);
    str::FreePtr(&password);
    delete mainLayout;
}

void PdfDecryptDialog::OnCancel() {
    Close();
}

void PdfDecryptDialog::OnBrowse() {
    BrowseForDest(hwnd, destEdit, L"PDF Files\0*.pdf\0All Files\0*.*\0", L"pdf");
}

void PdfDecryptDialog::DoDecrypt() {
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

static void PdfDecryptOnClose(Wnd::CloseEvent* ev) {
    auto dlg = (PdfDecryptDialog*)ev->e->self;
    delete dlg;
}

bool PdfDecryptDialog::Create(MainWindow* w, WindowTab* tab, Str pwd) {
    win = w;
    srcPath = str::Dup(tab->filePath);
    password = str::Dup(pwd);
    hFont = GetDefaultGuiFont();
    onClose = MkFunc1Void(PdfDecryptOnClose);

    CreateCustomArgs cargs;
    cargs.title = _TRA("Decrypt PDF");
    cargs.font = hFont;
    cargs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    cargs.visible = false;
    cargs.icon = GetAppIcon();
    if (UseDarkModeLib() && DarkMode::isEnabled()) {
        cargs.bgColor = ThemeWindowControlBackgroundColor();
    } else {
        cargs.bgColor = MkGray(0xee);
    }
    CreateCustom(cargs);
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)w->hwndFrame);

    bool isRtl = IsUIRtl();
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    // row 1: source path label
    pathLabel = CreatePathLabel(hwnd, hFont, srcPath, isRtl);
    vbox->AddChild(pathLabel);

    // row 2: dest edit (flex) + browse button
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainStart;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.font = hFont;
        args.text = MakeUniqueFilePathTemp(srcPath);
        args.isRtl = isRtl;
        destEdit = new Edit();
        destEdit->Create(args);
        hbox->AddChild(destEdit, 1);

        browseBtn = new Button();
        browseBtn->onClick = MkMethod0<PdfDecryptDialog, &PdfDecryptDialog::OnBrowse>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = "...";
        bargs.isRtl = isRtl;
        browseBtn->Create(bargs);
        hbox->AddChild(new Padding(browseBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // row 3: Decrypt + Cancel buttons (right-aligned), each sized to its label
    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainEnd;
        hbox->alignCross = CrossAxisAlign::CrossCenter;

        decryptBtn = new Button();
        decryptBtn->isDefault = true;
        decryptBtn->onClick = MkMethod0<PdfDecryptDialog, &PdfDecryptDialog::DoDecrypt>(this);
        Button::CreateArgs bargs;
        bargs.parent = hwnd;
        bargs.font = hFont;
        bargs.text = _TRA("Decrypt PDF");
        bargs.isRtl = isRtl;
        decryptBtn->Create(bargs);
        hbox->AddChild(decryptBtn);

        cancelBtn = new Button();
        cancelBtn->onClick = MkMethod0<PdfDecryptDialog, &PdfDecryptDialog::OnCancel>(this);
        Button::CreateArgs cargs2;
        cargs2.parent = hwnd;
        cargs2.font = hFont;
        cargs2.text = _TRA("Cancel");
        cargs2.isRtl = isRtl;
        cancelBtn->Create(cargs2);
        hbox->AddChild(new Padding(cancelBtn, DpiScaledInsets(hwnd, 0, 0, 0, 4)));

        vbox->AddChild(new Padding(hbox, DpiScaledInsets(hwnd, 6, 0, 0, 0)));
    }

    // align the source path label's text with the destination edit's text,
    // which is inset by the edit's border + internal left margin
    pathLabel->insets.left = destEdit->GetLeftTextMargin();
    mainLayout = new Padding(vbox, DpiScaledInsets(hwnd, 10));

    int minClientW = DpiScale(hwnd, 480);
    int clientW = CalcDlgWidth(hwnd, hFont, srcPath, minClientW, DpiScale(hwnd, 10));
    Size size = mainLayout->Layout(ExpandHeight(clientW));
    Rect bounds{0, 0, size.dx, size.dy};
    mainLayout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);

    CenterDialog(hwnd, w->hwndFrame);
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    SetIsVisible(true);
    HwndSetFocus(destEdit->hwnd);
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
    if (!CouldBePDFDoc(tab)) {
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

    auto dlg = new PdfDecryptDialog();
    if (!dlg->Create(win, tab, pwd)) {
        delete dlg;
    }
}
