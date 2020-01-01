/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// WPF-like layout system. Measure() should update DesiredSize()
// After Measure() the parent uses DesiredSize() to calculate the
// size of its children and uses Arrange() to set it.
// availableSize can have SizeInfinite as dx or dy to allow
// using as much space as the window wants
// Every Control implements ILayout for calculating their desired
// size but can also have independent ILayout (which is for controls
// that contain other controls). This allows decoupling layout logic
// from controls and implementing generic layouts.
class ILayout {
  public:
    AutoFree name;
    void SetName(const char* n) {
        if (n)
            name.SetCopy(n);
    }
    bool IsNamed(const char* s) const {
        return str::EqI(name.Get(), s);
    }
    virtual ~ILayout(){};
    virtual Gdiplus::Size Measure(const Gdiplus::Size availableSize) = 0;
    virtual Gdiplus::Size DesiredSize() = 0;
    virtual void Arrange(const Gdiplus::Rect finalRect) = 0;
};

#define SizeSelf 666.f

// Defines how we layout a single element within a container
// using either horizontal or vertical layout. Vertical/Horizontal
// layout are conceptually the same, just using different axis/dimensions
// for calculations. LayoutAxis is x for horizontal layout and y
// for vertical layout. NonLayoutAxis is the other one.
struct DirectionalLayoutData {
    ILayout* element = nullptr;
    // size is a float that determines how much of the remaining
    // available space of the container should be allocated to
    // this element. A magic value SizeSelf means the element should
    // get its desired size. This is similar to star sizing in WPF in
    // that the remaining space gets redistributed among all elements.
    // e.g. if there is only one element with size != SizeSelf, and size
    // is 0.5, it'll get half the remaining space, if size is 1.0, it'll
    // get the whole remaining space but if there are 2 elements and
    // both have size 1.0, they'll only get half remaining space each
    float sizeLayoutAxis = 0.f;
    // similar to sizeLayoutAxis except there's only one element in
    // this axis, so things are simpler
    float sizeNonLayoutAxis = 0.f;

    // within layout axis, elements are laid out sequentially.
    // alignNonLayoutAxis determines how to align the element
    // within container in the other axis
    ElAlignData alignNonLayoutAxis;

    // data to be used during layout process

    // desiredSize of the element after Measure() step
    Gdiplus::Size desiredSize;

    DirectionalLayoutData() : alignNonLayoutAxis(GetElAlignCenter()) {
    }

    DirectionalLayoutData(const DirectionalLayoutData& other)
        : element(other.element),
          sizeLayoutAxis(other.sizeLayoutAxis),
          sizeNonLayoutAxis(other.sizeNonLayoutAxis),
          alignNonLayoutAxis(other.alignNonLayoutAxis),
          desiredSize(other.desiredSize) {
    }

    void Set(ILayout* el, float sla, float snla, const ElAlignData& a) {
        element = el;
        sizeLayoutAxis = sla;
        sizeNonLayoutAxis = snla;
        alignNonLayoutAxis = a;
    }
};

class DirectionalLayout : public ILayout {
  protected:
    Vec<DirectionalLayoutData> els;
    Gdiplus::Size desiredSize;

  public:
    ~DirectionalLayout() override;
    Gdiplus::Size DesiredSize() override {
        return desiredSize;
    }

    DirectionalLayout& Add(const DirectionalLayoutData& ld);

    Gdiplus::Size Measure(const Gdiplus::Size availableSize) override;
    void Arrange(const Gdiplus::Rect finalRect) override {
        UNUSED(finalRect);
        CrashIf(true);
    }
};

class HorizontalLayout : public DirectionalLayout {
  public:
    HorizontalLayout() {
    }

    void Arrange(const Gdiplus::Rect finalRect) override;
};

class VerticalLayout : public DirectionalLayout {
  public:
    VerticalLayout() {
    }

    void Arrange(const Gdiplus::Rect finalRect) override;
};
