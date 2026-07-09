/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// A dialog for editing advanced settings, driven by the settings metadata
// (gGlobalPrefsInfo). Shows a filterable list of settings; clicking a bool
// toggles it, clicking an enum cycles its allowed values, clicking a string /
// color / number setting edits it in-place (Enter confirms, Esc cancels).
// Save writes the settings file and reloads it (so all derived state is
// re-computed); Cancel abandons the changes.

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"
#include "base/SettingsUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "GlobalPrefs.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "AdvancedSettingsDialog.h"

constexpr const char* kSettingsDocsUrl = "https://www.sumatrapdfreader.org/settings/settings3-7.html";

// enum settings: string settings restricted to a fixed set of values.
// the values come from the documentation comments in Settings.h
// clang-format off
static const char* gEnumDisplayMode[] = {
    "automatic", "single page", "facing", "book view",
    "continuous", "continuous facing", "continuous book view", nullptr,
};
static const char* gEnumToolbar[] = {"show", "hide", "overlay", nullptr};
static const char* gEnumToolbarPosition[] = {"top", "bottom", nullptr};
static const char* gEnumScrollbars[] = {"windows", "smart", "overlay", "hidden", nullptr};

struct EnumSettingDef {
    const char* name; // dotted path of the setting
    const char** values;
};
static const EnumSettingDef gEnumSettings[] = {
    {"DefaultDisplayMode", gEnumDisplayMode},
    {"Toolbar", gEnumToolbar},
    {"ToolbarPosition", gEnumToolbarPosition},
    {"Scrollbars", gEnumScrollbars},
};
// clang-format on

static const char** GetEnumValuesForSetting(Str name) {
    for (auto& def : gEnumSettings) {
        if (str::EqI(name, def.name)) {
            return def.values;
        }
    }
    return nullptr;
}

// a single editable setting; fieldPtr points into gGlobalPrefs, the pending
// (possibly edited) value is kept here and only written back on Save
struct SettingItem {
    Str name; // dotted path, e.g. "FixedPageUI.TextColor", owned
    SettingType type = SettingType::Bool;
    u8* fieldPtr = nullptr;
    const char** enumValues = nullptr; // non-null for enum (string) settings

    // pending value; strVal (owned) is used for String and Color
    bool boolVal = false;
    int intVal = 0;
    float floatVal = 0;
    Str strVal;

    bool changed = false;

    ~SettingItem() {
        str::Free(name);
        str::Free(strVal);
    }
};

// value of the setting formatted for display; the result is temp-allocated
static TempStr FormatSettingValueTemp(SettingItem* item) {
    switch (item->type) {
        case SettingType::Bool:
            return str::DupTemp(item->boolVal ? "true" : "false");
        case SettingType::Int:
            return fmt("%d", item->intVal);
        case SettingType::Float:
            return fmt("%g", item->floatVal);
        default:
            return str::DupTemp(item->strVal);
    }
}

static void SetItemChanged(SettingItem* item) {
    u8* p = item->fieldPtr;
    switch (item->type) {
        case SettingType::Bool:
            item->changed = item->boolVal != *(bool*)p;
            break;
        case SettingType::Int:
            item->changed = item->intVal != *(int*)p;
            break;
        case SettingType::Float:
            item->changed = item->floatVal != *(float*)p;
            break;
        default: {
            Str curr = *(Str*)p;
            bool bothEmpty = len(item->strVal) == 0 && len(curr) == 0;
            item->changed = !bothEmpty && !str::Eq(item->strVal, curr);
            break;
        }
    }
}

// collect editable leaf settings from the metadata, recursing into
// sub-structs with a dotted path prefix
static void CollectSettings(Vec<SettingItem*>& items, const StructInfo* info, u8* base, Str prefix) {
    const char* fieldName = info->fieldNames;
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        Str name(fieldName);
        fieldName += len(name) + 1;
        if (field.type == SettingType::Comment) {
            continue;
        }
        u8* fieldPtr = base + field.offset;
        TempStr path = len(prefix) > 0 ? fmt("%s.%s", prefix, name) : str::DupTemp(name);
        switch (field.type) {
            case SettingType::Struct:
            case SettingType::Prerelease: {
                if (field.type == SettingType::Prerelease && !gIsPreReleaseBuild && !gIsDebugBuild) {
                    // not written out in release builds, so not editable there
                    break;
                }
                auto sub = (const StructInfo*)field.value;
                CollectSettings(items, sub, fieldPtr, path);
                break;
            }
            case SettingType::Bool:
            case SettingType::Int:
            case SettingType::Float:
            case SettingType::String:
            case SettingType::Color: {
                auto item = new SettingItem();
                item->name = str::Dup(path);
                item->type = field.type;
                item->fieldPtr = fieldPtr;
                switch (field.type) {
                    case SettingType::Bool:
                        item->boolVal = *(bool*)fieldPtr;
                        break;
                    case SettingType::Int:
                        item->intVal = *(int*)fieldPtr;
                        break;
                    case SettingType::Float:
                        item->floatVal = *(float*)fieldPtr;
                        break;
                    default:
                        item->strVal = str::Dup(*(Str*)fieldPtr);
                        if (field.type == SettingType::String) {
                            item->enumValues = GetEnumValuesForSetting(path);
                        }
                        break;
                }
                items.Append(item);
                break;
            }
            default:
                // arrays, compact structs etc. can't be edited yet; the
                // "Open Settings File" button covers those
                break;
        }
    }
}

struct ListBoxModelSettings : ListBoxModel {
    Vec<SettingItem*>* items = nullptr; // not owned
    Vec<int> filtered;                  // indexes into items

    ~ListBoxModelSettings() override = default;
    int ItemsCount() override { return len(filtered); }
    Str Item(int i) override { return (*items)[filtered[i]]->name; }
    SettingItem* ItemAt(int i) {
        if (i < 0 || i >= len(filtered)) {
            return nullptr;
        }
        return (*items)[filtered[i]];
    }
};

struct AdvancedSettingsWnd : Wnd {
    ~AdvancedSettingsWnd() override;

    HFONT font = nullptr;
    MainWindow* win = nullptr;

    Edit* editFilter = nullptr;
    ListBox* listBox = nullptr;
    ListBoxModelSettings* model = nullptr; // owned by listBox
    Edit* editValue = nullptr;             // in-place value editor, created on demand
    int editItemIdx = -1;                  // index into items of the setting being edited

    Vec<SettingItem*> items;

    bool Create(MainWindow* win);
    bool PreTranslateMessage(MSG&) override;

    void QueryChanged();
    void DrawListBoxItem(ListBox::DrawItemEvent* ev);
    void OnItemClicked(int lbIdx);

    Rect ValueRectForItem(int idx); // in listBox client coords
    void BeginEditValue(int idx);
    void CommitEditValue();
    void CancelEditValue();

    void OnOpenSettingsFile();
    void OnHelp();
    void OnCancel();
    void OnSave();

    void ApplyChangesAndSave();
    void ScheduleDelete();
};

static AdvancedSettingsWnd* gAdvancedSettingsWnd = nullptr;

AdvancedSettingsWnd::~AdvancedSettingsWnd() {
    delete editFilter;
    delete editValue;
    delete listBox;
    DeleteVecMembers(items);
    delete layout;
}

void SafeDeleteAdvancedSettingsDialog() {
    if (!gAdvancedSettingsWnd) {
        return;
    }
    auto tmp = gAdvancedSettingsWnd;
    gAdvancedSettingsWnd = nullptr;
    delete tmp;
}

void AdvancedSettingsWnd::ScheduleDelete() {
    if (gAdvancedSettingsWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteAdvancedSettingsDialog);
    uitask::Post(fn, "SafeDeleteAdvancedSettingsDialog");
}

void AdvancedSettingsWnd::QueryChanged() {
    CancelEditValue();
    Str filter = editFilter->GetTextTemp();
    model->filtered.Reset();
    int n = len(items);
    for (int i = 0; i < n; i++) {
        if (len(filter) == 0 || str::ContainsI(items[i]->name, filter)) {
            model->filtered.Append(i);
        }
    }
    listBox->SetModel(model);
}

void AdvancedSettingsWnd::DrawListBoxItem(ListBox::DrawItemEvent* ev) {
    ListBox* lb = ev->listBox;
    auto m = (ListBoxModelSettings*)lb->model;
    SettingItem* item = m->ItemAt(ev->itemIndex);
    if (!item) {
        return;
    }

    HDC hdc = ev->hdc;
    RECT rc = ev->itemRect;

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    SetBkColor(hdc, colBg);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);

    int pad = DpiScale(hwnd, 4);

    // setting name on the left; changed settings are marked with '*'
    SetTextColor(hdc, colText);
    RECT rcName = rc;
    rcName.left += pad;
    TempStr name = item->changed ? fmt("* %s", item->name) : str::DupTemp(item->name);
    TempWStr ws = ToWStrTemp(name);
    DrawTextW(hdc, ws.s, -1, &rcName, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // value on the right
    TempStr val = FormatSettingValueTemp(item);
    if (item->changed) {
        SetTextColor(hdc, AccentColor(colText, 60));
    }
    RECT rcVal = rc;
    rcVal.right -= pad;
    ws = ToWStrTemp(val);
    DrawTextW(hdc, ws.s, -1, &rcVal, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

// the value occupies the right half of the item's rect
Rect AdvancedSettingsWnd::ValueRectForItem(int idx) {
    int lbIdx = -1;
    int n = model->ItemsCount();
    for (int i = 0; i < n; i++) {
        if (model->filtered[i] == idx) {
            lbIdx = i;
            break;
        }
    }
    if (lbIdx < 0) {
        return Rect();
    }
    RECT rc{};
    LRESULT res = SendMessageW(listBox->hwnd, LB_GETITEMRECT, (WPARAM)lbIdx, (LPARAM)&rc);
    if (res == LB_ERR) {
        return Rect();
    }
    Rect r = ToRect(rc);
    int half = r.dx / 2;
    r.x += half;
    r.dx -= half;
    return r;
}

void AdvancedSettingsWnd::BeginEditValue(int idx) {
    CancelEditValue();
    SettingItem* item = items[idx];
    Rect r = ValueRectForItem(idx);
    if (r.IsEmpty()) {
        return;
    }

    Edit::CreateArgs args;
    args.parent = listBox->hwnd;
    args.isMultiLine = false;
    args.withBorder = true;
    args.font = font;
    args.text = FormatSettingValueTemp(item);
    auto c = new Edit();
    c->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    HWND ok = c->Create(args);
    if (!ok) {
        delete c;
        return;
    }
    editValue = c;
    editItemIdx = idx;
    SetWindowPos(c->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_SHOWWINDOW);
    c->SelectAll();
    HwndSetFocus(c->hwnd);
}

void AdvancedSettingsWnd::CancelEditValue() {
    if (!editValue) {
        return;
    }
    auto tmp = editValue;
    editValue = nullptr;
    editItemIdx = -1;
    delete tmp;
    HwndSetFocus(listBox->hwnd);
}

void AdvancedSettingsWnd::CommitEditValue() {
    if (!editValue) {
        return;
    }
    SettingItem* item = items[editItemIdx];
    Str s = editValue->GetTextTemp();
    switch (item->type) {
        case SettingType::Int:
            item->intVal = ParseInt(s);
            break;
        case SettingType::Float: {
            float f = item->floatVal;
            str::Parse(s, "%f", &f);
            item->floatVal = f;
            break;
        }
        default:
            str::ReplaceWithCopy(&item->strVal, s);
            break;
    }
    SetItemChanged(item);
    CancelEditValue();
    InvalidateRect(listBox->hwnd, nullptr, TRUE);
}

void AdvancedSettingsWnd::OnItemClicked(int lbIdx) {
    SettingItem* item = model->ItemAt(lbIdx);
    if (!item) {
        return;
    }
    int idx = model->filtered[lbIdx];
    if (item->type == SettingType::Bool) {
        item->boolVal = !item->boolVal;
        SetItemChanged(item);
        InvalidateRect(listBox->hwnd, nullptr, TRUE);
        return;
    }
    if (item->enumValues) {
        // cycle to the next allowed value
        const char** vals = item->enumValues;
        int curr = -1;
        for (int i = 0; vals[i]; i++) {
            if (str::EqI(item->strVal, vals[i])) {
                curr = i;
                break;
            }
        }
        const char* next = vals[curr + 1] ? vals[curr + 1] : vals[0];
        str::ReplaceWithCopy(&item->strVal, next);
        SetItemChanged(item);
        InvalidateRect(listBox->hwnd, nullptr, TRUE);
        return;
    }
    BeginEditValue(idx);
}

void AdvancedSettingsWnd::ApplyChangesAndSave() {
    bool didChange = false;
    for (SettingItem* item : items) {
        if (!item->changed) {
            continue;
        }
        didChange = true;
        u8* p = item->fieldPtr;
        switch (item->type) {
            case SettingType::Bool:
                *(bool*)p = item->boolVal;
                break;
            case SettingType::Int:
                *(int*)p = item->intVal;
                break;
            case SettingType::Float:
                *(float*)p = item->floatVal;
                break;
            default:
                str::ReplaceWithCopy((Str*)p, item->strVal);
                break;
        }
        // SaveSettings() re-generates these strings from their parsed
        // representations, which would clobber the edit unless the parsed
        // representation is updated as well
        if (str::EqI(item->name, "DefaultDisplayMode")) {
            gGlobalPrefs->defaultDisplayModeEnum = DisplayModeFromString(item->strVal, DisplayMode::Automatic);
        } else if (str::EqI(item->name, "DefaultZoom")) {
            gGlobalPrefs->defaultZoomFloat = ZoomFromString(item->strVal, kZoomActualSize);
        } else if (str::EqI(item->name, "ImageUI.DefaultZoom")) {
            gGlobalPrefs->imageUI.defaultZoomFloat = ZoomFromString(item->strVal, 0);
        }
    }
    if (!didChange) {
        return;
    }
    SaveSettings();
    // reload so that all state derived from settings (theme, fonts, parsed
    // colors, custom commands, accelerators ...) is re-computed and applied
    ForceReloadSettings();
}

void AdvancedSettingsWnd::OnOpenSettingsFile() {
    if (!CanAccessDisk()) {
        return;
    }
    TempStr path = GetSettingsPathTemp();
    LaunchFileIfExists(path);
}

void AdvancedSettingsWnd::OnHelp() {
    SumatraLaunchBrowser(kSettingsDocsUrl);
}

void AdvancedSettingsWnd::OnCancel() {
    ScheduleDelete();
}

void AdvancedSettingsWnd::OnSave() {
    CommitEditValue();
    ApplyChangesAndSave();
    ScheduleDelete();
}

bool AdvancedSettingsWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message == WM_KEYDOWN) {
        bool isEditingValue = editValue && msg.hwnd == editValue->hwnd;
        if (msg.wParam == VK_ESCAPE) {
            if (isEditingValue) {
                CancelEditValue();
            } else {
                ScheduleDelete();
            }
            return true;
        }
        if (msg.wParam == VK_RETURN) {
            if (isEditingValue) {
                CommitEditValue();
                return true;
            }
            int lbIdx = listBox->GetCurrentSelection();
            if (msg.hwnd == listBox->hwnd && lbIdx >= 0) {
                OnItemClicked(lbIdx);
                return true;
            }
            return false;
        }
        int dir = 0;
        if (msg.wParam == VK_UP) {
            dir = -1;
        } else if (msg.wParam == VK_DOWN) {
            dir = 1;
        }
        if (dir != 0 && msg.hwnd == editFilter->hwnd) {
            int n = listBox->GetCount();
            if (n > 0) {
                int sel = (listBox->GetCurrentSelection() + dir + n) % n;
                listBox->SetCurrentSelection(sel);
            }
            return true;
        }
        return false;
    }
    if (msg.message == WM_LBUTTONUP && msg.hwnd == listBox->hwnd) {
        LRESULT res = SendMessageW(listBox->hwnd, LB_ITEMFROMPOINT, 0, msg.lParam);
        if (HIWORD(res) == 0) {
            OnItemClicked(LOWORD(res));
        }
        return false; // let the listbox also process the click
    }
    return false;
}

static void OnDestroy(Wnd::DestroyEvent*) {
    if (gAdvancedSettingsWnd) {
        gAdvancedSettingsWnd->ScheduleDelete();
    }
}

// center the dialog over the main window frame
static void PositionDialog(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = WindowRect(hwndRelative);
    Rect r = WindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    r = {x, y, r.dx, r.dy};
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

bool AdvancedSettingsWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    CollectSettings(items, &gGlobalPrefsInfo, (u8*)gGlobalPrefs, {});

    {
        CreateCustomArgs args;
        args.title = _TRA("Advanced Settings");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = font;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    bool isRtl = IsUIRtl();

    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = false;
        args.cueText = _TRA("enter search term to filter settings");
        args.font = font;
        args.isRtl = isRtl;
        auto c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::QueryChanged>(this);
        editFilter = c;
        vbox->AddChild(c);
    }

    {
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = isRtl;
        auto c = new ListBox();
        c->onDrawItem =
            MkMethod1<AdvancedSettingsWnd, ListBox::DrawItemEvent*, &AdvancedSettingsWnd::DrawListBoxItem>(this);
        c->idealSizeLines = 24;
        c->SetInsetsPt(4, 0);
        c->Create(args);
        c->SetColors(colTxt, colBg);
        listBox = c;
        model = new ListBoxModelSettings();
        model->items = &items;
        int n = len(items);
        for (int i = 0; i < n; i++) {
            model->filtered.Append(i);
        }
        c->SetModel(model);
        vbox->AddChild(c, 1);
    }

    {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainCenter;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        Button* b;
        b = CreateButton(hwnd, _TRA("Open Settings File"),
                         MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnOpenSettingsFile>(this), isRtl);
        hbox->AddChild(new Padding(b, pad));
        b = CreateButton(hwnd, _TRA("Help"), MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnHelp>(this), isRtl);
        hbox->AddChild(new Padding(b, pad));
        b = CreateButton(hwnd, _TRA("Cancel"), MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnCancel>(this),
                         isRtl);
        hbox->AddChild(new Padding(b, pad));
        b = CreateButton(hwnd, _TRA("Save"), MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnSave>(this), isRtl);
        hbox->AddChild(new Padding(b, pad));
        vbox->AddChild(hbox);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    auto rc = ClientRect(win->hwndFrame);
    int dy = limitValue(rc.dy - 72, 480, 800);
    int dx = limitValue(rc.dx - 256, 640, 800);
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    PositionDialog(hwnd, win->hwndFrame);

    SetIsVisible(true);
    HwndSetFocus(editFilter->hwnd);
    return true;
}

void ShowAdvancedSettingsDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gAdvancedSettingsWnd) {
        HwndSetFocus(gAdvancedSettingsWnd->hwnd);
        return;
    }
    auto wnd = new AdvancedSettingsWnd();
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    wnd->font = GetAppFont();
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gAdvancedSettingsWnd = wnd;
}
