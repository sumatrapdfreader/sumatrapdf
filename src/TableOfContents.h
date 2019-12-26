/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CreateToc(WindowInfo*);
void ClearTocBox(WindowInfo*);
void ToggleTocBox(WindowInfo*);
void LoadTocTree(WindowInfo*);
void UpdateTocSelection(WindowInfo*, int currPageNo);
void UpdateTocExpansionState(Vec<int>& tocState, TreeCtrl*, DocTocTree*);
void UnsubclassToc(WindowInfo*);
