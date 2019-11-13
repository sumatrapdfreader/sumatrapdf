/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class TreeCtrl2;

typedef std::function<void(TreeCtrl2*, NMTVGETINFOTIPW*)> OnGetInfoTip2;
typedef std::function<LRESULT(TreeCtrl2*, NMTREEVIEWW*, bool&)> OnTree2Notify;

typedef std::function<void()> EventHandler;

struct Event {
    std::vector<EventHandler> handlers;

    int Attach(const EventHandler& handler);
    void Detach(int);
};

struct EventPublisher {
    Event event;
    void Publish();
};

// TreeItem represents an item in a TreeView widget.
struct TreeItem {
    // Text returns the text of the item.
    virtual WCHAR* Text();

    // Parent returns the parent of the item.
    virtual TreeItem* Parent();

    // ChildCount returns the number of children of the item.
    virtual int ChildCount();

    // ChildAt returns the child at the specified index.
    virtual TreeItem* ChildAt(int index);
};

// HasChilder enables widgets like TreeView to determine if an item has any
// child, without enforcing to fully count all children.
struct HasChilder {
    bool HasChild();
};

typedef std::function<void(TreeItem*)> TreeItemEventHandler;

struct TreeItemEvent {
    std::vector<TreeItemEventHandler> handlers;
    int Attach(const TreeItemEventHandler&);
    void Detach(int);
};

struct TreeItemEventPublisher {
    TreeItemEvent event;
    void Publish(TreeItem*);
};


// TreeModel provides widgets like TreeView with item data.
struct TreeModel {
    // LazyPopulation returns if the model prefers on-demand population.
    //
    // This is useful for models that potentially contain huge amounts of items,
    // e.g. a model that represents a file system.
    virtual bool LazyPopulation();

    // RootCount returns the number of root items.
    virtual int RootCount();

    // RootAt returns the root item at the specified index.
    virtual TreeItem* RootAt(int);

    // ItemsReset returns the event that the model should publish when the
    // descendants of the specified item, or all items if no item is specified,
    // are reset.
    virtual TreeItemEvent* ItemsReset();
        

    // ItemChanged returns the event that the model should publish when an item
    // was changed.
    virtual TreeItemEvent* ItemChanged();
        

    // ItemInserted returns the event that the model should publish when an item
    // was inserted into the model.
    virtual TreeItemEvent* ItemInserted();
        

    // ItemRemoved returns the event that the model should publish when an item
    // was removed from the model.
    virtual TreeItemEvent* ItemRemoved();  
};

// TreeModelBase partially implements the TreeModel interface.
//
// You still need to provide your own implementation of at least the
// RootCount and RootAt methods. If your model needs lazy population,
// you will also have to implement LazyPopulation.
struct TreeModelBase : public TreeModel {
    TreeItemEventPublisher* itemsResetPublisher;
    TreeItemEventPublisher* itemChangedPublisher;
    TreeItemEventPublisher* itemInsertedPublisher;
    TreeItemEventPublisher* itemRemovedPublisher;

    bool LazyPopulation() override;
    int RootCount() override;
    TreeItem* RootAt(int) override;
    TreeItemEvent* ItemsReset();
    TreeItemEvent* ItemChanged() override;
    TreeItemEvent* ItemInserted() override;
    TreeItemEvent* ItemRemoved() override;
};

bool TreeModelBase::LazyPopulation() {
    return false;
}

TreeItemEvent* TreeModelBase::ItemsReset() {
    return &this->itemsResetPublisher->event;
}

TreeItemEvent* TreeModelBase::ItemChanged() {
    return &this->itemChangedPublisher->event;
}

TreeItemEvent* TreeModelBase::ItemInserted() {
    return &this->itemInsertedPublisher->event;
}

TreeItemEvent* TreeModelBase::ItemRemoved() {
    return &this->itemRemovedPublisher->event;
}


/* Creation sequence:
- auto ctrl = new TreeCtrl2()
- set creation parameters
- ctrl->Create()
*/

class TreeCtrl2 {
  public:
    TreeCtrl2(HWND parent, RECT* initialPosition);
    ~TreeCtrl2();

    void Clear();

    bool Create(const WCHAR* title);
    void SetFont(HFONT);

    // creation parameters. must be set before CreateTreeCtrl() call
    HWND parent = nullptr;
    RECT initialPos = {0, 0, 0, 0};
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                    TVS_SHOWSELALWAYS | TVS_TRACKSELECT | TVS_DISABLEDRAGDROP | TVS_NOHSCROLL | TVS_INFOTIP;
    DWORD dwExStyle = 0;
    HMENU menu = nullptr;
    COLORREF bgCol = 0;
    WCHAR infotipBuf[INFOTIPSIZE + 1]; // +1 just in case

    // this data can be set directly
    MsgFilter preFilter; // called at start of windows proc to allow intercepting messages
    // when set, allows the caller to set info tip by updating NMTVGETINFOTIP
    OnGetInfoTip2 onGetInfoTip;
    OnTree2Notify onTreeNotify;

    // private
    HWND hwnd = nullptr;
    UINT_PTR hwndSubclassId = 0;
    UINT_PTR hwndParentSubclassId = 0;
};
