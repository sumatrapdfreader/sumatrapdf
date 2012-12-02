/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiLayout_h
#error "dont include twice!"
#endif
#define MuiLayout_h

// WPF-like layout system. Measure() should update DesiredSize()
// After Measure() the parent uses DesiredSize() to calculate the
// size of its children and uses Arrange() to set it.
// availableSize can have SizeInfinite as dx or dy to allow
// using as much space as the window wants
// Every Control implements ILayout for calculating their desired
// size but can also have independent ILayout (which is for controls
// that contain other controls). This allows decoupling layout logic
// from controls and implementing generic layouts.
class ILayout
{
public:
    virtual ~ILayout() {};
    virtual void Measure(const Size availableSize) = 0;
    virtual Size DesiredSize() = 0;
    virtual void Arrange(const Rect finalRect) = 0;
};

#define SizeSelf    666.f

// Defines how we layout a single element within a container
// using either horizontal or vertical layout. Vertical/Horizontal
// layout are conceptually the same, just using different axis/dimensions
// for calculations. LayoutAxis is x for horizontal layout and y
// for vertical layout. NonLayoutAxis is the other one.
struct DirectionalLayoutData {
    ILayout *          element;
    // size is a float that determines how much of the remaining
    // available space of the container should be allocated to
    // this element. A magic value SizeSelf means the element should
    // get its desired size. This is similar to star sizing in WPF in
    // that the remaining space gets redistributed among all elements.
    // e.g. if there is only one element with size != SizeSelf, and size
    // is 0.5, it'll get half the remaining space, if size is 1.0, it'll
    // get the whole remaining space but if there are 2 elements and
    // both have size 1.0, they'll only get half remaining space each
    float              sizeLayoutAxis;
    // similar to sizeLayoutAxis except there's only one element in
    // this axis, so things are simpler
    float              sizeNonLayoutAxis;

    // within layout axis, elements are laid out sequentially.
    // alignNonLayoutAxis determines how to align the element
    // within container in the other axis
    ElAlignData        alignNonLayoutAxis;

    // if owns element, it'll delete it when is deleted itself
    // useful for embeding other layouts (controls are usually
    // deleted by their parent controls)
    bool               ownsElement;

    // data to be used during layout process

    // desiredSize of the element after Measure() step
    Size               desiredSize;

    DirectionalLayoutData() {
        element = 0;
        sizeLayoutAxis = 0.f;
        sizeNonLayoutAxis = 0.f;
        alignNonLayoutAxis = GetElAlignCenter();
        ownsElement = false;
        desiredSize = Size();
    }

    DirectionalLayoutData(const DirectionalLayoutData& other)
    {
        element = other.element;
        sizeLayoutAxis = other.sizeLayoutAxis;
        sizeNonLayoutAxis = other.sizeNonLayoutAxis;
        alignNonLayoutAxis = other.alignNonLayoutAxis;
        ownsElement = other.ownsElement;
        desiredSize = other.desiredSize;
    }

    void Set(ILayout *el, float sla, float snla, const ElAlignData& a) {
        element = el;
        sizeLayoutAxis = sla;
        sizeNonLayoutAxis = snla;
        alignNonLayoutAxis = a;
    }
};

class DirectionalLayout : public ILayout
{
protected:
    Vec<DirectionalLayoutData>  els;
    Size                        desiredSize;
public:
    virtual ~DirectionalLayout();
    virtual Size DesiredSize() { return desiredSize; }

    DirectionalLayout& Add(DirectionalLayoutData& ld, bool ownsElement=false);

    virtual void Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect) { CrashIf(true); }
};

class HorizontalLayout : public DirectionalLayout
{
public:
    HorizontalLayout() { }

    virtual void Arrange(const Rect finalRect);
};

class VerticalLayout : public DirectionalLayout
{
public:
    VerticalLayout() { }

    virtual void Arrange(const Rect finalRect);
};

