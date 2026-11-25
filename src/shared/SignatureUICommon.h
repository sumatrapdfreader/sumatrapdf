/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SignatureUICommon.cpp and each app's SignatureUI.cpp ---

struct MainWindow;
struct DisplayModel;
struct WindowTab;
struct PdfSigResult;
struct PdfSigSummary;
struct PdfSigEntry;
enum class SigVerdict;

constexpr int kSigBarDy = 32;
constexpr int kSigBarIconDx = 18;
constexpr int kSigBarPadDx = 10;
constexpr int kSigRowDy = 46;
constexpr int kSigRowIconDx = 18;
constexpr int kSigRowPadDx = 8;
constexpr int kSigMarkDx = 20;
constexpr int kSigDialogDx = 620;
constexpr int kSigDialogDy = 460;
constexpr int kSigStatusFontPx = 16;

PdfSigResult* TabSignatures(WindowTab*);
PdfSigResult* CurrentSignatures(MainWindow*);
bool TabHasSignatures(WindowTab*);
DisplayModel* SignatureMarkModel(MainWindow*);
Rect SignatureScreenRect(DisplayModel*, const PdfSigEntry*);
int SignatureAtPoint(MainWindow*, Point);
bool SigMarksOnLeftDown(MainWindow*, int x, int y);
SigVerdict OverallSigVerdict(const PdfSigSummary&);
Str OverallSigMessage(const PdfSigSummary&);
const char* SigVerdictIcon(SigVerdict);
Color SigVerdictColor(SigVerdict);
void SigBarColors(SigVerdict, Color* bg, Color* txt);
bool SigBarWanted(MainWindow*);
void GoToSignature(MainWindow*, const PdfSigEntry*);
TempStr SignatureDialogTextTemp(const PdfSigEntry*, Str nl);
void CheckTabSignatures(WindowTab*);

// implemented by each app
void OnTabSignaturesChecked(MainWindow*);
void ShowSignatureDialog(MainWindow*, int entryIdx);
