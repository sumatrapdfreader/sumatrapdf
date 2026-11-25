/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "DocProperties.h"

static SeqStrings gPropNames =
    "title\0"
    "author\0"
    "copyright\0"
    "subject\0"
    "creationDate\0"
    "modDate\0"
    "creatorApp\0"
    "unsupportedFeatures\0"
    "fontList\0"
    "pdfVersion\0"
    "pdfProducer\0"
    "pdfFileStructure\0"
    "files\0"
    "keywords\0"
    "encryption\0"
    "signatures\0"
    "imageSize\0"
    "dpi\0"
    "comment\0"
    "cameraMake\0"
    "cameraModel\0"
    "dateOriginal\0"
    "exposureTime\0"
    "fNumber\0"
    "isoSpeed\0"
    "focalLength\0"
    "focalLength35mm\0"
    "flash\0"
    "orientation\0"
    "exposureProgram\0"
    "meteringMode\0"
    "whiteBalance\0"
    "exposureBias\0"
    "bitsPerSample\0"
    "resolutionUnit\0"
    "software\0"
    "dateTime\0"
    "yCbCrPositioning\0"
    "exifVersion\0"
    "dateTimeDigitized\0"
    "componentsConfig\0"
    "compressedBpp\0"
    "maxAperture\0"
    "lightSource\0"
    "userComment\0"
    "flashpixVersion\0"
    "colorSpace\0"
    "pixelXDimension\0"
    "pixelYDimension\0"
    "fileSource\0"
    "sceneType\0"
    "imageFileSize\0"
    "imagePath\0"
    "publisher\0"
    "identifier\0"
    "source\0"
    "language\0"
    "\0";

// @gen-start docprop-pdf-info
// clang-format off
static SeqStrNum gPdfInfoPropsMap =
    "Title\0" "\x02"
    "Author\0" "\x04"
    "Subject\0" "\x08"
    "Copyright\0" "\x06"
    "CreationDate\0" "\x0a"
    "ModDate\0" "\x0c"
    "Creator\0" "\x0e"
    "Producer\0" "\x16"
    "\0";
// clang-format on
// @gen-end docprop-pdf-info

Str PdfInfoKeyFromProp(DocProp prop) {
    return SeqStrNumStrByNumber(gPdfInfoPropsMap, (i64)prop);
}

int GetPropIdx(const Props& props, DocProp prop) {
    int n = len(props);
    for (int i = 0; i < n; i++) {
        if (props[i].prop == prop) {
            return i;
        }
    }
    return -1;
}

Str GetPropValueTemp(const Props& props, DocProp prop) {
    int idx = GetPropIdx(props, prop);
    return idx < 0 ? Str{} : props[idx].val;
}

void AddProp(Props& props, DocProp prop, Str val, bool replaceIfExists) {
    ReportIf(prop == DocProp::None || len(val) == 0);
    int idx = GetPropIdx(props, prop);
    if (idx < 0) {
        VecAppend(props, {prop, val});
    } else if (replaceIfExists) {
        props[idx].val = val;
    }
}

// like AddProp but stores an owned (heap) copy of val. Use when props must
// outlive the buffer val points into (e.g. the temp arena). Free with FreeProps.
void AddPropOwned(Props& props, DocProp prop, Str val, bool replaceIfExists) {
    if (len(val) == 0) {
        return;
    }
    int idx = GetPropIdx(props, prop);
    if (idx < 0) {
        VecAppend(props, {prop, str::Dup(val)});
    } else if (replaceIfExists) {
        str::ReplaceWithCopy(&props[idx].val, val);
    }
}

// frees values stored by AddPropOwned and empties props
void FreeProps(Props& props) {
    for (const PropValue& prop : props) {
        str::Free(prop.val);
    }
    VecReset(props);
}

// gPropNames lists the names in DocProp order, so DocProp::Title (value 1) is
// the first name (index 0); the value is index + 1.
TempStr PropNameTemp(DocProp prop) {
    int idx = (int)prop - 1;
    if (idx < 0) {
        return {};
    }
    return SeqStrByIndex(gPropNames, idx);
}

// PDF date format (ISO 32000-1 7.9.4): "D:YYYYMMDDHHmmSSOHH'mm'",
// where "D:" and fields after YYYY are optional.
bool ParsePdfDate(Str date, PropDate* timeOut, int* timeZoneOut) {
    if (len(date) == 0) {
        return false;
    }

    *timeOut = PropDate();
    if (timeZoneOut) {
        *timeZoneOut = 0;
    }

    Str slice = date;
    str::TrimPrefix(slice, StrL("D:"));
    int year = 0;
    Str end = str::Parse(slice, "%4d", &year);
    if (!end.s) {
        return false;
    }
    int month = 1, day = 1, hour = 0, minute = 0, second = 0;
    bool hasSeconds = false;
    if (Str next = str::Parse(end, "%2d", &month); next.s) {
        end = next;
        if (next = str::Parse(end, "%2d", &day); next.s) {
            end = next;
            if (next = str::Parse(end, "%2d", &hour); next.s) {
                end = next;
                if (next = str::Parse(end, "%2d", &minute); next.s) {
                    end = next;
                    if (next = str::Parse(end, "%2d", &second); next.s) {
                        end = next;
                        hasSeconds = true;
                    }
                }
            }
        }
    }
    if (!hasSeconds && len(end) > 0 && end.s[0] != 'Z' && end.s[0] != '+' && end.s[0] != '-') {
        return false;
    }
    timeOut->year = year;
    timeOut->month = month;
    timeOut->day = day;
    timeOut->hour = hour;
    timeOut->minute = minute;
    timeOut->second = second;

    if (timeZoneOut && len(end) > 0) {
        if (end.s[0] == 'Z') {
            *timeZoneOut = 0;
        } else if (end.s[0] == '+' || end.s[0] == '-') {
            int sign = (end.s[0] == '+') ? 1 : -1;
            int tzHour = 0;
            int tzMin = 0;
            Str tz = Str(end.s + 1, end.len - 1);
            Str tzEnd = str::Parse(tz, "%2d'%2d", &tzHour, &tzMin);
            if (!tzEnd.s) {
                tzEnd = str::Parse(tz, "%2d:%2d", &tzHour, &tzMin);
            }
            if (!tzEnd.s) {
                str::Parse(tz, "%2d", &tzHour);
            }
            *timeZoneOut = sign * ((tzHour * 100) + tzMin);
        }
    }
    return true;
}

// ISO 8601 date format: "YYYY-MM-DD" with optional "THH:MM:SS" and timezone.
bool ParseIsoDate(Str date, PropDate* timeOut, int* timeZoneOut) {
    if (len(date) == 0) {
        return false;
    }

    *timeOut = PropDate();
    if (timeZoneOut) {
        *timeZoneOut = 0;
    }

    int year = 0, month = 0, day = 0;
    Str end = str::Parse(date, "%4d-%2d-%2d", &year, &month, &day);
    if (!end.s) {
        return false;
    }
    timeOut->year = year;
    timeOut->month = month;
    timeOut->day = day;
    int hour = 0, minute = 0, second = 0;
    Str timeEnd = str::Parse(end, "T%2d:%2d:%2d", &hour, &minute, &second);
    if (timeEnd.s) {
        timeOut->hour = hour;
        timeOut->minute = minute;
        timeOut->second = second;
        if (timeZoneOut && len(timeEnd) > 0) {
            if (timeEnd.s[0] == 'Z') {
                *timeZoneOut = 0;
            } else if (timeEnd.s[0] == '+' || timeEnd.s[0] == '-') {
                int sign = (timeEnd.s[0] == '+') ? 1 : -1;
                int tzHour = 0;
                int tzMin = 0;
                Str tz = Str(timeEnd.s + 1, timeEnd.len - 1);
                Str tzEnd = str::Parse(tz, "%2d:%2d", &tzHour, &tzMin);
                if (!tzEnd.s) {
                    str::Parse(tz, "%2d%2d", &tzHour, &tzMin);
                }
                *timeZoneOut = sign * ((tzHour * 100) + tzMin);
            }
        }
    }
    return true;
}
