/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace gpui {
struct PaintCtx;
} // namespace gpui

struct MainWindow;

void ToggleMeasurement(MainWindow*);
void CloseMeasurement(MainWindow*);
bool MeasurementCancel(MainWindow*);
bool MeasurementOnClick(MainWindow*, Point, bool ctrl);
bool MeasurementOnMove(MainWindow*, Point);
bool MeasurementOnSetCursor(MainWindow*);
void PaintMeasurement(MainWindow*, gpui::PaintCtx*);
TempStr TestMeasurement(MainWindow*, Str action, int pageNo, PointF, Str length, Str unit);
