/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MuiButton_h
#define MuiButton_h

// This is only meant to be included by Mui.h inside mui namespace

class Button : public Control
{
    // use SetStyles() to set
    Style *         styleDefault;    // gStyleButtonDefault if styleDefault is NULL
    Style *         styleMouseOver;  // gStyleButtonMouseOver if NULL

public:
    Button(const TCHAR *s);

    virtual ~Button();

    void SetText(const TCHAR *s);

    void RecalculateSize(bool repaintIfSizeDidntChange);

    virtual void Measure(const Size availableSize);
    virtual void Paint(Graphics *gfx, int offX, int offY);

    virtual void NotifyMouseEnter();
    virtual void NotifyMouseLeave();

    Size    GetBorderAndPaddingSize() const;

    void    SetStyles(Style *def, Style *mouseOver);

    TCHAR *         text;
    size_t          textDx; // cached measured text width
};

#endif

