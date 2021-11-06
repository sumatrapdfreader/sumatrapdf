/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

struct Point {
    int x{0};
    int y{0};

    Point() = default;
    Point(int x, int y);

    [[nodiscard]] bool IsEmpty() const;
    [[nodiscard]] bool Eq(int x, int y) const;
    bool operator==(const Point& other) const;
    bool operator!=(const Point& other) const;
};

struct PointF {
    float x{0};
    float y{0};

    PointF() = default;

    PointF(float x, float y);

    [[nodiscard]] bool IsEmpty() const;
    bool operator==(const PointF& other) const;
    bool operator!=(const PointF& other) const;
};

struct Size {
    int dx{0};
    int dy{0};

    Size() = default;
    Size(int dx, int dy);

    [[nodiscard]] bool IsEmpty() const;

    [[nodiscard]] bool Equals(const Size& other) const;
    bool operator==(const Size& other) const;
    bool operator!=(const Size& other) const;
};

struct SizeF {
    float dx{0};
    float dy{0};

    SizeF() = default;
    SizeF(float dx, float dy);

    [[nodiscard]] bool IsEmpty() const;

    bool operator==(const SizeF& other) const;
    bool operator!=(const SizeF& other) const;
};

struct Rect {
    int x{0};
    int y{0};
    int dx{0};
    int dy{0};

    Rect() = default;
    Rect(RECT r);           // NOLINT
    Rect(Gdiplus::RectF r); // NOLINT
    Rect(int x, int y, int dx, int dy);
    // TODO: why not working if in .cpp? Confused by Size also being a method?
    Rect(const Point pt, const Size sz) : x(pt.x), y(pt.y), dx(sz.dx), dy(sz.dy) {
    }
    Rect(Point min, Point max);

    [[nodiscard]] bool EqSize(int otherDx, int otherDy) const;
    [[nodiscard]] int Right() const;
    [[nodiscard]] int Bottom() const;
    static Rect FromXY(int xs, int ys, int xe, int ye);
    static Rect FromXY(Point TL, Point BR);
    [[nodiscard]] bool IsEmpty() const;
    [[nodiscard]] bool Contains(int x, int y) const;
    [[nodiscard]] bool Contains(Point pt) const;
    [[nodiscard]] Rect Intersect(Rect other) const;
    [[nodiscard]] Rect Union(Rect other) const;
    void Offset(int _x, int _y);
    void Inflate(int _x, int _y);
    [[nodiscard]] Point TL() const;
    [[nodiscard]] Point BR() const;
    [[nodiscard]] Size Size() const;
    static Rect FromRECT(const RECT& rect);
    [[nodiscard]] bool Equals(const Rect& other) const;
    bool operator==(const Rect& other) const;
    bool operator!=(const Rect& other) const;
};

struct RectF {
    float x{0};
    float y{0};
    float dx{0};
    float dy{0};

    RectF() = default;

    explicit RectF(RECT r);
    RectF(Gdiplus::RectF r); // NOLINT
    RectF(float x, float y, float dx, float dy);
    RectF(PointF pt, SizeF size);
    RectF(PointF min, PointF max);

    bool EqSize(float otherDx, float otherDy) const;
    [[nodiscard]] float Right() const;
    [[nodiscard]] float Bottom() const;
    static RectF FromXY(float xs, float ys, float xe, float ye);
    static RectF FromXY(PointF TL, PointF BR);
    [[nodiscard]] Rect Round() const;
    [[nodiscard]] bool IsEmpty() const;
    bool Contains(PointF pt) const;
    RectF Intersect(RectF other) const;
    RectF Union(RectF other);
    void Offset(float _x, float _y);
    void Inflate(float _x, float _y);
    [[nodiscard]] PointF TL() const;
    [[nodiscard]] PointF BR() const;
    [[nodiscard]] SizeF Size() const;
    static RectF FromRECT(const RECT& rect);
    bool operator==(const RectF& other) const;
    bool operator!=(const RectF& other) const;
};

PointF ToPointFl(Point p);
Gdiplus::Point ToGdipPoint(Point p);
Point ToPoint(PointF p);
Gdiplus::PointF ToGdipPointF(PointF p);

SIZE ToSIZE(Size s);
SizeF ToSizeFl(Size s);
Size ToSize(SizeF s);

RectF ToRectF(Rect r);
RECT ToRECT(Rect r);
RECT RECTFromRect(Gdiplus::Rect r);
Gdiplus::Rect ToGdipRect(Rect r);
Gdiplus::RectF ToGdipRectF(Rect r);

RECT ToRECT(RectF r);
Rect ToRect(RectF r);
Gdiplus::Rect ToGdipRect(RectF r);
Gdiplus::RectF ToGdipRectF(RectF r);

int NormalizeRotation(int rotation);
