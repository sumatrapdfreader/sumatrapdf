/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// ------------- Point

Point::Point(int x, int y) : x(x), y(y) {
}

bool Point::IsEmpty() const {
    return x == 0 && y == 0;
}

bool Point::Eq(int x, int y) const {
    return x == this->x && y == this->y;
}

bool Point::operator==(const Point& other) const {
    return this->x == other.x && this->y == other.y;
}

bool Point::operator!=(const Point& other) const {
    return !this->operator==(other);
}

// ------------- PointF

PointF::PointF(float x, float y) : x(x), y(y) {
}

bool PointF::IsEmpty() const {
    return x == 0 && y == 0;
}

bool PointF::operator==(const PointF& other) const {
    return this->x == other.x && this->y == other.y;
}

bool PointF::operator!=(const PointF& other) const {
    return !this->operator==(other);
}

// ------------- Size

Size::Size(int dx, int dy) : dx(dx), dy(dy) {
}

bool Size::IsEmpty() const {
    return dx == 0 || dy == 0;
}

bool Size::Equals(const Size& other) const {
    return this->dx == other.dx && this->dy == other.dy;
}

bool Size::operator==(const Size& other) const {
    return this->dx == other.dx && this->dy == other.dy;
}

bool Size::operator!=(const Size& other) const {
    return !this->operator==(other);
}

// ------------- SizeF

SizeF::SizeF(float dx, float dy) : dx(dx), dy(dy) {
}

bool SizeF::IsEmpty() const {
    return dx == 0 || dy == 0;
}

bool SizeF::operator==(const SizeF& other) const {
    return this->dx == other.dx && this->dy == other.dy;
}

bool SizeF::operator!=(const SizeF& other) const {
    return !this->operator==(other);
}

// ------------- Rect

Rect::Rect(const RECT r) {
    x = r.left;
    y = r.top;
    dx = r.right - r.left;
    dy = r.bottom - r.top;
}

Rect::Rect(const Gdiplus::RectF r) {
    x = (int)r.X;
    y = (int)r.Y;
    dx = (int)r.Width;
    dy = (int)r.Height;
}

Rect::Rect(int x, int y, int dx, int dy) : x(x), y(y), dx(dx), dy(dy) {
}

Rect::Rect(const Point min, const Point max) : x(min.x), y(min.y), dx(max.x - min.x), dy(max.y - min.y) {
}

bool Rect::EqSize(int otherDx, int otherDy) const {
    return (dx == otherDx) && (dy == otherDy);
}

int Rect::Right() const {
    return x + dx;
}

int Rect::Bottom() const {
    return y + dy;
}

Rect Rect::FromXY(int xs, int ys, int xe, int ye) {
    if (xs > xe) {
        std::swap(xs, xe);
    }
    if (ys > ye) {
        std::swap(ys, ye);
    }
    return Rect(xs, ys, xe - xs, ye - ys);
}

Rect Rect::FromXY(Point TL, Point BR) {
    return FromXY(TL.x, TL.y, BR.x, BR.y);
}

bool Rect::IsEmpty() const {
    return dx == 0 || dy == 0;
}

// endpoint-exclusive, like RECT: https://devblogs.microsoft.com/oldnewthing/20040218-00/?p=40563
bool Rect::Contains(int x, int y) const {
    if (x < this->x) {
        return false;
    }
    if (x >= this->x + this->dx) {
        return false;
    }
    if (y < this->y) {
        return false;
    }
    if (y >= this->y + this->dy) {
        return false;
    }
    return true;
}

bool Rect::Contains(Point pt) const {
    return Contains(pt.x, pt.y);
}

// TODO: check that it's endpoint-exclusive https://devblogs.microsoft.com/oldnewthing/20040218-00/?p=40563
/* Returns an empty rectangle if there's no intersection (see IsEmpty). */
Rect Rect::Intersect(Rect other) const {
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

// TODO: check that it's endpoint-exclusive https://devblogs.microsoft.com/oldnewthing/20040218-00/?p=40563
Rect Rect::Union(Rect other) const {
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

void Rect::Offset(int _x, int _y) {
    x += _x;
    y += _y;
}

void Rect::Inflate(int _x, int _y) {
    x -= _x;
    dx += 2 * _x;
    y -= _y;
    dy += 2 * _y;
}

Point Rect::TL() const {
    return Point(x, y);
}

Point Rect::BR() const {
    return Point(x + dx, y + dy);
}

Size Rect::Size() const {
    return {dx, dy};
}

bool Rect::Equals(const Rect& other) const {
    return this->x == other.x && this->y == other.y && this->dx == other.dx && this->dy == other.dy;
}

bool Rect::operator==(const Rect& other) const {
    return this->x == other.x && this->y == other.y && this->dx == other.dx && this->dy == other.dy;
}

bool Rect::operator!=(const Rect& other) const {
    return !this->operator==(other);
}

// ------------- RectF

// cf. fz_roundrect in mupdf/fitz/base_geometry.c
#ifndef FLT_EPSILON
#define FLT_EPSILON 1.192092896e-07f
#endif

RectF::RectF(const RECT r) {
    x = (float)r.left;
    y = (float)r.top;
    dx = (float)(r.right - r.left);
    dy = (float)(r.bottom - r.top);
}

RectF::RectF(const Gdiplus::RectF r) {
    x = r.X;
    y = r.Y;
    dx = r.Width;
    dy = r.Height;
}

RectF::RectF(float x, float y, float dx, float dy) : x(x), y(y), dx(dx), dy(dy) {
}

RectF::RectF(PointF pt, SizeF size) : x(pt.x), y(pt.y), dx(size.dx), dy(size.dy) {
}

RectF::RectF(PointF min, PointF max) : x(min.x), y(min.y), dx(max.x - min.x), dy(max.y - min.y) {
}

bool RectF::EqSize(float otherDx, float otherDy) const {
    return (dx == otherDx) && (dy == otherDy);
}

float RectF::Right() const {
    return x + dx;
}

float RectF::Bottom() const {
    return y + dy;
}

RectF RectF::FromXY(float xs, float ys, float xe, float ye) {
    if (xs > xe) {
        std::swap(xs, xe);
    }
    if (ys > ye) {
        std::swap(ys, ye);
    }
    return RectF(xs, ys, xe - xs, ye - ys);
}

RectF RectF::FromXY(PointF TL, PointF BR) {
    return FromXY(TL.x, TL.y, BR.x, BR.y);
}

Rect RectF::Round() const {
    return Rect::FromXY((int)floor(x + FLT_EPSILON), (int)floor(y + FLT_EPSILON), (int)ceil(x + dx - FLT_EPSILON),
                        (int)ceil(y + dy - FLT_EPSILON));
}

bool RectF::IsEmpty() const {
    return dx == 0 || dy == 0;
}

// endpoint-exclusive, like RECT: https://devblogs.microsoft.com/oldnewthing/20040218-00/?p=40563
bool RectF::Contains(PointF pt) const {
    if (pt.x < this->x) {
        return false;
    }
    if (pt.x >= this->x + this->dx) {
        return false;
    }
    if (pt.y < this->y) {
        return false;
    }
    if (pt.y >= this->y + this->dy) {
        return false;
    }
    return true;
}

// TODO: check that it's endpoint-exclusive https://devblogs.microsoft.com/oldnewthing/20040218-00/?p=40563
/* Returns an empty rectangle if there's no intersection (see IsEmpty). */
RectF RectF::Intersect(RectF other) const {
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

// TODO: check that it's endpoint-exclusive https://devblogs.microsoft.com/oldnewthing/20040218-00/?p=40563
RectF RectF::Union(RectF other) {
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

void RectF::Offset(float _x, float _y) {
    x += _x;
    y += _y;
}

void RectF::Inflate(float _x, float _y) {
    x -= _x;
    dx += 2 * _x;
    y -= _y;
    dy += 2 * _y;
}

PointF RectF::TL() const {
    return {x, y};
}

PointF RectF::BR() const {
    return {x + dx, y + dy};
}

SizeF RectF::Size() const {
    return SizeF(dx, dy);
}

bool RectF::operator==(const RectF& other) const {
    return this->x == other.x && this->y == other.y && this->dx == other.dx && this->dy == other.dy;
}

bool RectF::operator!=(const RectF& other) const {
    return !this->operator==(other);
}

// ------------- conversion functions

PointF ToPointFl(const Point p) {
    return {(float)p.x, (float)p.y};
}

Gdiplus::Point ToGdipPoint(const Point p) {
    return Gdiplus::Point(p.x, p.y);
}

Point ToPoint(const PointF p) {
    return Point{(int)p.x, (int)p.y};
}

POINT ToPOINT(const Point& p) {
    return {p.x, p.y};
}

Gdiplus::PointF ToGdipPointF(const PointF p) {
    return Gdiplus::PointF(p.x, p.y);
}

SizeF ToSizeFl(const Size s) {
    return {(float)s.dx, (float)s.dy};
}

SIZE ToSIZE(const Size s) {
    return {s.dx, s.dy};
}

Size ToSize(const SizeF s) {
    int dx = (int)floor(s.dx + 0.5);
    int dy = (int)floor(s.dy + 0.5);
    return Size(dx, dy);
}

RectF ToRectF(const Rect& r) {
    return {(float)r.x, (float)r.y, (float)r.dx, (float)r.dy};
}

RECT ToRECT(const Rect& r) {
    return {r.x, r.y, r.x + r.dx, r.y + r.dy};
}

Gdiplus::Rect ToGdipRect(const Rect& r) {
    return Gdiplus::Rect(r.x, r.y, r.dx, r.dy);
}

Gdiplus::RectF ToGdipRectF(const Rect& r) {
    return Gdiplus::RectF((float)r.x, (float)r.y, (float)r.dx, (float)r.dy);
}

RECT ToRECT(const RectF& r) {
    return {(int)r.x, (int)r.y, (int)(r.x + r.dx), (int)(r.y + r.dy)};
}

Rect ToRect(const RectF& r) {
    int x = (int)floor(r.x + 0.5);
    int y = (int)floor(r.y + 0.5);
    int dx = (int)floor(r.dx + 0.5);
    int dy = (int)floor(r.dy + 0.5);
    return Rect(x, y, dx, dy);
}

int RectDx(const RECT& r) {
    return r.right - r.left;
}
int RectDy(const RECT& r) {
    return r.bottom - r.top;
}

Rect ToRect(const RECT& r) {
    Rect r2 = {r.left, r.top, RectDx(r), RectDy(r)};
    return r2;
}

Gdiplus::Rect ToGdipRect(const RectF& r) {
    Rect rect = ToRect(r);
    return Gdiplus::Rect(rect.x, rect.y, rect.dx, rect.dy);
}

Gdiplus::RectF ToGdipRectF(const RectF& r) {
    return Gdiplus::RectF(r.x, r.y, r.dx, r.dy);
}

int NormalizeRotation(int rotation) {
    while (rotation < 0) {
        rotation += 360;
    }
    while (rotation >= 360) {
        rotation -= 360;
    }
    if ((rotation % 90) != 0) {
        CrashIf(true);
        return 0;
    }
    return rotation;
}
