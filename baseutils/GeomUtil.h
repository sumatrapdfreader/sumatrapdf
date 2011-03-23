/* Copyright 2006-2011 the SumatraPDF project authors (see ../AUTHORS file).
   License: FreeBSD (see ./COPYING) */

#ifndef GeomUtil_h
#define GeomUtil_h

#include "BaseUtil.h"
#include <math.h>

template <typename T>
class Point
{
public:
    T x, y;

    Point() : x(0), y(0) { }
    Point(T x, T y) : x(x), y(y) { }

    template <typename S>
    Point<S> Convert() const {
        return Point<S>((S)x, (S)y);
    }
    template <>
    Point<int> Convert() const {
        return Point<int>((int)floor(x + 0.5), (int)floor(y + 0.5));
    }
};

typedef Point<int> PointI;
typedef Point<float> PointF;
typedef Point<double> PointD;

template <typename T>
class Size
{
public :
    T dx, dy;

    Size() : dx(0), dy(0) { }
    Size(T dx, T dy) : dx(dx), dy(dy) { }

    template <typename S>
    Size<S> Convert() const {
        return Size<S>((S)dx, (S)dy);
    }
    template <>
    Size<int> Convert() const {
        return Size<int>((int)floor(dx + 0.5), (int)floor(dy + 0.5));
    }
};

typedef Size<int> SizeI;
typedef Size<float> SizeF;
typedef Size<double> SizeD;

template <typename T>
class Rect
{
public:
    T x, y;
    T dx, dy;

    Rect() : x(0), y(0), dx(0), dy(0) { }
    Rect(T x, T y, T dx, T dy) : x(x), y(y), dx(dx), dy(dy) { }
    Rect(Point<T> pt, Size<T> size) : x(pt.x), y(pt.y), dx(size.dx), dy(size.dy) { }
    Rect(Point<T> TL, Point<T> BR) : x(TL.x), y(TL.y), dx(BR.x - TL.x), dy(BR.y - TL.y) { }

    static Rect FromXY(T xs, T ys, T xe, T ye) {
        if (xs > xe)
            swap(xs, xe);
        if (ys > ye)
            swap(ys, ye);
        return Rect(xs, ys, xe - xs, ye - ys);
    }

    template <typename S>
    Rect<S> Convert() const {
        return Rect<S>((S)x, (S)y, (S)dx, (S)dy);
    }
    template <>
    Rect<int> Convert() const {
        return Rect<int>((int)floor(x + 0.5), (int)floor(y + 0.5),
                         (int)floor(dx + 0.5), (int)floor(dy + 0.5));
    }

    bool IsEmpty() const {
        return dx == 0 || dy == 0;
    }

    bool Inside(Point<T> pt) const {
        if (pt.x < this->x)
            return false;
        if (pt.x > this->x + this->dx)
            return false;
        if (pt.y < this->y)
            return false;
        if (pt.y > this->y + this->dy)
            return false;
        return true;
    }

    /* Returns an empty rectangle if there's no intersection (see IsEmpty). */
    Rect Intersect(Rect other) const {
        /* The intersection starts with the larger of the start
           coordinates and ends with the smaller of the end coordinates */
        T x = max(this->x, other.x);
        T dx = min(this->x + this->dx, other.x + other.dx) - x;
        T y = max(this->y, other.y);
        T dy = min(this->y + this->dy, other.y + other.dy) - y;

        /* return an empty rectangle if the dimensions aren't positive */
        if (dx <= 0 || dy <= 0)
            return Rect();
        return Rect(x, y, dx, dy);
    }

    Rect Union(Rect other) const {
        if (this->dx <= 0 && this->dy <= 0)
            return other;
        if (other.dx <= 0 && other.dy <= 0)
            return *this;

        T x = min(this->x, other.x);
        T y = min(this->y, other.y);
        T dx = max(this->x + this->dx, other.x + other.dx) - x;
        T dy = max(this->y + this->dy, other.y + other.dy) - y;

        return Rect(x, y, dx, dy);
    }

    void Offset(T _x, T _y) {
        x += _x;
        y += _y;
    }

    void Inflate(T _x, T _y) {
        x -= _x; dx += 2 * _x;
        y -= _y; dy += 2 * _y;
    }

    Point<T> TL() const { return Point<T>(x, y); }
    Point<T> BR() const { return Point<T>(x + dx, y + dy); }
    Size<T> Size() const { return ::Size<T>(dx, dy); }

    RECT ToRECT() const {
        Rect<int> rectI(this->Convert<int>());
        RECT result = { rectI.x, rectI.y, rectI.x + rectI.dx, rectI.y + rectI.dy };
        return result;
    }
};

typedef Rect<int> RectI;
typedef Rect<float> RectF;
typedef Rect<double> RectD;

#endif
