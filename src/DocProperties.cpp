/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "DocProperties.h"

const char* kPropTitle = "title";
const char* kPropAuthor = "author";
const char* kPropCopyright = "copyright";
const char* kPropSubject = "subject";
const char* kPropCreationDate = "creationDate";
const char* kPropModificationDate = "modDate";
const char* kPropCreatorApp = "creatorApp";
const char* kPropUnsupportedFeatures = "unsupportedFeatures";
const char* kPropFontList = "fontList";
const char* kPropPdfVersion = "pdfVersion";
const char* kPropPdfProducer = "pdfProducer";
const char* kPropPdfFileStructure = "pdfFileStructure";

int PropsCount(const Props& props) {
    int n = props.Size();
    CrashIf(n < 0 || (n % 2) != 0);
    return n / 2;
}

int FindPropIdx(const Props& props, const char* key) {
    int n = PropsCount(props);
    for (int i = 0; i < n; i++) {
        int idx = i * 2;
        char* v = props.At(idx);
        if (str::Eq(v, key)) {
            return idx;
        }
    }
    return -1;
}

char* FindProp(const Props& props, const char* key) {
    int idx = FindPropIdx(props, key);
    if (idx < 0) {
        return nullptr;
    }
    char* v = props.At(idx);
    return v;
}

void AddProp(Props& props, const char* key, const char* val, bool replaceIfExists) {
    CrashIf(!key || !val);
    int idx = FindPropIdx(props, key);
    if (idx < 0) {
        // doesn't exsit
        props.Append(key);
        props.Append(val);
        return;
    }
    if (!replaceIfExists) {
        return;
    }
    props.SetAt(idx + 1, val);
}
