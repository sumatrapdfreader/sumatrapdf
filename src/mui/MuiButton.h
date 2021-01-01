/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Button is a combined label/button control. It can have 2 visual states:
// regular and when mouse is over it.

class Button : public Control {
    // use SetStyles() to set
    Style* styleDefault;   // gStyleButtonDefault if styleDefault is nullptr
    Style* styleMouseOver; // gStyleButtonMouseOver if nullptr

    void UpdateAfterStyleChange();

  public:
    Button(const WCHAR* s, Style* def, Style* mouseOver);

    virtual ~Button();

    void SetText(const WCHAR* s);

    void RecalculateSize(bool repaintIfSizeDidntChange);

    Size Measure(const Size availableSize) override;
    void Paint(Graphics* gfx, int offX, int offY) override;

    void NotifyMouseEnter() override;
    void NotifyMouseLeave() override;

    void SetDefaultStyle(Style* style);
    void SetMouseOverStyle(Style* style);
    void SetStyles(Style* def, Style* mouseOver);

    WCHAR* text;
    int textDx; // cached measured text width
};

// TODO: maybe should combine Button and ButtonVector into one?
class ButtonVector : public Control {
    // use SetStyles() to set
    Style* styleDefault;   // gStyleButtonDefault if styleDefault is nullptr
    Style* styleMouseOver; // gStyleButtonMouseOver if nullptr

    GraphicsPath* graphicsPath;

    void UpdateAfterStyleChange();

  public:
    ButtonVector();
    ButtonVector(GraphicsPath* gp);

    virtual ~ButtonVector();

    void SetGraphicsPath(GraphicsPath* gp);

    void RecalculateSize(bool repaintIfSizeDidntChange);

    Size Measure(const Size availableSize) override;
    void Paint(Graphics* gfx, int offX, int offY) override;

    void NotifyMouseEnter() override;
    void NotifyMouseLeave() override;

    void SetDefaultStyle(Style* style);
    void SetMouseOverStyle(Style* style);
    void SetStyles(Style* def, Style* mouseOver);
};
