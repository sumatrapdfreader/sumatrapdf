/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "DocProperties.h"

Str kPropTitle = StrL("title");
Str kPropAuthor = StrL("author");
Str kPropCopyright = StrL("copyright");
Str kPropSubject = StrL("subject");
Str kPropCreationDate = StrL("creationDate");
Str kPropModificationDate = StrL("modDate");
Str kPropCreatorApp = StrL("creatorApp");
Str kPropUnsupportedFeatures = StrL("unsupportedFeatures");
Str kPropFontList = StrL("fontList");
Str kPropPdfVersion = StrL("pdfVersion");
Str kPropPdfProducer = StrL("pdfProducer");
Str kPropPdfFileStructure = StrL("pdfFileStructure");
Str kPropFiles = StrL("files");
Str kPropKeywords = StrL("keywords");
Str kPropEncryption = StrL("encryption");
Str kPropSignatures = StrL("signatures");
Str kPropImageSize = StrL("imageSize");
Str kPropDpi = StrL("dpi");
Str kPropComment = StrL("comment");
Str kPropCameraMake = StrL("cameraMake");
Str kPropCameraModel = StrL("cameraModel");
Str kPropDateOriginal = StrL("dateOriginal");
Str kPropExposureTime = StrL("exposureTime");
Str kPropFNumber = StrL("fNumber");
Str kPropIsoSpeed = StrL("isoSpeed");
Str kPropFocalLength = StrL("focalLength");
Str kPropFocalLength35mm = StrL("focalLength35mm");
Str kPropFlash = StrL("flash");
Str kPropOrientation = StrL("orientation");
Str kPropExposureProgram = StrL("exposureProgram");
Str kPropMeteringMode = StrL("meteringMode");
Str kPropWhiteBalance = StrL("whiteBalance");
Str kPropExposureBias = StrL("exposureBias");
Str kPropBitsPerSample = StrL("bitsPerSample");
Str kPropResolutionUnit = StrL("resolutionUnit");
Str kPropSoftware = StrL("software");
Str kPropDateTime = StrL("dateTime");
Str kPropYCbCrPositioning = StrL("yCbCrPositioning");
Str kPropExifVersion = StrL("exifVersion");
Str kPropDateTimeDigitized = StrL("dateTimeDigitized");
Str kPropComponentsConfig = StrL("componentsConfig");
Str kPropCompressedBpp = StrL("compressedBpp");
Str kPropMaxAperture = StrL("maxAperture");
Str kPropLightSource = StrL("lightSource");
Str kPropUserComment = StrL("userComment");
Str kPropFlashpixVersion = StrL("flashpixVersion");
Str kPropColorSpace = StrL("colorSpace");
Str kPropPixelXDimension = StrL("pixelXDimension");
Str kPropPixelYDimension = StrL("pixelYDimension");
Str kPropFileSource = StrL("fileSource");
Str kPropSceneType = StrL("sceneType");
Str kPropImageFileSize = StrL("imageFileSize");
Str kPropImagePath = StrL("imagePath");

// clang-format off
Str gAllProps[] = {
     kPropTitle,
     kPropAuthor,
     kPropCopyright,
     kPropSubject,
     kPropCreationDate,
     kPropModificationDate,
     kPropCreatorApp,
     kPropUnsupportedFeatures,
     kPropFontList,
     kPropPdfVersion,
     kPropPdfProducer,
     kPropPdfFileStructure,
     Str(),
};
// clang-format off

int PropsCount(const Props& props) {
    int n = len(props);
    ReportIf(n < 0 || (n % 2) != 0);
    return n / 2;
}

int GetPropIdx(const Props& props, Str name) {
    int n = PropsCount(props);
    for (int i = 0; i < n; i++) {
        int idx = i * 2;
        if (str::Eq(props[idx], name)) {
            return idx;
        }
    }
    return -1;
}

Str GetPropValueTemp(const Props& props, Str name) {
    int idx = GetPropIdx(props, name);
    if (idx < 0) {
        return {};
    }
    return props[idx + 1];
}

void AddProp(Props& props, Str name, Str val, bool replaceIfExists) {
    ReportIf(!name || !val);
    int idx = GetPropIdx(props, name);
    if (idx < 0) {
        // doesn't exsit
        props.Append(name);
        props.Append(val);
        return;
    }
    if (!replaceIfExists) {
        return;
    }
    props.SetAt(idx + 1, val);
}

// strings are pairs of str1, str2 laid in sequence, with empty Str to mark the end
// we find str1 matching s and return str2 or empty Str if not found
Str GetMatchingString(const Str* strings, Str s) {
    while (*strings) {
        Str str1 = *strings++;
        Str str2 = *strings++;
        if (str1.s == s.s || str::Eq(str1, s)) {
            return str2;
        }
    }
    return {};
}