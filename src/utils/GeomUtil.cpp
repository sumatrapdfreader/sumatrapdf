/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

Point::Point(int x, int y) : x(x), y(y) {
}

bool Point::empty()  const{
    return x == 0 && y == 0;
}

bool Point::operator==(const Point& other)  const{
    return this->x == other.x && this->y == other.y;
}
bool Point::operator!=(const Point& other)  const{
    return !this->operator==(other);
}

PointFl::PointFl(float x, float y) : x(x), y(y) {
}

bool PointFl::empty()  const{
    return x == 0 && y == 0;
}

bool PointFl::operator==(const PointFl& other)  const{
    return this->x == other.x && this->y == other.y;
}
bool PointFl::operator!=(const PointFl& other)  const{
    return !this->operator==(other);
}

Size::Size(int dx, int dy) : dx(dx), dy(dy) {
}

bool Size::IsEmpty() const {
    return dx == 0 || dy == 0;
}

// TODO: temporary
bool Size::empty() const {
    return dx == 0 || dy == 0;
}

bool Size::operator==(const Size& other) const {
    return this->dx == other.dx && this->dy == other.dy;
}
bool Size::operator!=(const Size& other) const {
    return !this->operator==(other);
}

SizeFl::SizeFl(float dx, float dy) : dx(dx), dy(dy) {
}

bool SizeFl::IsEmpty() const {
    return dx == 0 || dy == 0;
}

// TODO: temporary
bool SizeFl::empty() const {
    return dx == 0 || dy == 0;
}

bool SizeFl::operator==(const SizeFl& other) const {
    return this->dx == other.dx && this->dy == other.dy;
}

bool SizeFl::operator!=(const SizeFl& other) const {
    return !this->operator==(other);
}

PointFl ToPointFl(const Point p) {
    return {(float)p.x, (float)p.y};
}

Point ToPoint(const PointFl p) {
    return Point{(int)p.x, (int)p.y};
}

SizeFl ToSizeFl(const Size s) {
    return {(float)s.dx, (float)s.dy};
}

SIZE ToSIZE(const Size s) {
    return {s.dx, s.dy};
}

Size ToSize(const SizeFl s) {
    int dx = (int)floor(s.dx + 0.5);
    int dy = (int)floor(s.dy + 0.5);
    return Size(dx, dy);
}

RectFl ToRectFl(const Rect r) {
    return {(float)r.x, (float)r.y, (float)r.dx, (float)r.dy};
}

RECT ToRECT(const Rect r) {
    return {r.x, r.y, r.x + r.dx, r.y + r.dy};
}

Gdiplus::Rect ToGdipRect(const Rect r) {
    return Gdiplus::Rect(r.x, r.y, r.dx, r.dy);
}

Gdiplus::RectF ToGdipRectF(const Rect r) {
    return Gdiplus::RectF((float)r.x, (float)r.y, (float)r.dx, (float)r.dy);
}

RECT ToRECT(const RectFl r) {
    return {(int)r.x, (int)r.y, (int)(r.x + r.dx), (int)(r.y + r.dy)};
}

Rect ToRect(const RectFl r) {
    int x = (int)floor(r.x + 0.5);
    int y = (int)floor(r.y + 0.5);
    int dx = (int)floor(r.dx + 0.5);
    int dy = (int)floor(r.dy + 0.5);
    return Rect(x, y, dx, dy);
}

Gdiplus::Rect ToGdipRect(const RectFl r) {
    Rect rect = ToRect(r);
    return Gdiplus::Rect(rect.x, rect.y, rect.dx, rect.dy);
}

Gdiplus::RectF ToGdipRectF(const RectFl r) {
    return Gdiplus::RectF(r.x, r.y, r.dx, r.dy);
}
