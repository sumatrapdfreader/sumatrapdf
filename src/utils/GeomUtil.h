/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

struct Point {
    int x{0};
    int y{0};

    Point() = default;
    Point(int x, int y);

    // TODO: rename to IsEmpty()
    bool empty();
    bool operator==(const Point& other);
    bool operator!=(const Point& other);
};

struct PointFl {
    float x{0};
    float y{0};

    PointFl() = default;

    PointFl(float x, float y);

    // TODO: rename to IsEmpty()
    bool empty();
    bool operator==(const PointFl& other);
    bool operator!=(const PointFl& other);

    Point ToInt() {
        return Point{(int)x, (int)y};
    }
};

struct Size {
    int dx{0};
    int dy{0};

    Size() = default;
    Size(int dx, int dy) : dx(dx), dy(dy) {
    }

    bool IsEmpty() const {
        return dx == 0 || dy == 0;
    }

    // TODO: temporary
    bool empty() const {
        return dx == 0 || dy == 0;
    }

    bool operator==(const Size& other) const {
        return this->dx == other.dx && this->dy == other.dy;
    }
    bool operator!=(const Size& other) const {
        return !this->operator==(other);
    }
};

struct SizeFl {
    float dx{0};
    float dy{0};

    SizeFl() = default;
    SizeFl(float dx, float dy) : dx(dx), dy(dy) {
    }

    Size ToInt() const {
        return Size((int)floor(dx + 0.5), (int)floor(dy + 0.5));
    }

    bool IsEmpty() const {
        return dx == 0 || dy == 0;
    }

    // TODO: temporary
    bool empty() const {
        return dx == 0 || dy == 0;
    }

    bool operator==(const SizeFl& other) const {
        return this->dx == other.dx && this->dy == other.dy;
    }

    bool operator!=(const SizeFl& other) const {
        return !this->operator==(other);
    }
};

struct Rect {
    int x{0};
    int y{0};
    int dx{0};
    int dy{0};

    Rect() = default;

    Rect(const RECT r) {
        x = r.left;
        y = r.top;
        dx = r.right - r.left;
        dy = r.bottom - r.top;
    }

    Rect(const Gdiplus::RectF r) {
        x = (int)r.X;
        y = (int)r.Y;
        dx = (int)r.Width;
        dy = (int)r.Height;
    }

    Rect(int x, int y, int dx, int dy) : x(x), y(y), dx(dx), dy(dy) {
    }

    Rect(Point pt, Size size) : x(pt.x), y(pt.y), dx(size.dx), dy(size.dy) {
    }

    Rect(Point min, Point max) : x(min.x), y(min.y), dx(max.x - min.x), dy(max.y - min.y) {
    }

    int Width() const {
        return dx;
    }

    int Height() const {
        return dy;
    }

    int Dx() const {
        return dx;
    }

    int Dy() const {
        return dy;
    }

    bool EqSize(int otherDx, int otherDy) const {
        return (dx == otherDx) && (dy == otherDy);
    }

    int Right() const {
        return x + dx;
    }

    int Bottom() const {
        return y + dy;
    }

    static Rect FromXY(int xs, int ys, int xe, int ye) {
        if (xs > xe) {
            std::swap(xs, xe);
        }
        if (ys > ye) {
            std::swap(ys, ye);
        }
        return Rect(xs, ys, xe - xs, ye - ys);
    }

    static Rect FromXY(Point TL, Point BR) {
        return FromXY(TL.x, TL.y, BR.x, BR.y);
    }

    bool IsEmpty() const {
        return dx == 0 || dy == 0;
    }
    bool empty() const {
        return dx == 0 || dy == 0;
    }

    bool Contains(Point pt) const {
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
    Rect Intersect(Rect other) const {
        /* The intersection starts with the larger of the start coordinates
           and ends with the smaller of the end coordinates */
        int _x = std::max(this->x, other.x);
        int _y = std::max(this->y, other.y);
        int _dx = std::min(this->x + this->dx, other.x + other.dx) - _x;
        int _dy = std::min(this->y + this->dy, other.y + other.dy) - _y;

        /* return an empty rectangle if the dimensions aren't positive */
        if (_dx <= 0 || _dy <= 0) {
            return {};
        }
        return {_x, _y, _dx, _dy};
    }

    Rect Union(Rect other) const {
        if (this->dx <= 0 && this->dy <= 0) {
            return other;
        }
        if (other.dx <= 0 && other.dy <= 0) {
            return *this;
        }

        /* The union starts with the smaller of the start coordinates
           and ends with the larger of the end coordinates */
        int _x = std::min(this->x, other.x);
        int _y = std::min(this->y, other.y);
        int _dx = std::max(this->x + this->dx, other.x + other.dx) - _x;
        int _dy = std::max(this->y + this->dy, other.y + other.dy) - _y;

        return {_x, _y, _dx, _dy};
    }

    void Offset(int _x, int _y) {
        x += _x;
        y += _y;
    }

    void Inflate(int _x, int _y) {
        x -= _x;
        dx += 2 * _x;
        y -= _y;
        dy += 2 * _y;
    }

    Point TL() const {
        return Point(x, y);
    }

    Point BR() const {
        return Point(x + dx, y + dy);
    }

    Size Size() const {
        return {dx, dy};
    }

#ifdef _WIN32
    RECT ToRECT() const {
        return {x, y, x + dx, y + dy};
    }

    static Rect FromRECT(const RECT& rect) {
        return FromXY(rect.left, rect.top, rect.right, rect.bottom);
    }

#if 1 // def GDIPVER, note: GDIPVER not defined in mingw?
    Gdiplus::Rect ToGdipRect() {
        return Gdiplus::Rect(x, y, dx, dy);
    }
    Gdiplus::RectF ToGdipRectF() {
        return Gdiplus::RectF((float)x, (float)y, (float)dx, (float)dy);
    }
#endif
#endif

    bool operator==(const Rect& other) const {
        return this->x == other.x && this->y == other.y && this->dx == other.dx && this->dy == other.dy;
    }
    bool operator!=(const Rect& other) const {
        return !this->operator==(other);
    }
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

    Rect ToInt() const {
        return Rect((int)floor(x + 0.5), (int)floor(y + 0.5), (int)floor(dx + 0.5), (int)floor(dy + 0.5));
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

#ifdef _WIN32
    RECT ToRECT() const {
        Rect rectI(this->ToInt());
        return {rectI.x, rectI.y, rectI.x + rectI.dx, rectI.y + rectI.dy};
    }

    static RectFl FromRECT(const RECT& rect) {
        return FromXY((float)rect.left, (float)rect.top, (float)rect.right, (float)rect.bottom);
    }

#if 1 // def GDIPVER, note: GDIPVER not defined in mingw?
    Gdiplus::Rect ToGdipRect() const {
        Rect rect(this->ToInt());
        return Gdiplus::Rect(rect.x, rect.y, rect.dx, rect.dy);
    }
    Gdiplus::RectF ToGdipRectF() const {
        return Gdiplus::RectF(x, y, dx, dy);
    }
#endif
#endif

    bool operator==(const RectFl& other) const {
        return this->x == other.x && this->y == other.y && this->dx == other.dx && this->dy == other.dy;
    }
    bool operator!=(const RectFl& other) const {
        return !this->operator==(other);
    }
};

inline PointFl ToPointFl(Point p) {
    return {(float)p.x, (float)p.y};
}

inline SizeFl ToSizeFl(Size s) {
    return {(float)s.dx, (float)s.dy};
}

inline SIZE ToSIZE(Size s) {
    return {s.dx, s.dy};
}

inline RectFl ToRectFl(const Rect& r) {
    return {(float)r.x, (float)r.y, (float)r.dx, (float)r.dy};
}
