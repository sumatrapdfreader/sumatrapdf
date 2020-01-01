/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

#include "utils/BaseUtil.h"
#include "utils/GdiPlusUtil.h"
#include "SvgPath.h"
#include "utils/VecSegmented.h"

using namespace Gdiplus;

namespace svg {

enum class PathInstr {
    MoveAbs = 0,
    MoveRel, // M, m
    LineToAbs,
    LineToRel, // L, l
    HLineAbs,
    HLineRel, // H, h
    VLineAbs,
    VLineRel, // V, v
    BezierCAbs,
    BezierCRel, // C, c
    BezierSAbs,
    BezierSRel, // S, s
    BezierQAbs,
    BezierQRel, // Q, q
    BezierTAbs,
    BezierTRel, // T, t
    ArcAbs,
    ArcRel, // A, a
    Close,
    Close2, // Z, z
    Count,
    Unknown = Count
};

// the order must match order of PathInstr enums
static char* instructions = "MmLlHhVvCcSsQqTtAaZz";

struct SvgPathInstr {
    SvgPathInstr(PathInstr type) : type(type) {
    }

    PathInstr type;
    // the meaning of values depends on InstrType. We could be more safe
    // by giving them symbolic names but this gives us simpler parsing
    float v[6];
    bool largeArc, sweep;
};

static PathInstr GetInstructionType(char c) {
    const char* pos = str::FindChar(instructions, c);
    if (!pos) {
        return PathInstr::Unknown;
    }
    return (PathInstr)(pos - instructions);
}

static const char* skipWs(const char* s) {
    while (str::IsWs(*s)) {
        s++;
    }
    return s;
}

static bool ParseSvgPathData(const char* s, VecSegmented<SvgPathInstr>& instr) {
    s = skipWs(s);

    while (*s) {
        SvgPathInstr i(GetInstructionType(*s++));
        switch (i.type) {
            case PathInstr::Close:
            case PathInstr::Close2:
                break;

            case PathInstr::HLineAbs:
            case PathInstr::HLineRel:
            case PathInstr::VLineAbs:
            case PathInstr::VLineRel:
                s = str::Parse(s, "%f", &i.v[0]);
                break;

            case PathInstr::MoveAbs:
            case PathInstr::MoveRel:
            case PathInstr::LineToAbs:
            case PathInstr::LineToRel:
            case PathInstr::BezierTAbs:
            case PathInstr::BezierTRel:
                s = str::Parse(s, "%f%_%?,%_%f", &i.v[0], &i.v[1]);
                break;

            case PathInstr::BezierSAbs:
            case PathInstr::BezierSRel:
            case PathInstr::BezierQAbs:
            case PathInstr::BezierQRel:
                s = str::Parse(s, "%f%_%?,%_%f,%f%_%?,%_%f", &i.v[0], &i.v[1], &i.v[2], &i.v[3]);
                break;

            case PathInstr::BezierCAbs:
            case PathInstr::BezierCRel:
                s = str::Parse(s, "%f%_%?,%_%f,%f%_%?,%_%f,%f%_%?,%_%f", &i.v[0], &i.v[1], &i.v[2], &i.v[3], &i.v[4],
                               &i.v[5]);
                break;

            case PathInstr::ArcAbs:
            case PathInstr::ArcRel: {
                int largeArc, sweep;
                s = str::Parse(s, "%f%_%?,%_%f%_%?,%_%f%_%?,%_%d%_%?,%_%d%_%?,%_%f%_%?,%_%f", &i.v[0], &i.v[1], &i.v[2],
                               &largeArc, &sweep, &i.v[3], &i.v[4]);
                i.largeArc = (largeArc != 0);
                i.sweep = (sweep != 0);
            } break;

            default:
                CrashIf(true);
                return false;
        }

        if (!s) {
            return false;
        }
        instr.push_back(i);

        s = skipWs(s);
    }

    return true;
}

static void RelPointToAbs(const PointF& lastEnd, float* xy) {
    xy[0] = lastEnd.X + xy[0];
    xy[1] = lastEnd.Y + xy[1];
}

static void RelXToAbs(const PointF& lastEnd, float* x) {
    *x = lastEnd.X + *x;
}

static void RelYToAbs(const PointF& lastEnd, float* y) {
    *y = lastEnd.Y + *y;
}

GraphicsPath* GraphicsPathFromPathData(const char* s) {
    VecSegmented<SvgPathInstr> instr;
    if (!ParseSvgPathData(s, instr)) {
        return nullptr;
    }
    GraphicsPath* gp = ::new GraphicsPath();
    PointF prevEnd(0.f, 0.f);
    for (SvgPathInstr& i : instr) {
        PathInstr type = i.type;

        // convert relative coordinates to absolute based on end position of
        // previous element
        // TODO: support the rest of instructions
        if (PathInstr::MoveRel == type) {
            RelPointToAbs(prevEnd, i.v);
            type = PathInstr::MoveAbs;
        } else if (PathInstr::LineToRel == type) {
            RelPointToAbs(prevEnd, i.v);
            type = PathInstr::LineToAbs;
        } else if (PathInstr::HLineRel == type) {
            RelXToAbs(prevEnd, i.v);
            type = PathInstr::HLineAbs;
        } else if (PathInstr::VLineRel == type) {
            RelYToAbs(prevEnd, i.v);
            type = PathInstr::VLineAbs;
        }

        if (PathInstr::MoveAbs == type) {
            PointF p(i.v[0], i.v[1]);
            prevEnd = p;
            gp->StartFigure();
        } else if (PathInstr::LineToAbs == type) {
            PointF p(i.v[0], i.v[1]);
            gp->AddLine(prevEnd, p);
            prevEnd = p;
        } else if (PathInstr::HLineAbs == type) {
            PointF p(prevEnd);
            p.X = i.v[0];
            gp->AddLine(prevEnd, p);
            prevEnd = p;
        } else if (PathInstr::VLineAbs == type) {
            PointF p(prevEnd);
            p.Y = i.v[0];
            gp->AddLine(prevEnd, p);
            prevEnd = p;
        } else if ((PathInstr::Close == type) || (PathInstr::Close2 == type)) {
            gp->CloseFigure();
        } else {
            CrashIf(true);
        }
    }
    return gp;
}
} // namespace svg
