/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

DirectionalLayout::~DirectionalLayout()
{
    for (DirectionalLayoutData *e = els.IterStart(); e; e = els.IterNext()) {
        if (e->ownsElement)
            delete e->element;
    }
}

DirectionalLayout& DirectionalLayout::Add(DirectionalLayoutData& ld, bool ownsElement)
{
    ld.ownsElement = ownsElement;
    els.Append(ld);
    return *this;
}

void DirectionalLayout::Measure(const Size availableSize)
{
    for (DirectionalLayoutData *e = els.IterStart(); e; e = els.IterNext()) {
        e->element->Measure(availableSize);
        e->desiredSize = e->element->DesiredSize();
    }
}

static int CalcScaledClippedSize(int size, float scale, int selfSize)
{
    int scaledSize = selfSize;
    if (SizeSelf != scale)
        scaledSize = (int)((float)size * scale);
    if (scaledSize > size)
        scaledSize = size;
    return scaledSize;
}

struct SizeInfo {
    int     size;
    float   scale;

    int     finalPos;
    int     finalSize;
};

static void RedistributeSizes(SizeInfo *sizes, size_t sizesCount, int totalSize)
{
    SizeInfo *si;
    float toDistributeTotal = 0.f;
    int remainingSpace = totalSize;

    for (size_t i = 0; i < sizesCount; i++) {
        si = &(sizes[i]);
        if (SizeSelf == si->scale)
            remainingSpace -= si->size;
        else
            toDistributeTotal += si->scale;
    }

    int pos = 0;
    for (size_t i = 0; i < sizesCount; i++) {
        si = &(sizes[i]);
        if (SizeSelf == si->scale) {
            si->finalSize = si->size;
        } else {
            si->finalSize = 0;
            if ((remainingSpace > 0) && (0.f != toDistributeTotal)) {
                si->finalSize = (int)(((float)remainingSpace * si->scale) / toDistributeTotal);
            }
        }
        si->finalPos = pos;
        pos += si->finalSize;
    }
}

void HorizontalLayout::Arrange(const Rect finalRect)
{
    DirectionalLayoutData * e;
    SizeInfo *              si;
    Vec<SizeInfo>           sizes;

    for (e = els.IterStart(); e; e = els.IterNext()) {
        SizeInfo sizeInfo = { e->desiredSize.Width, e->sizeLayoutAxis, 0, 0 };
        sizes.Append(sizeInfo);
    }
    RedistributeSizes(sizes.LendData(), sizes.Count(), finalRect.Width);

    for (e = els.IterStart(), si = sizes.IterStart(); e; e = els.IterNext(), si = sizes.IterNext()) {
        int dy = CalcScaledClippedSize(finalRect.Height, e->sizeNonLayoutAxis, e->desiredSize.Height);
        int y  = e->alignNonLayoutAxis.CalcOffset(dy, finalRect.Height);
        e->element->Arrange(Rect(si->finalPos, y, si->finalSize, dy));
    }
}

void VerticalLayout::Arrange(const Rect finalRect)
{
    DirectionalLayoutData * e;
    SizeInfo *              si;
    Vec<SizeInfo>           sizes;

    for (e = els.IterStart(); e; e = els.IterNext()) {
        SizeInfo sizeInfo = { e->desiredSize.Height, e->sizeLayoutAxis, 0, 0 };
        sizes.Append(sizeInfo);
    }
    RedistributeSizes(sizes.LendData(), sizes.Count(), finalRect.Height);

    for (e = els.IterStart(), si = sizes.IterStart(); e; e = els.IterNext(), si = sizes.IterNext()) {
        int dx = CalcScaledClippedSize(finalRect.Width, e->sizeNonLayoutAxis, e->desiredSize.Width);
        int x  = e->alignNonLayoutAxis.CalcOffset(dx, finalRect.Width);
        e->element->Arrange(Rect(x, si->finalPos, dx, si->finalSize));
    }
}

// TODO: to properly handle adding element to the grid at any
// time, we would need to request repaint here, so we would need
// to know parent window, the way Control does, further
// unifying the notion of control and layout.
GridLayout& GridLayout::Add(GridLayoutData& ld)
{
    els.Append(ld);
    dirty = true;
    return *this;
}

GridLayout::~GridLayout()
{
    free(cells);
}

GridLayout::GridCell *GridLayout::GetCell(int row, int col) const
{
    CrashIf(row >= rows);
    CrashIf(col >= cols);
    return &cells[col + row * rows];
}

void GridLayout::RebuildCellData()
{
    // calculate how many columns and rows we need and build 2d cells
    // array, a cell for each column/row
    int cols = 0, rows = 0;
    for (GridLayoutData *d = els.IterStart(); d; d = els.IterNext()) {
        if (d->col >= cols)
            cols = d->col + 1;
        if (d->row >= rows)
            rows = d->row + 1;
    }
    this->rows = rows;
    this->cols = cols;

    free(cells);
    cells = AllocArray<GridCell>(cols * rows);
    GridCell *cell;
    for (GridLayoutData *d = els.IterStart(); d; d = els.IterNext()) {
        cell = GetCell(d->row, d->col);
        cell->el = d->el;
    }
    // TODO: collapse empty rows (where each cell has no element)

    // TODO: not sure if I want to disallow empty grids, but do for now
    CrashIf(0 == rows);
    CrashIf(0 == cols);
}

void GridLayout::Measure(const Size availableSize)
{
    // if there were elements added/removed from the grid,
    // we need to rebuild info about cells
    if (dirty)
        RebuildCellData();

    GridCell *cell;
    ILayout *el;
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            cell = GetCell(row, col);
            el = cell->el;
            if (!el)
                continue;
            el->Measure(availableSize);
            cell->desiredSize = el->DesiredSize();
        }
    }
    // TODO: finish me
}

void GridLayout::Arrange(const Rect finalRect)
{
    // TODO: write me
}

}
