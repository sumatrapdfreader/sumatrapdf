/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
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
#include "GdiPlusUtil.h"
#include "SvgPath.h"
#include "VecSegmented.h"

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

// the order must match order of PathInstrType enums
static char *instructions = "MmLlHhVvCcSsQqTtAaZz";

struct SvgPathInstr {
    SvgPathInstr(PathInstrType type) : type(type) { }

    PathInstrType   type;
    // the meaning of values depends on InstrType. We could be more safe
    // by giving them symbolic names but this gives us simpler parsing
    float           v[6];
    bool            largeArc, sweep;
};

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
        return nullptr;
    GraphicsPath *gp = ::new GraphicsPath();
    PointF prevEnd(0.f, 0.f);
    for (SvgPathInstr& i : instr) {
        PathInstrType type = i.type;

        // convert relative coordinates to absolute based on end position of
        // previous element
        // TODO: support the rest of instructions
        if (MoveRel == type) {
            RelPointToAbs(prevEnd, i.v);
            type = MoveAbs;
        } else if (LineToRel == type) {
            RelPointToAbs(prevEnd, i.v);
            type = LineToAbs;
        } else if (HLineRel == type) {
            RelXToAbs(prevEnd, i.v);
            type = HLineAbs;
        } else if (VLineRel == type) {
            RelYToAbs(prevEnd, i.v);
            type = VLineAbs;
        }

        if (MoveAbs == type) {
            PointF p(i.v[0], i.v[1]);
            prevEnd = p;
            gp->StartFigure();
        } else if (LineToAbs == type) {
            PointF p(i.v[0], i.v[1]);
            gp->AddLine(prevEnd, p);
            prevEnd = p;
        } else if (HLineAbs == type) {
            PointF p(prevEnd);
            p.X = i.v[0];
            gp->AddLine(prevEnd, p);
            prevEnd = p;
        } else if (VLineAbs == type) {
            PointF p(prevEnd);
            p.Y = i.v[0];
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
