/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef Doc_h
#define Doc_h

// In the past operations on supported document files were done as BaseEngine
// subclass. When we added MobiDoc and MobiWindow, not every document is an engine.
// Class Doc (a short for Document, since it's going to be used frequently)
// is a wrapper/abstractions for them.
// It simply wraps all document objects, allows querying the type, casting
// to the wrapped object and present as much of the unified interface as
// possible.
// It's small enough to be passed as value.

class BaseEngine;
class PdfEngine;
class XpsEngine;
class ChmEngine;
class DjVuEngine;
class ImageEngine;
class ImageDirEngine;
class CbxEngine;
class PsEngine;
class EpubEngine;
class Fb2Engine;
class MobiEngine;
class Chm2Engine;
class MobiDoc;

class Doc
{
public:
    enum DocType {
        None,
        PdfEng, XpsEng, ChmEng, Chm2Eng, DjVuEng,
        ImageEng, ImageDirEng, CbxEng,
        PsEng, EpubEng, MobiEng, Fb2Eng,
        Mobi
    };

    DocType type;
    union {
        void *dummy;
        PdfEngine * pdfEngine;
        XpsEngine * xpsEngine;
        ChmEngine * chmEngine;
        Chm2Engine *chm2Engine;
        DjVuEngine * djVuEngine;
        ImageEngine * imageEngine;
        ImageDirEngine * imageDirEngine;
        CbxEngine * cbxEngine;
        PsEngine * psEngine;
        EpubEngine *epubEngine;
        Fb2Engine *fb2Engine;
        MobiEngine *mobiEngine;
        MobiDoc * mobiDoc;
    };

    Doc() { type = None; dummy = NULL; }
    Doc(PdfEngine *doc) { type = PdfEng; pdfEngine = doc; }
    Doc(XpsEngine *doc) { type = XpsEng; xpsEngine = doc; }
    Doc(ChmEngine *doc) { type = ChmEng; chmEngine = doc; }
    Doc(Chm2Engine *doc) { type = Chm2Eng; chm2Engine = doc; }
    Doc(DjVuEngine *doc) { type = DjVuEng; djVuEngine = doc; }
    Doc(ImageEngine *doc) { type = ImageEng; imageEngine = doc; }
    Doc(ImageDirEngine *doc) { type = ImageDirEng; imageDirEngine = doc; }
    Doc(CbxEngine *doc) { type = CbxEng; cbxEngine = doc; }
    Doc(PsEngine *doc) { type = PsEng; psEngine = doc; }
    Doc(EpubEngine *doc) { type = EpubEng; epubEngine = doc; }
    Doc(Fb2Engine *doc) { type = Fb2Eng; fb2Engine = doc; }
    Doc(MobiEngine *doc) { type = MobiEng; mobiEngine = doc; }
    Doc(MobiDoc *doc) { type = Mobi; mobiDoc = doc; }

    BaseEngine *AsEngine() const;
};

#endif
