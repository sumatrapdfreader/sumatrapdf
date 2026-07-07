/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "DocProperties.h"

static SeqStrNum gPropNames =
    "title\0"
    "\x02"
    "author\0"
    "\x04"
    "copyright\0"
    "\x06"
    "subject\0"
    "\x08"
    "creationDate\0"
    "\x0a"
    "modDate\0"
    "\x0c"
    "creatorApp\0"
    "\x0e"
    "unsupportedFeatures\0"
    "\x10"
    "fontList\0"
    "\x12"
    "pdfVersion\0"
    "\x14"
    "pdfProducer\0"
    "\x16"
    "pdfFileStructure\0"
    "\x18"
    "files\0"
    "\x1a"
    "keywords\0"
    "\x1c"
    "encryption\0"
    "\x1e"
    "signatures\0"
    "\x20"
    "imageSize\0"
    "\x22"
    "dpi\0"
    "\x24"
    "comment\0"
    "\x26"
    "cameraMake\0"
    "\x28"
    "cameraModel\0"
    "\x2a"
    "dateOriginal\0"
    "\x2c"
    "exposureTime\0"
    "\x2e"
    "fNumber\0"
    "\x30"
    "isoSpeed\0"
    "\x32"
    "focalLength\0"
    "\x34"
    "focalLength35mm\0"
    "\x36"
    "flash\0"
    "\x38"
    "orientation\0"
    "\x3a"
    "exposureProgram\0"
    "\x3c"
    "meteringMode\0"
    "\x3e"
    "whiteBalance\0"
    "\x40"
    "exposureBias\0"
    "\x42"
    "bitsPerSample\0"
    "\x44"
    "resolutionUnit\0"
    "\x46"
    "software\0"
    "\x48"
    "dateTime\0"
    "\x4a"
    "yCbCrPositioning\0"
    "\x4c"
    "exifVersion\0"
    "\x4e"
    "dateTimeDigitized\0"
    "\x50"
    "componentsConfig\0"
    "\x52"
    "compressedBpp\0"
    "\x54"
    "maxAperture\0"
    "\x56"
    "lightSource\0"
    "\x58"
    "userComment\0"
    "\x5a"
    "flashpixVersion\0"
    "\x5c"
    "colorSpace\0"
    "\x5e"
    "pixelXDimension\0"
    "\x60"
    "pixelYDimension\0"
    "\x62"
    "fileSource\0"
    "\x64"
    "sceneType\0"
    "\x66"
    "imageFileSize\0"
    "\x68"
    "imagePath\0"
    "\x6a"
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

TempStr PropNameTemp(DocProp prop) {
    return SeqStrNumStrByNumber(gPropNames, (i64)prop);
}

DocProp PropFromName(Str name) {
    i64 n = 0;
    if (SeqStrNumIndex(gPropNames, name, &n) < 0) {
        return DocProp::None;
    }
    if (n < 0 || n > 255) {
        return DocProp::None;
    }
    return (DocProp)n;
}
