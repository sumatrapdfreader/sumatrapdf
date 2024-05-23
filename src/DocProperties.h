/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern const char* kPropTitle;
extern const char* kPropAuthor;
extern const char* kPropCopyright;
extern const char* kPropSubject;
extern const char* kPropCreationDate;
extern const char* kPropModificationDate;
extern const char* kPropCreatorApp;
extern const char* kPropUnsupportedFeatures;
extern const char* kPropFontList;
extern const char* kPropPdfVersion;
extern const char* kPropPdfProducer;
extern const char* kPropPdfFileStructure;

extern const char* gAllProps[];

// Props are stored in StrVec as name, value sequentially
using Props = StrVec;
int PropsCount(const Props& props);
int GetPropIdx(const Props& props, const char* name);
char* GetPropValueTemp(const Props& props, const char* name);
void AddProp(Props& props, const char* name, const char* val, bool replaceIfExists = false);

const char* GetMatchingString(const char**, const char*);
