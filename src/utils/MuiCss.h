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
    const TCHAR * name;
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

    bool Eq(Prop *other);

    static Prop *AllocFontName(const TCHAR *name);
    static Prop *AllocFontSize(float size);
    static Prop *AllocFontWeight(FontStyle style);
};

struct PropSet {

    PropSet() : inheritsFrom(NULL) {
    }

    Vec<Prop*>  props;

    // if property is not found here, we'll search the
    // inheritance chain
    PropSet *   inheritsFrom;

    void Set(Prop *prop);
};

void Initialize();
void Destroy();

Font *CachedFontFromProps(PropSet *propsFirst, PropSet *propsSecond);

// globally known properties for elements we know about
// we fill them with default values and they can be
// modified by an app for global visual makeover
extern PropSet *gPropSetButtonRegular;
extern PropSet *gPropSetButtonMouseOver;

} // namespace css
} // namespace mui

#endif

