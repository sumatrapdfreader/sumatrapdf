/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiCss_h
#define MuiCss_h

#include "Vec.h"

namespace mui {
namespace css {

using namespace Gdiplus;

enum PropType {
    PropFontName,
    PropFontSize,
    PropFontWeight,
};

struct FontNameData {
    TCHAR * name;
};

struct FontSizeData {
    float   size;
};

struct FontWeightData {
    FontStyle style;
};

class Prop {
private:
    // can only allocate via Alloc*() functions so that we can
    // optimize allocations
    Prop(PropType type) : type(type) {}

public:
    PropType    type;

    union {
        FontNameData    fontName;
        FontSizeData    fontSize;
        FontWeightData  fontWeight;
    };

    ~Prop();

    static Prop *AllocFontName(const TCHAR *name);
    static Prop *AllocFontSize(float size);
    static Prop *AllocFontWeight(FontStyle style);
};

struct PropSet {
    Vec<Prop*> props;
    void Set(Prop *prop);
};

void Initialize();
void Destroy();

} // namespace css
} // namespace mui

#endif

