/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Grid consits of cells arranged as an array of rows and columns
// It's also its own layout, because layout needs intimate knowledge
// of grid data
class Grid : public Control {
  public:
    struct CellData {
        Control* el{nullptr};
        CachedStyle* cachedStyle{nullptr};
        int row{0}, col{0};
        int colSpan{1};
        // cell of the grid can be bigger than the element.
        // vertAlign and horizAlign define how the element
        // is laid out within the cell
        ElAlignData vertAlign;
        ElAlignData horizAlign;

        CellData() {
            vertAlign = GetElAlignTop();
            horizAlign = GetElAlignLeft();
        }

        // TODO: make it default?
        CellData(const CellData& other) {
            CrashIf(this == &other);
            el = other.el;
            cachedStyle = other.cachedStyle;
            row = other.row;
            col = other.col;
            colSpan = other.colSpan;
            vertAlign = other.vertAlign;
            horizAlign = other.horizAlign;
        }

        // TODO: make it default?
        CellData& operator=(const CellData& other) {
            CrashIf(this == &other);
            el = other.el;
            cachedStyle = other.cachedStyle;
            row = other.row;
            col = other.col;
            colSpan = other.colSpan;
            vertAlign = other.vertAlign;
            horizAlign = other.horizAlign;
            return *this;
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
    int rows{0};
    int cols{0};

    // if dirty is true, rows/cols and ld must be rebuilt from els
    bool dirty{true};
    // cells is rows * cols in size
    Cell* cells{nullptr};
    // maxColWidth is an array of cols size and contains
    // maximum width of each column (the width of the widest
    // cell in that column)
    int* maxColWidth{nullptr};
    int* maxRowHeight{nullptr};

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
    void Paint(Graphics* gfx, int offX, int offY) override;

    // ILayout
    Size Measure(const Size availableSize) override;
    Size DesiredSize() override {
        return desiredSize;
    }
    void Arrange(const Rect finalRect) override;
};
