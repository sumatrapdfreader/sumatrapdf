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
