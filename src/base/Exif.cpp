/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "ByteReader.h"
#include "GuessFileType.h"
#include "Exif.h"

namespace {

enum TiffType : u16 {
    TiffByte = 1,
    TiffAscii = 2,
    TiffShort = 3,
    TiffLong = 4,
    TiffRational = 5,
    TiffSByte = 6,
    TiffUndefined = 7,
    TiffSShort = 8,
    TiffSLong = 9,
    TiffSRational = 10,
};

enum class IfdGroup : u8 {
    Image,
    Exif,
    Gps,
    Interop,
    Thumbnail,
    MakerNote,
};

struct TagDef {
    u16 id;
    Str name;
};

static const TagDef kImageTags[] = {
    {0x010E, "ImageDescription"},
    {0x010F, "Make"},
    {0x0110, "Model"},
    {0x0112, "Orientation"},
    {0x011A, "XResolution"},
    {0x011B, "YResolution"},
    {0x0128, "ResolutionUnit"},
    {0x0131, "Software"},
    {0x0132, "DateTime"},
    {0x013B, "Artist"},
    {0x013C, "HostComputer"},
    {0x0213, "YCbCrPositioning"},
    {0x8298, "Copyright"},
    {0x8769, "ExifOffset"},
    {0x8825, "GPSInfo"},
    {0x9C9B, "XPTitle"},
    {0x9C9C, "XPComment"},
    {0x9C9D, "XPAuthor"},
    {0x9C9E, "XPKeywords"},
    {0x9C9F, "XPSubject"},
    {0xC4A5, "PrintIM"},
    {0x0100, "ImageWidth"},
    {0x0101, "ImageLength"},
    {0x0102, "BitsPerSample"},
    {0x0103, "Compression"},
    {0x0201, "JPEGInterchangeFormat"},
    {0x0202, "JPEGInterchangeFormatLength"},
};

static const TagDef kExifTags[] = {
    {0x829A, "ExposureTime"},
    {0x829D, "FNumber"},
    {0x8822, "ExposureProgram"},
    {0x8827, "ISOSpeedRatings"},
    {0x9000, "ExifVersion"},
    {0x9003, "DateTimeOriginal"},
    {0x9004, "DateTimeDigitized"},
    {0x9010, "OffsetTime"},
    {0x9011, "OffsetTimeOriginal"},
    {0x9012, "OffsetTimeDigitized"},
    {0x9101, "ComponentsConfiguration"},
    {0x9102, "CompressedBitsPerPixel"},
    {0x9201, "ShutterSpeedValue"},
    {0x9202, "ApertureValue"},
    {0x9203, "BrightnessValue"},
    {0x9204, "ExposureBiasValue"},
    {0x9205, "MaxApertureValue"},
    {0x9207, "MeteringMode"},
    {0x9208, "LightSource"},
    {0x9209, "Flash"},
    {0x920A, "FocalLength"},
    {0x9214, "SubjectArea"},
    {0x927C, "MakerNote"},
    {0x9286, "UserComment"},
    {0x9290, "SubSecTime"},
    {0x9291, "SubSecTimeOriginal"},
    {0x9292, "SubSecTimeDigitized"},
    {0xA000, "FlashPixVersion"},
    {0xA001, "ColorSpace"},
    {0xA002, "ExifImageWidth"},
    {0xA003, "ExifImageLength"},
    {0xA005, "InteroperabilityOffset"},
    {0xA20E, "FocalPlaneXResolution"},
    {0xA20F, "FocalPlaneYResolution"},
    {0xA210, "FocalPlaneResolutionUnit"},
    {0xA217, "SensingMethod"},
    {0xA300, "FileSource"},
    {0xA301, "SceneType"},
    {0xA401, "CustomRendered"},
    {0xA402, "ExposureMode"},
    {0xA403, "WhiteBalance"},
    {0xA404, "DigitalZoomRatio"},
    {0xA405, "FocalLengthIn35mmFilm"},
    {0xA406, "SceneCaptureType"},
    {0xA407, "GainControl"},
    {0xA408, "Contrast"},
    {0xA409, "Saturation"},
    {0xA40A, "Sharpness"},
    {0xA432, "LensSpecification"},
    {0xA433, "LensMake"},
    {0xA434, "LensModel"},
    {0x8830, "SensitivityType"},
};

static const TagDef kGpsTags[] = {
    {0x0000, "GPSVersionID"},
    {0x0001, "GPSLatitudeRef"},
    {0x0002, "GPSLatitude"},
    {0x0003, "GPSLongitudeRef"},
    {0x0004, "GPSLongitude"},
    {0x0005, "GPSAltitudeRef"},
    {0x0006, "GPSAltitude"},
    {0x0007, "GPSTimeStamp"},
    {0x0009, "GPSStatus"},
    {0x000A, "GPSMeasureMode"},
    {0x000B, "GPSDOP"},
    {0x000C, "GPSSpeedRef"},
    {0x000D, "GPSSpeed"},
    {0x0010, "GPSImgDirectionRef"},
    {0x0011, "GPSImgDirection"},
    {0x001D, "GPSDate"},
    {0x001B, "GPSProcessingMethod"},
};

static const TagDef kInteropTags[] = {
    {0x0001, "InteroperabilityIndex"},
    {0x0002, "InteroperabilityVersion"},
    {0x1001, "RelatedImageWidth"},
    {0x1002, "RelatedImageLength"},
};

static Str LookupTagName(const TagDef* tags, int n, u16 id) {
    for (int i = 0; i < n; i++) {
        if (tags[i].id == id) {
            return Str(tags[i].name);
        }
    }
    return {};
}

static Str GroupPrefix(IfdGroup g) {
    switch (g) {
        case IfdGroup::Image:
            return StrL("Image");
        case IfdGroup::Exif:
            return StrL("EXIF");
        case IfdGroup::Gps:
            return StrL("GPS");
        case IfdGroup::Interop:
            return StrL("Interoperability");
        case IfdGroup::Thumbnail:
            return StrL("Thumbnail");
        case IfdGroup::MakerNote:
            return StrL("MakerNote");
    }
    return StrL("");
}

static Str TypeName(u16 type) {
    switch (type) {
        case TiffByte:
            return StrL("Byte");
        case TiffAscii:
            return StrL("ASCII");
        case TiffShort:
            return StrL("Short");
        case TiffLong:
            return StrL("Long");
        case TiffRational:
            return StrL("Ratio");
        case TiffSByte:
            return StrL("Signed Byte");
        case TiffUndefined:
            return StrL("Undefined");
        case TiffSShort:
            return StrL("Signed Short");
        case TiffSLong:
            return StrL("Signed Long");
        case TiffSRational:
            return StrL("Signed Ratio");
        default:
            return StrL("Unknown");
    }
}

static int TypeElemSize(u16 type) {
    switch (type) {
        case TiffByte:
        case TiffAscii:
        case TiffSByte:
        case TiffUndefined:
            return 1;
        case TiffShort:
        case TiffSShort:
            return 2;
        case TiffLong:
        case TiffSLong:
            return 4;
        case TiffRational:
        case TiffSRational:
            return 8;
        default:
            return 0;
    }
}

static const TagDef* TagsForGroup(IfdGroup g, int& n) {
    switch (g) {
        case IfdGroup::Image:
        case IfdGroup::Thumbnail:
            n = dimof(kImageTags);
            return kImageTags;
        case IfdGroup::Exif:
            n = dimof(kExifTags);
            return kExifTags;
        case IfdGroup::Gps:
            n = dimof(kGpsTags);
            return kGpsTags;
        case IfdGroup::Interop:
            n = dimof(kInteropTags);
            return kInteropTags;
        case IfdGroup::MakerNote:
            n = 0;
            return nullptr;
    }
    n = 0;
    return nullptr;
}

static Str TagName(IfdGroup g, u16 tag) {
    int n;
    const TagDef* tags = TagsForGroup(g, n);
    return tags ? LookupTagName(tags, n, tag) : Str{};
}

static Str LookupEnum(SeqStrings names, u32 val) {
    if (val <= (u32)INT_MAX) {
        return SeqStrByIndex(names, (int)val);
    }
    return {};
}

static Str FormatOrientation(u32 val) {
    static SeqStrings names =
        "Horizontal (normal)\0"
        "Mirrored horizontal\0"
        "Rotated 180\0"
        "Mirrored vertical\0"
        "Mirrored horizontal then rotated 90 CCW\0"
        "Rotated 90 CW\0"
        "Mirrored horizontal then rotated 90 CW\0"
        "Rotated 90 CCW\0";
    return val > 0 ? LookupEnum(names, val - 1) : Str{};
}

static Str FormatExposureProgram(u32 val) {
    static SeqStrings names =
        "Unidentified\0"
        "Manual\0"
        "Program Normal\0"
        "Aperture Priority\0"
        "Shutter Priority\0"
        "Program Creative\0"
        "Program Action\0"
        "Portrait Mode\0"
        "Landscape Mode\0";
    return LookupEnum(names, val);
}

static Str FormatMeteringMode(u32 val) {
    static SeqStrings names =
        "Unidentified\0"
        "Average\0"
        "CenterWeightedAverage\0"
        "Spot\0"
        "MultiSpot\0"
        "Pattern\0"
        "Partial\0";
    return LookupEnum(names, val);
}

static Str FormatColorSpace(u32 val) {
    if (val == 1) {
        return StrL("sRGB");
    }
    if (val == 0xFFFF) {
        return StrL("Uncalibrated");
    }
    return {};
}

static Str FormatWhiteBalance(u32 val) {
    return val == 0 ? StrL("Auto") : StrL("Manual");
}

static Str FormatExposureMode(u32 val) {
    static SeqStrings names = "Auto Exposure\0Manual Exposure\0Auto Bracket\0";
    return LookupEnum(names, val);
}

static Str FormatSceneCaptureType(u32 val) {
    static SeqStrings names = "Standard\0Landscape\0Portrait\0Night\0";
    return LookupEnum(names, val);
}

static Str FormatGainControl(u32 val) {
    static SeqStrings names =
        "None\0"
        "Low gain up\0"
        "High gain up\0"
        "Low gain down\0"
        "High gain down\0";
    return LookupEnum(names, val);
}

static Str FormatContrastSatSharp(u32 val) {
    static SeqStrings names = "Normal\0Soft\0Hard\0";
    return LookupEnum(names, val);
}

static Str FormatCustomRendered(u32 val) {
    return val == 0 ? StrL("Normal") : StrL("Custom");
}

static Str FormatSensitivityType(u32 val) {
    static SeqStrings names =
        "Unknown\0"
        "Standard Output Sensitivity\0"
        "Recommended Exposure Index\0"
        "ISO Speed\0"
        "Standard Output Sensitivity and Recommended Exposure Index\0"
        "Standard Output Sensitivity and ISO Speed\0"
        "Recommended Exposure Index and ISO Speed\0"
        "Standard Output Sensitivity, Recommended Exposure Index and ISO Speed\0";
    return LookupEnum(names, val);
}

static Str FormatResolutionUnit(u32 val) {
    if (val == 2) {
        return StrL("Pixels/Inch");
    }
    if (val == 3) {
        return StrL("Pixels/Centimeter");
    }
    return {};
}

static Str FormatYCbCrPositioning(u32 val) {
    return val == 1 ? StrL("Centered") : StrL("Co-sited");
}

static Str FormatCompression(u32 val) {
    if (val == 6) {
        return StrL("JPEG (old-style)");
    }
    if (val == 7) {
        return StrL("JPEG");
    }
    return {};
}

static Str FormatFileSource(u8 val) {
    return val == 3 ? StrL("Digital Camera") : Str{};
}

static Str FormatSceneType(u8 val) {
    return val == 1 ? StrL("Directly Photographed") : Str{};
}

static Str FormatFlash(u32 val) {
    switch (val) {
        case 0:
            return StrL("Flash did not fire");
        case 1:
            return StrL("Flash fired");
        case 5:
            return StrL("Strobe return light not detected");
        case 7:
            return StrL("Strobe return light detected");
        case 9:
            return StrL("Flash fired, compulsory flash mode");
        case 16:
            return StrL("Flash did not fire, compulsory flash mode");
        case 24:
            return StrL("Flash did not fire, auto mode");
        case 25:
            return StrL("Flash fired, auto mode");
        default:
            return {};
    }
}

static bool IsXpProp(ExifProp prop) {
    return prop == ExifProp::XPTitle || prop == ExifProp::XPComment || prop == ExifProp::XPAuthor ||
           prop == ExifProp::XPKeywords || prop == ExifProp::XPSubject;
}

static bool IsAsciiUndefinedProp(ExifProp prop) {
    return prop == ExifProp::ExifVersion || prop == ExifProp::FlashpixVersion;
}

static u16 ReadLE16(const u8* p) {
    return (u16)(p[0] | (p[1] << 8));
}

static TempStr Utf16LeToUtf8Temp(Str data) {
    str::Builder out;
    int n = data.len & ~1;
    for (int i = 0; i + 1 < n; i += 2) {
        u32 c = ReadLE16((const u8*)data.s + i);
        if (c == 0) {
            break;
        }
        if (0xD800 <= c && c <= 0xDBFF && i + 3 < n) {
            u32 lo = ReadLE16((const u8*)data.s + i + 2);
            if (0xDC00 <= lo && lo <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        char buf[4];
        int off = 0;
        str::Utf8Encode(buf, off, (int)c);
        out.Append(Str(buf, off));
    }
    return ToStrTemp(out);
}

static TempStr TrimmedAsciiTemp(ByteReader r, int off, u32 count) {
    if (off < 0 || off >= r.len) {
        return str::DupTemp("");
    }
    int n = (int)count;
    if (off + n > r.len) {
        n = r.len - off;
    }
    while (n > 0 && r.Byte(off + n - 1) == 0) {
        n--;
    }
    return strconv::AnsiToUtf8Temp(Str((char*)(r.d + off), n));
}

static const ExifEntry* FindEntry(const ExifParser& parser, ExifProp prop) {
    u16 tag = (u16)prop;
    for (const ExifEntry& entry : parser.entries) {
        if (entry.tag == tag) {
            return &entry;
        }
    }
    return nullptr;
}

static u16 ReadWord(const ExifParser& parser, int off) {
    return ByteReader(parser.exifBlob).Word(off, parser.isBE);
}

static u32 ReadDWord(const ExifParser& parser, int off) {
    return ByteReader(parser.exifBlob).DWord(off, parser.isBE);
}

static bool EntryBoundsOk(const ExifParser& parser, const ExifEntry& entry, int bytes) {
    return entry.dataOff >= 0 && entry.dataOff + bytes <= parser.exifBlob.len;
}

static int ValueOffset(const ExifParser& parser, u16 type, u32 count, u32 inlineVal, int entryOff) {
    int elemSize = TypeElemSize(type);
    if (elemSize == 0) {
        return 0;
    }
    i64 total = (i64)count * elemSize;
    if (total <= 4) {
        return entryOff + 8;
    }
    return (int)inlineVal + parser.tiffBase;
}

static void AppendLine(ExifParser& parser, IfdGroup g, u16 tag, u16 type, TempStr value) {
    Str prefix = GroupPrefix(g);
    Str name = TagName(g, tag);
    char tagNameBuf[32];
    if (!name) {
        snprintf(tagNameBuf, sizeof(tagNameBuf), "Tag 0x%04X", tag);
        name = Str(tagNameBuf);
    }
    TempStr line = fmt("%s %s (%s): %s", prefix, name, TypeName(type), value);
    parser.dumpLines.Append(line);
}

static TempStr FormatRationalPair(u32 num, u32 den, bool asFraction) {
    if (den == 0) {
        return str::DupTemp("0");
    }
    if (asFraction && num == 1) {
        return fmt("1/%u", den);
    }
    if (asFraction && num != 0 && den != 1) {
        return fmt("%u/%u", num, den);
    }
    if (den == 1) {
        return fmt("%u", num);
    }
    return fmt("%u/%u", num, den);
}

static TempStr FormatComponentsConfig(ByteReader r, int off, u32 count) {
    static SeqStrings compNames = "Y\0Cb\0Cr\0R\0G\0B\0";
    str::Builder s;
    for (u32 i = 0; i < count && off + (int)i < r.len; i++) {
        u8 c = r.Byte(off + i);
        if (c == 0) {
            break;
        }
        if (len(s) > 0) {
            s.Append(", ");
        }
        Str compName = SeqStrByIndex(compNames, c - 1);
        if (compName) {
            s.Append(compName);
        }
    }
    if (len(s) == 0) {
        return str::DupTemp("YCbCr");
    }
    return ToStrTemp(s);
}

static TempStr FormatUndefinedBytesTemp(ByteReader r, int off, u32 count, bool asList) {
    if (count == 0) {
        return str::DupTemp("");
    }
    if (off + (int)count > r.len) {
        count = (u32)(r.len - off);
    }
    bool isAscii = true;
    for (u32 i = 0; i < count; i++) {
        u8 b = r.Byte(off + i);
        if (b == 0) {
            break;
        }
        if (b < 0x20 || b >= 0x7f) {
            isAscii = false;
            break;
        }
    }
    if (isAscii) {
        return TrimmedAsciiTemp(r, off, count);
    }
    if (!asList) {
        return fmt("[%u bytes]", count);
    }
    str::Builder s;
    s.Append("[");
    u32 show = count > 20 ? 20 : count;
    for (u32 i = 0; i < show; i++) {
        if (i > 0) {
            s.Append(", ");
        }
        s.Append(fmt("%u", r.Byte(off + i)));
    }
    if (count > show) {
        s.Append(", ... ");
    }
    s.Append("]");
    return ToStrTemp(s);
}

static bool ReadRational(const ExifParser& parser, int off, ExifRational* valOut, bool isSigned) {
    if (off + 8 > parser.exifBlob.len) {
        return false;
    }
    if (isSigned) {
        valOut->num = (i32)ReadDWord(parser, off);
        valOut->den = (i32)ReadDWord(parser, off + 4);
    } else {
        valOut->num = ReadDWord(parser, off);
        valOut->den = ReadDWord(parser, off + 4);
    }
    return true;
}

static TempStr FormatValuesTemp(const ExifParser& parser, IfdGroup g, u16 tag, u16 type, u32 count, int off) {
    ByteReader r(parser.exifBlob);
    if (count == 0) {
        return str::DupTemp("");
    }

    if (type == TiffShort && count == 1) {
        u32 val = off + 2 <= r.len ? ReadWord(parser, off) : 0;
        Str s;
        if (g == IfdGroup::Image || g == IfdGroup::Thumbnail) {
            if (tag == (u16)ExifProp::Orientation) {
                s = FormatOrientation(val);
            } else if (tag == (u16)ExifProp::ResolutionUnit) {
                s = FormatResolutionUnit(val);
            } else if (tag == (u16)ExifProp::YCbCrPositioning) {
                s = FormatYCbCrPositioning(val);
            } else if (tag == (u16)ExifProp::Compression) {
                s = FormatCompression(val);
            }
        } else if (g == IfdGroup::Exif) {
            if (tag == (u16)ExifProp::ExposureProgram) {
                s = FormatExposureProgram(val);
            } else if (tag == (u16)ExifProp::MeteringMode) {
                s = FormatMeteringMode(val);
            } else if (tag == (u16)ExifProp::ColorSpace) {
                s = FormatColorSpace(val);
            } else if (tag == (u16)ExifProp::WhiteBalance) {
                s = FormatWhiteBalance(val);
            } else if (tag == (u16)ExifProp::ExposureMode) {
                s = FormatExposureMode(val);
            } else if (tag == (u16)ExifProp::SceneCaptureType) {
                s = FormatSceneCaptureType(val);
            } else if (tag == (u16)ExifProp::GainControl) {
                s = FormatGainControl(val);
            } else if (tag == (u16)ExifProp::Contrast || tag == (u16)ExifProp::Saturation ||
                       tag == (u16)ExifProp::Sharpness) {
                s = FormatContrastSatSharp(val);
            } else if (tag == 0xA401) {
                s = FormatCustomRendered(val);
            } else if (tag == (u16)ExifProp::SensitivityType) {
                s = FormatSensitivityType(val);
            } else if (tag == (u16)ExifProp::Flash) {
                s = FormatFlash(val);
            }
        }
        if (s) {
            return s;
        }
    }

    if (type == TiffAscii) {
        return TrimmedAsciiTemp(r, off, count);
    }

    if (type == TiffUndefined) {
        if (tag == (u16)ExifProp::ComponentsConfiguration) {
            return FormatComponentsConfig(r, off, count);
        }
        if (tag == (u16)ExifProp::FileSource && count >= 1) {
            Str s = FormatFileSource(r.Byte(off));
            if (s) {
                return s;
            }
        }
        if (tag == (u16)ExifProp::SceneType && count >= 1) {
            Str s = FormatSceneType(r.Byte(off));
            if (s) {
                return s;
            }
        }
        if (tag == (u16)ExifProp::UserComment && count > 8 && memcmp(r.d + off, "ASCII\0\0\0", 8) == 0) {
            return TrimmedAsciiTemp(r, off + 8, count - 8);
        }
        return FormatUndefinedBytesTemp(r, off, count, true);
    }

    if (type == TiffRational || type == TiffSRational) {
        str::Builder s;
        bool sr = type == TiffSRational;
        for (u32 i = 0; i < count; i++) {
            int eoff = off + (int)i * 8;
            ExifRational rat;
            if (!ReadRational(parser, eoff, &rat, sr)) {
                break;
            }
            if (i > 0) {
                s.Append(", ");
            }
            bool gpsDms = g == IfdGroup::Gps && (tag == 0x0002 || tag == 0x0004) && count == 3;
            if (gpsDms || (count > 1 && tag == (u16)ExifProp::LensSpecification)) {
                if (i == 0) {
                    s.Append("[");
                }
                if (sr) {
                    s.Append(fmt("%d", rat.num));
                    if (rat.den != 1) {
                        s.Append(fmt("/%d", rat.den));
                    }
                } else {
                    s.Append(FormatRationalPair((u32)rat.num, (u32)rat.den, true));
                }
                if (i < count - 1) {
                    s.Append(", ");
                } else {
                    s.Append("]");
                }
            } else if (sr) {
                s.Append(fmt("%d", rat.num));
                if (rat.den != 0 && rat.den != 1) {
                    s.Append(fmt("/%d", rat.den));
                }
            } else {
                bool frac = tag == (u16)ExifProp::ExposureTime || tag == 0x9201;
                s.Append(FormatRationalPair((u32)rat.num, (u32)rat.den, frac));
            }
        }
        return ToStrTemp(s);
    }

    if (type == TiffShort || type == TiffSShort) {
        str::Builder s;
        for (u32 i = 0; i < count; i++) {
            int eoff = off + (int)i * 2;
            if (i > 0) {
                s.Append(", ");
            }
            if (type == TiffSShort) {
                s.Append(fmt("%d", (i16)ReadWord(parser, eoff)));
            } else {
                u32 v = ReadWord(parser, eoff);
                if (g == IfdGroup::Exif && tag == (u16)ExifProp::Flash && count == 1) {
                    Str fs = FormatFlash(v);
                    if (fs) {
                        return fs;
                    }
                }
                s.Append(fmt("%u", v));
            }
        }
        if (count > 1 && tag == 0x9214) {
            return fmt("[%s]", ToStr(s));
        }
        return ToStrTemp(s);
    }

    if (type == TiffLong || type == TiffSLong) {
        str::Builder s;
        for (u32 i = 0; i < count; i++) {
            int eoff = off + (int)i * 4;
            if (i > 0) {
                s.Append(", ");
            }
            if (type == TiffSLong) {
                s.Append(fmt("%d", (i32)ReadDWord(parser, eoff)));
            } else {
                s.Append(fmt("%u", ReadDWord(parser, eoff)));
            }
        }
        return ToStrTemp(s);
    }

    if (type == TiffByte || type == TiffSByte) {
        str::Builder s;
        s.Append("[");
        for (u32 i = 0; i < count; i++) {
            if (i > 0) {
                s.Append(", ");
            }
            s.Append(fmt("%u", r.Byte(off + i)));
        }
        s.Append("]");
        return ToStrTemp(s);
    }

    return str::DupTemp("");
}

static void AddEntry(ExifParser& parser, IfdGroup group, u16 tag, u16 type, u32 count, int dataOff) {
    ExifEntry entry;
    entry.tag = tag;
    entry.type = type;
    entry.count = count;
    entry.dataOff = dataOff;
    entry.group = (u8)group;
    parser.entries.Append(entry);
}

static void ParseIfd(ExifParser& parser, IfdGroup group, int ifdRel, int makerNoteEndian = 0);

static void ParseMakerNote(ExifParser& parser, int dataOff, u32 count) {
    ByteReader r(parser.exifBlob);
    if (dataOff >= r.len || count < 6) {
        return;
    }
    TempStr val = FormatUndefinedBytesTemp(r, dataOff, count, true);
    AppendLine(parser, IfdGroup::Exif, (u16)ExifProp::MakerNote, TiffUndefined, val);

    int mnBase = dataOff;
    int mnOff = 8;
    int makerNoteEndian = 0;
    if (count > 10 && (r.Byte(dataOff) == 'I' || r.Byte(dataOff) == 'M')) {
        mnOff = 0;
        makerNoteEndian = r.Byte(dataOff);
    } else if (count > 10 && r.Byte(dataOff + 6) == 'I' && r.Byte(dataOff + 7) == 'I') {
        mnOff = 8;
        makerNoteEndian = 'I';
    } else if (count > 10 && r.Byte(dataOff + 6) == 'M' && r.Byte(dataOff + 7) == 'M') {
        mnOff = 8;
        makerNoteEndian = 'M';
    }
    if ((u32)mnOff + 4 > count) {
        return;
    }
    bool savedBE = parser.isBE;
    int savedBase = parser.tiffBase;
    parser.isBE = makerNoteEndian == 'M';
    int ifdRel = (int)ReadDWord(parser, dataOff + mnOff);
    parser.tiffBase = mnBase;
    ParseIfd(parser, IfdGroup::MakerNote, ifdRel, makerNoteEndian);
    parser.tiffBase = savedBase;
    parser.isBE = savedBE;
}

static void ParseIfd(ExifParser& parser, IfdGroup group, int ifdRel, int makerNoteEndian) {
    ByteReader r(parser.exifBlob);
    int ifdAbs = parser.tiffBase + ifdRel;
    if (ifdAbs + 2 > r.len) {
        return;
    }
    bool savedBE = parser.isBE;
    if (makerNoteEndian) {
        parser.isBE = makerNoteEndian == 'M';
    }
    u16 nTags = ReadWord(parser, ifdAbs);
    int nextIfdOff = 0;
    for (u16 i = 0; i < nTags; i++) {
        int ent = ifdAbs + 2 + i * 12;
        if (ent + 12 > r.len) {
            break;
        }
        u16 tag = ReadWord(parser, ent);
        u16 type = ReadWord(parser, ent + 2);
        u32 count = ReadDWord(parser, ent + 4);
        u32 inlineVal = ReadDWord(parser, ent + 8);
        if (count > 10'000) {
            continue;
        }
        int elemSize = TypeElemSize(type);
        int dataOff = ValueOffset(parser, type, count, inlineVal, ent);
        i64 totalSize = (i64)elemSize * count;
        if (elemSize == 0 || totalSize > INT_MAX ||
            !EntryBoundsOk(parser, {tag, type, count, dataOff, (u8)group}, (int)totalSize)) {
            continue;
        }
        AddEntry(parser, group, tag, type, count, dataOff);

        if (group == IfdGroup::Image && tag == 0x0201) {
            parser.hasJpegThumbnail = true;
        }

        if (group == IfdGroup::Image || group == IfdGroup::Exif) {
            if (tag == (u16)ExifProp::ExifOffset && type == TiffLong && count >= 1) {
                u32 rel = ReadDWord(parser, dataOff);
                ParseIfd(parser, IfdGroup::Exif, (int)rel);
                continue;
            }
            if (tag == (u16)ExifProp::GpsInfo && type == TiffLong && count >= 1) {
                u32 rel = ReadDWord(parser, dataOff);
                ParseIfd(parser, IfdGroup::Gps, (int)rel);
                continue;
            }
            if (tag == (u16)ExifProp::InteroperabilityOffset && type == TiffLong && count >= 1) {
                u32 rel = ReadDWord(parser, dataOff);
                ParseIfd(parser, IfdGroup::Interop, (int)rel);
                continue;
            }
        }

        if (group == IfdGroup::Exif && tag == (u16)ExifProp::MakerNote) {
            ParseMakerNote(parser, dataOff, count);
            continue;
        }

        TempStr val = FormatValuesTemp(parser, group, tag, type, count, dataOff);
        AppendLine(parser, group, tag, type, val);
    }

    if (ifdAbs + 2 + nTags * 12 + 4 <= r.len) {
        nextIfdOff = (int)ReadDWord(parser, ifdAbs + 2 + nTags * 12);
    }
    parser.isBE = savedBE;

    if (group == IfdGroup::Image && nextIfdOff != 0) {
        ParseIfd(parser, IfdGroup::Thumbnail, nextIfdOff);
    }
}

static bool ParseTiff(ExifParser& parser) {
    ByteReader r(parser.exifBlob);
    if (r.len < 8) {
        return false;
    }
    char b0 = (char)r.Byte(0);
    char b1 = (char)r.Byte(1);
    if ((b0 == 'I' && b1 == 'I') || (b0 == 'M' && b1 == 'M')) {
        parser.isBE = b0 == 'M';
        parser.tiffBase = 0;
    } else if (r.len >= 6 && memcmp(r.d, "Exif\0\0", 6) == 0) {
        parser.tiffBase = 6;
        parser.isBE = r.Byte(parser.tiffBase) == 'M';
    } else {
        return false;
    }
    if (parser.tiffBase + 8 > r.len) {
        return false;
    }
    u32 ifd0 = ReadDWord(parser, parser.tiffBase + 4);
    ParseIfd(parser, IfdGroup::Image, (int)ifd0);
    return len(parser.entries) > 0 || len(parser.dumpLines) > 0;
}

static bool ExtractJpegExif(Str d, Str& out) {
    ByteReader r(d);
    if (r.len < 4 || r.Byte(0) != 0xFF || r.Byte(1) != 0xD8) {
        return false;
    }
    int idx = 2;
    for (;;) {
        if (idx + 4 > r.len || r.Byte(idx) != 0xFF) {
            return false;
        }
        u8 marker = r.Byte(idx + 1);
        if (marker == 0xDA) {
            return false;
        }
        int segLen = r.WordBE(idx + 2);
        if (marker == 0xE1 && idx + 10 <= r.len && memcmp(r.d + idx + 4, "Exif\0\0", 6) == 0) {
            int payload = idx + 4;
            int total = segLen + 2;
            if (payload + total - 4 > r.len) {
                return false;
            }
            out = Str((char*)(r.d + payload), total - 4);
            return true;
        }
        int next = idx + segLen + 2;
        if (next <= idx) {
            return false;
        }
        idx = next;
    }
}

static bool HasWebpSignature(Str d) {
    return d.len >= 12 && memeq(d.s, "RIFF", 4) && memeq(d.s + 8, "WEBP", 4);
}

static bool ExtractWebpExif(Str d, Str& out) {
    if (!HasWebpSignature(d)) {
        return false;
    }
    ByteReader r(d);
    int idx = 12;
    while (idx + 8 <= r.len) {
        if (r.Byte(idx) == 'E' && r.Byte(idx + 1) == 'X' && r.Byte(idx + 2) == 'I' && r.Byte(idx + 3) == 'F') {
            int size = (int)r.DWordLE(idx + 4);
            int payload = idx + 8;
            if (payload + size <= r.len && size >= 8) {
                out = Str((char*)(r.d + payload), size);
                return true;
            }
        }
        int size = (int)r.DWordLE(idx + 4);
        int chunkSize = size + (size & 1);
        if (chunkSize < size) {
            return false;
        }
        idx += 8 + chunkSize;
        if (idx < 8) {
            return false;
        }
    }
    return false;
}

static bool LooksLikeTiffExif(const u8* p, int n) {
    if (n < 12) {
        return false;
    }
    bool le = p[0] == 'I' && p[1] == 'I' && p[2] == 42 && p[3] == 0;
    bool be = p[0] == 'M' && p[1] == 'M' && p[2] == 0 && p[3] == 42;
    if (!le && !be) {
        return false;
    }
    u32 ifdOff = le ? (u32)(p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24))
                    : (u32)((p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7]);
    if (ifdOff < 8 || (int)ifdOff + 2 >= n) {
        return false;
    }
    u16 nTags = le ? (u16)(p[ifdOff] | (p[ifdOff + 1] << 8)) : (u16)((p[ifdOff] << 8) | p[ifdOff + 1]);
    return nTags > 0 && nTags < 512;
}

static bool CopyTiffBlob(const u8* data, int n, int tiffOff, Str& out, u8** ownedOut) {
    int blobLen = n - tiffOff;
    constexpr int kMaxExifBytes = 256 * 1024;
    if (blobLen > kMaxExifBytes) {
        blobLen = kMaxExifBytes;
    }
    u8* copy = (u8*)malloc((size_t)blobLen);
    if (!copy) {
        return false;
    }
    memcpy(copy, data + tiffOff, (size_t)blobLen);
    *ownedOut = copy;
    out = Str((char*)(copy), blobLen);
    return true;
}

static bool ExtractHeifExifFromBytes(Str d, Str& out, u8** ownedOut) {
    *ownedOut = nullptr;
    const u8* data = (u8*)d.s;
    int n = d.len;
    if (!data || n < 16) {
        return false;
    }
    for (int i = 0; i + 12 < n; i++) {
        if (data[i] != 'E' || data[i + 1] != 'x' || data[i + 2] != 'i' || data[i + 3] != 'f') {
            continue;
        }
        int tiffOff = i + 4;
        if (tiffOff + 6 >= n || data[tiffOff] != 0 || data[tiffOff + 1] != 0) {
            continue;
        }
        tiffOff += 4;
        if (!LooksLikeTiffExif(data + tiffOff, n - tiffOff)) {
            continue;
        }
        int blobLen = n - tiffOff;
        if (i >= 4) {
            u32 boxSize =
                ((u32)data[i - 4] << 24) | ((u32)data[i - 3] << 16) | ((u32)data[i - 2] << 8) | (u32)data[i - 1];
            if (boxSize >= 8 && (i - 4) + (int)boxSize <= n) {
                int boxEnd = (i - 4) + (int)boxSize;
                if (boxEnd > tiffOff) {
                    blobLen = boxEnd - tiffOff;
                }
            }
        }
        u8* copy = (u8*)malloc((size_t)blobLen);
        if (!copy) {
            return false;
        }
        memcpy(copy, data + tiffOff, (size_t)blobLen);
        *ownedOut = copy;
        out = Str((char*)(copy), blobLen);
        return true;
    }
    for (int i = 0; i + 8 < n; i++) {
        if (LooksLikeTiffExif(data + i, n - i)) {
            return CopyTiffBlob(data, n, i, out, ownedOut);
        }
    }
    return false;
}

static bool ExtractExifBlob(Str d, Str& out, u8** ownedOut) {
    *ownedOut = nullptr;
    FileType kind = GuessFileTypeFromData(d);
    if (kind == FileType::Jpeg) {
        return ExtractJpegExif(d, out);
    }
    if (kind == FileType::Tiff) {
        out = d;
        return true;
    }
    if (kind == FileType::Webp) {
        return ExtractWebpExif(d, out);
    }
    if (kind == FileType::Heic || kind == FileType::Avif) {
        if (ExtractHeifExifFromBytes(d, out, ownedOut)) {
            return true;
        }
    }
    if (d.len >= 4) {
        const u8* p = (u8*)d.s;
        if ((p[0] == 'I' && p[1] == 'I') || (p[0] == 'M' && p[1] == 'M')) {
            out = d;
            return true;
        }
    }
    return false;
}

} // namespace

ExifParser::~ExifParser() {
    Reset();
}

void ExifParser::Reset() {
    dumpLines.Reset();
    entries.Reset();
    free(ownedExif);
    ownedExif = nullptr;
    data = {};
    exifBlob = {};
    isBE = false;
    tiffBase = 0;
    hasJpegThumbnail = false;
}

bool ExifParser::Parse(Str imageData) {
    Reset();
    data = imageData;
    if (!ExtractExifBlob(imageData, exifBlob, &ownedExif)) {
        return false;
    }
    return ParseTiff(*this);
}

bool ExifParser::HasProp(ExifProp prop) const {
    return FindEntry(*this, prop) != nullptr;
}

ExifValueKind ExifParser::GetPropKind(ExifProp prop) const {
    const ExifEntry* entry = FindEntry(*this, prop);
    if (!entry) {
        return ExifValueKind::Unknown;
    }
    if (entry->type == TiffAscii || IsXpProp(prop) || IsAsciiUndefinedProp(prop) || prop == ExifProp::UserComment) {
        return ExifValueKind::String;
    }
    if (entry->type == TiffShort || entry->type == TiffLong || entry->type == TiffSShort || entry->type == TiffSLong) {
        return ExifValueKind::Int;
    }
    if (entry->type == TiffRational || entry->type == TiffSRational) {
        return ExifValueKind::Rational;
    }
    return ExifValueKind::Bytes;
}

TempStr ExifParser::GetStringProp(ExifProp prop, ExifProp altProp) const {
    const ExifEntry* entry = FindEntry(*this, prop);
    if (!entry) {
        return altProp == ExifProp::None ? nullptr : GetStringProp(altProp);
    }
    ByteReader r(exifBlob);
    if (!EntryBoundsOk(*this, *entry, TypeElemSize(entry->type) * (int)entry->count)) {
        return altProp == ExifProp::None ? nullptr : GetStringProp(altProp);
    }
    if (IsXpProp(prop)) {
        TempStr res = Utf16LeToUtf8Temp(Str((char*)(r.d + entry->dataOff), (int)entry->count));
        return str::IsEmptyOrWhiteSpace(res) && altProp != ExifProp::None ? GetStringProp(altProp) : res;
    }
    TempStr res = nullptr;
    if (entry->type == TiffAscii) {
        res = TrimmedAsciiTemp(r, entry->dataOff, entry->count);
    } else if (entry->type == TiffUndefined && IsAsciiUndefinedProp(prop)) {
        res = TrimmedAsciiTemp(r, entry->dataOff, entry->count);
    } else if (prop == ExifProp::UserComment && entry->type == TiffUndefined && EntryBoundsOk(*this, *entry, 8)) {
        Str bytes((char*)(r.d + entry->dataOff), (int)entry->count);
        if (bytes.len > 8 && memeq(bytes.s, "ASCII\0\0\0", 8)) {
            res = TrimmedAsciiTemp(r, entry->dataOff + 8, entry->count - 8);
        } else if (bytes.len > 10 && memeq(bytes.s, "UNICODE\0", 8)) {
            res = Utf16LeToUtf8Temp(Str(bytes.s + 8, bytes.len - 8));
        }
    }
    if (str::IsEmptyOrWhiteSpace(res)) {
        return altProp == ExifProp::None ? nullptr : GetStringProp(altProp);
    }
    return res;
}

bool ExifParser::GetIntProp(ExifProp prop, i64* valOut) const {
    const ExifEntry* entry = FindEntry(*this, prop);
    if (!entry || !valOut) {
        return false;
    }
    if (entry->type == TiffByte || entry->type == TiffSByte || entry->type == TiffUndefined) {
        if (!EntryBoundsOk(*this, *entry, 1)) {
            return false;
        }
        u8 v = ByteReader(exifBlob).Byte(entry->dataOff);
        *valOut = entry->type == TiffSByte ? (i8)v : v;
        return true;
    }
    if (entry->type == TiffShort || entry->type == TiffSShort) {
        if (!EntryBoundsOk(*this, *entry, 2)) {
            return false;
        }
        u16 v = ReadWord(*this, entry->dataOff);
        *valOut = entry->type == TiffSShort ? (i16)v : v;
        return true;
    }
    if (entry->type == TiffLong || entry->type == TiffSLong) {
        if (!EntryBoundsOk(*this, *entry, 4)) {
            return false;
        }
        u32 v = ReadDWord(*this, entry->dataOff);
        *valOut = entry->type == TiffSLong ? (i32)v : v;
        return true;
    }
    return false;
}

bool ExifParser::GetRationalProp(ExifProp prop, ExifRational* valOut) const {
    const ExifEntry* entry = FindEntry(*this, prop);
    if (!entry || !valOut || (entry->type != TiffRational && entry->type != TiffSRational)) {
        return false;
    }
    return ReadRational(*this, entry->dataOff, valOut, entry->type == TiffSRational);
}

bool ExifParser::GetFloatProp(ExifProp prop, double* valOut) const {
    if (!valOut) {
        return false;
    }
    ExifRational rat;
    if (GetRationalProp(prop, &rat)) {
        if (rat.den == 0) {
            return false;
        }
        *valOut = (double)rat.num / (double)rat.den;
        return true;
    }
    i64 val;
    if (GetIntProp(prop, &val)) {
        *valOut = (double)val;
        return true;
    }
    return false;
}

TempStr ExifParser::GetFormattedPropTemp(ExifProp prop) const {
    const ExifEntry* entry = FindEntry(*this, prop);
    if (!entry) {
        return nullptr;
    }
    return FormatValuesTemp(*this, (IfdGroup)entry->group, entry->tag, entry->type, entry->count, entry->dataOff);
}

void ExifParser::GetDumpLines(StrVec& linesOut) const {
    for (Str line : dumpLines) {
        linesOut.Append(line);
    }
}
