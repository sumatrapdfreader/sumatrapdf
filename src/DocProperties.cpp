/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "DocProperties.h"

static SeqStrings gPropNames =
    "title\0"
    "author\0"
    "copyright\0"
    "subject\0"
    "creationDate\0"
    "modDate\0"
    "creatorApp\0"
    "unsupportedFeatures\0"
    "fontList\0"
    "pdfVersion\0"
    "pdfProducer\0"
    "pdfFileStructure\0"
    "files\0"
    "keywords\0"
    "encryption\0"
    "signatures\0"
    "imageSize\0"
    "dpi\0"
    "comment\0"
    "cameraMake\0"
    "cameraModel\0"
    "dateOriginal\0"
    "exposureTime\0"
    "fNumber\0"
    "isoSpeed\0"
    "focalLength\0"
    "focalLength35mm\0"
    "flash\0"
    "orientation\0"
    "exposureProgram\0"
    "meteringMode\0"
    "whiteBalance\0"
    "exposureBias\0"
    "bitsPerSample\0"
    "resolutionUnit\0"
    "software\0"
    "dateTime\0"
    "yCbCrPositioning\0"
    "exifVersion\0"
    "dateTimeDigitized\0"
    "componentsConfig\0"
    "compressedBpp\0"
    "maxAperture\0"
    "lightSource\0"
    "userComment\0"
    "flashpixVersion\0"
    "colorSpace\0"
    "pixelXDimension\0"
    "pixelYDimension\0"
    "fileSource\0"
    "sceneType\0"
    "imageFileSize\0"
    "imagePath\0"
    "\0";

// clang-format off
DocProp gAllProps[] = {
    DocProp::Title,
    DocProp::Author,
    DocProp::Copyright,
    DocProp::Subject,
    DocProp::CreationDate,
    DocProp::ModificationDate,
    DocProp::CreatorApp,
    DocProp::UnsupportedFeatures,
    DocProp::FontList,
    DocProp::PdfVersion,
    DocProp::PdfProducer,
    DocProp::PdfFileStructure,
    DocProp::None,
};
// clang-format on

int PropsCount(const Props& props) {
    int n = len(props);
    ReportIf(n < 0);
    return n;
}

int GetPropIdx(const Props& props, DocProp prop) {
    int n = PropsCount(props);
    for (int i = 0; i < n; i++) {
        if (props[i].prop == prop) {
            return i;
        }
    }
    return -1;
}

Str GetPropValueTemp(const Props& props, DocProp prop) {
    int idx = GetPropIdx(props, prop);
    if (idx < 0) {
        return {};
    }
    return props[idx].val;
}

void AddProp(Props& props, DocProp prop, Str val, bool replaceIfExists) {
    ReportIf(prop == DocProp::None || !val);
    int idx = GetPropIdx(props, prop);
    if (idx < 0) {
        // doesn't exsit
        props.Append({prop, val});
        return;
    }
    if (!replaceIfExists) {
        return;
    }
    props[idx].val = val;
}

void AddPropOwned(Props& props, DocProp prop, Str val, bool replaceIfExists) {
    if (!val) {
        return;
    }
    int idx = GetPropIdx(props, prop);
    if (idx >= 0 && !replaceIfExists) {
        return;
    }
    Str owned = str::Dup(val);
    if (idx < 0) {
        props.Append({prop, owned});
        return;
    }
    str::Free(props[idx].val);
    props[idx].val = owned;
}

void FreeProps(Props& props) {
    int n = PropsCount(props);
    for (int i = 0; i < n; i++) {
        str::Free(props[i].val);
    }
    props.Reset();
}

// gPropNames lists the names in DocProp order, so DocProp::Title (value 1) is
// the first name (index 0); the value is index + 1.
TempStr PropNameTemp(DocProp prop) {
    int idx = (int)prop - 1;
    if (idx < 0) {
        return {};
    }
    return SeqStrByIndex(gPropNames, idx);
}

DocProp PropFromName(Str name) {
    int idx = SeqStrIndex(gPropNames, name);
    if (idx < 0) {
        return DocProp::None;
    }
    return (DocProp)(idx + 1);
}
