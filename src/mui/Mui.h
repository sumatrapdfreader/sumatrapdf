/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct TxtNode;

using Gdiplus::FontStyle;
using Gdiplus::Graphics;

namespace mui {

#include "TextRender.h"

void Initialize();
void Destroy();

void InitGraphicsMode(Graphics* g);

Graphics* AllocGraphicsForMeasureText();
void FreeGraphicsForMeasureText(Graphics* gfx);

} // namespace mui

class ScopedMui {
  public:
    ScopedMui() { mui::Initialize(); }
    ~ScopedMui() { mui::Destroy(); }
};
