/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

namespace geomutil {

template <typename T>
class PointT {
  public:
    T x, y;

    PointT() : x(0), y(0) {
    }
    PointT(T x, T y) : x(x), y(y) {
    }

    template <typename S>
    PointT<S> Convert() const {
        return PointT<S>((S)x, (S)y);
    }

    PointT<int> ToInt() const {
        return PointT<int>((int)floor(x + 0.5), (int)floor(y + 0.5));
    }

    bool operator==(const PointT<T>& other) const {
        return this->x == other.x && this->y == other.y;
    }
    bool operator!=(const PointT<T>& other) const {
        return !this->operator==(other);
    }
};

template <typename T>
class SizeT {
  public:
    T dx, dy;

    SizeT() : dx(0), dy(0) {
    }
    SizeT(T dx, T dy) : dx(dx), dy(dy) {
    }

    template <typename S>
    SizeT<S> Convert() const {
        return SizeT<S>((S)dx, (S)dy);
    }

    SizeT<int> ToInt() const {
        return SizeT<int>((int)floor(dx + 0.5), (int)floor(dy + 0.5));
    }

    bool IsEmpty() const {
        return dx == 0 || dy == 0;
    }

    bool operator==(const SizeT<T>& other) const {
        return this->dx == other.dx && this->dy == other.dy;
    }
    bool operator!=(const SizeT<T>& other) const {
        return !this->operator==(other);
    }
};

template <typename T>
class RectT {
  public:
    T x, y;
    T dx, dy;

    RectT() : x(0), y(0), dx(0), dy(0) {
    }
    RectT(T x, T y, T dx, T dy) : x(x), y(y), dx(dx), dy(dy) {
    }
    RectT(PointT<T> pt, SizeT<T> size) : x(pt.x), y(pt.y), dx(size.dx), dy(size.dy) {
    }

    static RectT FromXY(T xs, T ys, T xe, T ye) {
        if (xs > xe)
            std::swap(xs, xe);
        if (ys > ye)
            std::swap(ys, ye);
        return RectT(xs, ys, xe - xs, ye - ys);
    }
    static RectT FromXY(PointT<T> TL, PointT<T> BR) {
        return FromXY(TL.x, TL.y, BR.x, BR.y);
    }

    template <typename S>
    RectT<S> Convert() const {
        return RectT<S>((S)x, (S)y, (S)dx, (S)dy);
    }

    RectT<int> ToInt() const {
        return RectT<int>((int)floor(x + 0.5), (int)floor(y + 0.5), (int)floor(dx + 0.5), (int)floor(dy + 0.5));
    }
    // cf. fz_roundrect in mupdf/fitz/base_geometry.c
#ifndef FLT_EPSILON
#define FLT_EPSILON 1.192092896e-07f
#endif
    RectT<int> Round() const {
        return RectT<int>::FromXY((int)floor(x + FLT_EPSILON), (int)floor(y + FLT_EPSILON),
                                  (int)ceil(x + dx - FLT_EPSILON), (int)ceil(y + dy - FLT_EPSILON));
    }

    bool IsEmpty() const {
        return dx == 0 || dy == 0;
    }
    bool empty() const {
        return dx == 0 || dy == 0;
    }

    bool Contains(PointT<T> pt) const {
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
    RectT Intersect(RectT other) const {
        /* The intersection starts with the larger of the start coordinates
           and ends with the smaller of the end coordinates */
        T _x = std::max(this->x, other.x);
        T _y = std::max(this->y, other.y);
        T _dx = std::min(this->x + this->dx, other.x + other.dx) - _x;
        T _dy = std::min(this->y + this->dy, other.y + other.dy) - _y;

        /* return an empty rectangle if the dimensions aren't positive */
        if (_dx <= 0 || _dy <= 0)
            return RectT();
        return RectT(_x, _y, _dx, _dy);
    }

    RectT Union(RectT other) const {
        if (this->dx <= 0 && this->dy <= 0)
            return other;
        if (other.dx <= 0 && other.dy <= 0)
            return *this;

        /* The union starts with the smaller of the start coordinates
           and ends with the larger of the end coordinates */
        T _x = std::min(this->x, other.x);
        T _y = std::min(this->y, other.y);
        T _dx = std::max(this->x + this->dx, other.x + other.dx) - _x;
        T _dy = std::max(this->y + this->dy, other.y + other.dy) - _y;

        return RectT(_x, _y, _dx, _dy);
    }

    void Offset(T _x, T _y) {
        x += _x;
        y += _y;
    }

    void Inflate(T _x, T _y) {
        x -= _x;
        dx += 2 * _x;
        y -= _y;
        dy += 2 * _y;
    }

    PointT<T> TL() const {
        return PointT<T>(x, y);
    }
    PointT<T> BR() const {
        return PointT<T>(x + dx, y + dy);
    }
    SizeT<T> Size() const {
        return SizeT<T>(dx, dy);
    }

#ifdef _WIN32
    RECT ToRECT() const {
        RectT<int> rectI(this->ToInt());
        RECT result = {rectI.x, rectI.y, rectI.x + rectI.dx, rectI.y + rectI.dy};
        return result;
    }
    static RectT FromRECT(const RECT& rect) {
        return FromXY(rect.left, rect.top, rect.right, rect.bottom);
    }

#if 1 // def GDIPVER, note: GDIPVER not defined in mingw?
    Gdiplus::Rect ToGdipRect() const {
        RectT<int> rect(this->ToInt());
        return Gdiplus::Rect(rect.x, rect.y, rect.dx, rect.dy);
    }
    Gdiplus::RectF ToGdipRectF() const {
        RectT<float> rectF(this->Convert<float>());
        return Gdiplus::RectF(rectF.x, rectF.y, rectF.dx, rectF.dy);
    }
#endif
#endif

    bool operator==(const RectT<T>& other) const {
        return this->x == other.x && this->y == other.y && this->dx == other.dx && this->dy == other.dy;
    }
    bool operator!=(const RectT<T>& other) const {
        return !this->operator==(other);
    }
};

} // namespace geomutil

typedef geomutil::SizeT<int> SizeI;
typedef geomutil::SizeT<double> SizeD;

typedef geomutil::PointT<int> PointI;
typedef geomutil::PointT<double> PointD;

typedef geomutil::RectT<int> RectI;
typedef geomutil::RectT<double> RectD;

inline SIZE ToSIZE(SizeI s) {
    return {s.dx, s.dy};
}
#ifdef _WIN32

class ClientRect : public RectI {
  public:
    explicit ClientRect(HWND hwnd) {
        RECT rc;
        if (GetClientRect(hwnd, &rc)) {
            x = rc.left;
            dx = rc.right - rc.left;
            y = rc.top;
            dy = rc.bottom - rc.top;
        }
    }
};

class WindowRect : public RectI {
  public:
    explicit WindowRect(HWND hwnd) {
        RECT rc;
        if (GetWindowRect(hwnd, &rc)) {
            x = rc.left;
            dx = rc.right - rc.left;
            y = rc.top;
            dy = rc.bottom - rc.top;
        }
    }
};

inline RectI MapRectToWindow(RectI rect, HWND hwndFrom, HWND hwndTo) {
    RECT rc = rect.ToRECT();
    MapWindowPoints(hwndFrom, hwndTo, (LPPOINT)&rc, 2);
    return RectI::FromRECT(rc);
}

#endif
