/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Interactive editing for hybrid AcroForm+XFA forms (XFA overlay fields).

#include "XfaTypes.h"

struct MainWindow;
class EngineBase;
enum class WidgetCursorKind;

bool StartXfaFieldEdit(MainWindow* win, const XfaFieldHit& field);
void CommitXfaFieldEdit(bool save);
bool IsXfaFieldEditActive();
bool ToggleXfaFieldButton(EngineBase* engine, const XfaFieldHit& field);
WidgetCursorKind GetXfaFieldCursorKind(const XfaFieldHit& field);