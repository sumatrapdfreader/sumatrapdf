/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiDoc_h
#define MobiDoc_h

class EbookTocVisitor;
class HuffDicDecompressor;
class PdbReader;
enum DocumentProperty;
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
    size_t              docTocIndex;

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

    explicit MobiDoc(const WCHAR *filePath);

    bool    ParseHeader();
    bool    LoadDocRecordIntoBuffer(size_t recNo, str::Str<char>& strOut);
    void    LoadImages();
    bool    LoadImage(size_t imageNo);
    bool    LoadDocument(PdbReader *pdbReader);
    bool    DecodeExthHeader(const char *data, size_t dataLen);

public:
    str::Str<char> *    doc;

    size_t              imagesCount;

    ~MobiDoc();

    char *              GetHtmlData(size_t& lenOut) const;
    size_t              GetHtmlDataSize() const { return doc->Size(); }
    ImageData *         GetCoverImage();
    ImageData *         GetImage(size_t imgRecIndex) const;
    const WCHAR *       GetFileName() const { return fileName; }
    WCHAR *             GetProperty(DocumentProperty prop);
    PdbDocType          GetDocType() const { return docType; }

    bool                HasToc();
    bool                ParseToc(EbookTocVisitor *visitor);

    static bool         IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static MobiDoc *    CreateFromFile(const WCHAR *fileName);
    static MobiDoc *    CreateFromStream(IStream *stream);
};

#endif
