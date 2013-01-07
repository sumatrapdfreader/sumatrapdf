/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiDoc_h
#define MobiDoc_h

#include "EbookBase.h"

class PdbReader;
class HuffDicDecompressor;
struct ImageData;

enum PdbDocType { Pdb_Unknown, Pdb_Mobipocket, Pdb_PalmDoc, Pdb_TealDoc };

class MobiDoc
{
    WCHAR *             fileName;

    PdbReader *         pdbReader;

    PdbDocType          docType;
    size_t              docRecCount;
    int                 compressionType;
    size_t              docUncompressedSize;
    int                 textEncoding;

    bool                multibyte;
    size_t              trailersCount;
    size_t              imageFirstRec; // 0 if no images
    size_t              coverImageRec; // 0 if no cover image

    ImageData *         images;

    HuffDicDecompressor *huffDic;

    struct Metadata {
        DocumentProperty    prop;
        char *              value;
    };
    Vec<Metadata>       props;

    MobiDoc(const WCHAR *filePath);

    bool    ParseHeader();
    bool    LoadDocRecordIntoBuffer(size_t recNo, str::Str<char>& strOut);
    void    LoadImages();
    bool    LoadImage(size_t imageNo);
    bool    LoadDocument();
    bool    DecodeExthHeader(const char *data, size_t dataLen);

public:
    str::Str<char> *    doc;

    size_t              imagesCount;

    ~MobiDoc();

    char *              GetBookHtmlData(size_t& lenOut) const;
    size_t              GetBookHtmlSize() const { return doc->Size(); }
    ImageData *         GetCoverImage();
    ImageData *         GetImage(size_t imgRecIndex) const;
    const WCHAR *       GetFileName() const { return fileName; }
    WCHAR *             GetProperty(DocumentProperty prop);
    PdbDocType          GetDocType() const { return docType; }

    static bool         IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static MobiDoc *    CreateFromFile(const WCHAR *fileName);
};

// for testing MobiFormatter
class MobiTestDoc {
    str::Str<char> htmlData;

public:
    MobiTestDoc(const char *html, size_t len) {
        htmlData.Append(html, len);
    }

    const char *GetBookHtmlData(size_t& lenOut) const {
        lenOut = htmlData.Size();
        return htmlData.Get();
    }
    size_t      GetBookHtmlSize() const { return htmlData.Size(); }
};

#endif
