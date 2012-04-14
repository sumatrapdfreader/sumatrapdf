/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef MobiDoc_h
#define MobiDoc_h

class HuffDicDecompressor;
struct ImageData;

// http://en.wikipedia.org/wiki/PDB_(Palm_OS)
#define kDBNameLength    32
#define kPdbHeaderLen    78

struct PdbHeader
{
     /* 31 chars + 1 null terminator */
    char        name[kDBNameLength];
    uint16      attributes;
    uint16      version;
    uint32      createTime;
    uint32      modifyTime;
    uint32      backupTime;
    uint32      modificationNumber;
    uint32      appInfoID;
    uint32      sortInfoID;
    char        typeCreator[8];
    uint32      idSeed;
    uint32      nextRecordList;
    uint16      numRecords;
};

#define kMaxRecordSize 64*1024

enum PdbDocType { Pdb_Unknown, Pdb_Mobipocket, Pdb_PalmDoc, Pdb_TealDoc };

class MobiDoc
{
    TCHAR *             fileName;
    HANDLE              fileHandle;

    PdbHeader           pdbHeader;
    // offset of each pdb record within the file + a sentinel
    // value equal to file size to simplify use
    Vec<uint32>         recordOffsets;
    char *              firstRecData;

    PdbDocType          docType;
    size_t              docRecCount;
    int                 compressionType;
    size_t              docUncompressedSize;
    int                 textEncoding;

    bool                multibyte;
    size_t              trailersCount;
    size_t              imageFirstRec; // 0 if no images

    // we use bufStatic if record fits in it, bufDynamic otherwise
    char                bufStatic[kMaxRecordSize];
    char *              bufDynamic;
    size_t              bufDynamicSize;

    ImageData *         images;

    HuffDicDecompressor *huffDic;

    MobiDoc();

    bool    ParseHeader();
    char *  GetBufForRecordData(size_t size);
    size_t  GetRecordSize(size_t recNo);
    char*   ReadRecord(size_t recNo, size_t& sizeOut);
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
