/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern const char* kPropTitle;
extern const char* kPropAuthor;
extern const char* kPropCopyright;
extern const char* kPropSubject;
extern const char* kPropCreationDate;
extern const char* kPropModificationDate;
extern const char* kPropCreatorApp;
extern const char* kPropUnsupportedFeatures;
extern const char* kPropFontList;
extern const char* kPropPdfVersion;
extern const char* kPropPdfProducer;
extern const char* kPropPdfFileStructure;
extern const char* kPropFiles;
extern const char* kPropKeywords;
extern const char* kPropEncryption;
extern const char* kPropSignatures;
extern const char* kPropImageSize;
extern const char* kPropDpi;
extern const char* kPropComment;
extern const char* kPropCameraMake;
extern const char* kPropCameraModel;
extern const char* kPropDateOriginal;
extern const char* kPropExposureTime;
extern const char* kPropFNumber;
extern const char* kPropIsoSpeed;
extern const char* kPropFocalLength;
extern const char* kPropFocalLength35mm;
extern const char* kPropFlash;
extern const char* kPropOrientation;
extern const char* kPropExposureProgram;
extern const char* kPropMeteringMode;
extern const char* kPropWhiteBalance;
extern const char* kPropExposureBias;
extern const char* kPropBitsPerSample;
extern const char* kPropResolutionUnit;
extern const char* kPropSoftware;
extern const char* kPropDateTime;
extern const char* kPropYCbCrPositioning;
extern const char* kPropExifVersion;
extern const char* kPropDateTimeDigitized;
extern const char* kPropComponentsConfig;
extern const char* kPropCompressedBpp;
extern const char* kPropMaxAperture;
extern const char* kPropLightSource;
extern const char* kPropUserComment;
extern const char* kPropFlashpixVersion;
extern const char* kPropColorSpace;
extern const char* kPropPixelXDimension;
extern const char* kPropPixelYDimension;
extern const char* kPropFileSource;
extern const char* kPropSceneType;
extern const char* kPropImageFileSize;
extern const char* kPropImagePath;

extern const char* gAllProps[];

// Props are stored in StrVec as name, value sequentially
using Props = StrVec;
int PropsCount(const Props& props);
int GetPropIdx(const Props& props, const char* name);
char* GetPropValueTemp(const Props& props, const char* name);
void AddProp(Props& props, const char* name, const char* val, bool replaceIfExists = false);

const char* GetMatchingString(const char**, const char*);
