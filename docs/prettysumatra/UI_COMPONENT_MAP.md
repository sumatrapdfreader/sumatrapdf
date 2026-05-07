# UI Component Mapping (Mockup -> Native)

## Toolbar
- Open button -> Win32 command button + file dialog
- Prev/Next page -> native command buttons
- Page jump -> Edit + spin control
- Zoom selector -> ComboBox
- Search box -> Edit control + native find command binding
- Annotation mode toggle -> command toggle button
- Import/Export annotations -> commands + JSON file bridge

## Sidebar
- Tabs (Thumbnails/Outline/Notes) -> owner-drawn tab strip
- Thumbnails list -> virtualized custom list view
- Outline tree -> TreeView control
- Notes list -> owner-drawn list with actions

## Reader Surface
- PDF canvas -> existing Sumatra render surface (no rewrite)
- Text layer behavior -> bridge to engine text extraction APIs
- Highlights/comments -> overlay model tied to page/text ranges

## System UX
- Theme toggle -> app setting + system theme integration
- Keyboard shortcuts -> command dispatcher table
- Status toast -> lightweight native status line/overlay
- Native note modal -> dialog-hosted editor (no browser prompt/confirm)

## Migration Rule
UI changes must preserve command semantics from Sumatra.
