/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiGrid_h
#error "dont include twice!"
#endif
#define MuiGrid_h

// Grid consits of cells arranged as an array of rows and columns
// It's also its own layout, because layout needs intimate knowledge
// of grid data
class Grid : public Control
{
public:
    struct CellData {
        Control *     el;
        CachedStyle * cachedStyle;
        int           row, col;
        int           colSpan;
        // cell of the grid can be bigger than the element.
        // vertAlign and horizAlign define how the element
        // is laid out within the cell
        ElAlignData   vertAlign;
        ElAlignData   horizAlign;

        CellData() {
            el = NULL;
            cachedStyle = NULL;
            row = 0;
            col = 0;
            colSpan = 1;
            vertAlign.Set(ElAlignTop);
            horizAlign.Set(ElAlignLeft);
        }

        CellData(const CellData& other) {
            el = other.el;
            cachedStyle = other.cachedStyle;
            row = other.row;
            col = other.col;
            colSpan = other.colSpan;
            vertAlign = other.vertAlign;
            horizAlign = other.horizAlign;
        }

        void Set(Control *el, int row, int col, ElAlign horizAlign = ElAlignLeft, ElAlign vertAlign = ElAlignBottom) {
            this->el = el;
            this->cachedStyle = NULL;
            this->row = row;
            this->col = col;
            this->colSpan = 1; // this can be re-used, so re-set to default value
            this->vertAlign.Set(vertAlign);
            this->horizAlign.Set(horizAlign);
        }

        bool SetStyle(Style *s) {
            CachedStyle *newCachedStyle = CacheStyle(s);
            if (newCachedStyle != cachedStyle) {
                cachedStyle = newCachedStyle;
                return true;
            }
            return false;
        }
    };

    struct Cell {
        Size desiredSize;
        // TODO: more data
    };

private:
    int     rows;
    int     cols;

    // if dirty is true, rows/cols and ld must be rebuilt from els
    bool    dirty;
    // cells is rows * cols in size
    Cell *cells;
    // maxColWidth is an array of cols size and contains
    // maximum width of each column (the width of the widest
    // cell in that column)
    int *maxColWidth;
    int *maxRowHeight;

    Size    desiredSize; // calculated in Measure()

    void   RebuildCellDataIfNeeded();
    Cell * GetCell(int row, int col) const;
    Point  GetCellPos(int row, int col) const;
    Rect   GetCellBbox(Grid::CellData *d);

public:
    Vec<CellData>  els;

    Grid(Style *style = NULL);
    virtual ~Grid();

    Grid& Add(CellData&);

    // Control
    virtual void Paint(Graphics *gfx, int offX, int offY);

    // ILayout
    virtual Size Measure(const Size availableSize);
    virtual Size DesiredSize() { return desiredSize; }
    virtual void Arrange(const Rect finalRect);
};
