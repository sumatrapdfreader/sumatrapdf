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

// clang-format off
const char* gAllProps[] = {
     kPropTitle,
     kPropAuthor,
     kPropCopyright,
     kPropSubject,
     kPropCreationDate,
     kPropModificationDate,
     kPropCreatorApp,
     kPropUnsupportedFeatures,
     kPropFontList,
     kPropPdfVersion,
     kPropPdfProducer,
     kPropPdfFileStructure,
    nullptr,
};
// clang-format off

int PropsCount(const Props& props) {
    int n = props.Size();
    ReportIf(n < 0 || (n % 2) != 0);
    return n / 2;
}

int GetPropIdx(const Props& props, const char* name) {
    int n = PropsCount(props);
    for (int i = 0; i < n; i++) {
        int idx = i * 2;
        char* v = props.At(idx);
        if (str::Eq(v, name)) {
            return idx;
        }
    }
    return -1;
}

char* GetPropValueTemp(const Props& props, const char* name) {
    int idx = GetPropIdx(props, name);
    if (idx < 0) {
        return nullptr;
    }
    char* v = props.At(idx);
    return v;
}

void AddProp(Props& props, const char* name, const char* val, bool replaceIfExists) {
    ReportIf(!name || !val);
    int idx = GetPropIdx(props, name);
    if (idx < 0) {
        // doesn't exsit
        props.Append(name);
        props.Append(val);
        return;
    }
    if (!replaceIfExists) {
        return;
    }
    props.SetAt(idx + 1, val);
}

// strings are pairs of str1, str2 laid in sequence, with nullptr to mark the en
// we find str1 matching s and return str2 or nullptr if not found
const char* GetMatchingString(const char** strings, const char* s) {
    while (*strings) {
        const char* str1 = *strings++;
        const char* str2 = *strings++;
        if (str1 == s || str::Eq(str1, s)) {
            return str2;
        }
    }
    return nullptr;
}
