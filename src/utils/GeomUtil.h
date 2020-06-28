/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

struct Point {
    int x{0};
    int y{0};

    Point() = default;
    Point(int x, int y);

    // TODO: rename to IsEmpty()
    bool empty() const;
    bool operator==(const Point& other) const;
    bool operator!=(const Point& other) const;
};

struct PointFl {
    float x{0};
    float y{0};

    PointFl() = default;

    PointFl(float x, float y);

    // TODO: rename to IsEmpty()
    bool empty() const;
    bool operator==(const PointFl& other) const;
    bool operator!=(const PointFl& other) const;
};

struct Size {
    int dx{0};
    int dy{0};

    Size() = default;
    Size(int dx, int dy);

    bool IsEmpty() const;
    // TODO: temporary
    bool empty() const;
    bool operator==(const Size& other) const;
    bool operator!=(const Size& other) const;
};

struct SizeFl {
    float dx{0};
    float dy{0};

    SizeFl() = default;
    SizeFl(float dx, float dy);

    bool IsEmpty() const;
    // TODO: temporary
    bool empty() const;
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

    int Width() const;
    int Height() const;
    int Dx() const;
    int Dy() const;
    bool EqSize(int otherDx, int otherDy) const;
    int Right() const;
    int Bottom() const;
    static Rect FromXY(int xs, int ys, int xe, int ye);
    static Rect FromXY(Point TL, Point BR);
    bool IsEmpty() const;
    bool empty() const;
    bool Contains(Point pt) const;
    Rect Intersect(Rect other) const;
    Rect Union(Rect other) const;
    void Offset(int _x, int _y);
    void Inflate(int _x, int _y);
    Point TL() const;
    Point BR() const;
    Size Size() const;
    static Rect FromRECT(const RECT& rect);
    bool operator==(const Rect& other) const;
    bool operator!=(const Rect& other) const;
};

struct RectFl {
    float x{0};
    float y{0};
    float dx{0};
    float dy{0};

    RectFl() = default;

    RectFl(const RECT r) {
        x = (float)r.left;
        y = (float)r.top;
        dx = (float)(r.right - r.left);
        dy = (float)(r.bottom - r.top);
    }

    RectFl(const Gdiplus::RectF r) {
        x = r.X;
        y = r.Y;
        dx = r.Width;
        dy = r.Height;
    }

    RectFl(float x, float y, float dx, float dy) : x(x), y(y), dx(dx), dy(dy) {
    }

    RectFl(PointFl pt, SizeFl size) : x(pt.x), y(pt.y), dx(size.dx), dy(size.dy) {
    }

    RectFl(PointFl min, PointFl max) : x(min.x), y(min.y), dx(max.x - min.x), dy(max.y - min.y) {
    }

    float Width() const {
        return dx;
    }

    float Height() const {
        return dy;
    }

    float Dx() const {
        return dx;
    }

    float Dy() const {
        return dy;
    }

    bool EqSize(float otherDx, float otherDy) {
        return (dx == otherDx) && (dy == otherDy);
    }

    float Right() const {
        return x + dx;
    }

    float Bottom() const {
        return y + dy;
    }

    static RectFl FromXY(float xs, float ys, float xe, float ye) {
        if (xs > xe) {
            std::swap(xs, xe);
        }
        if (ys > ye) {
            std::swap(ys, ye);
        }
        return RectFl(xs, ys, xe - xs, ye - ys);
    }
    static RectFl FromXY(PointFl TL, PointFl BR) {
        return FromXY(TL.x, TL.y, BR.x, BR.y);
    }

    // cf. fz_roundrect in mupdf/fitz/base_geometry.c
#ifndef FLT_EPSILON
#define FLT_EPSILON 1.192092896e-07f
#endif
    Rect Round() const {
        return Rect::FromXY((int)floor(x + FLT_EPSILON), (int)floor(y + FLT_EPSILON), (int)ceil(x + dx - FLT_EPSILON),
                            (int)ceil(y + dy - FLT_EPSILON));
    }

    bool IsEmpty() const {
        return dx == 0 || dy == 0;
    }
    bool empty() const {
        return dx == 0 || dy == 0;
    }

    bool Contains(PointFl pt) {
        if (pt.x < this->x) {
            return false;
        }
        if (pt.x > this->x + this->dx) {
            return false;
        }
        if (pt.y < this->y) {
            return false;
        }
        if (pt.y > this->y + this->dy) {
            return false;
        }
        return true;
    }

    /* Returns an empty rectangle if there's no intersection (see IsEmpty). */
    RectFl Intersect(RectFl other) {
        /* The intersection starts with the larger of the start coordinates
           and ends with the smaller of the end coordinates */
        float _x = std::max(this->x, other.x);
        float _y = std::max(this->y, other.y);
        float _dx = std::min(this->x + this->dx, other.x + other.dx) - _x;
        float _dy = std::min(this->y + this->dy, other.y + other.dy) - _y;

        /* return an empty rectangle if the dimensions aren't positive */
        if (_dx <= 0 || _dy <= 0) {
            return {};
        }
        return {_x, _y, _dx, _dy};
    }

    RectFl Union(RectFl other) {
        if (this->dx <= 0 && this->dy <= 0) {
            return other;
        }
        if (other.dx <= 0 && other.dy <= 0) {
            return *this;
        }

        /* The union starts with the smaller of the start coordinates
           and ends with the larger of the end coordinates */
        float _x = std::min(this->x, other.x);
        float _y = std::min(this->y, other.y);
        float _dx = std::max(this->x + this->dx, other.x + other.dx) - _x;
        float _dy = std::max(this->y + this->dy, other.y + other.dy) - _y;

        return {_x, _y, _dx, _dy};
    }

    void Offset(float _x, float _y) {
        x += _x;
        y += _y;
    }

    void Inflate(float _x, float _y) {
        x -= _x;
        dx += 2 * _x;
        y -= _y;
        dy += 2 * _y;
    }

    PointFl TL() const {
        return {x, y};
    }

    PointFl BR() const {
        return {x + dx, y + dy};
    }

    SizeFl Size() const {
        return SizeFl(dx, dy);
    }

    static RectFl FromRECT(const RECT& rect) {
        return FromXY((float)rect.left, (float)rect.top, (float)rect.right, (float)rect.bottom);
    }

    bool operator==(const RectFl& other) const {
        return this->x == other.x && this->y == other.y && this->dx == other.dx && this->dy == other.dy;
    }
    bool operator!=(const RectFl& other) const {
        return !this->operator==(other);
    }
};

PointFl ToPointFl(const Point p);
Point ToPoint(const PointFl p);

SIZE ToSIZE(const Size s);
SizeFl ToSizeFl(const Size s);
Size ToSize(const SizeFl s);

RectFl ToRectFl(const Rect r);
RECT ToRECT(const Rect r);
Gdiplus::Rect ToGdipRect(const Rect r);
Gdiplus::RectF ToGdipRectF(const Rect r);

RECT ToRECT(const RectFl r);
Rect ToRect(const RectFl r);
Gdiplus::Rect ToGdipRect(const RectFl r);
Gdiplus::RectF ToGdipRectF(const RectFl r);
