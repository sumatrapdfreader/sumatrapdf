/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Exif_h
#define Exif_h

enum class ExifProp : u16 {
    None = 0,
    ImageWidth = 0x0100,
    ImageLength = 0x0101,
    BitsPerSample = 0x0102,
    Compression = 0x0103,
    ImageDescription = 0x010E,
    Make = 0x010F,
    Model = 0x0110,
    Orientation = 0x0112,
    XResolution = 0x011A,
    YResolution = 0x011B,
    ResolutionUnit = 0x0128,
    Software = 0x0131,
    DateTime = 0x0132,
    Artist = 0x013B,
    YCbCrPositioning = 0x0213,
    Copyright = 0x8298,
    ExifOffset = 0x8769,
    GpsInfo = 0x8825,
    XPTitle = 0x9C9B,
    XPComment = 0x9C9C,
    XPAuthor = 0x9C9D,
    XPKeywords = 0x9C9E,
    XPSubject = 0x9C9F,
    ExposureTime = 0x829A,
    FNumber = 0x829D,
    ExposureProgram = 0x8822,
    ISOSpeed = 0x8827,
    ExifVersion = 0x9000,
    DateTimeOriginal = 0x9003,
    DateTimeDigitized = 0x9004,
    ComponentsConfiguration = 0x9101,
    CompressedBitsPerPixel = 0x9102,
    ExposureBiasValue = 0x9204,
    MaxApertureValue = 0x9205,
    MeteringMode = 0x9207,
    LightSource = 0x9208,
    Flash = 0x9209,
    FocalLength = 0x920A,
    MakerNote = 0x927C,
    UserComment = 0x9286,
    FlashpixVersion = 0xA000,
    ColorSpace = 0xA001,
    ExifImageWidth = 0xA002,
    ExifImageLength = 0xA003,
    InteroperabilityOffset = 0xA005,
    FileSource = 0xA300,
    SceneType = 0xA301,
    ExposureMode = 0xA402,
    WhiteBalance = 0xA403,
    FocalLengthIn35mmFilm = 0xA405,
    SceneCaptureType = 0xA406,
    GainControl = 0xA407,
    Contrast = 0xA408,
    Saturation = 0xA409,
    Sharpness = 0xA40A,
    LensSpecification = 0xA432,
    LensMake = 0xA433,
    LensModel = 0xA434,
    SensitivityType = 0x8830,
};

enum class ExifValueKind : u8 {
    Unknown,
    Bytes,
    String,
    Int,
    Rational,
};

struct ExifRational {
    i64 num = 0;
    i64 den = 0;
};

struct ExifEntry {
    u16 tag = 0;
    u16 type = 0;
    u32 count = 0;
    int dataOff = 0;
    u8 group = 0;
};

struct ExifParser {
    Str data;
    Str exifBlob;
    u8* ownedExif = nullptr;
    bool isBE = false;
    int tiffBase = 0;
    bool hasJpegThumbnail = false;
    Vec<ExifEntry> entries;
    StrVec dumpLines;

    ~ExifParser();
    void Reset();
    bool Parse(Str imageData);
    bool HasProp(ExifProp prop) const;
    ExifValueKind GetPropKind(ExifProp prop) const;
    TempStr GetStringProp(ExifProp prop, ExifProp altProp = ExifProp::None) const;
    bool GetIntProp(ExifProp prop, i64* valOut) const;
    bool GetRationalProp(ExifProp prop, ExifRational* valOut) const;
    bool GetFloatProp(ExifProp prop, double* valOut) const;
    TempStr GetFormattedPropTemp(ExifProp prop) const;
    void GetDumpLines(StrVec& linesOut) const;
};

#endif
