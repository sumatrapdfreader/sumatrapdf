/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

Grid::Grid(Style *style) : dirty(true), cells(NULL), maxColWidth(NULL), maxRowHeight(NULL)
{
    SetStyle(style);
}

Grid::~Grid()
{
    free(cells);
    free(maxColWidth);
    free(maxRowHeight);
}

// TODO: to properly handle adding element to the grid at any
// time, we would need to request repaint here, so we would need
// to know parent window, the way Control does, further
// unifying the notion of control and layout.
Grid& Grid::Add(Grid::CellData& ld)
{
    CrashIf(!ld.el);
    els.Append(ld);
    AddChild(ld.el);
    dirty = true;
    return *this;
}

Grid::Cell *Grid::GetCell(int row, int col) const
{
    CrashIf(row >= rows);
    CrashIf(col >= cols);
    int n = (row * cols) + col;
    CrashIf(n < 0);
    CrashIf(n >= nCells);
    Cell *res = cells + n;
    CrashIf(res >= lastCell);
    CrashIf(res < cells);
    return res;
}

Point Grid::GetCellPos(int row, int col) const
{
    int x = 0, y = 0;
    for (int c = 0; c < col; c++) {
        x += maxColWidth[c];
    }
    for (int r = 0; r < row; r++) {
        y += maxRowHeight[r];
    }
    return Point(x, y);
}

// if there were elements added/removed from the grid,
// we need to rebuild info about cells
void Grid::RebuildCellDataIfNeeded()
{
    if (!dirty)
        return;

    // calculate how many columns and rows we need and build 2d cells
    // array, a cell for each column/row
    cols = 0;
    rows = 0;

    for (Grid::CellData *d = els.IterStart(); d; d = els.IterNext()) {
        if (d->col >= cols)
            cols = d->col + 1;
        if (d->row >= rows)
            rows = d->row + 1;
    }

    free(cells);
    nCells = cols * rows;
    cells = AllocArray<Cell>(nCells);
    lastCell = cells + nCells;

    // TODO: not sure if I want to disallow empty grids, but do for now
    CrashIf(0 == rows);
    CrashIf(0 == cols);

    free(maxColWidth);
    maxColWidth = AllocArray<int>(this->cols);
    free(maxRowHeight);
    maxRowHeight = AllocArray<int>(this->rows);
    dirty = false;
}

void Grid::Paint(Graphics *gfx, int offX, int offY)
{
    if (!IsVisible())
        return;

    CachedStyle *s = cachedStyle;

    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    Brush *brBgColor = BrushFromColorData(s->bgColor, bbox);
    gfx->FillRectangle(brBgColor, bbox);

    Rect r(offX, offY, pos.Width, pos.Height);
    DrawBorder(gfx, r, s);
}

void Grid::Measure(const Size availableSize)
{
    RebuildCellDataIfNeeded();

    Size borderSize(GetBorderAndPaddingSize(cachedStyle));

    Cell *cell;
    Control *el;
    int col, row;
    for (Grid::CellData *d = els.IterStart(); d; d = els.IterNext()) {
        col = d->col;
        row = d->row;
        cell = GetCell(row, col);
        cell->desiredSize.Width = 0;
        cell->desiredSize.Height = 0;
        el = d->el;
        if (!el->IsVisible())
            continue;

        el->Measure(availableSize);
        cell->desiredSize = el->DesiredSize();
        if (cell->desiredSize.Width > maxColWidth[col])
            maxColWidth[col] = cell->desiredSize.Width;
        if (cell->desiredSize.Height > maxRowHeight[row])
            maxRowHeight[row] = cell->desiredSize.Height;
    }
    int desiredWidth = 0;
    int desiredHeight = 0;
    for (row=0; row < rows; row++) {
        desiredHeight += maxRowHeight[row];
    }
    for (col=0; col < cols; col++) {
        desiredWidth += maxColWidth[col];
    }
    // TODO: what to do if desired size is more than availableSize?
    desiredSize.Width = desiredWidth + borderSize.Width;
    desiredSize.Height = desiredHeight + borderSize.Height;
}

void Grid::Arrange(const Rect finalRect)
{
    Cell *cell;
    Control *el;

    for (Grid::CellData *d = els.IterStart(); d; d = els.IterNext()) {
        cell = GetCell(d->row, d->col);
        el = d->el;
        Point pos(GetCellPos(d->row, d->col));
        int elDx = el->DesiredSize().Width;
        int containerDx = maxColWidth[d->col];
        int xOff = d->horizAlign.CalcOffset(elDx, containerDx);
        pos.X += xOff;
        int elDy = el->DesiredSize().Height;
        int containerDy = maxRowHeight[d->row];
        int yOff = d->vertAlign.CalcOffset(elDy, containerDy);
        pos.Y += yOff;
        Rect r(pos, cell->desiredSize);
        el->Arrange(r);
    }
    // if we're smaller than finalRect, we'll only use as much as
    // we need
    // TODO: this probably belongs in MuiHandWrapper::TopLevelLayout
    // so that it works for any type of control, but for now it'll do
    Rect r(finalRect);
    if (r.Width > desiredSize.Width)
        r.Width = desiredSize.Width;
    if (r.Height > desiredSize.Height)
        r.Height = desiredSize.Height;
    SetPosition(r);
}

} // namespace mui
