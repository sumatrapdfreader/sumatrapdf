/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct PaintCtx;
}

struct MainWindow;
struct DisplayModel;
struct AnnotCreateArgs;
enum class AnnotationType;

bool CommandUsesPlacementMode(int cmdId);
AnnotPlacementKind PlacementKindFromCommand(int cmdId);

bool IsPlacingAnnotation(MainWindow*);
bool IsPlacingPointAnnotation(MainWindow*);
bool IsPlacingLineAnnotation(MainWindow*);
bool IsPlacingPolyLineAnnotation(MainWindow*);
bool IsPlacingShapeAnnotation(MainWindow*);
bool IsPlacingInkAnnotation(MainWindow*);
bool IsPlacingHighlighterAnnotation(MainWindow*);
Point SnapLineEndpoint(Point start, Point end);

void StartAnnotationPlacement(MainWindow*, int cmdId);
bool CancelAnnotationPlacement(MainWindow*);
bool FinishAnnotationPlacement(MainWindow*);
bool FinishPolyLineAnnotationPlacement(MainWindow*);
bool FinishInkAnnotationPlacement(MainWindow*);

bool AnnotationPlacementOnLeftDown(MainWindow*, Point, bool isShift, bool isCtrl);
bool AnnotationPlacementOnLeftUp(MainWindow*, Point, bool isShift);
bool AnnotationPlacementOnLeftDblClk(MainWindow*, Point);
bool AnnotationPlacementOnRightDown(MainWindow*);
bool AnnotationPlacementOnMouseMove(MainWindow*, Point, bool isShift, bool lButtonDown);
bool AnnotationPlacementOnSetCursor(MainWindow*);
bool AnnotationPlacementOnKeyDown(MainWindow*, int vkey);
bool AnnotationPlacementEraseAt(MainWindow*, Point);
void AnnotationPlacementOnSelectionStop(MainWindow*);

void PaintAnnotationPlacement(MainWindow*, gpui::PaintCtx*, DisplayModel*);
bool AnnotationPlacementFillCreate(MainWindow*, AnnotationType, Point&, int&, PointF&, PointF&, AnnotCreateArgs&);
SizeF FreeTextPlacementPageSize(const AnnotCreateArgs&);

TempStr AnnotationPlacementStateTemp(MainWindow*);
