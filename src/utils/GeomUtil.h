/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

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

    bool operator==(Point<T>& other) {
        return this->x == other.x && this->y == other.y;
    }
    bool operator!=(Point<T>& other) {
        return !this->operator==(other);
    }
};

typedef Point<int> PointI;
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

    bool operator==(Size<T>& other) {
        return this->dx == other.dx && this->dy == other.dy;
    }
    bool operator!=(Size<T>& other) {
        return !this->operator==(other);
    }
};

typedef Size<int> SizeI;
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

    static Rect FromXY(T xs, T ys, T xe, T ye) {
        if (xs > xe)
            swap(xs, xe);
        if (ys > ye)
            swap(ys, ye);
        return Rect(xs, ys, xe - xs, ye - ys);
    }
    static Rect FromXY(Point<T> TL, Point<T> BR) {
        return FromXY(TL.x, TL.y, BR.x, BR.y);
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
    // cf. fz_roundrect in mupdf/fitz/base_geometry.c
    Rect<int> Round() const {
        return Rect<int>::FromXY((int)floor(x + 0.001),
                                 (int)floor(y + 0.001),
                                 (int)ceil(x + dx - 0.001),
                                 (int)ceil(y + dy - 0.001));
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

#ifdef _WIN32
    RECT ToRECT() const {
        Rect<int> rectI(this->Convert<int>());
        RECT result = { rectI.x, rectI.y, rectI.x + rectI.dx, rectI.y + rectI.dy };
        return result;
    }
    static Rect FromRECT(RECT& rect) {
        return FromXY(rect.left, rect.top, rect.right, rect.bottom);
    }
#endif

    bool operator==(Rect<T>& other) {
        return this->x == other.x && this->y == other.y &&
               this->dx == other.dx && this->dy == other.dy;
    }
    bool operator!=(Rect<T>& other) {
        return !this->operator==(other);
    }
};

typedef Rect<int> RectI;
typedef Rect<double> RectD;

#ifdef _WIN32

class ClientRect : public RectI {
public:
    ClientRect(HWND hwnd) {
        RECT rc;
        if (GetClientRect(hwnd, &rc)) {
            x = rc.left; dx = rc.right - rc.left;
            y = rc.top; dy = rc.bottom - rc.top;
        }
    }
};

class WindowRect : public RectI {
public:
    WindowRect(HWND hwnd) {
        RECT rc;
        if (GetWindowRect(hwnd, &rc)) {
            x = rc.left; dx = rc.right - rc.left;
            y = rc.top; dy = rc.bottom - rc.top;
        }
    }
};

inline RectI MapRectToWindow(RectI rect, HWND hwndFrom, HWND hwndTo)
{
    RECT rc = rect.ToRECT();
    MapWindowPoints(hwndFrom, hwndTo, (LPPOINT)&rc, 2);
    return RectI::FromRECT(rc);
}

#endif

#endif
