/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "DocProperties.h"

const char* kPropTitle = "title";
const char* kPropAuthor = "author";
const char* kPropCopyright = "copyright";
const char* kPropSubject = "subject";
const char* kPropCreationDate = "creationDate";
const char* kPropModificationDate = "modDate";
const char* kPropCreatorApp = "creatorApp";
const char* kPropUnsupportedFeatures = "unsupportedFeatures";
const char* kPropFontList = "fontList";
const char* kPropPdfVersion = "pdfVersion";
const char* kPropPdfProducer = "pdfProducer";
const char* kPropPdfFileStructure = "pdfFileStructure";
const char* kPropFiles = "files";
const char* kPropKeywords = "keywords";
const char* kPropEncryption = "encryption";
const char* kPropSignatures = "signatures";
const char* kPropImageSize = "imageSize";
const char* kPropDpi = "dpi";
const char* kPropComment = "comment";
const char* kPropCameraMake = "cameraMake";
const char* kPropCameraModel = "cameraModel";
const char* kPropDateOriginal = "dateOriginal";
const char* kPropExposureTime = "exposureTime";
const char* kPropFNumber = "fNumber";
const char* kPropIsoSpeed = "isoSpeed";
const char* kPropFocalLength = "focalLength";
const char* kPropFocalLength35mm = "focalLength35mm";
const char* kPropFlash = "flash";
const char* kPropOrientation = "orientation";
const char* kPropExposureProgram = "exposureProgram";
const char* kPropMeteringMode = "meteringMode";
const char* kPropWhiteBalance = "whiteBalance";
const char* kPropExposureBias = "exposureBias";
const char* kPropBitsPerSample = "bitsPerSample";
const char* kPropResolutionUnit = "resolutionUnit";
const char* kPropSoftware = "software";
const char* kPropDateTime = "dateTime";
const char* kPropYCbCrPositioning = "yCbCrPositioning";
const char* kPropExifVersion = "exifVersion";
const char* kPropDateTimeDigitized = "dateTimeDigitized";
const char* kPropComponentsConfig = "componentsConfig";
const char* kPropCompressedBpp = "compressedBpp";
const char* kPropMaxAperture = "maxAperture";
const char* kPropLightSource = "lightSource";
const char* kPropUserComment = "userComment";
const char* kPropFlashpixVersion = "flashpixVersion";
const char* kPropColorSpace = "colorSpace";
const char* kPropPixelXDimension = "pixelXDimension";
const char* kPropPixelYDimension = "pixelYDimension";
const char* kPropFileSource = "fileSource";
const char* kPropSceneType = "sceneType";
const char* kPropImageFileSize = "imageFileSize";
const char* kPropImagePath = "imagePath";

// clang-format off
const char* gAllProps[] = {
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
     nullptr,
};
// clang-format off

int PropsCount(const Props& props) {
    int n = props.Size();
    ReportIf(n < 0 || (n % 2) != 0);
    return n / 2;
}

int GetPropIdx(const Props& props, const char* name) {
    int n = PropsCount(props);
    for (int i = 0; i < n; i++) {
        int idx = i * 2;
        char* v = props.At(idx);
        if (str::Eq(v, name)) {
            return idx;
        }
    }
    return -1;
}

char* GetPropValueTemp(const Props& props, const char* name) {
    int idx = GetPropIdx(props, name);
    if (idx < 0) {
        return nullptr;
    }
    char* v = props.At(idx + 1);
    return v;
}

void AddProp(Props& props, const char* name, const char* val, bool replaceIfExists) {
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

// strings are pairs of str1, str2 laid in sequence, with nullptr to mark the en
// we find str1 matching s and return str2 or nullptr if not found
const char* GetMatchingString(const char** strings, const char* s) {
    while (*strings) {
        const char* str1 = *strings++;
        const char* str2 = *strings++;
        if (str1 == s || str::Eq(str1, s)) {
            return str2;
        }
    }
    return nullptr;
}
