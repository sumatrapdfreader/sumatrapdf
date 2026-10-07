/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;

void ShowPdfBakeDialog(MainWindow* win);
void ShowPdfExtractTextDialog(MainWindow* win);
void ShowPdfCompressDialog(MainWindow* win);
void ShowPdfDecompressDialog(MainWindow* win);
void ShowPdfDeletePageDialog(MainWindow* win);
void ShowPdfExtractPagesDialog(MainWindow* win);
void ShowMergePdfDialog(MainWindow* win);
TempStr MergePdfResultTemp(Str action, Str arg, int n, int* exitCodeOut);
void ShowPdfEncryptDialog(MainWindow* win);
void ShowPdfDecryptDialog(MainWindow* win);
// comic books / image folders / single images → multi-page PDF (issue #4118)
void ShowConvertToPdfDialog(MainWindow* win);
TempStr ConvertImageCollectionToPdfResultTemp(Str srcPath, Str destPath, int* exitCodeOut);
TempStr ExtractPdfPagesResultTemp(Str destPath, Str pagesSpec, int annotsOnly, int* exitCodeOut);
// PDF pages → PNG / JPEG / BMP files (issue #5991)
void ShowConvertPdfToImagesDialog(MainWindow* win);
TempStr ConvertPagesToImagesResultTemp(Str templatePath, Str pagesSpec, int dpi, int* exitCodeOut);
void ShowSaveSelectionAsImageDialog(MainWindow* win);
TempStr SaveSelectionAsImageResultTemp(Str destPath, int dpi, int pageNo, int x, int y, int dx, int dy,
                                       int* exitCodeOut);

// ng: orig's tool dialogs are windows of their own; here each one is a gpui
// dialog the shell asks for once a frame, so they share one Show / Close /
// IsVisible / Build quadruple (only one can be up at a time)
void ClosePdfToolDialog();
bool IsPdfToolDialogVisible();
bool PdfToolDialogOnEnter();
// the Merge PDF dialog's keys (arrows, Delete, Ctrl + A) and dropped PDFs
// the Apps key in the Merge PDF grid: its context menu at the cursor
bool PdfToolDialogContextMenuFromKey(MainWindow* win, gpui::Ctx* cx);
// the delete / extract pages dialogs
bool PdfToolDialogIsPagesKind();
// Esc during a Merge PDF drag: ends the drag, the dialog stays
bool PdfToolDialogEndDrag();
bool PdfToolDialogOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift);
// which window a drag is over: the frame, or a tool window of its own
enum class DropHost {
    Frame,
    ToolWindow,
};
// orig's MergeDropTarget::DragOver / DragLeave: files are being dragged over
// the window (`pt` in dips of that window; null when the drag left or ended).
// False when Merge PDF isn't up in that window; `accept` says whether a drop
// at `pt` would be taken, and the grid shows the bar where the pages would go.
bool PdfToolDialogOnDragOver(MainWindow* win, const PointF* pt, bool hasPdf, bool* accept,
                             DropHost host = DropHost::Frame);
// Merge PDF is up as a dialog in the frame
bool PdfToolDialogIsMerge(MainWindow* win);
bool PdfToolDialogOnDropFiles(MainWindow* win, const Str* paths, int n, const PointF* pt,
                              DropHost host = DropHost::Frame);
void PdfToolDialogTick(MainWindow* win, int ms);
gpui::El* PdfToolDialogBuild(MainWindow* win, gpui::Ctx* cx);
// the open dialog's fields as one line, for the log and the tests
TempStr PdfToolStateTemp();
