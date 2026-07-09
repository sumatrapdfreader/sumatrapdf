/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "DocProperties.h"

static SeqStrNum gPropNames =
    "title\0"
    "\x01"
    "author\0"
    "\x02"
    "copyright\0"
    "\x03"
    "subject\0"
    "\x04"
    "creationDate\0"
    "\x05"
    "modDate\0"
    "\x06"
    "creatorApp\0"
    "\x07"
    "unsupportedFeatures\0"
    "\x08"
    "fontList\0"
    "\x09"
    "pdfVersion\0"
    "\x0a"
    "pdfProducer\0"
    "\x0b"
    "pdfFileStructure\0"
    "\x0c"
    "files\0"
    "\x0d"
    "keywords\0"
    "\x0e"
    "encryption\0"
    "\x0f"
    "signatures\0"
    "\x10"
    "imageSize\0"
    "\x11"
    "dpi\0"
    "\x12"
    "comment\0"
    "\x13"
    "cameraMake\0"
    "\x14"
    "cameraModel\0"
    "\x15"
    "dateOriginal\0"
    "\x16"
    "exposureTime\0"
    "\x17"
    "fNumber\0"
    "\x18"
    "isoSpeed\0"
    "\x19"
    "focalLength\0"
    "\x1a"
    "focalLength35mm\0"
    "\x1b"
    "flash\0"
    "\x1c"
    "orientation\0"
    "\x1d"
    "exposureProgram\0"
    "\x1e"
    "meteringMode\0"
    "\x1f"
    "whiteBalance\0"
    "\x20"
    "exposureBias\0"
    "\x21"
    "bitsPerSample\0"
    "\x22"
    "resolutionUnit\0"
    "\x23"
    "software\0"
    "\x24"
    "dateTime\0"
    "\x25"
    "yCbCrPositioning\0"
    "\x26"
    "exifVersion\0"
    "\x27"
    "dateTimeDigitized\0"
    "\x28"
    "componentsConfig\0"
    "\x29"
    "compressedBpp\0"
    "\x2a"
    "maxAperture\0"
    "\x2b"
    "lightSource\0"
    "\x2c"
    "userComment\0"
    "\x2d"
    "flashpixVersion\0"
    "\x2e"
    "colorSpace\0"
    "\x2f"
    "pixelXDimension\0"
    "\x30"
    "pixelYDimension\0"
    "\x31"
    "fileSource\0"
    "\x32"
    "sceneType\0"
    "\x33"
    "imageFileSize\0"
    "\x34"
    "imagePath\0"
    "\x35"
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
