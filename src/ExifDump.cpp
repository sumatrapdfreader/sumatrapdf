/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ByteReader.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/WebpReader.h"
#include "base/Win.h"

#include "Settings.h"
#include "Flags.h"

using namespace Gdiplus;

namespace {

enum TiffType : u16 {
    Byte = 1,
    Ascii = 2,
    Short = 3,
    Long = 4,
    Rational = 5,
    SByte = 6,
    Undefined = 7,
    SShort = 8,
    SLong = 9,
    SRational = 10,
};

enum class IfdGroup {
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

// IFD0 / image tags
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

// EXIF sub-IFD tags
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
        case TiffType::Byte:
            return StrL("Byte");
        case TiffType::Ascii:
            return StrL("ASCII");
        case TiffType::Short:
            return StrL("Short");
        case TiffType::Long:
            return StrL("Long");
        case TiffType::Rational:
            return StrL("Ratio");
        case TiffType::SByte:
            return StrL("Signed Byte");
        case TiffType::Undefined:
            return StrL("Undefined");
        case TiffType::SShort:
            return StrL("Signed Short");
        case TiffType::SLong:
            return StrL("Signed Long");
        case TiffType::SRational:
            return StrL("Signed Ratio");
        default:
            return StrL("Unknown");
    }
}

static Str LookupEnum(const Str* names, int n, u32 val) {
    if (val < (u32)n) {
        return names[val];
    }
    return {};
}

static Str FormatOrientation(u32 val) {
    static const Str names[] = {
        StrL(""), // 0 unused
        StrL("Horizontal (normal)"),
        StrL("Mirrored horizontal"),
        StrL("Rotated 180"),
        StrL("Mirrored vertical"),
        StrL("Mirrored horizontal then rotated 90 CCW"),
        StrL("Rotated 90 CW"),
        StrL("Mirrored horizontal then rotated 90 CW"),
        StrL("Rotated 90 CCW"),
    };
    return LookupEnum(names, dimof(names), val);
}

static Str FormatExposureProgram(u32 val) {
    static const Str names[] = {
        StrL("Unidentified"),      StrL("Manual"),           StrL("Program Normal"),
        StrL("Aperture Priority"), StrL("Shutter Priority"), StrL("Program Creative"),
        StrL("Program Action"),    StrL("Portrait Mode"),    StrL("Landscape Mode"),
    };
    return LookupEnum(names, dimof(names), val);
}

static Str FormatMeteringMode(u32 val) {
    static const Str names[] = {
        StrL("Unidentified"), StrL("Average"), StrL("CenterWeightedAverage"), StrL("Spot"), StrL("MultiSpot"),
        StrL("Pattern"),      StrL("Partial"),
    };
    return LookupEnum(names, dimof(names), val);
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
    static const Str names[] = {StrL("Auto Exposure"), StrL("Manual Exposure"), StrL("Auto Bracket")};
    return LookupEnum(names, dimof(names), val);
}

static Str FormatSceneCaptureType(u32 val) {
    static const Str names[] = {StrL("Standard"), StrL("Landscape"), StrL("Portrait"), StrL("Night")};
    return LookupEnum(names, dimof(names), val);
}

static Str FormatGainControl(u32 val) {
    static const Str names[] = {StrL("None"), StrL("Low gain up"), StrL("High gain up"), StrL("Low gain down"),
                                StrL("High gain down")};
    return LookupEnum(names, dimof(names), val);
}

static Str FormatContrastSatSharp(u32 val) {
    static const Str names[] = {StrL("Normal"), StrL("Soft"), StrL("Hard")};
    return LookupEnum(names, dimof(names), val);
}

static Str FormatCustomRendered(u32 val) {
    return val == 0 ? StrL("Normal") : StrL("Custom");
}

static Str FormatSensitivityType(u32 val) {
    static const Str names[] = {
        StrL("Unknown"),
        StrL("Standard Output Sensitivity"),
        StrL("Recommended Exposure Index"),
        StrL("ISO Speed"),
        StrL("Standard Output Sensitivity and Recommended Exposure Index"),
        StrL("Standard Output Sensitivity and ISO Speed"),
        StrL("Recommended Exposure Index and ISO Speed"),
        StrL("Standard Output Sensitivity, Recommended Exposure Index and ISO Speed"),
    };
    return LookupEnum(names, dimof(names), val);
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
    if (val == 3) {
        return StrL("Digital Camera");
    }
    return {};
}

static Str FormatSceneType(u8 val) {
    if (val == 1) {
        return StrL("Directly Photographed");
    }
    return {};
}

// Flash values per exif-py
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

struct TiffParser {
    ByteReader r;
    explicit TiffParser(ByteReader reader) : r(reader) {}
    bool isBE = false;
    size_t tiffBase = 0;
    StrVec lines;
    bool hasJpegThumbnail = false;
    int makerNoteEndian = 0; // 0=use main, 'I' or 'M' for makernote

    const TagDef* TagsForGroup(IfdGroup g) const {
        switch (g) {
            case IfdGroup::Image:
            case IfdGroup::Thumbnail:
                return kImageTags;
            case IfdGroup::Exif:
                return kExifTags;
            case IfdGroup::Gps:
                return kGpsTags;
            case IfdGroup::Interop:
                return kInteropTags;
            case IfdGroup::MakerNote:
                return nullptr;
        }
        return nullptr;
    }

    int TagCountForGroup(IfdGroup g) const {
        switch (g) {
            case IfdGroup::Image:
            case IfdGroup::Thumbnail:
                return dimof(kImageTags);
            case IfdGroup::Exif:
                return dimof(kExifTags);
            case IfdGroup::Gps:
                return dimof(kGpsTags);
            case IfdGroup::Interop:
                return dimof(kInteropTags);
            case IfdGroup::MakerNote:
                return 0;
        }
        return 0;
    }

    Str TagName(IfdGroup g, u16 tag) const {
        const TagDef* tags = TagsForGroup(g);
        int n = TagCountForGroup(g);
        if (tags) {
            return LookupTagName(tags, n, tag);
        }
        return {};
    }

    u16 ReadWord(size_t off) const { return r.Word(off, isBE); }

    u32 ReadDWord(size_t off) const { return r.DWord(off, isBE); }

    i32 ReadSWord(size_t off) const { return (i32)ReadWord(off); }

    i32 ReadSDWord(size_t off) const { return (i32)ReadDWord(off); }

    bool ReadRational(size_t off, u32& num, u32& den) const {
        if (off + 8 > r.len) {
            return false;
        }
        num = ReadDWord(off);
        den = ReadDWord(off + 4);
        return true;
    }

    bool ReadSRational(size_t off, i32& num, i32& den) const {
        if (off + 8 > r.len) {
            return false;
        }
        num = ReadSDWord(off);
        den = ReadSDWord(off + 4);
        return true;
    }

    size_t ValueOffset(u16 type, u32 count, u32 inlineVal, size_t entryOff) const {
        size_t elemSize = 1;
        switch (type) {
            case TiffType::Byte:
            case TiffType::Ascii:
            case TiffType::SByte:
            case TiffType::Undefined:
                elemSize = 1;
                break;
            case TiffType::Short:
            case TiffType::SShort:
                elemSize = 2;
                break;
            case TiffType::Long:
            case TiffType::SLong:
            case TiffType::Rational:
            case TiffType::SRational:
                elemSize = 4;
                break;
            default:
                return 0;
        }
        if (type == TiffType::Rational || type == TiffType::SRational) {
            elemSize = 8;
        }
        size_t total = (size_t)count * elemSize;
        // TIFF stores values that fit in 4 bytes inside the directory entry itself.
        if (total <= 4) {
            return entryOff + 8;
        }
        return (size_t)inlineVal + tiffBase;
    }

    void AppendLine(IfdGroup g, u16 tag, u16 type, TempStr value) {
        Str prefix = GroupPrefix(g);
        Str name = TagName(g, tag);
        char tagNameBuf[32];
        if (!name) {
            snprintf(tagNameBuf, sizeof(tagNameBuf), "Tag 0x%04X", tag);
            name = Str(tagNameBuf);
        }
        TempStr line = fmt("%s %s (%s): %s", prefix, name, TypeName(type), value);
        lines.Append(str::Dup(line));
    }

    TempStr FormatAscii(size_t off, u32 count) const {
        if (off >= r.len) {
            return str::DupTemp("");
        }
        size_t n = count;
        if (off + n > r.len) {
            n = r.len - off;
        }
        // trim trailing nulls
        while (n > 0 && r.Byte(off + n - 1) == 0) {
            n--;
        }
        return str::DupTemp(Str((char*)(r.d + off), (int)(n)));
    }

    TempStr FormatRationalPair(u32 num, u32 den, bool asFraction) const {
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

    TempStr FormatComponentsConfig(size_t off, u32 count) const {
        static const Str compNames[] = {StrL(""), StrL("Y"), StrL("Cb"), StrL("Cr"), StrL("R"), StrL("G"), StrL("B")};
        str::Builder s;
        for (u32 i = 0; i < count && off + i < r.len; i++) {
            u8 c = r.Byte(off + i);
            if (c == 0) {
                break;
            }
            if (len(s) > 0) {
                s.Append(", ");
            }
            if (c < dimof(compNames) && compNames[c]) {
                s.Append(compNames[c]);
            }
        }
        if (len(s) == 0) {
            return str::DupTemp("YCbCr");
        }
        return ToStrTemp(s);
    }

    TempStr FormatUndefinedBytesTemp(size_t off, u32 count, bool asList) const {
        if (count == 0) {
            return str::DupTemp("");
        }
        if (off + count > r.len) {
            count = (u32)(r.len - off);
        }
        // short ASCII in undefined
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
            return FormatAscii(off, count);
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

    TempStr FormatValuesTemp(IfdGroup g, u16 tag, u16 type, u32 count, size_t off) const {
        if (count == 0) {
            return str::DupTemp("");
        }

        // enum overrides
        if (type == TiffType::Short && count == 1) {
            u32 val = off + 2 <= r.len ? ReadWord(off) : 0;
            Str s;
            if (g == IfdGroup::Image || g == IfdGroup::Thumbnail) {
                if (tag == 0x0112) {
                    s = FormatOrientation(val);
                } else if (tag == 0x0128) {
                    s = FormatResolutionUnit(val);
                } else if (tag == 0x0213) {
                    s = FormatYCbCrPositioning(val);
                } else if (tag == 0x0103) {
                    s = FormatCompression(val);
                }
            } else if (g == IfdGroup::Exif) {
                if (tag == 0x8822) {
                    s = FormatExposureProgram(val);
                } else if (tag == 0x9207) {
                    s = FormatMeteringMode(val);
                } else if (tag == 0xA001) {
                    s = FormatColorSpace(val);
                } else if (tag == 0xA403) {
                    s = FormatWhiteBalance(val);
                } else if (tag == 0xA402) {
                    s = FormatExposureMode(val);
                } else if (tag == 0xA406) {
                    s = FormatSceneCaptureType(val);
                } else if (tag == 0xA407) {
                    s = FormatGainControl(val);
                } else if (tag == 0xA408 || tag == 0xA409 || tag == 0xA40A) {
                    s = FormatContrastSatSharp(val);
                } else if (tag == 0xA401) {
                    s = FormatCustomRendered(val);
                } else if (tag == 0x8830) {
                    s = FormatSensitivityType(val);
                } else if (tag == 0x9208) {
                    s = FormatFlash(val);
                }
            }
            if (s) {
                return s;
            }
        }

        if (type == TiffType::Ascii) {
            return FormatAscii(off, count);
        }

        if (type == TiffType::Undefined) {
            if (tag == 0x9101) {
                return FormatComponentsConfig(off, count);
            }
            if (tag == 0xA300 && count >= 1) {
                Str s = FormatFileSource(r.Byte(off));
                if (s) {
                    return s;
                }
            }
            if (tag == 0xA301 && count >= 1) {
                Str s = FormatSceneType(r.Byte(off));
                if (s) {
                    return s;
                }
            }
            if (tag == 0x9286 && count > 8) {
                // UserComment
                if (memcmp(r.d + off, "ASCII\0\0\0", 8) == 0) {
                    return FormatAscii(off + 8, count - 8);
                }
            }
            return FormatUndefinedBytesTemp(off, count, true);
        }

        if (type == TiffType::Rational || type == TiffType::SRational) {
            str::Builder s;
            bool sr = type == TiffType::SRational;
            for (u32 i = 0; i < count; i++) {
                size_t eoff = off + (size_t)i * 8;
                u32 num, den;
                i32 snum, sden;
                if (sr) {
                    if (!ReadSRational(eoff, snum, sden)) {
                        break;
                    }
                    if (i > 0) {
                        s.Append(", ");
                    }
                    if (g == IfdGroup::Gps && (tag == 0x0002 || tag == 0x0004) && count == 3) {
                        // GPS lat/lon as DMS array
                        if (i == 0) {
                            s.Append("[");
                        }
                        s.Append(fmt("%d", snum));
                        if (sden != 1) {
                            s.Append(fmt("/%d", sden));
                        }
                        if (i < count - 1) {
                            s.Append(", ");
                        } else {
                            s.Append("]");
                        }
                    } else {
                        s.Append(fmt("%d", snum));
                        if (sden != 0 && sden != 1) {
                            s.Append(fmt("/%d", sden));
                        }
                    }
                } else {
                    if (!ReadRational(eoff, num, den)) {
                        break;
                    }
                    if (i > 0) {
                        s.Append(", ");
                    }
                    bool frac = (tag == 0x829A || tag == 0x9201); // exposure/shutter as fraction
                    if (g == IfdGroup::Gps && (tag == 0x0002 || tag == 0x0004) && count == 3) {
                        if (i == 0) {
                            s.Append("[");
                        }
                        TempStr part = FormatRationalPair(num, den, true);
                        s.Append(part);
                        if (i < count - 1) {
                            s.Append(", ");
                        } else {
                            s.Append("]");
                        }
                    } else if (count > 1 && tag == 0xA432) {
                        if (i == 0) {
                            s.Append("[");
                        }
                        s.Append(FormatRationalPair(num, den, true));
                        if (i < count - 1) {
                            s.Append(", ");
                        } else {
                            s.Append("]");
                        }
                    } else {
                        s.Append(FormatRationalPair(num, den, frac));
                    }
                }
            }
            return ToStrTemp(s);
        }

        if (type == TiffType::Short || type == TiffType::SShort) {
            str::Builder s;
            for (u32 i = 0; i < count; i++) {
                size_t eoff = off + (size_t)i * 2;
                if (i > 0) {
                    s.Append(", ");
                }
                if (type == TiffType::SShort) {
                    s.Append(fmt("%d", (i16)ReadWord(eoff)));
                } else {
                    u32 v = ReadWord(eoff);
                    if (g == IfdGroup::Exif && tag == 0x9208 && count == 1) {
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

        if (type == TiffType::Long || type == TiffType::SLong) {
            str::Builder s;
            for (u32 i = 0; i < count; i++) {
                size_t eoff = off + (size_t)i * 4;
                if (i > 0) {
                    s.Append(", ");
                }
                if (type == TiffType::SLong) {
                    s.Append(fmt("%d", ReadSDWord(eoff)));
                } else {
                    s.Append(fmt("%u", ReadDWord(eoff)));
                }
            }
            return ToStrTemp(s);
        }

        if (type == TiffType::Byte || type == TiffType::SByte) {
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

    void ParseIfd(IfdGroup group, size_t ifdRel, bool mnEndian = false) {
        size_t ifdAbs = tiffBase + ifdRel;
        if (ifdAbs + 2 > r.len) {
            return;
        }
        bool savedBE = isBE;
        if (mnEndian) {
            isBE = makerNoteEndian == 'M';
        }
        u16 nTags = ReadWord(ifdAbs);
        size_t nextIfdOff = 0;
        for (u16 i = 0; i < nTags; i++) {
            size_t ent = ifdAbs + 2 + (size_t)i * 12;
            if (ent + 12 > r.len) {
                break;
            }
            u16 tag = ReadWord(ent);
            u16 type = ReadWord(ent + 2);
            u32 count = ReadDWord(ent + 4);
            u32 inlineVal = ReadDWord(ent + 8);
            if (count > 10'000) {
                continue;
            }

            if (group == IfdGroup::Image && tag == 0x0201) {
                hasJpegThumbnail = true;
            }

            // sub-IFD pointers
            if (group == IfdGroup::Image || group == IfdGroup::Exif) {
                if (tag == 0x8769 && type == TiffType::Long && count >= 1) {
                    size_t subOff = ValueOffset(type, count, inlineVal, ent);
                    u32 rel = ReadDWord(subOff);
                    ParseIfd(IfdGroup::Exif, rel);
                    continue;
                }
                if (tag == 0x8825 && type == TiffType::Long && count >= 1) {
                    size_t subOff = ValueOffset(type, count, inlineVal, ent);
                    u32 rel = ReadDWord(subOff);
                    ParseIfd(IfdGroup::Gps, rel);
                    continue;
                }
                if (tag == 0xA005 && type == TiffType::Long && count >= 1) {
                    size_t subOff = ValueOffset(type, count, inlineVal, ent);
                    u32 rel = ReadDWord(subOff);
                    ParseIfd(IfdGroup::Interop, rel);
                    continue;
                }
            }

            if (group == IfdGroup::Exif && tag == 0x927C) {
                size_t dataOff = ValueOffset(type, count, inlineVal, ent);
                ParseMakerNote(dataOff, count);
                continue;
            }

            if (tag == 0x8769 || tag == 0x8825 || tag == 0xA005) {
                // pointer tags - still emit
                size_t dataOff = ValueOffset(type, count, inlineVal, ent);
                TempStr val = FormatValuesTemp(group, tag, type, count, dataOff);
                AppendLine(group, tag, type, val);
                continue;
            }

            size_t dataOff = ValueOffset(type, count, inlineVal, ent);
            TempStr val = FormatValuesTemp(group, tag, type, count, dataOff);
            AppendLine(group, tag, type, val);
        }

        if (ifdAbs + 2 + (size_t)nTags * 12 + 4 <= r.len) {
            nextIfdOff = ReadDWord(ifdAbs + 2 + (size_t)nTags * 12);
        }
        isBE = savedBE;

        if (group == IfdGroup::Image && nextIfdOff != 0) {
            ParseIfd(IfdGroup::Thumbnail, nextIfdOff);
        }
    }

    void ParseMakerNote(size_t dataOff, u32 count) {
        if (dataOff >= r.len || count < 6) {
            return;
        }
        // emit raw makernote entry
        TempStr val = FormatUndefinedBytesTemp(dataOff, count, true);
        AppendLine(IfdGroup::Exif, 0x927C, TiffType::Undefined, val);

        // Canon/Olympus TIFF-style makernote at offset 8
        size_t mnBase = dataOff;
        int mnOff = 8;
        if (count > 10 && (r.Byte(dataOff) == 'I' || r.Byte(dataOff) == 'M')) {
            mnOff = 0;
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
        bool mnBE = makerNoteEndian == 'M';
        if (mnOff == 0) {
            mnBE = r.Byte(dataOff) == 'M';
        }
        bool savedBE = isBE;
        isBE = mnBE;
        size_t ifdRel = 0;
        if (mnOff == 0) {
            ifdRel = ReadDWord(dataOff + 4);
            mnBase = dataOff;
        } else {
            ifdRel = ReadDWord(dataOff + mnOff);
            mnBase = dataOff;
        }
        // temporary tiff base shift for makernote IFD
        size_t savedBase = tiffBase;
        tiffBase = mnBase;
        ParseIfd(IfdGroup::MakerNote, ifdRel, true);
        tiffBase = savedBase;
        isBE = savedBE;
    }

    bool Parse() {
        if (r.len < 8) {
            return false;
        }
        char b0 = (char)r.Byte(0);
        char b1 = (char)r.Byte(1);
        if ((b0 == 'I' && b1 == 'I') || (b0 == 'M' && b1 == 'M')) {
            isBE = b0 == 'M';
            tiffBase = 0;
        } else if (r.len >= 6 && memcmp(r.d, "Exif\0\0", 6) == 0) {
            tiffBase = 6;
            isBE = r.Byte(tiffBase) == 'M';
        } else {
            return false;
        }
        if (tiffBase + 8 > r.len) {
            return false;
        }
        u32 ifd0 = ReadDWord(tiffBase + 4);
        ParseIfd(IfdGroup::Image, ifd0);
        return len(lines) > 0;
    }
};

static bool ExtractJpegExif(Str d, Str& out) {
    ByteReader r(d);
    if (r.len < 4 || r.Byte(0) != 0xFF || r.Byte(1) != 0xD8) {
        return false;
    }
    size_t idx = 2;
    for (;;) {
        if (idx + 4 > r.len) {
            return false;
        }
        if (r.Byte(idx) != 0xFF) {
            return false;
        }
        u8 marker = r.Byte(idx + 1);
        if (marker == 0xDA) {
            return false;
        }
        size_t segLen = (size_t)r.WordBE(idx + 2);
        if (marker == 0xE1 && idx + 10 <= r.len) {
            if (memcmp(r.d + idx + 4, "Exif\0\0", 6) == 0) {
                size_t payload = idx + 4;
                size_t total = segLen + 2;
                if (payload + total - 4 > r.len) {
                    return false;
                }
                out = Str((char*)(r.d + payload), (int)(total - 4));
                return true;
            }
        }
        size_t next = idx + segLen + 2;
        if (next <= idx) {
            return false;
        }
        idx = next;
    }
}

static bool ExtractWebpExif(Str d, Str& out) {
    if (!webp::HasSignature(d)) {
        return false;
    }
    ByteReader r(d);
    size_t idx = 12;
    while (idx + 8 <= r.len) {
        if (r.Byte(idx) == 'E' && r.Byte(idx + 1) == 'X' && r.Byte(idx + 2) == 'I' && r.Byte(idx + 3) == 'F') {
            size_t size = (size_t)r.DWordLE(idx + 4);
            size_t payload = idx + 8;
            if (payload + size <= r.len && size >= 8) {
                out = Str((char*)(r.d + payload), (int)(size));
                return true;
            }
        }
        size_t size = (size_t)r.DWordLE(idx + 4);
        size_t chunkSize = size + (size & 1);
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

static bool LooksLikeTiffExif(const u8* p, size_t n) {
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
    if (ifdOff < 8 || ifdOff + 2 >= n) {
        return false;
    }
    u16 nTags = le ? (u16)(p[ifdOff] | (p[ifdOff + 1] << 8)) : (u16)((p[ifdOff] << 8) | p[ifdOff + 1]);
    return nTags > 0 && nTags < 512;
}

static bool CopyTiffBlob(const u8* data, size_t n, size_t tiffOff, Str& out, u8** ownedOut) {
    size_t blobLen = n - tiffOff;
    constexpr size_t kMaxExifBytes = 256 * 1024;
    if (blobLen > kMaxExifBytes) {
        blobLen = kMaxExifBytes;
    }
    u8* copy = (u8*)malloc(blobLen);
    if (!copy) {
        return false;
    }
    memcpy(copy, data + tiffOff, blobLen);
    *ownedOut = copy;
    out = Str((char*)(copy), (int)(blobLen));
    return true;
}

// Scan HEIF/AVIF container for TIFF EXIF (avoids libheif metadata API hang/slowness).
static bool ExtractHeifExifFromBytes(Str d, Str& out, u8** ownedOut) {
    *ownedOut = nullptr;
    const u8* data = (u8*)d.s;
    size_t n = (size_t)d.len;
    if (!data || n < 16) {
        return false;
    }
    for (size_t i = 0; i + 12 < n; i++) {
        if (data[i] != 'E' || data[i + 1] != 'x' || data[i + 2] != 'i' || data[i + 3] != 'f') {
            continue;
        }
        size_t tiffOff = i + 4;
        if (tiffOff + 6 >= n || data[tiffOff] != 0 || data[tiffOff + 1] != 0) {
            continue;
        }
        tiffOff += 4;
        if (!LooksLikeTiffExif(data + tiffOff, n - tiffOff)) {
            continue;
        }
        size_t blobLen = n - tiffOff;
        if (i >= 4) {
            u32 boxSize =
                ((u32)data[i - 4] << 24) | ((u32)data[i - 3] << 16) | ((u32)data[i - 2] << 8) | (u32)data[i - 1];
            if (boxSize >= 8 && (size_t)(i - 4) + boxSize <= n) {
                size_t boxEnd = (i - 4) + boxSize;
                if (boxEnd > tiffOff) {
                    blobLen = boxEnd - tiffOff;
                }
            }
        }
        u8* copy = (u8*)malloc(blobLen);
        if (!copy) {
            return false;
        }
        memcpy(copy, data + tiffOff, blobLen);
        *ownedOut = copy;
        out = Str((char*)(copy), (int)(blobLen));
        return true;
    }
    for (size_t i = 0; i + 8 < n; i++) {
        if (!LooksLikeTiffExif(data + i, n - i)) {
            continue;
        }
        return CopyTiffBlob(data, n, i, out, ownedOut);
    }
    return false;
}

static bool ExtractExifBlob(Str d, Str& out, u8** ownedOut) {
    *ownedOut = nullptr;
    Kind kind = GuessFileTypeFromContent(d);
    if (kind == kindFileJpeg) {
        return ExtractJpegExif(d, out);
    }
    if (kind == kindFileTiff) {
        out = d;
        return true;
    }
    if (kind == kindFileWebp) {
        return ExtractWebpExif(d, out);
    }
    if (kind == kindFileHeic || kind == kindFileAvif) {
        if (ExtractHeifExifFromBytes(d, out, ownedOut)) {
            return true;
        }
    }
    // TIFF magic anywhere
    if ((size_t)d.len >= 4) {
        const u8* p = (u8*)d.s;
        if ((p[0] == 'I' && p[1] == 'I') || (p[0] == 'M' && p[1] == 'M')) {
            out = d;
            return true;
        }
    }
    return false;
}

static void DumpFromGdiplus(Str d, StrVec& lines) {
    IStream* strm = CreateStreamFromData(d);
    if (!strm) {
        return;
    }
    Bitmap* bmp = Bitmap::FromStream(strm);
    strm->Release();
    if (!bmp || bmp->GetLastStatus() != Ok) {
        delete bmp;
        return;
    }
    UINT total = 0, num = 0;
    if (bmp->GetPropertySize(&total, &num) != Ok || num == 0) {
        delete bmp;
        return;
    }
    PROPID* ids = AllocArray<PROPID>(num);
    if (!ids || bmp->GetPropertyIdList(num, ids) != Ok) {
        free(ids);
        delete bmp;
        return;
    }
    for (UINT i = 0; i < num; i++) {
        UINT sz = bmp->GetPropertyItemSize(ids[i]);
        if (sz == 0) {
            continue;
        }
        u8* buf = AllocArray<u8>(sz);
        if (!buf || bmp->GetPropertyItem(ids[i], sz, (PropertyItem*)buf) != Ok) {
            free(buf);
            continue;
        }
        PropertyItem* item = (PropertyItem*)buf;
        TempStr val;
        if (item->type == PropertyTagTypeASCII) {
            val = str::DupTemp(Str((char*)(item->value), (int)(item->length)));
        } else if (item->type == PropertyTagTypeShort && item->length >= 2) {
            val = fmt("%u", *(u16*)item->value);
        } else if (item->type == PropertyTagTypeLong && item->length >= 4) {
            val = fmt("%u", *(u32*)item->value);
        } else if (item->type == PropertyTagTypeRational && item->length >= 8) {
            u32 numr = ((u32*)item->value)[0];
            u32 den = ((u32*)item->value)[1];
            val = den ? fmt("%u/%u", numr, den) : fmt("%u", numr);
        } else {
            val = fmt("[%u bytes]", item->length);
        }
        TempStr line = fmt("EXIF Tag 0x%04X (%s): %s", (u16)ids[i], TypeName(item->type), val);
        lines.Append(str::Dup(line));
        free(buf);
    }
    free(ids);
    delete bmp;
}

} // namespace

// GUI-subsystem exes lose CRT stdout when spawned with a pipe (issue #5677).
static void CliWrite(Str s, size_t n = 0) {
    if (!s) {
        return;
    }
    if (n == 0) {
        n = (size_t)s.len;
    }
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, s.s, (DWORD)n, &written, nullptr);
        return;
    }
    fwrite(s.s, 1, n, stdout);
}

static void CliPrint(Str s) {
    CliWrite(s);
    CliWrite(StrL("\n"), 1);
}

bool DumpExifFile(Str path) {
    if (!path) {
        return false;
    }
    CliPrint(fmt("Opening: %s", path));
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        CliPrint(StrL("No EXIF information found"));
        return false;
    }

    Str exifBlob;
    u8* ownedExif = nullptr;
    bool hasBlob = ExtractExifBlob(data, exifBlob, &ownedExif);
    TiffParser parser{ByteReader(exifBlob)};
    bool found = false;

    if (hasBlob) {
        found = parser.Parse();
    }

    if (!found) {
        DumpFromGdiplus(data, parser.lines);
        found = len(parser.lines) > 0;
    }

    if (!found) {
        CliPrint(StrL("No EXIF information found"));
        free(ownedExif);
        str::Free(data);
        return false;
    }

    if (parser.hasJpegThumbnail) {
        CliPrint(StrL("File has JPEG thumbnail"));
    }

    for (Str line : parser.lines) {
        CliPrint(line);
    }

    free(ownedExif);
    str::Free(data);
    return true;
}

void DumpExif(const Flags& flags) {
    bool any = false;
    for (int i = 0; i < len(flags.fileNames); i++) {
        if (DumpExifFile(flags.fileNames.At(i))) {
            any = true;
        }
    }
    if (!any && len(flags.fileNames) == 0) {
        CliPrint("No file specified for -dump-exif");
    }
}
