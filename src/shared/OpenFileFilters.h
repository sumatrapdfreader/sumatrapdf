/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by OpenFileFilters_win.cpp and orig's SumatraPDF.cpp and ng's gui/NativeFileDlg_win.cpp ---

// File-type filters for IFileOpenDialog. Heap-owned wide strings stay alive
// for the whole Show() call (modal dialog pumps messages / temp arena).
struct OpenFileFilterList {
    Vec<WStr> names;
    Vec<WStr> patterns;
    Vec<COMDLG_FILTERSPEC> specs;

    ~OpenFileFilterList() {
        for (int i = 0; i < len(names); i++) {
            wstr::Free(names[i]);
        }
        for (int i = 0; i < len(patterns); i++) {
            wstr::Free(patterns[i]);
        }
    }

    void Add(Str name, Str pattern) {
        WStr nw = ToWStr(name);
        WStr pw = ToWStr(pattern);
        VecAppend(names, nw);
        VecAppend(patterns, pw);
        COMDLG_FILTERSPEC s{};
        s.pszName = nw.s;
        s.pszSpec = pw.s;
        VecAppend(specs, s);
    }
};
void BuildOpenFileFilters(OpenFileFilterList& out);
