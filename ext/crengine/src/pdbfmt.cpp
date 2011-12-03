#include "../include/pdbfmt.h"
#include <ctype.h>

struct PDBHdr
{
    lUInt8    name[32];
    lUInt16   attributes;
    lUInt16   version;
    lUInt32    creationDate;
    lUInt32    modificationDate;
    lUInt32    lastBackupDate;
    lUInt32    modificationNumber;
    lUInt32    appInfoID;
    lUInt32    sortInfoID;
    lUInt8     type[4];
    lUInt8     creator[4];
    lUInt32    uniqueIDSeed;
    lUInt32    nextRecordList;
    lUInt16    recordCount;
    lUInt16    firstEntry;
    bool read( LVStreamRef stream ) {
        // TODO: byte order support
        lvsize_t bytesRead = 0;
        if ( stream->Read(this, sizeof(PDBHdr), &bytesRead )!=LVERR_OK )
            return false;
        if ( bytesRead!=sizeof(PDBHdr) )
            return false;
        lvByteOrderConv cnv;
        if ( cnv.lsf() )
        {
            cnv.rev(&attributes);
            cnv.rev(&version);
            cnv.rev(&creationDate);
            cnv.rev(&modificationDate);
            cnv.rev(&lastBackupDate);
            cnv.rev(&modificationNumber);
            cnv.rev(&appInfoID);
            cnv.rev(&sortInfoID);
            cnv.rev(&uniqueIDSeed);
            cnv.rev(&nextRecordList);
            cnv.rev(&recordCount);
            cnv.rev(&firstEntry);
        }
        return true;
    }
    bool checkType( const char * str ) {
        return type[0]==str[0] && type[1]==str[1] && type[2]==str[2] && type[3]==str[3];
    }

    bool checkCreator( const char * str ) {
        return creator[0]==str[0] && creator[1]==str[1] && creator[2]==str[2] && creator[3]==str[3];
    }
};

struct PDBRecordEntry
{
    lUInt32 localChunkId;
    lUInt8  attributes[4];
    //lUInt8  uniqueID[3];
    bool read( LVStreamRef stream ) {
        // TODO: byte order support
        lvsize_t bytesRead = 0;
        if ( stream->Read(this, sizeof(PDBRecordEntry), &bytesRead )!=LVERR_OK )
            return false;
        if ( bytesRead!=sizeof(PDBRecordEntry) )
            return false;
        lvByteOrderConv cnv;
        if ( cnv.lsf() )
        {
            cnv.rev(&localChunkId);
        }
        return true;
    }
};

struct PalmDocPreamble
{
    lUInt16 compression; // 2  Compression   1 == no compression, 2 = PalmDOC compression (see below)
    lUInt16 unused;      // 2  Unused  Always zero
    lUInt32 textLength;  // 4  text length  Uncompressed length of the entire text of the book
    lUInt16 recordCount; // 2  record count  Number of PDB records used for the text of the book.
    lUInt16 recordSize;  // 2  record size  Maximum size of each record containing text, always 4096
    bool read( LVStreamRef stream ) {
        // TODO: byte order support
        lvsize_t bytesRead = 0;
        if ( stream->Read(this, sizeof(PalmDocPreamble), &bytesRead )!=LVERR_OK )
            return false;
        if ( bytesRead!=sizeof(PalmDocPreamble) )
            return false;
        lvByteOrderConv cnv;
        if ( cnv.lsf() )
        {
            cnv.rev(&compression); // 2  Compression   1 == no compression, 2 = PalmDOC compression (see below)
            cnv.rev(&textLength);  // 4  text length  Uncompressed length of the entire text of the book
            cnv.rev(&recordCount); // 2  record count  Number of PDB records used for the text of the book.
            cnv.rev(&recordSize);  // 2  record size  Maximum size of each record containing text, always 4096
        }
        if ( compression!=1 && compression!=2 )
            return false;
        return true;
    }
};

struct MobiPreamble : public PalmDocPreamble
{
    lUInt16 mobiEncryption;  // 2  Encryption Type	0 == no encryption, 1 = Old Mobipocket Encryption, 2 = Mobipocket Encryption
    lUInt16 unused2;     // 2  unknown, usually 0

    lUInt8  mobiSignature[4]; // 16	4	identifier	the characters M O B I
    lUInt32 hederLength; // 20	4	header length	the length of the MOBI header, including the previous 4 bytes
    lUInt32 mobiType;    //    24	4	Mobi type	The kind of Mobipocket file this is
            //    2 Mobipocket Book
            //    3 PalmDoc Book
            //    4 Audio
            //    257 News
            //    258 News_Feed
            //    259 News_Magazine
            //    513 PICS
            //    514 WORD
            //    515 XLS
            //    516 PPT
            //    517 TEXT
            //    518 HTML
    lUInt32 encoding; //    28	4	text Encoding	1252 = CP1252 (WinLatin1); 65001 = UTF-8
    lUInt32 uid; //    32	4	Unique-ID	Some kind of unique ID number (random?)
    lUInt32 fileVersion; //    36	4	File version	Version of the Mobipocket format used in this file.
    lUInt32 reserved[10]; //    40	40	Reserved	all 0xFF. In case of a dictionary, or some newer file formats, a few bytes are used from this range of 40 0xFFs
    lUInt32 firstNonBookIndex; //    80	4	First Non-book index?	First record number (starting with 0) that's not the book's text
    lUInt32 fullNameOffset; //    84	4	Full Name Offset	Offset in record 0 (not from start of file) of the full name of the book
    lUInt32 fullNameLength; //    88	4	Full Name Length	Length in bytes of the full name of the book
    lUInt32 locale; //    92	4	Locale	Book locale code. Low byte is main language 09= English, next byte is dialect, 08 = British, 04 = US. Thus US English is 1033, UK English is 2057.
    lUInt32 inputLanguage; //    96	4	Input Language	Input language for a dictionary
    lUInt32 outputLanguage; //    100	4	Output Language	Output language for a dictionary
    lUInt32 minVersion; //    104	4	Min version	Minimum mobipocket version support needed to read this file.
    lUInt32 firstImageIndex; //    108	4	First Image index?	First record number (starting with 0) that contains an image. Image records should be sequential.
    lUInt32 huffmanRecordOffset; //    112	4	Huffman Record Offset	The record number of the first huffman compression record.
    lUInt32 huffmanRecordCount; //    116	4	Huffman Record Count	The number of huffman compression records.
    lUInt32 reserved2[2]; //    120	8	?	eight bytes, often zeros
    lUInt32 mobiFlags; //    128	4	EXTH flags	bitfield. if bit 6 (0x40) is set, then there's an EXTH record
    lUInt32 unknown3[8]; //    132	32	?	32 unknown bytes, if MOBI is long enough
    lUInt32 drmOffset; //    164	4	DRM Offset	Offset to DRM key info in DRMed files. 0xFFFFFFFF if no DRM
    lUInt32 drmCount; //    168	4	DRM Count	Number of entries in DRM info. 0xFFFFFFFF if no DRM
    lUInt32 drmSize; //    172	4	DRM Size	Number of bytes in DRM info.
    lUInt32 drmFlags; //    176	4	DRM Flags	Some flags concerning the DRM info.


    bool read( LVStreamRef stream ) {
        // TODO: byte order support
        lvsize_t bytesRead = 0;
        if ( stream->Read(this, sizeof(MobiPreamble), &bytesRead )!=LVERR_OK )
            return false;
        if ( bytesRead!=sizeof(MobiPreamble) )
            return false;
        lvByteOrderConv cnv;
        if ( cnv.lsf() )
        {
            cnv.rev(&compression); // 2  Compression   1 == no compression, 2 = PalmDOC compression (see below)
            cnv.rev(&textLength);  // 4  text length  Uncompressed length of the entire text of the book
            cnv.rev(&recordCount); // 2  record count  Number of PDB records used for the text of the book.
            cnv.rev(&recordSize);  // 2  record size  Maximum size of each record containing text, always 4096
            cnv.rev(&mobiEncryption);// 2  Encryption Type	0 == no encryption, 1 = Old Mobipocket Encryption, 2 = Mobipocket Encryption
            cnv.rev(&hederLength); // 20	4	header length	the length of the MOBI header, including the previous 4 bytes
            cnv.rev(&mobiType);    //    24	4	Mobi type	The kind of Mobipocket file this is
            cnv.rev(&encoding); //    28	4	text Encoding	1252 = CP1252 (WinLatin1); 65001 = UTF-8
            cnv.rev(&uid); //    32	4	Unique-ID	Some kind of unique ID number (random?)
            cnv.rev(&fileVersion); //    36	4	File version	Version of the Mobipocket format used in this file.
            cnv.rev(&firstNonBookIndex); //    80	4	First Non-book index?	First record number (starting with 0) that's not the book's text
            cnv.rev(&fullNameOffset); //    84	4	Full Name Offset	Offset in record 0 (not from start of file) of the full name of the book
            cnv.rev(&fullNameLength); //    88	4	Full Name Length	Length in bytes of the full name of the book
            cnv.rev(&locale); //    92	4	Locale	Book locale code. Low byte is main language 09= English, next byte is dialect, 08 = British, 04 = US. Thus US English is 1033, UK English is 2057.
            cnv.rev(&inputLanguage); //    96	4	Input Language	Input language for a dictionary
            cnv.rev(&outputLanguage); //    100	4	Output Language	Output language for a dictionary
            cnv.rev(&minVersion); //    104	4	Min version	Minimum mobipocket version support needed to read this file.
            cnv.rev(&firstImageIndex); //    108	4	First Image index?	First record number (starting with 0) that contains an image. Image records should be sequential.
            cnv.rev(&huffmanRecordOffset); //    112	4	Huffman Record Offset	The record number of the first huffman compression record.
            cnv.rev(&huffmanRecordCount); //    116	4	Huffman Record Count	The number of huffman compression records.
            cnv.rev(&mobiFlags); //    128	4	EXTH flags	bitfield. if bit 6 (0x40) is set, then there's an EXTH record
            cnv.rev(&drmOffset); //    164	4	DRM Offset	Offset to DRM key info in DRMed files. 0xFFFFFFFF if no DRM
            cnv.rev(&drmCount); //    168	4	DRM Count	Number of entries in DRM info. 0xFFFFFFFF if no DRM
            cnv.rev(&drmSize); //    172	4	DRM Size	Number of bytes in DRM info.
            cnv.rev(&drmFlags); //    176	4	DRM Flags	Some flags concerning the DRM info.
        }
        if ( compression!=1 && compression!=2 )
            return false;
        if ( mobiType!=2 && mobiType!=3 && mobiType!=517 && mobiType!=518
                 && mobiType!=257 && mobiType!=258 && mobiType!=259 )
            return false; // unsupported type
        if ( mobiEncryption!=0 )
            return false; // encryption is not supported
        return true;
    }
};

// format description from http://wiki.mobileread.com/wiki/EReader
struct EReaderHeader
{
    lUInt16 compression;    //    0-2	compression	Specifies compression and drm. 2 = palmdoc, 10 = zlib. 260 and 272 = DRM
    lUInt16 unknown1[2];    //    2-6	unknown	Value of 0 is used
    lUInt16 encoding;       //    6-8	encoding	Always 25152 (0x6240). All text must be encoded as Latin-1 cp1252
    lUInt16 smallPageCount; //    8-10	Number of small pages	The number of small font pages. If page index is not build in then 0.
    lUInt16 largePageCount; //    10-12	Number of large pages	The number of large font pages. If page index is not build in then 0.
    lUInt16 nonTextRecordStart; //12-14	Non-Text record start	The location of the first non text records. record 1 to this value minus 1 are all text records
    lUInt16 numberOfChapters;//    14-16	Number of chapters	The number of chapter index records contained in the file
    lUInt16 smallPageRecordCount; //    16-18	Number of small index	The number of small font page index records contained in the file
    lUInt16 largePageRecordCount; //    18-20	Number of large index	The number of large font page index records contained in the file
    lUInt16 imageCount;        //    20-22	Number of images	The number of images contained in the file
    lUInt16 linkCount;         //    22-24	Number of links	The number of links contained in the file
    lUInt16 metadataAvailable; //    24-26	Metadata avaliable	Is there a metadata record in the file? 0 = None, 1 = There is a metadata record
    lUInt16 unknown2; //    26-28	Unknown	Value of 0 is used
    lUInt16 footnoteRecordsCount; //    28-30	Number of Footnotes	The number of footnote records in the file
    lUInt16 sidebarRecordsCount; //    30-32	Number of Sidebars	The number of sidebar records in the file
    lUInt16 chapterIndexStart; //    32-34	Chapter index record start	The location of chapter index records. If there are no chapters use the value for the Last data record.
    lUInt16 unknown3; //    34-36	2560	Magic value that must be set to 2560
    lUInt16 smallPageIndexStart; //    36-38	Small page index start	The location of small font page index records. If page table is not built in use the value for the Last data record.
    lUInt16 largePageIndexStart; //    38-40	Large page index start	The location of large font page index records. If page table is not built in use the value for the Last data record.
    lUInt16 imageDataRecordStart; //    40-42	Image data record start	The location of the first image record. If there are no images use the value for the Last data record.
    lUInt16 linksRecordStart; //    42-44	Links record start	The location of the first link index record. If there are no links use the value for the Last data record.
    lUInt16 metadataRecordStart; //    44-46	Metadata record start	The location of the metadata record. If there is no metadata use the value for the Last data record.
    lUInt16 unknown4; //    46-48	Unknown	Value of 0 is used
    lUInt16 footnoteRecordStart; //    48-50	Footnote record start	The location of the first footnote record. If there are no footnotes use the value for the Last data record.
    lUInt16 sidebarRecordStart; //    50-52	Sidebar record start	The location of the first sidebar record. If there are no sidebars use the value for the Last data record.
    lUInt16 lastDataRecord; //    52-54	Last data record	The location of the last data record
    lUInt16 unknown5[39]; //    54-132	Unknown	Value of 0 is used
    bool read( LVStreamRef stream ) {
        lvsize_t bytesRead = 0;
        if ( stream->Read(this, sizeof(EReaderHeader), &bytesRead )!=LVERR_OK )
            return false;
        if ( bytesRead!=sizeof(EReaderHeader) )
            return false;
        lvByteOrderConv cnv;
        if ( cnv.lsf() )
        {
            cnv.rev(&compression);    //    0-2	compression	Specifies compression and drm. 2 = palmdoc, 10 = zlib. 260 and 272 = DRM
            cnv.rev(&encoding);       //    6-8	encoding	Always 25152 (0x6240). All text must be encoded as Latin-1 cp1252
            cnv.rev(&smallPageCount); //    8-10	Number of small pages	The number of small font pages. If page index is not build in then 0.
            cnv.rev(&largePageCount); //    10-12	Number of large pages	The number of large font pages. If page index is not build in then 0.
            cnv.rev(&nonTextRecordStart); //12-14	Non-Text record start	The location of the first non text records. record 1 to this value minus 1 are all text records
            cnv.rev(&numberOfChapters);//    14-16	Number of chapters	The number of chapter index records contained in the file
            cnv.rev(&smallPageRecordCount); //    16-18	Number of small index	The number of small font page index records contained in the file
            cnv.rev(&largePageRecordCount); //    18-20	Number of large index	The number of large font page index records contained in the file
            cnv.rev(&imageCount);        //    20-22	Number of images	The number of images contained in the file
            cnv.rev(&linkCount);         //    22-24	Number of links	The number of links contained in the file
            cnv.rev(&metadataAvailable); //    24-26	Metadata avaliable	Is there a metadata record in the file? 0 = None, 1 = There is a metadata record
            cnv.rev(&footnoteRecordsCount); //    28-30	Number of Footnotes	The number of footnote records in the file
            cnv.rev(&sidebarRecordsCount); //    30-32	Number of Sidebars	The number of sidebar records in the file
            cnv.rev(&chapterIndexStart); //    32-34	Chapter index record start	The location of chapter index records. If there are no chapters use the value for the Last data record.
            cnv.rev(&smallPageIndexStart); //    36-38	Small page index start	The location of small font page index records. If page table is not built in use the value for the Last data record.
            cnv.rev(&largePageIndexStart); //    38-40	Large page index start	The location of large font page index records. If page table is not built in use the value for the Last data record.
            cnv.rev(&imageDataRecordStart); //    40-42	Image data record start	The location of the first image record. If there are no images use the value for the Last data record.
            cnv.rev(&linksRecordStart); //    42-44	Links record start	The location of the first link index record. If there are no links use the value for the Last data record.
            cnv.rev(&metadataRecordStart); //    44-46	Metadata record start	The location of the metadata record. If there is no metadata use the value for the Last data record.
            cnv.rev(&footnoteRecordStart); //    48-50	Footnote record start	The location of the first footnote record. If there are no footnotes use the value for the Last data record.
            cnv.rev(&sidebarRecordStart); //    50-52	Sidebar record start	The location of the first sidebar record. If there are no sidebars use the value for the Last data record.
            cnv.rev(&lastDataRecord); //    52-54	Last data record	The location of the last data record
        }
        if ( compression!=1 && compression!=2 && compression!=10 )
            return false;
        return true;
    }
};

struct PluckerPreamble {
    lUInt32 signature; // 	4 	Numeric 	Must contain the value 0x6C6E6368.
    lUInt16 hdrVersion; // 	2 	Numeric 	Must have the value 3.
    lUInt16 hdrEncoding; // 	2 	Numeric 	Must have the value 0.
    lUInt16 verStrWords; // 	2 	Numeric 	The number of two-byte words following, containing the version string.
//    char  	2 * verStrWords 	String 	NUL-terminated ISO Latin-1 string, padded at end if necessary with a zero byte to an even-byte boundary, containing a version string to display to the user containing version information for the document.
//    pqaTitleWords 	2 	Numeric 	The number of two-byte words in the following pqaTitleStr.
//    pqaTitleStr 	2 * pqaTitleWords 	String 	NUL-terminated ISO Latin-1 string, padded at end if necessary with a zero byte to an even-byte boundary, containing a title string for iconic display of the document.
//    iconWords 	2 	Numeric 	Number of two-byte words in the following icon image.
//    icon 	2 * iconWords 	Image 	Image (32x32) in Palm image format to be used as an icon to represent the document on a desktop-style display. The image may not use a custom color map.
//    smIconWords 	2 	Numeric 	Number of two-byte words in the following icon image.
//    smIcon 	2 * smIconWords 	Image 	Small image (15x9) in Palm image format to be used as an icon to represent the document on a desktop-style display. The image may not use a custom color map.
};

/// unpack data from _compbuf to _buf
bool ldomUnpack( const lUInt8 * compbuf, int compsize, lUInt8 * &dstbuf, lUInt32 & dstsize  );

class PDBFile;

class LVPDBContainerItem : public LVContainerItemInfo {
protected:
    LVStreamRef _stream;
    PDBFile * _file;
    int _startBlock;
    int _size;
    lString16 _name;
public:
    /// returns object size (file size or directory entry count)
    virtual lverror_t GetSize( lvsize_t * pSize ) {
        *pSize = _size;
		return LVERR_OK;
    }
    virtual lvsize_t        GetSize() const { return _size; }
    virtual const lChar16 * GetName() const { return _name.c_str(); }
    virtual lUInt32         GetFlags() const { return 0; }
    virtual bool            IsContainer() const { return false; }
    virtual LVStreamRef openStream() {
        // TODO: implement stream creation
        return LVStreamRef();
    }
    LVPDBContainerItem( LVStreamRef stream, PDBFile * file, lString16 name, int startBlockIndex, int size )
        : _stream(stream), _file(file), _startBlock(startBlockIndex), _size(size), _name(name) {
    }
};

class LVPDBRegionContainerItem : public LVPDBContainerItem {
public:
    /// returns object size (file size or directory entry count)
    virtual lUInt32         GetFlags() const { return 0; }
    virtual LVStreamRef openStream() {
        // return region of base stream
        return LVStreamRef( new LVStreamFragment( _stream, _startBlock, _size ) );
    }
    LVPDBRegionContainerItem( LVStreamRef stream, PDBFile * file, lString16 name, int startOffset, int size )
        : LVPDBContainerItem(stream, file, name, startOffset, size) {
    }
};

class LVPDBContainer : public LVContainer
{
    LVPtrVector<LVPDBContainerItem> _list;
    LVStreamRef _stream;
public:
    virtual LVContainer * GetParentContainer() { return NULL; }

    void addItem ( LVPDBContainerItem * item ) {
        _list.add(item);
    }

    //virtual const LVContainerItemInfo * GetObjectInfo(const wchar_t * pname);
    virtual const LVContainerItemInfo * GetObjectInfo(int index) {
        if ( index>=0 && index<_list.length() )
            return _list[index];
		return NULL;
    }
    virtual int GetObjectCount() const { return _list.length(); }
    /// returns object size (file size or directory entry count)
    virtual lverror_t GetSize( lvsize_t * pSize ) {
        *pSize = _list.length();
		return LVERR_OK;
    }

    virtual LVStreamRef OpenStream( const lChar16 * fname, lvopen_mode_t mode ) {
        if ( mode!=LVOM_READ )
            return LVStreamRef();
        for ( int i=0; i<_list.length(); i++ ) {
            //CRLog::trace("OpenStream(%s) : %s", LCSTR(lString16(fname)), LCSTR(lString16(_list[i]->GetName())) );
            if ( !lStr_cmp(_list[i]->GetName(), fname) )
                return _list[i]->openStream();
        }
        return LVStreamRef();
    }

    void setStream( LVStreamRef stream ) {
        _stream = stream;
    }

    LVPDBContainer( ) {
        //_contentStream = LVStreamRef((LVStream*)file);
    }
    virtual ~LVPDBContainer() { }
};

static bool pattern_cmp( const lUInt8 * buf, const char * pattern ) {
    for ( int i=0; pattern[i]; i++ )
        if ( tolower(buf[i])!=pattern[i] )
            return false;
    return true;
}

class PDBFile : public LVNamedStream {
    struct Record {
        lUInt32 offset;
        lUInt32 size;
        lUInt32 unpoffset;
        lUInt32 unpsize;
    };
    LVArray<Record> _records;
    LVStreamRef _stream;
    enum Format {
        UNKNOWN,
        PALMDOC,
        EREADER,
        PLUCKER,
        MOBI,
    };
    Format _format;
    int _compression;
    lUInt32 _textSize;
    int _recordCount;
    // read buffer
    LVArray<lUInt8> _buf;
    int     _bufIndex;
    lvpos_t _bufOffset;
    lvsize_t _bufSize;
    lvpos_t _pos;
    //LVPDBContainer * _container;
    bool unpack( LVArray<lUInt8> & dst, LVArray<lUInt8> & src ) {
        int srclen = src.length();
        dst.clear();
        dst.reserve(srclen);

        if ( _compression==2 ) {
            // PalmDOC
            int pos = 0;
            lUInt32 b;

            while (pos<srclen) {
                b = src[pos];
                pos++;
                if (b > 0 && b < 9) {
                    for (int i=0; i<(int)b; i++)
                        dst.add(src[pos++]);
                } else if (b < 128) {
                    dst.add((lUInt8)b);
                } else if (b > 0xc0) {
                    dst.add(' ');
                    dst.add(b & 0x7f);
                } else {
                    if (pos >= srclen)
                        break;
                    int z = ((int)b << 8) | src[pos];
                    pos++;
                    int m = (z & 0x3fff) >> 3;
                    int n = (z & 7) + 3;
                    for (int i=0; i<n; i++)
                        dst.add(dst[dst.length()-m]);
                }
            }
        } else if ( _compression==10 ) {
            // zlib
            /// unpack data from _compbuf to _buf
            lUInt8 * dstbuf;
            lUInt32 dstsize;
            if ( !ldomUnpack( src.get(), src.size(), dstbuf, dstsize ) )
                return false;
            dst.add(dstbuf, dstsize);
            free(dstbuf);
        } else if ( _compression==17480 ) {
            // zlib
            /// unpack data from _compbuf to _buf
            lUInt8 * dstbuf;
            lUInt32 dstsize;
            if ( !ldomUnpack( src.get(), src.size(), dstbuf, dstsize ) )
                return false;
            dst.add(dstbuf, dstsize);
            free(dstbuf);
        }
        return true;
    }

    bool readRecord( int index, LVArray<lUInt8> * dstbuf ) {
        if ( index>=_records.length() )
            return false;
        LVArray<lUInt8> srcbuf;
        LVArray<lUInt8> * buf = _compression ? &srcbuf : dstbuf;
        buf->clear();
        buf->addSpace(_records[index].size);
        lvsize_t bytesRead = 0;
        _stream->SetPos(_records[index].offset);
        if ( _stream->Read(buf->get(), _records[index].size, &bytesRead )!=LVERR_OK )
            return false;
        if ( bytesRead!=_records[index].size )
            return false;
        if ( !_compression )
            return true;
        // unpack
        return unpack(*dstbuf, srcbuf);
    }

    bool readBlock( int index ) {
        if ( index<0 || index>=_recordCount )
            return false;
        if ( index==_bufIndex )
            return true; // already read
        bool res = readRecord( index+1, &_buf );
        if ( !res )
            return false;
        _bufIndex = index;
        _bufOffset = _records[index+1].unpoffset;
        _bufSize = _records[index+1].unpsize;
        return true;
    }

    int findBlock( lvpos_t pos ) {
        if ( pos==_textSize )
            return _recordCount-1;
        for ( int i=0; i<_recordCount; i++ ) {
            if ( pos>=_records[i+1].unpoffset && pos<_records[i+1].unpoffset+_records[i+1].unpsize )
                return i;
        }
        return -1;
    }

    bool seek( lvpos_t pos ) {
        int index = findBlock(pos);
        if ( index<0 )
            return false;
        bool res = readBlock( index );
        if ( !res )
            return false;
        _pos = pos;
        return true;
    }

public:

//    LVContainerRef getContainer() {
//        if ( !_container )
//            _container = new LVPDBContainer();
//        return LVContainerRef(&_container);
//    }


//    static PDBFile * create( LVStreamRef stream, int & format ) {
//        format = 0;
//        PDBFile * res = new PDBFile();
//        if ( res->open(stream, true, format) ) {
//            format = res->_format;
//            return res;
//        }
//        delete res;
//        return NULL;
//    }

    void detectFormat( doc_format_t & contentFormat ) {
        if ( contentFormat == doc_format_none ) {
            // autodetect format
            LVArray<lUInt8> buf;
            readRecord(1, &buf);
            int bytesRead = buf.length();
            if ( bytesRead>0 ) {
                int pmlCount = 0;
                int htmlCount = 0;
                lString16 pmlChars("pXxCcriuovtnsblaUBSmqQI");
                for ( int i=0; i<bytesRead-10; i++ ) {
                    const lUInt8 * p = buf.get() + i;
                    if ( p[0]=='\\' ) {
                        if ( pmlChars.pos(lString16((const lChar8 *)p+1, 1)) >=0 )
                            pmlCount++;
                    } else if (p[0]=='<') {
                        if ( pattern_cmp(p+1, "html") )
                            htmlCount+=100;
                        if ( pattern_cmp(p+1, "head") )
                            htmlCount+=50;
                        if ( pattern_cmp(p+1, "body") )
                            htmlCount+=50;
                        if ( pattern_cmp(p+1, "h1") || pattern_cmp(p+1, "h2") || pattern_cmp(p+1, "h3") || pattern_cmp(p+1, "h4"))
                            htmlCount+=5;
                        if ( pattern_cmp(p+1, "p>") || pattern_cmp(p+1, "b>") || pattern_cmp(p+1, "i>") || pattern_cmp(p+1, "li>") || pattern_cmp(p+1, "ul>"))
                            htmlCount+=10;
                    }
                }
                if ( pmlCount<5 && htmlCount<10 ) {
                    contentFormat = doc_format_txt;
                } else if ( pmlCount > htmlCount ) {
                    contentFormat = doc_format_fb2;
                } else {
                    contentFormat = doc_format_html;
                }
            }
            SetPos(0);
        }
    }

    bool open( LVStreamRef stream, LVPDBContainer * container, bool validateContent, doc_format_t & contentFormat ) {
        contentFormat = doc_format_none;
        _format = UNKNOWN;
        stream->SetPos(0);
        lUInt32 fsize = stream->GetSize();
        PDBHdr hdr;
        PDBRecordEntry entry;
        if ( !hdr.read(stream) )
            return false;
        if ( hdr.recordCount==0 )
            return 0;

        if ( hdr.checkType("TEXt") && hdr.checkCreator("REAd") )
            _format = PALMDOC;
        if ( hdr.checkType("PNRd") && hdr.checkCreator("PPrs") )
            _format = EREADER;
        if ( hdr.checkType("BOOK") && hdr.checkCreator("MOBI") )
            _format = MOBI;
        if ( hdr.checkType("Data") && hdr.checkCreator("Plkr") )
            _format = PLUCKER;
//        if ( hdr.checkType("ToGo") && hdr.checkCreator("ToGo") )
//            _format = ISILO;
        if ( _format==UNKNOWN )
            return false; // UNKNOWN FORMAT

        stream->SetPos(0x4E);
        lUInt32 lastEntryStart = 0;
        _records.addSpace(hdr.recordCount);
        for ( int i=0; i<hdr.recordCount; i++ ) {
            if ( !entry.read(stream) )
                return false;
            lUInt32 pos = entry.localChunkId;
            if ( pos<lastEntryStart || pos>=fsize )
                return false;
            _records[i].offset = pos;
            if ( i>0 )
                _records[i-1].size = pos - _records[i-1].offset;
            lastEntryStart = pos;
        }
        _records[_records.length()-1].size = fsize - _records[_records.length()-1].offset;


        _stream = stream;

        if ( _format==EREADER ) {
            if ( _records[0].size<sizeof(EReaderHeader) )
                return false;
            EReaderHeader preamble;
            stream->SetPos(_records[0].offset);
            if ( !preamble.read(stream) )
                return false; // invalid preamble
            _recordCount = preamble.nonTextRecordStart - 1;
            if ( _recordCount>=_records.length() )
                return false;
            _compression = preamble.compression;
            if ( _compression==1 )
                _compression = 0;
            _textSize = -1;
            if ( preamble.imageCount && container ) {
                for ( int index=preamble.imageDataRecordStart; index<preamble.imageDataRecordStart+preamble.imageCount; index++ ) {
                    lUInt32 start = _records[index].offset + 62;
                    lUInt32 size = _records[index].size - 62;
                    if ( start<fsize && start+size<=fsize ) {
                        stream->SetPos(_records[index].offset);
                        if ( stream->ReadByte()=='P' && stream->ReadByte()=='N' && stream->ReadByte()=='G' && stream->ReadByte()==' ' ) {
                            // header ok, adding item
                            char name[33];
                            memset(name, 0, 33);
                            lvsize_t bytesRead = 0;
                            stream->Read(name, 32, &bytesRead);
                            if ( name[0] ) {
                                lString16 fname = lString16(name);
                                container->addItem( new LVPDBRegionContainerItem( stream, this, fname, start, size ) );
                            }
                        }
                    }
                }
            }
        } else if (_format==MOBI ) {
            if ( _records[0].size<sizeof(MobiPreamble) )
                return false;
            MobiPreamble preamble;
            stream->SetPos(_records[0].offset);
            if ( !preamble.read(stream) )
                return false; // invalid preamble
            if ( preamble.recordCount>=_records.length() )
                return false;
            _compression = preamble.compression;
            if ( _compression==1 )
                _compression = 0;
            _textSize = preamble.textLength;
            _recordCount = preamble.firstNonBookIndex - 1;
            if ( container ) {
                for ( int index=preamble.firstImageIndex; index<_records.length(); index++ ) {
                    _records[index].offset;
                    stream->SetPos(_records[index].offset);
                    lUInt8 buf[256];
                    stream->Read(buf, 16, NULL);
                    //CRLog::debug("Image record %d [%02x %02x %02x %02x %02x]", index, buf[0], buf[1], buf[2], buf[3], buf[4]);
                    const char * fmt = NULL;
                    if (buf[0]==0xff && buf[1]==0xd8 && buf[2]==0xFF && buf[3]==0xe0)
                        fmt = "jpeg";
                    if (buf[0]==0x89 && buf[1]=='P' && buf[2]=='N' && buf[3]=='G')
                        fmt = "png";
                    if (buf[0]=='G' && buf[1]=='I' && buf[2]=='F')
                        fmt = "gif";
                    if (fmt) {
                        lString16 name = lString16(MOBI_IMAGE_NAME_PREFIX) + lString16::itoa((int)(index-preamble.firstImageIndex));
                        //CRLog::debug("Adding image %s [%d] %s", LCSTR(name), _records[index].size, fmt);
                        container->addItem( new LVPDBRegionContainerItem( stream, this, name, _records[index].offset, _records[index].size ) );
                        // TODO: set coverpage
                    }
                }
            }
        } else if (_format==PALMDOC ) {
            if ( _records[0].size<sizeof(PalmDocPreamble) )
                return false;
            PalmDocPreamble preamble;
            stream->SetPos(_records[0].offset);
            if ( !preamble.read(stream) )
                return false; // invalid preamble
            if ( preamble.recordCount>=_records.length() )
                return false;
            _compression = preamble.compression;
            if ( _compression==1 )
                _compression = 0;
            _textSize = preamble.textLength;
            _recordCount = preamble.recordCount;
        } else if (_format==PLUCKER ) {
            // TODO
            return false;
        }

        detectFormat( contentFormat );

        if ( !validateContent )
            return true; // for simple format check

        LVArray<lUInt8> buf;
        lUInt32 unpoffset = 0;
        _crc = 0;
        for ( int k=0; k<_recordCount; k++ ) {
            readRecord(k+1, &buf);
            _records[k+1].unpoffset = unpoffset;
            _records[k+1].unpsize = buf.length();
            unpoffset += buf.length();
            _crc = lStr_crc32( _crc, buf.get(), buf.length() );
//            if ( unpoffset>=_textSize ) {
//                _recordCount = k;
//                break;
//            }
        }
        if ( _textSize==-1 )
            _textSize = unpoffset;
        else if ( unpoffset<_textSize )
            return false; // text size does not match


        _bufIndex = -1;
        _bufSize = 0;
        _bufOffset = 0;

        SetName(_stream->GetName());
        m_mode = LVOM_READ;


        return true;
    }

    /// Seek (change file pos)
    /**
        \param offset is file offset (bytes) relateve to origin
        \param origin is offset base
        \param pNewPos points to place to store new file position
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t Seek( lvoffset_t offset, lvseek_origin_t origin, lvpos_t * pNewPos ) {
        lvpos_t npos = 0;
        lvpos_t currpos = _pos;
        switch (origin) {
        case LVSEEK_SET:
            npos = offset;
            break;
        case LVSEEK_CUR:
            npos = currpos + offset;
            break;
        case LVSEEK_END:
            npos = _textSize + offset;
            break;
        }
        if (npos > _textSize)
            return LVERR_FAIL;
        if (!seek(npos) )
            return LVERR_FAIL;
        if (pNewPos)
            *pNewPos =  _pos;
        return LVERR_OK;
    }

    /// Get file position
    /**
        \return lvpos_t file position
    */
    virtual lvpos_t GetPos()
    {
        return _pos;
    }

    /// Get file size
    /**
        \return lvsize_t file size
    */
    virtual lvsize_t  GetSize()
    {
        return _textSize;
    }

    virtual lverror_t GetSize( lvsize_t * pSize )
    {
        *pSize = _textSize;
        return LVERR_OK;
    }

    /// Set file size
    /**
        \param size is new file size
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t SetSize( lvsize_t size ) {
        return LVERR_NOTIMPL;
    }

    /// Read
    /**
        \param buf is buffer to place bytes read from stream
        \param count is number of bytes to read from stream
        \param nBytesRead is place to store real number of bytes read from stream
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t Read( void * buf, lvsize_t count, lvsize_t * nBytesRead ) {
        lvsize_t bytesRead = 0;
        if ( nBytesRead )
            *nBytesRead = bytesRead;
        lUInt8 * dst = (lUInt8 *)buf;
        while ( count > 0 ) {
            if ( ! seek(_pos) ) {
                if ( _pos>=_textSize )
                    break;
                return LVERR_FAIL;
            }
            int bytesLeft = (int)(_bufOffset + _bufSize - _pos);
            if ( bytesLeft<=0 )
                break;
            int sz = count;
            if ( sz>bytesLeft )
                sz = bytesLeft;
            for ( int i=0; i<sz; i++ )
                dst[i] = _buf[_pos - _bufOffset + i];
            _pos += sz;
            dst += sz;
            count -= sz;
            bytesRead += sz;
        }
        if ( nBytesRead )
            *nBytesRead = bytesRead;
        return LVERR_OK;
    }

    /// Write
    /**
        \param buf is data to write to stream
        \param count is number of bytes to write
        \param nBytesWritten is place to store real number of bytes written to stream
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t Write( const void * buf, lvsize_t count, lvsize_t * nBytesWritten ) {
        return LVERR_NOTIMPL;
    }

    /// Check whether end of file is reached
    /**
        \return true if end of file reached
    */
    virtual bool Eof() {
        return _pos>=_textSize;
    }

    /// Constructor
    PDBFile() {
        //_container.AddRef();
    }

    /// Destructor
    virtual ~PDBFile() { }

};

// open PDB stream from stream
//LVStreamRef LVOpenPDBStream( LVStreamRef srcstream, int &format )
//{
//    PDBFile * stream = PDBFile::create( srcstream, format );
//    srcstream->SetPos(0);
//    if ( stream!=NULL )
//    {
//        return LVStreamRef( stream );
//    }
//    return LVStreamRef();
//}

bool DetectPDBFormat( LVStreamRef stream, doc_format_t & contentFormat )
{
    PDBFile pdb;
    if ( !pdb.open(stream, NULL, false, contentFormat) )
        return false;
    return true;
}

bool ImportPDBDocument( LVStreamRef & stream, ldomDocument * doc, LVDocViewCallback * progressCallback, CacheLoadingCallback * formatCallback, doc_format_t & contentFormat )
{
    contentFormat = doc_format_none;
    PDBFile * pdb = new PDBFile();
    LVPDBContainer * container = new LVPDBContainer();
    if ( !pdb->open(stream, container, true, contentFormat) ) {
        delete container;
        return false;
    }
    stream = LVStreamRef(pdb);
    container->setStream(stream);
    doc->setContainer(LVContainerRef(container));

#if BUILD_LITE!=1
    if ( doc->openFromCache(formatCallback) ) {
        if ( progressCallback ) {
            progressCallback->OnLoadFileEnd( );
        }
        return true;
    }
#endif

    switch ( contentFormat ) {
    case doc_format_html:
        // HTML
        {
            ldomDocumentWriterFilter writerFilter(doc, false,
                    HTML_AUTOCLOSE_TABLE);

            LVHTMLParser parser(stream, &writerFilter);
            parser.setProgressCallback(progressCallback);
            if ( !parser.CheckFormat() ) {
                return false;
            } else {
                if (!parser.Parse()) {
                    return false;
                }
            }
        }
        break;
    default:
    //case doc_format_txt:
        // TXT
        {
            ldomDocumentWriter writer(doc);
            LVTextParser parser(stream, &writer, false);
            parser.setProgressCallback(progressCallback);
            if ( !parser.CheckFormat() ) {
                return false;
            } else {
                if (!parser.Parse()) {
                    return false;
                }
            }
        }
        break;
        // PML
        {
//            ldomDocumentWriterFilter writerFilter(*doc, false,
//                    HTML_AUTOCLOSE_TABLE);

//            LVHTMLParser parser(m_stream, &writerFilter);
//            parser->setProgressCallback(progressCallback);
//            if ( !parser->CheckFormat() ) {
//                return false;
//            } else {
//                if (!parser->Parse()) {
//                    return false;
//                }
//            }
        }
        break;
    }

    return true;
}
