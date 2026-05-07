# PrettySumatra Integration Notes

- Base branch: prettysumatra-base
- Host strategy: C++ Win32 + optional WebView2 shell
- Keep rendering core unchanged
- Port components according to native-hybrid/docs/UI_COMPONENT_MAP.md

## Integration Started

- Added documentation package under docs/prettysumatra:
	- BRIDGE_CONTRACT.md
	- UI_COMPONENT_MAP.md
	- ROADMAP_PHASE0.md
- Added initial command namespace spec at src/prettysumatra/CommandBridgeSpec.h.

## Native Toolbar Migration (Phase 1)

- Switched default startup path to native toolbar (Win32):
	- src/prettysumatra/BridgeDispatcher.cpp now defaults PRETTYSUMATRA_WEBVIEW_TOOLBAR to false.
- Kept WebView toolbar only as temporary fallback behind PRETTYSUMATRA_WEBVIEW_TOOLBAR=1.
- Hardened toolbar visibility logic:
	- Native rebar is hidden only when a hybrid toolbar instance was actually created.
	- No early hide based only on env flags.
- Updated frame relayout behavior:
	- Hybrid toolbar space is reserved only when hybrid toolbar window exists.
	- Prevents blank top strip when WebView is unavailable.
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully after these changes.

## Native Toolbar Visual Pass (Phase 2)

- Added native custom-draw "pill" highlight for active view controls:
	- `CmdZoomFitWidthAndContinuous`
	- `CmdZoomFitPageAndSinglePage`
	- `CmdToggleTableOfContents` (Sidebar)
- Sidebar button checked state now tracks TOC visibility so the pill state is meaningful.
- Added density tuning for small screens:
	- automatic compact mode for narrow frame widths
	- optional override via `PRETTYSUMATRA_TOOLBAR_DENSITY=compact|normal|auto`
- Compact mode reduces toolbar horizontal padding, separator width, and find-box width.
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully with phase 2 changes.

## Native Toolbar Modern Polish (Phase 3)

- Finalized a flatter native Win32 look without external UI libraries.
- Floating shell and group containers were refined:
	- larger outer margins to avoid edge-to-edge rebar feel
	- subtler shadow and stroke for cleaner depth
	- tighter rounded group containers clamped inside the shell bounds
- Toolbar item custom draw was expanded:
	- modern hover/pressed visuals for standard buttons (not only pill buttons)
	- no classic etched/raised toolbar edges
	- active pill buttons keep strong contrast and white text
- Find/Page background boxes now use rounded themed borders for consistency with the shell.
- Removed legacy rebar child-edge styling on the main toolbar band.
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully after phase 3 updates.

## Native Toolbar Redesign (Phase 4)

- Reworked toolbar visual integration from scratch while keeping native Win32 controls.
- Floating shell is now based on real content bounds (buttons + page/find controls), instead of spanning full width.
- Updated glass styling:
	- cleaner rounded shell and highlight
	- compact shadow profile
	- group capsules with wider inner margins
- Updated interaction styling:
	- larger inset and refined radii for hover/active button states
	- spacing and button sizing tuned for better visual rhythm
- Goal achieved:
	- stronger integration with the app surface and less "detached bar" appearance.
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully after phase 4 updates.

## Native Toolbar Win11 Refresh (Phase 5)

- Applied a full visual modernization pass for the native Win32 toolbar with a Windows 11 style direction, while preserving command behavior and layout semantics.
- Introduced centralized visual tokens in Toolbar.cpp for shell, groups, buttons, and states:
	- shell and shadow colors
	- hover/pressed/checked state colors
	- radius values for shell, groups, normal buttons, and pill buttons
- Updated floating shell rendering for a cleaner Fluent-like surface:
	- softer top tint instead of strong glossy effect
	- subtler blend integration into document background
	- compact-aware shell padding and group insets
- Updated item custom-draw behavior:
	- consistent Win11-style hover, pressed, and checked states for both normal and pill controls
	- accent checked state with high-contrast text
	- no functional changes to checked-state logic or command routing
- Refined embedded page/find box visuals to match the new shell:
	- rounded fill and stroke aligned with toolbar token set
	- theme-aware blending when controls are colorized
- Rebalanced spacing and density metrics for modern rhythm across compact/normal modes:
	- toolbar horizontal/vertical padding
	- separator widths
	- text button widths (Open, Sidebar)
	- button heights
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully after phase 5 updates.

## Win11 Coherence Pass (Phase 6)

- Extended Win11 visual language from the main toolbar to the custom menu toolbar (tabs-in-titlebar mode).
- Added menu-specific floating shell paint path that reuses the same visual token system as the main toolbar.
- Updated menu toolbar item custom-draw states for consistency:
	- hover state
	- pressed/active state
	- disabled text treatment
- Normalized menu toolbar metrics and height rhythm to align with the main toolbar density modes (compact/normal).
- Preserved menu behavior semantics (popup navigation, keyboard/mouse switching) with no command routing changes.
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully after phase 6 coherence updates.

## App-Wide Style Rollout (Phased)

- Goal: propagate the same modern PrettySumatra visual language across the whole app, not just the toolbar.
- Strategy: phase-based rollout with small, testable slices to avoid regressions.

### Phase A - Foundation (done)
- Added shared style tokens in `Theme` as reusable APIs:
	- `PrettyStyleEnabled()`
	- `PrettySurfaceColor()`
	- `PrettySurfaceAltColor()`
	- `PrettyBorderColor()`
	- `PrettyAccentColor()`
- Added env toggle `PRETTYSUMATRA_STYLE_V1` (default enabled).
- Applied tokens to Home/Start page surfaces:
	- background area
	- search box surface + border
	- thumbnail borders
	- tip panel surface + links
- Applied tokenized edit background in canvas-about edit color handler.
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully after Phase A.

### Phase B - Sidebar & Navigation (in progress)
- Implemented initial sidebar tokenization pass:
	- TOC/Favorites tree backgrounds now follow `PrettySurfaceAltColor()` when Pretty style is enabled.
	- TOC filter edit surface now uses `PrettySurfaceColor()` for clearer contrast from tree area.
	- Sidebar and favorites splitters now use `PrettyBorderColor()` for subtler separators.
	- TOC container and favorites container painting now uses Pretty sidebar background tokens.
	- TOC custom draw selection/highlight path now respects Pretty accent/surface tokens.
	- Favorites tree now uses custom draw for focused/unfocused selection to match TOC behavior.
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully after Phase B initial pass.
- Remaining in Phase B:
	- Apply the same sidebar token discipline to any remaining navigation panes with custom drawing.
	- Fine tune hover/selection contrast after runtime screenshot pass.

### Phase C - Dialogs & Utilities
- In progress:
	- Command Palette initial visual migration to Pretty tokens:
		- window/query/list surfaces now use Pretty surface tokens
		- list selected row uses focused/unfocused selection colors aligned with sidebar behavior
		- matched text highlight uses Pretty border tone
		- right-side metadata text uses Pretty accent color
	- Command Palette shell restyle (launcher-like):
		- borderless floating popup container with rounded corners
		- subtle outline and separators around results area
		- refined spacing, paddings and cue text polish for a cleaner visual hierarchy
		- frosty background treatment (DWM backdrop/blur + translucency fallback)
	- Edit Annotations window utility pass:
		- window background now follows Pretty surface token
		- list/edit/dropdowns/trackbars/buttons now use Pretty surface/alt surface colors
		- labels and control text now use themed foreground color for consistency
	- Utility dialogs Pretty token pass (SumatraDialogs):
		- added shared WM_ERASEBKGND / WM_CTLCOLOR* helper for Pretty surfaces
		- applied to Go To Page, Find, Change Language, Custom Zoom, Change Scrollbar, Settings and Add Favorite dialogs
		- static labels now blend with Pretty dialog background, edit/list controls use alt surface for readability
- Build status:
	- Debug x64 target SumatraPDF-dll compiles successfully after Command Palette Phase C pass.
- Remaining in Phase C:
	- Extend the same token discipline to remaining annotation-related dialogs not yet migrated.
	- Finalize consistency sweep for utility window border/surface/accent roles.

### Phase D - Polish & Accessibility
- Contrast pass (light/dark), DPI pass (100/150/200), and focus visuals.
- Final consistency sweep for icon tone and interaction feedback.
