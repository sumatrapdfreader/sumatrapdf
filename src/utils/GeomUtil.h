/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

struct Point {
    int x = 0;
    int y = 0;

    Point() = default;
    Point(int x, int y);

    bool IsEmpty() const;
    bool Eq(int x, int y) const;
    bool operator==(const Point& other) const;
    bool operator!=(const Point& other) const;
};

struct PointF {
    float x = 0;
    float y = 0;

    PointF() = default;

    PointF(float x, float y);

    bool IsEmpty() const;
    bool operator==(const PointF& other) const;
    bool operator!=(const PointF& other) const;
};

struct Size {
    int dx = 0;
    int dy = 0;

    Size() = default;
    Size(int dx, int dy);

    bool IsEmpty() const;

    bool Equals(const Size& other) const;
    bool operator==(const Size& other) const;
    bool operator!=(const Size& other) const;
};

struct SizeF {
    float dx = 0;
    float dy = 0;

    SizeF() = default;
    SizeF(float dx, float dy);

    bool IsEmpty() const;

    bool operator==(const SizeF& other) const;
    bool operator!=(const SizeF& other) const;
};

struct Rect {
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;

    Rect() = default;
    Rect(RECT r);           // NOLINT
    Rect(Gdiplus::RectF r); // NOLINT
    Rect(int x, int y, int dx, int dy);
    // TODO: why not working if in .cpp? Confused by Size also being a method?
    Rect(const Point pt, const Size sz) : x(pt.x), y(pt.y), dx(sz.dx), dy(sz.dy) {
    }
    Rect(Point min, Point max);

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
    bool Equals(const Rect& other) const;
    bool operator==(const Rect& other) const;
    bool operator!=(const Rect& other) const;
};

struct RectF {
    float x = 0;
    float y = 0;
    float dx = 0;
    float dy = 0;

    RectF() = default;

    explicit RectF(RECT r);
    RectF(Gdiplus::RectF r); // NOLINT
    RectF(float x, float y, float dx, float dy);
    RectF(PointF pt, SizeF size);
    RectF(PointF min, PointF max);

    bool EqSize(float otherDx, float otherDy) const;
    float Right() const;
    float Bottom() const;
    static RectF FromXY(float xs, float ys, float xe, float ye);
    static RectF FromXY(PointF TL, PointF BR);
    Rect Round() const;
    bool IsEmpty() const;
    bool Contains(PointF pt) const;
    RectF Intersect(RectF other) const;
    RectF Union(RectF other);
    void Offset(float _x, float _y);
    void Inflate(float _x, float _y);
    PointF TL() const;
    PointF BR() const;
    SizeF Size() const;
    bool operator==(const RectF& other) const;
    bool operator!=(const RectF& other) const;
};

int RectDx(const RECT& r);
int RectDy(const RECT& r);

PointF ToPointFl(Point p);
Gdiplus::Point ToGdipPoint(Point p);
Point ToPoint(PointF p);
Gdiplus::PointF ToGdipPointF(PointF p);
POINT ToPOINT(const Point& p);

SIZE ToSIZE(Size s);
SizeF ToSizeFl(Size s);
Size ToSize(SizeF s);

RectF ToRectF(const Rect& r);

RECT ToRECT(const Rect& r);
RECT ToRECT(const RectF& r);

Rect ToRect(const RectF& r);
Rect ToRect(const RECT& r);

Gdiplus::Rect ToGdipRect(const Rect& r);
Gdiplus::RectF ToGdipRectF(const Rect& r);

Gdiplus::Rect ToGdipRect(const RectF& r);
Gdiplus::RectF ToGdipRectF(const RectF& r);

int NormalizeRotation(int rotation);
