/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The Layers sidebar view: a PDF's layers (optional content groups), to turn
// on and off.

struct LayersCtrl;
struct WindowTab;
struct MainWindow;
struct VirtCtrl;

bool TabHasLayers(WindowTab*);
void CreateLayersCtrl(MainWindow*);
VirtCtrl* LayersView(MainWindow*);
void DeleteLayersCtrl(MainWindow*);
void UpdateLayersCtrl(MainWindow*);
void LayersCtrlColorsAndDpi(MainWindow*, int dpi);
bool ToggleLayerRow(MainWindow*, int row);
TempStr LayersStateTemp(MainWindow*, Str action);
