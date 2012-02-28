/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/*
Parser for SVG path string (also used in WPF). The idea is
to have an easy way to use vector graphics. Parsed path can
be converted to Gdiplus::GraphicsPath which then can be used
in a Button etc.

http://www.w3.org/TR/SVG/paths.html
http://tutorials.jenkov.com/svg/path-element.html
https://developer.mozilla.org/en/SVG/Tutorial/Paths

The eventual goal is to be able easily use vector data in SVG and XAML
formats as there are many nice, freely licensable icons in those formats.
Parsing svg path syntax is the first step.

TODO: support composite paths (i.e. comprising of multiple GraphicsPaths
that have attributes like color, pen size etc.). Some graphics require that.

Note: this is not meant to fully support SVG or XAML syntax. We only need the
basic subset and we can assume that to import data from SVG/XAML files we'll
write a pre-processing script that will convert them to something that we
understand.
*/

#include "BaseUtil.h"
#include "SvgPath.h"
#include "StrUtil.h"
#include "VecSegmented.h"

// define to let str::Parse do most of the parsing
// TODO: it fails unit tests
//#define USE_STR_PARSE

namespace svg {

enum PathInstrType {
    MoveAbs = 0, MoveRel,   // M, m
    LineToAbs, LineToRel,   // L, l
    HLineAbs, HLineRel,     // H, h
    VLineAbs, VLineRel,     // V, v
    BezierCAbs, BezierCRel, // C, c
    BezierSAbs, BezierSRel, // S, s
    BezierQAbs, BezierQRel, // Q, q
    BezierTAbs, BezierTRel, // T, t
    ArcAbs, ArcRel,         // A, a
    Close, Close2,          // Z, z
    Count, Unknown = Count
};

struct SvgPathInstr {
    SvgPathInstr(PathInstrType type) : type(type) { }

    PathInstrType   type;
    // the meaning of values depends on InstrType. We could be more safe 
    // by giving them symbolic names but this way simplifies parsing
    float           v[6];
    bool            largeArc, sweep;
};

// the order must match order of InstrType enums
static char *instructions = "MmLlHhVvCcSsQqTtAaZz";

#ifndef USE_STR_PARSE

static bool GetInstructionType(char c, PathInstrType& instrOut)
{
   const char *pos = str::FindChar(instructions, c);
   if (!pos)
       return false;
   instrOut = (PathInstrType)(pos - instructions);
   return true;
}

static bool IsSvgSpace(char c)
{
    return (' ' == c) || (0x9 == c) || (0xd == c) || (0xa == c);
}

static bool IsSpaceOrComma(char c)
{
    return IsSvgSpace(c) || (',' == c);
}

// note: we could probably be more strict by only allowing
// one comma and multiple spaces, but this is simpler
static void SkipSpacesAndCommas(const char *& s)
{
    while (IsSpaceOrComma(*s)) {
       ++s;
    }
}

static void SkipSpaces(const char *& s)
{
    while (IsSvgSpace(*s)) {
       ++s;
    }
}

static bool ParseFloat(const char *& s, float& f)
{
    SkipSpacesAndCommas(s);
    s = str::Parse(s, "%f", &f);
    return (s != NULL);
}

static bool ParseBool(const char *& s, bool& b)
{
    SkipSpacesAndCommas(s);
    char c = *s++;
    if ('0' == c)
        b = false;
    else if ('1' == c)
        b = true;
    else
        return false;
    return true;
}

// Parse 2 floats separated by one or more spaces
static bool ParseFloat2(const char *&s, float& f1, float& f2)
{
    if (!ParseFloat(s, f1))
        return false;
    return ParseFloat(s, f2);
}

// parse 4 floats in the form:
// "f1 f2, f3 f4"
// where one or more spaces might separate the values
static bool ParseFloat22(const char *&s, float *valsOut)
{
    float f1, f2;
    if (!ParseFloat2(s, f1, f2))
        return false;
    valsOut[0] = f1; valsOut[1] = f2;
    if (!ParseFloat2(s, f1, f2))
        return false;
    valsOut[2] = f1; valsOut[3] = f2;
    return true;
}

// parse 6 floats in the form:
// "f1 f2, f3 f4, f5 f6"
// where one or more spaces might separate the values
static bool ParseFloat222(const char *&s, float *valsOut)
{
    float f1, f2;
    if (!ParseFloat2(s, f1, f2))
        return false;
    valsOut[0] = f1; valsOut[1] = f2;
    if (!ParseFloat2(s, f1, f2))
        return false;
    valsOut[2] = f1; valsOut[3] = f2;
    if (!ParseFloat2(s, f1, f2))
        return false;
    valsOut[4] = f1; valsOut[5] = f2;
    return true;
}

// parse "rx ry x-axis-rotation large-arc-flag sweep-flag x y"
static bool ParseArc(const char *&s, struct SvgPathInstr *pi)
{
    float f1, f2;
    bool  b;
    if (!ParseFloat2(s, f1, f2))
        return false;
    pi->v[0] = f1;
    pi->v[1] = f2;
    if (!ParseFloat(s, f1))
        return false;
    pi->v[2] = f1;
    if (!ParseBool(s, b))
        return false;
    pi->largeArc = b;
    if (!ParseBool(s, b))
        return false;
    pi->sweep = b;
    if (!ParseFloat2(s, f1, f2))
        return false;
    pi->v[3] = f1;
    pi->v[4] = f2;
    return true;
}

static bool ParseSvgPathData(const char * s, VecSegmented<SvgPathInstr>& instr)
{
    char c;
    bool ok;

    for (;;) {
        SkipSpaces(s);
        c = *s++;
        if (!c)
            return true;
        ok = true;
        PathInstrType type;
        if (!GetInstructionType(c, type))
            return false;
        SvgPathInstr i(type);
        switch (type) {
        case Close:
        case Close2:
            break;
        case HLineAbs:
        case HLineRel:
        case VLineAbs:
        case VLineRel:
            ok = ParseFloat(s, i.v[0]);
            break;
        case MoveAbs:
        case MoveRel:
        case LineToAbs:
        case LineToRel:
        case BezierTAbs:
        case BezierTRel:
            ok = ParseFloat2(s, i.v[0], i.v[1]);
            break;
        case BezierSAbs:
        case BezierSRel:
        case BezierQAbs:
        case BezierQRel:
            ok = ParseFloat22(s, i.v);
            break;
        case BezierCAbs:
        case BezierCRel:
            ok = ParseFloat222(s, i.v);
            break;
        case ArcAbs:
        case ArcRel:
            ok = ParseArc(s, &i);
            break;
        default:
            CrashIf(true);
            ok = false;
            break;
        }
        if (!ok)
            return false;
        instr.Append(i);
    }
    return true;
}

#else

static PathInstrType GetInstructionType(char c)
{
   const char *pos = str::FindChar(instructions, c);
   if (!pos)
       return Unknown;
   return (PathInstrType)(pos - instructions);
}

static bool ParseSvgPathData(const char * s, VecSegmented<SvgPathInstr>& instr)
{
    for (; str::IsWs(*s); s++);

    while (*s) {
        SvgPathInstr i(GetInstructionType(*s++));
        switch (i.type) {
        case Close: case Close2:
            break;

        case HLineAbs: case HLineRel:
        case VLineAbs: case VLineRel:
            s = str::Parse(s, "%f", &i.v[0]);
            break;

        case MoveAbs: case MoveRel:
        case LineToAbs: case LineToRel:
        case BezierTAbs: case BezierTRel:
            s = str::Parse(s, "%f%_%?,%_%f", &i.v[0], &i.v[1]);
            break;

        case BezierSAbs: case BezierSRel:
        case BezierQAbs: case BezierQRel:
            s = str::Parse(s, "%f%_%?,%_%f,%f%_%?,%_%f",
                &i.v[0], &i.v[1], &i.v[2], &i.v[3]);
            break;

        case BezierCAbs: case BezierCRel:
            s = str::Parse(s, "%f%_%?,%_%f,%f%_%?,%_%f,%f%_%?,%_%f",
                &i.v[0], &i.v[1], &i.v[2], &i.v[3], &i.v[4], &i.v[5]);
            break;

        case ArcAbs: case ArcRel:
            {
                int largeArc, sweep;
                s = str::Parse(s, "%f%_%?,%_%f%_%?,%_%f%_%?,%_%d%_%?,%_%d%_%?,%_%f%_%?,%_%f",
                    &i.v[0], &i.v[1], &i.v[2], &largeArc, &sweep, &i.v[3], &i.v[4]);
                i.largeArc = (largeArc != 0); i.sweep = (sweep != 0);
            }
            break;

        default:
            CrashIf(true);
            return false;
        }
        if (!s)
            return false;
        instr.Append(i);

        for (; str::IsWs(*s); s++);
    }

    return true;
}

#endif

static void RelPointToAbs(const PointF& lastEnd, float *xy)
{
    xy[0] = lastEnd.X + xy[0];
    xy[1] = lastEnd.Y + xy[1];
}

static void RelXToAbs(const PointF& lastEnd, float *x)
{
    *x = lastEnd.X + *x;
}

static void RelYToAbs(const PointF& lastEnd, float *y)
{
    *y = lastEnd.Y + *y;
}

GraphicsPath *GraphicsPathFromPathData(const char *s)
{
    VecSegmented<SvgPathInstr> instr;
    if (!ParseSvgPathData(s, instr))
        return NULL;
    GraphicsPath *gp = ::new GraphicsPath();
    PointF prevEnd(0.f, 0.f);
    for (SvgPathInstr *i = instr.IterStart(); i; i = instr.IterNext())
    {
        PathInstrType type = i->type;

        // convert relative coordinates to absolute based on end position of
        // previous element
        // TODO: support the rest of instructions
        if (MoveRel == type) {
            RelPointToAbs(prevEnd, i->v);
            type = MoveAbs;
        } else if (LineToRel == type) {
            RelPointToAbs(prevEnd, i->v);
            type = LineToAbs;
        } else if (HLineRel == type) {
            RelXToAbs(prevEnd, i->v);
            type = HLineAbs;
        } else if (VLineRel == type) {
            RelYToAbs(prevEnd, i->v);
            type = VLineAbs;
        }

        if (MoveAbs == type) {
            PointF p(i->v[0], i->v[1]);
            prevEnd = p;
            gp->StartFigure();
        } else if (LineToAbs == type) {
            PointF p(i->v[0], i->v[1]);
            gp->AddLine(prevEnd, p);
            prevEnd = p;
        } else if (HLineAbs == type) {
            PointF p(prevEnd);
            p.X = i->v[0];
            gp->AddLine(prevEnd, p);
            prevEnd = p;
        } else if (VLineAbs == type) {
            PointF p(prevEnd);
            p.Y = i->v[0];
            gp->AddLine(prevEnd, p);
            prevEnd = p;
        } else if ((Close == type) || (Close2 == type)) {
            gp->CloseFigure();
        } else {
            CrashIf(true);
        }
    }
    return gp;
}

}

#ifdef DEBUG
#include "SvgPath_ut.cpp"
#endif

