# PrettySumatra Smoke UI Checklist

## Scope

- Validate WebView toolbar path, tab/home consistency, menu parity, sidebar behavior, and core commands.
- Run on Debug x64 after a clean app launch.

## Preconditions

- Build Debug x64 target `SumatraPDF-dll`.
- Start app with WebView2 runtime available.
- Use a sample set:
  - one PDF with at least 20 pages
  - one PDF with images + text
  - one non-PDF document supported by app

## 1. Launch and Global Surface

- [ ] App launches without crash and frame paints correctly.
- [ ] Native toolbar is not visible.
- [ ] WebView toolbar is visible and interactive.
- [ ] Home tab renders without overlaps or clipped controls.
- [ ] Switching theme (Light/Dark, follow Windows) updates toolbar and content colors coherently.

## 2. Home Tab

- [ ] "Recently Opened" title aligns to left content edge.
- [ ] Search edit is usable and not visually duplicated by a fake control.
- [ ] Typing in Home search filters recent files live.
- [ ] Recent cards remain fully visible with no cut-off at top.
- [ ] Opening an item from recent list loads document.

## 3. Toolbar Core Commands (WebView)

- [ ] Open button opens file picker and loads a document.
- [ ] Prev/Next page controls update page correctly.
- [ ] Page input jump works for valid page and clamps invalid values.
- [ ] Zoom in/out and zoom preset select update rendering.
- [ ] View mode selector applies single/facing/book behavior.
- [ ] Continuous toggle changes scrolling layout.
- [ ] Search box + next/prev arrows find text in document.

## 4. Menu and Command Parity

- [ ] Menu commands (Open, Print, Rotate, Properties, etc.) still work with WebView toolbar active.
- [ ] Command palette opens and executes commands normally.
- [ ] No duplicated/conflicting action between menu and WebView toolbar for same command.

## 5. Sidebar, TOC, Favorites

- [ ] Sidebar toggle from toolbar opens/closes TOC reliably.
- [ ] Favorites toggle behaves independently from TOC visibility.
- [ ] TOC selection jumps to expected page.
- [ ] Favorites selection opens expected destination.

## 6. Tabs and Navigation

- [ ] Open 3+ docs in tabs; switching tabs preserves page and zoom state.
- [ ] Close/reopen tab actions remain stable.
- [ ] Home tab and document tabs coexist without layout jumps.

## 7. Dialogs and Utilities

- [ ] Find dialog, Go To Page, Settings, and Properties open with coherent colors.
- [ ] Notifications render readable text and progress states.
- [ ] No control text/background contrast regression in pretty theme.

## 8. Rendering and Dark Behavior

- [ ] In dark mode, text remains readable.
- [ ] PDF image-heavy pages do not look incorrectly inverted.
- [ ] Switching theme while document is open does not require restart.

## 9. Stability Checks

- [ ] Enter/exit fullscreen does not reveal native toolbar.
- [ ] Enter/exit presentation mode preserves expected controls.
- [ ] Resize window (small/medium/large) keeps toolbar and page layout coherent.
- [ ] App exit/relaunch restores expected startup state without UI corruption.

## 10. Pass Criteria

- [ ] No crashes, no assertion dialogs, no command dead-ends.
- [ ] No native-toolbar reappearance during tested flows.
- [ ] No major visual overlap/cut-off in Home, tabs, sidebar, or dialogs.

## Quick Log Template

- Build: PASS/FAIL
- Launch: PASS/FAIL
- Home: PASS/FAIL
- Toolbar commands: PASS/FAIL
- Menu parity: PASS/FAIL
- Sidebar/TOC/Favorites: PASS/FAIL
- Tabs/navigation: PASS/FAIL
- Dialogs/utilities: PASS/FAIL
- Rendering/theme: PASS/FAIL
- Stability: PASS/FAIL
- Notes:
