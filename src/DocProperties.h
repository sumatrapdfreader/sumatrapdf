/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef DocProperties_h
#define DocProperties_h

enum class DocProp : u8 {
    None = 0,
    Title = 1,
    Author = 2,
    Copyright = 3,
    Subject = 4,
    CreationDate = 5,
    ModificationDate = 6,
    CreatorApp = 7,
    UnsupportedFeatures = 8,
    FontList = 9,
    PdfVersion = 10,
    PdfProducer = 11,
    PdfFileStructure = 12,
    Files = 13,
    Keywords = 14,
    Encryption = 15,
    Signatures = 16,
    ImageSize = 17,
    Dpi = 18,
    Comment = 19,
    CameraMake = 20,
    CameraModel = 21,
    DateOriginal = 22,
    ExposureTime = 23,
    FNumber = 24,
    IsoSpeed = 25,
    FocalLength = 26,
    FocalLength35mm = 27,
    Flash = 28,
    Orientation = 29,
    ExposureProgram = 30,
    MeteringMode = 31,
    WhiteBalance = 32,
    ExposureBias = 33,
    BitsPerSample = 34,
    ResolutionUnit = 35,
    Software = 36,
    DateTime = 37,
    YCbCrPositioning = 38,
    ExifVersion = 39,
    DateTimeDigitized = 40,
    ComponentsConfig = 41,
    CompressedBpp = 42,
    MaxAperture = 43,
    LightSource = 44,
    UserComment = 45,
    FlashpixVersion = 46,
    ColorSpace = 47,
    PixelXDimension = 48,
    PixelYDimension = 49,
    FileSource = 50,
    SceneType = 51,
    ImageFileSize = 52,
    ImagePath = 53,
};

extern DocProp gAllProps[];

struct PropValue {
    DocProp prop;
    Str val;
};

using Props = Vec<PropValue>;
int PropsCount(const Props& props);
int GetPropIdx(const Props& props, DocProp prop);
Str GetPropValueTemp(const Props& props, DocProp prop);
void AddProp(Props& props, DocProp prop, Str val, bool replaceIfExists = false);

TempStr PropNameTemp(DocProp prop);
DocProp PropFromName(Str name);

#endif
