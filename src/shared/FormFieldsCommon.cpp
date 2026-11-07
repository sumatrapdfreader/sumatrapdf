/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"

#include <mupdf/pdf.h>

#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Annotation.h"
#include "SumatraPDF.h"
#include "Commands.h"
#include "Toolbar.h"
#include "SumatraDialogs.h"
#include "FormFields.h"
#include "AppHelpersCommon.h"

// Start editing a text form field in place (floats an edit box over the field).
// Returns false if `widget` isn't an editable (non-read-only) text widget.
// Clicking a signature field the document's author left unsigned opens Sign
// Document with that field selected. Signed fields are left alone (clicking one
// shouldn't offer to overwrite it), and so is everything else (issue #5964).
bool StartSignatureFieldSigning(MainWindow* win, Annotation* widget) {
    if (!win || !AnnotationIsLive(widget)) {
        return false;
    }
    if (GetWidgetType(widget) != PDF_WIDGET_TYPE_SIGNATURE) {
        return false;
    }
    if (GetWidgetFieldFlags(widget) & PDF_FIELD_IS_READ_ONLY) {
        return false;
    }
    // signing rewrites the PDF, so it needs the same engine support annotations
    // do - and the same gate that decides whether the Sign Document command is
    // shown at all, so a click can't reach a dialog the menu is hiding
    DisplayModel* dm = win->AsFixed();
    if (!dm || !EngineSupportsAnnotations(dm->GetEngine()) || win->IsFullScreen()) {
        return false;
    }
    TempStr fieldName;
    if (!IsUnsignedSignatureWidget(widget, &fieldName)) {
        return false;
    }
    CommitFormFieldEdit(true); // don't leave an in-place edit hanging
    ShowSignDocumentDialog(win, fieldName, true);
    return true;
}
