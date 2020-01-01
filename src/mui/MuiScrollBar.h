/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// TODO: this is meant to evolve into a scroll bar (which can also serve
// as a non-scrollable rectangle bar for e.g. indicating progress) but
// it's far from it. Currently it's just horizontal progress bar that visually
// represents percentage progress of some activity.
// It can also report clicks within itself.
// The background is drawn with PropBgColor and the part that represents
// percentage with PropColor.
// It has a fixed height, provided by a caller, but the width can vary
// depending on layout.
// For a bit of an extra effect, it has 2 heights: one when mouse is over
// the control and another when mouse is not over.
// For the purpose of layout, we use the bigger of the to as the real
// height of the control
// Clients can register fo IClickEvent events for this window to implement
// interactivity

// TODO: needs to work for both vertical and horizontal cases

// TODO: needs all the other scrollbar functionality

// TODO: we don't take padding int account yet
class ScrollBar : public Control {
    int onOverDy;   // when mouse is over
    int inactiveDy; // when mouse is not over

    // what percentage of the rectangle is filled with PropColor
    // (the rest is filled with PropBgColor).
    // The range is from 0 to 1
    float filledPerc;

  public:
    ScrollBar(int onOverDy = 12, int inactiveDy = 5);
    ~ScrollBar() {
    }
    virtual Size Measure(const Size availableSize);
    virtual void NotifyMouseEnter();
    virtual void NotifyMouseLeave();

    virtual void Paint(Graphics* gfx, int offX, int offY);

    void SetFilled(float perc);
    float GetPercAt(int x);
};

float PercFromInt(int total, int n);
int IntFromPerc(int total, float perc);
