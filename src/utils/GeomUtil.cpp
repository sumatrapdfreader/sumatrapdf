/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

Point::Point(int x, int y) : x(x), y(y) {
}

bool Point::empty() {
    return x == 0 && y == 0;
}

bool Point::operator==(const Point& other) {
    return this->x == other.x && this->y == other.y;
}
bool Point::operator!=(const Point& other) {
    return !this->operator==(other);
}

PointFl::PointFl(float x, float y) : x(x), y(y) {
}

bool PointFl::empty() {
    return x == 0 && y == 0;
}

bool PointFl::operator==(const PointFl& other) {
    return this->x == other.x && this->y == other.y;
}
bool PointFl::operator!=(const PointFl& other) {
    return !this->operator==(other);
}
