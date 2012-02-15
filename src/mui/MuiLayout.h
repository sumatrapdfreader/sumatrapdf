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

class Control;

// A generalized way of specifying alignment (on a single axis,
// vertical or horizontal) of an element relative to its container.
// Each point of both the container and element can be represented
// as a float in the <0.f - 1.f> range.
// O.f represents left (in horizontal case) or top (in vertical) case point.
// 1.f represents right/bottom point and 0.5f represents a middle.
// We define a point inside cotainer and point inside element and layout
// positions element so that those points are the same.
// For example:
//  - (0.5f, 0.5f) centers element inside of the container.
//  - (0.f, 0.f) makes left edge of the element align with left edge of the container
//    i.e. ||el| container|
//  - (1.f, 0.f) makes left edge of the element align with right edge of the container
//    i.e. |container||el|
// This is more flexible than, say, VerticalAlignment property in WPF.
// Note: this can be extended for values outside of <0.f - 1.f> range.
struct ElInContainerAlign {

    float containerPoint;
    float elementPoint;

    ElInContainerAlign() : containerPoint(0.5f), elementPoint(0.5f) { }
    ElInContainerAlign(float cp, float ep) : containerPoint(cp), elementPoint(ep) { }

    ElInContainerAlign(const ElInContainerAlign& other) {
        containerPoint = other.containerPoint;
        elementPoint = other.elementPoint;
    }
};

#define SizeSelf    666.f

// Defines how we layout a single element within a container
// using either horizontal or vertical layout. Vertical/Horizontal
// layout are conceptually the same, just using different axis/dimensions
// for calculations. LayoutAxis is x for horizontal layout and y
// for vertical layout. NonLayoutAxis is the otehr one.
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
    ElInContainerAlign alignNonLayoutAxis;

    // if owns element, it'll delete it when is deleted itself
    // useful for embeding other layouts (controls are usually
    // deleted by their parent controls)
    bool               ownsElement;

    // data to be used during layout process

    // desiredSize of the element after Measure() step
    Size               desiredSize;

    // position we calculated for this element
    Rect               finalPos;

    DirectionalLayoutData() {
        element = 0;
        sizeLayoutAxis = 0.f;
        sizeNonLayoutAxis = 0.f;
        alignNonLayoutAxis = ElInContainerAlign();
        ownsElement = false;
        desiredSize = Size();
        finalPos = Rect();
    }

    DirectionalLayoutData(const DirectionalLayoutData& other)
    {
        element = other.element;
        sizeLayoutAxis = other.sizeLayoutAxis;
        sizeNonLayoutAxis = other.sizeNonLayoutAxis;
        alignNonLayoutAxis = other.alignNonLayoutAxis;
        ownsElement = other.ownsElement;
        desiredSize = other.desiredSize;
        finalPos = other.finalPos;
    }

    void Set(ILayout *el, float sla, float snla, ElInContainerAlign& a) {
        element = el;
        sizeLayoutAxis = sla;
        sizeNonLayoutAxis = snla;
        alignNonLayoutAxis = a;
    }
};

class HorizontalLayout : public ILayout
{
    Vec<DirectionalLayoutData>  els;
    Size                        desiredSize;

public:
    HorizontalLayout() {
    }
    virtual ~HorizontalLayout();

    HorizontalLayout& Add(DirectionalLayoutData& ld, bool ownsElement=false);

    virtual void Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);
    virtual Size DesiredSize();
};

class VerticalLayout : public ILayout
{
    Vec<DirectionalLayoutData>  els;
    Size                        desiredSize;

public:
    VerticalLayout() {
    }
    virtual ~VerticalLayout();

    VerticalLayout& Add(DirectionalLayoutData& ld, bool ownsElement=false);

    virtual void Measure(const Size availableSize);
    virtual void Arrange(const Rect finalRect);
    virtual Size DesiredSize();
};

