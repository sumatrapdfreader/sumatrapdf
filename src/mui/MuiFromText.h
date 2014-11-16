/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class ParsedMui {
public:
    Vec<Control *>      allControls;
    Vec<ButtonVector*>  vecButtons;
    Vec<Button*>        buttons;
    Vec<ScrollBar*>     scrollBars;
    Vec<ILayout*>       layouts;
};

bool            MuiFromText(char *s, ParsedMui& res);
Button *        FindButtonNamed(const ParsedMui& muiInfo, const char *name);
ButtonVector *  FindButtonVectorNamed(const ParsedMui& muiInfo, const char *name);
ScrollBar *     FindScrollBarNamed(const ParsedMui& muiInfo, const char *name);
Control *       FindControlNamed(const ParsedMui& muiInfo, const char *name);
ILayout *       FindLayoutNamed(const ParsedMui& muiInfo, const char *name);

typedef Control * (*ControlCreatorFunc)(TxtNode *);
void RegisterControlCreatorFor(const char *typeName, ControlCreatorFunc creator);
void FreeControlCreators();

typedef ILayout * (*LayoutCreatorFunc)(ParsedMui *, TxtNode *);
void RegisterLayoutCreatorFor(const char *layoutName, LayoutCreatorFunc creator);
void FreeLayoutCreators();
