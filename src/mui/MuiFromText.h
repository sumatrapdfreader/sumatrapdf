/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiFromText_h
#error "dont include twice!"
#endif
#define MuiFromText_h

class ParsedMui {
public:
    Vec<ILayout *>      all;
    Vec<ButtonVector*>  vecButtons;
    Vec<Button*>        buttons;
    Vec<ScrollBar*>     scrollBars;
    Vec<Style*>         styles;
};

bool            MuiFromText(char *s, ParsedMui& res);
Button *        FindButtonNamed(ParsedMui& muiInfo, const char *name);
ButtonVector *  FindButtonVectorNamed(ParsedMui& muiInfo, const char *name);
ScrollBar *     FindScrollBarNamed(ParsedMui& muiInfo, const char *name);
