/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool CouldBeArg(const WCHAR*);

struct ArgsIter {
    WCHAR** args{nullptr};
    int curr{1}; // first argument is exe path, which we skip
    int nArgs{0};
    const WCHAR* currArg{nullptr};

    explicit ArgsIter(const WCHAR* cmdLine);
    ~ArgsIter();

    const WCHAR* NextArg();
    const WCHAR* EatParam();
    void RewindParam();
    const WCHAR* AdditionalParam(int n) const;

    WCHAR* at(int) const;
};
