/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct DisplayModel;
struct AnnotCreateArgs;
enum class AnnotationType;

constexpr WORD kAnnotationPlacementCommandCode = 0x5341;

bool CommandUsesPlacementMode(int cmdId);
AnnotPlacementKind PlacementKindFromCommand(int cmdId);

bool IsPlacingAnnotation(MainWindow*);
bool IsPlacingPointAnnotation(MainWindow*);
bool IsPlacingLineAnnotation(MainWindow*);
bool IsPlacingPolyLineAnnotation(MainWindow*);
bool IsPlacingShapeAnnotation(MainWindow*);
bool IsPlacingInkAnnotation(MainWindow*);
Point SnapLineEndpoint(Point start, Point end);

void StartAnnotationPlacement(MainWindow*, int cmdId);
bool CancelAnnotationPlacement(MainWindow*);
bool FinishAnnotationPlacement(MainWindow*);
bool FinishPolyLineAnnotationPlacement(MainWindow*);
bool FinishInkAnnotationPlacement(MainWindow*);
bool CloseAnnotationPlacementHint(MainWindow*);

bool AnnotationPlacementOnLeftDown(MainWindow*, Point, WPARAM);
bool AnnotationPlacementOnLeftUp(MainWindow*, Point, WPARAM);
bool AnnotationPlacementOnLeftDblClk(MainWindow*, Point);
bool AnnotationPlacementOnRightDown(MainWindow*);
bool AnnotationPlacementOnMouseMove(MainWindow*, Point, WPARAM);
bool AnnotationPlacementOnSetCursor(MainWindow*);
bool AnnotationPlacementOnKeyDown(MainWindow*, WPARAM);
bool AnnotationPlacementEraseAt(MainWindow*, Point);

void PaintAnnotationPlacement(MainWindow*, HDC, DisplayModel*);
bool AnnotationPlacementFillCreate(MainWindow*, AnnotationType, Point&, int&, PointF&, PointF&, AnnotCreateArgs&);

void DeleteAnnotationPlacementCursors();
TempStr AnnotationPlacementStateTemp(MainWindow*);
