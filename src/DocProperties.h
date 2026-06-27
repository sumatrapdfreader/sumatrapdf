/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Str kPropTitle;
extern Str kPropAuthor;
extern Str kPropCopyright;
extern Str kPropSubject;
extern Str kPropCreationDate;
extern Str kPropModificationDate;
extern Str kPropCreatorApp;
extern Str kPropUnsupportedFeatures;
extern Str kPropFontList;
extern Str kPropPdfVersion;
extern Str kPropPdfProducer;
extern Str kPropPdfFileStructure;
extern Str kPropFiles;
extern Str kPropKeywords;
extern Str kPropEncryption;
extern Str kPropSignatures;
extern Str kPropImageSize;
extern Str kPropDpi;
extern Str kPropComment;
extern Str kPropCameraMake;
extern Str kPropCameraModel;
extern Str kPropDateOriginal;
extern Str kPropExposureTime;
extern Str kPropFNumber;
extern Str kPropIsoSpeed;
extern Str kPropFocalLength;
extern Str kPropFocalLength35mm;
extern Str kPropFlash;
extern Str kPropOrientation;
extern Str kPropExposureProgram;
extern Str kPropMeteringMode;
extern Str kPropWhiteBalance;
extern Str kPropExposureBias;
extern Str kPropBitsPerSample;
extern Str kPropResolutionUnit;
extern Str kPropSoftware;
extern Str kPropDateTime;
extern Str kPropYCbCrPositioning;
extern Str kPropExifVersion;
extern Str kPropDateTimeDigitized;
extern Str kPropComponentsConfig;
extern Str kPropCompressedBpp;
extern Str kPropMaxAperture;
extern Str kPropLightSource;
extern Str kPropUserComment;
extern Str kPropFlashpixVersion;
extern Str kPropColorSpace;
extern Str kPropPixelXDimension;
extern Str kPropPixelYDimension;
extern Str kPropFileSource;
extern Str kPropSceneType;
extern Str kPropImageFileSize;
extern Str kPropImagePath;

extern Str gAllProps[];

// Props are stored in StrVec as name, value sequentially
using Props = StrVec;
int PropsCount(const Props& props);
int GetPropIdx(const Props& props, Str name);
Str GetPropValueTemp(const Props& props, Str name);
void AddProp(Props& props, Str name, Str val, bool replaceIfExists = false);

Str GetMatchingString(const Str* strings, Str s);