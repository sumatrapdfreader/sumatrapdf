/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct RefHoverState;

// Plain-text citation hover: when no link element is under the cursor, try
// to detect a "(Surname et al., 2020)" / "Surname (2020)" pattern at pagePos
// on srcPage, find the bibliography entry that matches, and return its
// location. Returns true on success and fills destPage/destX/destY.
// Lookups are cached on s.
// srcRectOut: on success, set to a stable per-occurrence source key (page
// coords) so the caller can tell two occurrences of the same citation apart
// and reposition the popup instead of treating it as the same hover.
bool RefHoverTryPlainText(RefHoverState* s, EngineBase* engine, int srcPage, Point pagePos, int& destPageOut,
                          float& destXOut, float& destYOut, RectF& srcRectOut);

// Free the lazy-init plain-text lookup cache held on the hover state.
void RefHoverFreeLookupCache(RefHoverState* s);
