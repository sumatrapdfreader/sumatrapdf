/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Action {
    // uniquley identifies action for e.g. for menu item
    int id;
    const char* name;
    // TODO: add hash code for fast lookup
};

// ids of pre-defined actions. Must start at 0 because
// are used as index into gActions for quick retrieval
enum class Actions {

};

Action* GetActionByClass(enum Actions action);
Action* GetActionByName(const char* name);
