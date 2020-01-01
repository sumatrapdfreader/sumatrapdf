/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Grid consits of cells arranged as an array of rows and columns
// It's also its own layout, because layout needs intimate knowledge
// of grid data
class Grid : public Control {
  public:
    struct CellData {
        Control* el;
        CachedStyle* cachedStyle;
        int row, col;
        int colSpan;
        // cell of the grid can be bigger than the element.
        // vertAlign and horizAlign define how the element
        // is laid out within the cell
        ElAlignData vertAlign;
        ElAlignData horizAlign;

        CellData()
            : el(nullptr),
              cachedStyle(nullptr),
              row(0),
              col(0),
              colSpan(1),
              vertAlign(GetElAlignTop()),
              horizAlign(GetElAlignLeft()) {
        }

        CellData(const CellData& other)
            : el(other.el),
              cachedStyle(other.cachedStyle),
              row(other.row),
              col(other.col),
              colSpan(other.colSpan),
              vertAlign(other.vertAlign),
              horizAlign(other.horizAlign) {
        }

        void Set(Control* el, int row, int col, ElAlign horizAlign = ElAlign::Left,
                 ElAlign vertAlign = ElAlign::Bottom) {
            this->el = el;
            this->cachedStyle = nullptr;
            this->row = row;
            this->col = col;
            this->colSpan = 1; // this can be re-used, so re-set to default value
            this->vertAlign = GetElAlign(vertAlign);
            this->horizAlign = GetElAlign(horizAlign);
        }

        bool SetStyle(Style* s) {
            bool changed;
            cachedStyle = CacheStyle(s, &changed);
            return changed;
        }
    };

    struct Cell {
        Size desiredSize;
        // TODO: more data
    };

  private:
    int rows;
    int cols;

    // if dirty is true, rows/cols and ld must be rebuilt from els
    bool dirty;
    // cells is rows * cols in size
    Cell* cells;
    // maxColWidth is an array of cols size and contains
    // maximum width of each column (the width of the widest
    // cell in that column)
    int* maxColWidth;
    int* maxRowHeight;

    Size desiredSize; // calculated in Measure()

    void RebuildCellDataIfNeeded();
    Cell* GetCell(int row, int col) const;
    Point GetCellPos(int row, int col) const;
    Rect GetCellBbox(Grid::CellData* d);

  public:
    Vec<CellData> els;

    Grid(Style* style = nullptr);
    virtual ~Grid();

    Grid& Add(CellData&);

    // Control
    virtual void Paint(Graphics* gfx, int offX, int offY);

    // ILayout
    virtual Size Measure(const Size availableSize);
    virtual Size DesiredSize() {
        return desiredSize;
    }
    virtual void Arrange(const Rect finalRect);
};
