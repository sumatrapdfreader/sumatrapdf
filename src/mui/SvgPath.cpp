/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* Parser for SVG path string (also used in WPF). The idea is
to have an easy way to use vector graphics. The path is
then converted to GraphicsPath which then can be used
in a Button etc.
*/

#include "BaseUtil.h"
#include "VecSegmented.h"

struct MoveAbsData {
    float x, y;
};

struct SvgPathInstr {
    enum InstrType {
        MoveAbs, LineTo, HLine, VLine, Arc, Close
    };

    SvgPathInstr(InstrType type) : type(type) {
    }

    InstrType   type;
    union {
        MoveAbsData moveAbs;

    };
};

static bool ParseFloat2(const char *&s, float& f1, float& f2)
{
    return false;
}

bool ParseSvgPath(const char * s, VecSegmented<SvgPathInstr>& instr)
{
    for (char c = *s++; c; c = *s++) {
        if ('M' == c) {
            SvgPathInstr i(SvgPathInstr::MoveAbs);
            if (!ParseFloat2(s, i.moveAbs.x, i.moveAbs.y))
                return false;
            instr.Append(i);
        }
    }
    return true;
}

