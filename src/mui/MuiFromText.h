/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiFromText_h
#error "dont include twice!"
#endif
#define MuiFromText_h

typedef Control * (* UnknownControlCallback)(const char *name, SquareTreeNode *data);

class ParsedMui {
    Vec<Control *>      controls;
    Vec<char *>         controlTypes;
    Vec<ILayout*>       layouts;

public:
    ~ParsedMui() {
        FreeVecMembers(controlTypes);
    }

    void AddControl(Control *ctrl, const char *type);
    void AddLayout(ILayout *layout);

    Control *FindControl(const char *name, const char *type) const;
    ILayout *FindLayout(const char *name, bool alsoControls=false) const;

    static ParsedMui *Create(char *s, HwndWrapper *owner, UnknownControlCallback cb);
};
