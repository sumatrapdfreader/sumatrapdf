/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiDoc_h
#define MobiDoc_h

class PdbReader;
class HuffDicDecompressor;
struct ImageData;

enum PdbDocType { Pdb_Unknown, Pdb_Mobipocket, Pdb_PalmDoc, Pdb_TealDoc };

class MobiDoc
{
    TCHAR *             fileName;

    PdbReader *         pdbReader;

    PdbDocType          docType;
    size_t              docRecCount;
    int                 compressionType;
    size_t              docUncompressedSize;
    int                 textEncoding;

    bool                multibyte;
    size_t              trailersCount;
    size_t              imageFirstRec; // 0 if no images

    ImageData *         images;

    HuffDicDecompressor *huffDic;

    MobiDoc(const TCHAR *filePath);

    bool    ParseHeader();
    bool    LoadDocRecordIntoBuffer(size_t recNo, str::Str<char>& strOut);
    void    LoadImages();
    bool    LoadImage(size_t imageNo);
    bool    LoadDocument();

public:
    str::Str<char> *    doc;

    size_t              imagesCount;

    ~MobiDoc();

    char *              GetBookHtmlData(size_t& lenOut) const;
    size_t              GetBookHtmlSize() const { return doc->Size(); }
    ImageData *         GetCoverImage();
    ImageData *         GetImage(size_t imgRecIndex) const;
    const TCHAR *       GetFileName() const { return fileName; }
    PdbDocType          GetDocType() const { return docType; }

    static bool         IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static MobiDoc *    CreateFromFile(const TCHAR *fileName);
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
