/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

struct Point {
    int x{0};
    int y{0};

    Point() = default;
    Point(int x, int y);

    bool IsEmpty() const;
    bool operator==(const Point& other) const;
    bool operator!=(const Point& other) const;
};

struct PointF {
    float x{0};
    float y{0};

    PointF() = default;

    PointF(float x, float y);

    bool IsEmpty() const;
    bool operator==(const PointF& other) const;
    bool operator!=(const PointF& other) const;
};

struct Size {
    int dx{0};
    int dy{0};

    Size() = default;
    Size(int dx, int dy);

    bool IsEmpty() const;

    bool Equals(const Size& other) const;
    bool operator==(const Size& other) const;
    bool operator!=(const Size& other) const;
};

struct SizeFl {
    float dx{0};
    float dy{0};

    SizeFl() = default;
    SizeFl(float dx, float dy);

    bool IsEmpty() const;

    bool operator==(const SizeFl& other) const;
    bool operator!=(const SizeFl& other) const;
};

struct Rect {
    int x{0};
    int y{0};
    int dx{0};
    int dy{0};

    Rect() = default;
    Rect(const RECT r);
    Rect(const Gdiplus::RectF r);
    Rect(int x, int y, int dx, int dy);
    // TODO: why not working if in .cpp? Confused by Size also being a method?
    Rect::Rect(const Point pt, const Size sz) : x(pt.x), y(pt.y), dx(sz.dx), dy(sz.dy) {
    }
    Rect(const Point min, const Point max);

    bool EqSize(int otherDx, int otherDy) const;
    int Right() const;
    int Bottom() const;
    static Rect FromXY(int xs, int ys, int xe, int ye);
    static Rect FromXY(Point TL, Point BR);
    bool IsEmpty() const;
    bool Contains(int x, int y) const;
    bool Contains(Point pt) const;
    Rect Intersect(Rect other) const;
    Rect Union(Rect other) const;
    void Offset(int _x, int _y);
    void Inflate(int _x, int _y);
    Point TL() const;
    Point BR() const;
    Size Size() const;
    static Rect FromRECT(const RECT& rect);
    bool Equals(const Rect& other) const;
    bool operator==(const Rect& other) const;
    bool operator!=(const Rect& other) const;
};

struct RectFl {
    float x{0};
    float y{0};
    float dx{0};
    float dy{0};

    RectFl() = default;

    RectFl(const RECT r);
    RectFl(const Gdiplus::RectF r);
    RectFl(float x, float y, float dx, float dy);
    RectFl(PointF pt, SizeFl size);
    RectFl(PointF min, PointF max);

    bool EqSize(float otherDx, float otherDy);
    float Right() const;
    float Bottom() const;
    static RectFl FromXY(float xs, float ys, float xe, float ye);
    static RectFl FromXY(PointF TL, PointF BR);
    Rect Round() const;
    bool IsEmpty() const;
    bool Contains(PointF pt);
    RectFl Intersect(RectFl other);
    RectFl Union(RectFl other);
    void Offset(float _x, float _y);
    void Inflate(float _x, float _y);
    PointF TL() const;
    PointF BR() const;
    SizeFl Size() const;
    static RectFl FromRECT(const RECT& rect);
    bool operator==(const RectFl& other) const;
    bool operator!=(const RectFl& other) const;
};

PointF ToPointFl(const Point p);
Gdiplus::Point ToGdipPoint(const Point p);
Point ToPoint(const PointF p);
Gdiplus::PointF ToGdipPointF(const PointF p);

SIZE ToSIZE(const Size s);
SizeFl ToSizeFl(const Size s);
Size ToSize(const SizeFl s);

RectFl ToRectFl(const Rect r);
RECT ToRECT(const Rect r);
RECT RECTFromRect(Gdiplus::Rect r);
Gdiplus::Rect ToGdipRect(const Rect r);
Gdiplus::RectF ToGdipRectF(const Rect r);

RECT ToRECT(const RectFl r);
Rect ToRect(const RectFl r);
Gdiplus::Rect ToGdipRect(const RectFl r);
Gdiplus::RectF ToGdipRectF(const RectFl r);
