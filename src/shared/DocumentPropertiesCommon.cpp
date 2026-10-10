/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "gui/Dpi.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "gui/PlatformFont.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "AppTools.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "Print.h"
#include "Theme.h"
#include "EutlTrust.h"
#include "DocumentProperties.h"
#include "DocumentPropertiesCommon.h"

// One "W x H unit" fragment for FormatPageSizeTemp (size.dx/dy are inches).
static TempStr FormatPageSizeUnitTemp(SizeF sizeInches, double unitsPerInch, Str unit) {
    double width = sizeInches.dx * unitsPerInch;
    double height = sizeInches.dy * unitsPerInch;
    if (((int)(width * 100)) % 100 == 99) {
        width += 0.01;
    }
    if (((int)(height * 100)) % 100 == 99) {
        height += 0.01;
    }
    TempStr strWidth = str::FormatFloatWithThousandSepTemp(width);
    TempStr strHeight = str::FormatFloatWithThousandSepTemp(height);
    return fmt("%sx%s %s", strWidth, strHeight, unit);
}

// Format page size in cm/mm/in (locale unit first) and points (issue #2186).
// Metric: "21.0 x 29.7 cm, 210 x 297 mm, 8.27 x 11.69 in, 595 x 842 pt (A4)"
// US:     "8.27 x 11.69 in, 21.0 x 29.7 cm, 210 x 297 mm, 595 x 842 pt (A4)"
TempStr FormatPageSizeTemp(EngineBase* engine, int pageNo, int rotation) {
    RectF mediabox = engine->PageMediabox(pageNo);
    float fileDpi = engine->fileDPI;
    float zoom = 1.0f / fileDpi;
    SizeF size = engine->Transform(mediabox, pageNo, zoom, rotation).Size();

    Str formatName;
    switch (GetPaperFormatFromSizeApprox(size)) {
        case PaperFormat::A2:
            formatName = StrL(" (A2)");
            break;
        case PaperFormat::A3:
            formatName = StrL(" (A3)");
            break;
        case PaperFormat::A4:
            formatName = StrL(" (A4)");
            break;
        case PaperFormat::A5:
            formatName = StrL(" (A5)");
            break;
        case PaperFormat::A6:
            formatName = StrL(" (A6)");
            break;
        case PaperFormat::Letter:
            formatName = StrL(" (Letter)");
            break;
        case PaperFormat::Legal:
            formatName = StrL(" (Legal)");
            break;
        case PaperFormat::Tabloid:
            formatName = StrL(" (Tabloid)");
            break;
        case PaperFormat::Statement:
            formatName = StrL(" (Statement)");
            break;
        case PaperFormat::Other:
            formatName = StrL(" (Other)");
            break;
    }

    TempStr inStr = FormatPageSizeUnitTemp(size, 1.0, StrL("in"));
    TempStr cmStr = FormatPageSizeUnitTemp(size, 2.54, StrL("cm"));
    TempStr mmStr = FormatPageSizeUnitTemp(size, 25.4, StrL("mm"));

    int ptW = (int)lroundf(size.dx * 72.0f);
    int ptH = (int)lroundf(size.dy * 72.0f);
    TempStr ptStr = fmt("%dx%d pt", ptW, ptH);

    // Locale unit first, then the other two, then points (issue #2186)
    bool isMetric = GetMeasurementSystem() == 0;
    if (isMetric) {
        return fmt("%s, %s, %s, %s%s", cmStr, mmStr, inStr, ptStr, formatName);
    }
    return fmt("%s, %s, %s, %s%s", inStr, cmStr, mmStr, ptStr, formatName);
}

// returns a list of permissions denied by this document
TempStr FormatPermissionsTemp(DocController* ctrl) {
    if (!ctrl->AsFixed()) {
        return {};
    }

    StrVec denials;

    EngineBase* engine = ctrl->AsFixed()->GetEngine();
    if (!engine->AllowsPrinting()) {
        denials.Append(Tr("printing document"));
    }
    if (!engine->allowsCopyingText) {
        denials.Append(Tr("copying text"));
    }

    return JoinTemp(&denials, StrL(", "));
}

void AppendProp(str::Builder& out, Str key, Str value) {
    if (len(value) == 0) {
        return;
    }
    out.Append(fmt("%s %s\n", key, value));
}

void AppendPdfFileStructure(str::Builder& out, Str fstruct, Str filePath) {
    if (len(fstruct) == 0) {
        bool isPDF = str::EndsWithI(filePath, StrL(".pdf"));
        if (isPDF) {
            AppendProp(out, str::JoinTemp(Tr("Fast Web View"), StrL(":")), Tr("No"));
        }
        return;
    }
    StrVec parts;
    Split(&parts, fstruct, StrL(","), true);

    StrVec props;

    Str linearized = Tr("No");
    if (parts.Contains(StrL("linearized"))) {
        linearized = Tr("Yes");
    }
    AppendProp(out, str::JoinTemp(Tr("Fast Web View"), StrL(":")), linearized);

    if (parts.Contains(StrL("tagged"))) {
        props.Append(Tr("Tagged PDF"));
    }
    if (parts.Contains(StrL("PDFX"))) {
        props.Append(StrL("PDF/X (ISO 15930)"));
    }
    if (parts.Contains(StrL("PDFA1"))) {
        props.Append(StrL("PDF/A (ISO 19005)"));
    }
    if (parts.Contains(StrL("PDFE1"))) {
        props.Append(StrL("PDF/E (ISO 24517)"));
    }

    TempStr val = JoinTemp(&props, StrL(", "));
    AppendProp(out, Tr("PDF Optimizations:"), val);
}

void GetAllProps(DocController* ctrl, Props& propsOut) {
    DisplayModel* dm = ctrl->AsFixed();
    if (dm) {
        EngineBase* engine = dm->GetEngine();
        engine->GetProperties(propsOut);
        return;
    }
    for (DocProp prop : kCommonDocProps) {
        TempStr val = ctrl->GetPropertyTemp(prop);
        if (val) {
            AddProp(propsOut, prop, val);
        }
    }
}

// Types whose name is more than an upper-cased extension. Everything else
// reads fine as-is (".pdf" -> "PDF"), so only the exceptions live here.
static Str FileTypeDisplayName(FileType ft) {
    if (ft == FileType::Jxl) {
        return StrL("JPEG-XL");
    }
    return {};
}

// The file type as sniffed from the content, which is what the app went by
// when it picked an engine - the extension can say something else entirely.
// Only for a plain file on disk: a directory document has no content of its
// own, an embedded PDF stream isn't a file, and in plugin mode the "path" is
// a URL we can't read.
void AppendFileType(str::Builder& out, Str path) {
    if (gPluginMode || len(path) == 0) {
        return;
    }
    if (path::IsDirectory(path) || !file::Exists(path)) {
        return;
    }
    FileType ft = GuessFileTypeFromFile(path);
    if (ft == FileType::Unknown) {
        return;
    }
    Str name = FileTypeDisplayName(ft);
    if (len(name) == 0) {
        TempStr ext = GetExtForFileTypeTemp(ft);
        if (len(ext) < 2) {
            return;
        }
        // ".pdf" reads better as "PDF" next to the file name it belongs to
        TempStr fromExt = str::DupTemp(Str(ext.s + 1, ext.len - 1));
        str::ToUpperInPlace(fromExt);
        name = fromExt;
    }
    AppendProp(out, Tr("File Type:"), name);
}

// Which way the pages run, and where that came from. A PDF can state it with
// ViewerPreferences /Direction; an EPUB with page-progression-direction.
// Otherwise it's the manga-mode default or the user's own toggle
// (issues #1264, #2022).
// Not shown for a standalone image (no page sequence) or a one-page document
// that neither declares a direction nor has had manga mode toggled (#5950).
static bool ShouldShowReadingDirection(DisplayModel* dm) {
    if (!dm) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine || engine->kind == kindEngineImage) {
        return false;
    }
    const PageLayout& layout = engine->preferredLayout;
    if (layout.r2lDeclared) {
        return true;
    }
    if (dm->GetDisplayR2L() != layout.r2l) {
        return true;
    }
    return dm->PageCount() > 1;
}

void AppendReadingDirection(str::Builder& out, DisplayModel* dm) {
    if (!ShouldShowReadingDirection(dm)) {
        return;
    }
    bool r2l = dm->GetDisplayR2L();
    Str dir = r2l ? Tr("Right to left") : Tr("Left to right");
    Str src;
    const PageLayout& layout = dm->GetEngine()->preferredLayout;
    if (layout.r2lDeclared && r2l == layout.r2l) {
        src = Tr("from the document");
    } else if (r2l != layout.r2l) {
        src = Tr("changed by you");
    }
    TempStr val = src ? str::JoinTemp(dir, StrL(" ("), src, StrL(")")) : TempStr(dir);
    AppendProp(out, Tr("Reading Direction:"), val);
}

// make text end with exactly one '\n' (if not empty) so that appending
// a section preceded by "\n" yields a single empty line, not more.
// the last property is optional (e.g. "Files:") so text can end with
// a stray blank line
void EndWithSingleNewline(str::Builder& b) {
    while (len(b) > 0 && b.LastChar() == '\n') {
        b.RemoveLast();
    }
    if (len(b) > 0) {
        b.AppendChar('\n');
    }
}

static int GetPropertyLabelWidth(Str line, int* labelBytesOut) {
    for (int i = 0; i + 2 < line.len; i++) {
        if (line.s[i] != ':' || line.s[i + 1] != ' ') {
            continue;
        }
        TempWStr label = ToWStrTemp(Str(line.s, i + 1));
        *labelBytesOut = i + 1;
        return len(label);
    }
    return -1;
}

void AlignPropertiesText(str::Builder& text) {
    int maxLabelWidth = 0;
    Str content = ToStr(text);
    for (int off = 0; off < content.len;) {
        Str rest = Str(content.s + off, content.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        int labelBytes = 0;
        int labelWidth = GetPropertyLabelWidth(Str(rest.s, lineLen), &labelBytes);
        maxLabelWidth = std::max(labelWidth, maxLabelWidth);
        off += lineLen + (nl >= 0 ? 1 : 0);
    }
    if (maxLabelWidth == 0) {
        return;
    }

    str::Builder aligned;
    for (int off = 0; off < content.len;) {
        Str rest = Str(content.s + off, content.len - off);
        int nl = str::IndexOfChar(rest, '\n');
        int lineLen = nl >= 0 ? nl : rest.len;
        int labelBytes = 0;
        int labelWidth = GetPropertyLabelWidth(Str(rest.s, lineLen), &labelBytes);
        if (labelWidth >= 0) {
            int nSpacesBefore = maxLabelWidth - labelWidth;
            for (int i = 0; i < nSpacesBefore; i++) {
                aligned.AppendChar(' ');
            }
            aligned.Append(Str(rest.s, labelBytes));
            aligned.Append(StrL("  "));
            aligned.Append(Str(rest.s + labelBytes + 1, lineLen - labelBytes - 1));
        } else {
            aligned.Append(Str(rest.s, lineLen));
        }
        if (nl >= 0) {
            aligned.AppendChar('\n');
        }
        off += lineLen + (nl >= 0 ? 1 : 0);
    }
    text.Reset(ToStr(aligned));
}
