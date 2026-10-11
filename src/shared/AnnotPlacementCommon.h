/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by AnnotPlacementCommon.cpp and each app's AnnotPlacement.cpp ---

extern Kind kNotifPointAnnotationPlacement;
extern Kind kNotifLineAnnotationPlacement;
extern Kind kNotifPolyLineAnnotationPlacement;
extern Kind kNotifShapeAnnotationPlacement;
extern Kind kNotifInkAnnotationPlacement;
extern Kind kNotifHighlighterPlacement;
void FreeTextPlacementArgs(int cmdId, AnnotCreateArgs& args);
int FreeTextFontSize(const AnnotCreateArgs& args);
float FreeTextPadding(const AnnotCreateArgs& args);
Str FreeTextPlacementContent(const AnnotCreateArgs& args);
AnnotPlacementKind KindOf(MainWindow* win);
Kind NotifGroupForKind(AnnotPlacementKind kind);
int OrigCommandId(int cmdId);
bool IsPointPlacementKind(AnnotPlacementKind kind);
bool HasPreview(AnnotPlacementKind kind);
Str PlacementNotification(AnnotPlacementKind kind, bool circle, int cmdId);
void EndCurrentPlacement(MainWindow* win);
Point ShapePlacementEnd(const AnnotPlacement& p, DisplayModel* dm);
Rect ShapePlacementScreenRect(const AnnotPlacement& p, DisplayModel* dm);
float PxPerPagePt(DisplayModel* dm, int pageNo);
Rect PlacementPreviewScreenRect(DisplayModel* dm, int pageNo, Point pt, PointF pagePt, RectF pageRect);
Rect FreeTextPlacementScreenRect(MainWindow* win, DisplayModel* dm);

bool CommitShapePlacement(MainWindow* win);
bool PlacePointAnnotationAt(MainWindow* win, Point pt);

// implemented by each app
void CommitPlacementCommand(MainWindow* win, Point pt);
void SetPlacementCursor(MainWindow* win);

// Free text is placed like a stamp: a preview box the size of the annotation
// follows the cursor and a click creates it there. MuPDF lays free text out
// with padding = 2 * border width, a 1.2 * font size line height and a
// 0.8 * font size baseline (pdf_write_free_text_appearance), so a box that
// fits one line of the placeholder text is that tall.
constexpr float kFreeTextLineHeight = 1.2f;

// MuPDF's default stamp is {12,12,12+190,12+50}; caret is {12,12,12+18,12+15}
// with the caret mark at the middle of the left edge; file attachment is
// {12,12,12+16,12+16}.
constexpr float kStampAnnotDefaultDx = 190.f;

bool AppendInkPoint(MainWindow* win, DisplayModel* dm, Point pt);

// implemented by each app: what placement asks of the window system
void PlacementFocusFrame(MainWindow* win);
void PlacementCaptureMouse(MainWindow* win);
void PlacementReleaseMouse(MainWindow* win);
void ReleasePlacementCapture(MainWindow* win);
bool PlacementDumpCursor(MainWindow* win, AnnotPlacementKind kind, bool active);
void RestoreCanvasCursor(MainWindow* win);
void PlacementRunCreateCmd(MainWindow* win, int cmdId, Point pt);
// orig's Canvas.cpp, ng's gui/DocCanvas.cpp
bool IsDragDistance(int x1, int x2, int y1, int y2);

TempStr PointPlacementDumpLineTemp(MainWindow* win, AnnotPlacementKind kind, Str key, bool svgCursor);

constexpr float kStampAnnotDefaultDy = 50.f;

constexpr float kCaretAnnotDefaultDx = 18.f;

constexpr float kCaretAnnotDefaultDy = 15.f;

constexpr float kFileAttachmentAnnotDefaultDx = 16.f;

constexpr float kFileAttachmentAnnotDefaultDy = 16.f;

// 40% yellow, when Annotations.InkColor is not a color. How translucent a
// stroke is comes from its color's alpha.
constexpr Color kInkDefaultColor = 0x6600ffff;

// MeasureString already includes generous side bearings; a little more keeps
// MuPDF from wrapping the text we previewed on a single line
constexpr float kFreeTextWidthSlack = 1.02f;
