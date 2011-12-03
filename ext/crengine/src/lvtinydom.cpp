/*******************************************************

   CoolReader Engine

   lvtinydom.cpp: fast and compact XML DOM tree

   (c) Vadim Lopatin, 2000-2011
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

/// change in case of incompatible changes in swap/cache file format to avoid using incompatible swap file
#define CACHE_FILE_FORMAT_VERSION "3.04.07"

#ifndef DOC_DATA_COMPRESSION_LEVEL
/// data compression level (0=no compression, 1=fast compressions, 3=normal compression)
#define DOC_DATA_COMPRESSION_LEVEL 1 // 0, 1, 3 (0=no compression)
#endif

#ifndef STREAM_AUTO_SYNC_SIZE
#define STREAM_AUTO_SYNC_SIZE 300000
#endif //STREAM_AUTO_SYNC_SIZE

//=====================================================
// Document data caching parameters
//=====================================================

#ifndef DOC_BUFFER_SIZE
#define DOC_BUFFER_SIZE 0xA00000 // default buffer size
#endif

//--------------------------------------------------------
// cache memory sizes
//--------------------------------------------------------
#ifndef ENABLED_BLOCK_WRITE_CACHE
#define ENABLED_BLOCK_WRITE_CACHE 1
#endif

#define WRITE_CACHE_TOTAL_SIZE    (10*DOC_BUFFER_SIZE/100)

#define TEXT_CACHE_UNPACKED_SPACE (25*DOC_BUFFER_SIZE/100)
#define TEXT_CACHE_CHUNK_SIZE     0x008000 // 32K
#define ELEM_CACHE_UNPACKED_SPACE (45*DOC_BUFFER_SIZE/100)
#define ELEM_CACHE_CHUNK_SIZE     0x004000 // 16K
#define RECT_CACHE_UNPACKED_SPACE (15*DOC_BUFFER_SIZE/100)
#define RECT_CACHE_CHUNK_SIZE     0x008000 // 32K
#define STYLE_CACHE_UNPACKED_SPACE (10*DOC_BUFFER_SIZE/100)
#define STYLE_CACHE_CHUNK_SIZE    0x00C000 // 48K
//--------------------------------------------------------

#define COMPRESS_NODE_DATA          true
#define COMPRESS_NODE_STORAGE_DATA  true
#define COMPRESS_MISC_DATA          true
#define COMPRESS_PAGES_DATA         true
#define COMPRESS_TOC_DATA           true
#define COMPRESS_STYLE_DATA         true

//#define CACHE_FILE_SECTOR_SIZE 4096
#define CACHE_FILE_SECTOR_SIZE 1024
#define CACHE_FILE_WRITE_BLOCK_PADDING 1

/// set t 1 to log storage reads/writes
#define DEBUG_DOM_STORAGE 0
//#define TRACE_AUTOBOX
/// set to 1 to enable crc check of all blocks of cache file on open
#ifndef ENABLE_CACHE_FILE_CONTENTS_VALIDATION
#define ENABLE_CACHE_FILE_CONTENTS_VALIDATION 0
#endif

#define RECT_DATA_CHUNK_ITEMS_SHIFT 11
#define STYLE_DATA_CHUNK_ITEMS_SHIFT 12

// calculated parameters
#define WRITE_CACHE_BLOCK_SIZE 0x4000
#define WRITE_CACHE_BLOCK_COUNT (WRITE_CACHE_TOTAL_SIZE/WRITE_CACHE_BLOCK_SIZE)
#define TEST_BLOCK_STREAM 0

#define PACK_BUF_SIZE 0x10000
#define UNPACK_BUF_SIZE 0x40000

#define RECT_DATA_CHUNK_ITEMS (1<<RECT_DATA_CHUNK_ITEMS_SHIFT)
#define RECT_DATA_CHUNK_SIZE (RECT_DATA_CHUNK_ITEMS*sizeof(lvdomElementFormatRec))
#define RECT_DATA_CHUNK_MASK (RECT_DATA_CHUNK_ITEMS-1)

#define STYLE_DATA_CHUNK_ITEMS (1<<STYLE_DATA_CHUNK_ITEMS_SHIFT)
#define STYLE_DATA_CHUNK_SIZE (STYLE_DATA_CHUNK_ITEMS*sizeof(ldomNodeStyleInfo))
#define STYLE_DATA_CHUNK_MASK (STYLE_DATA_CHUNK_ITEMS-1)


#define STYLE_HASH_TABLE_SIZE     512
#define FONT_HASH_TABLE_SIZE      256


static const char CACHE_FILE_MAGIC[] = "CoolReader 3 Cache"
                                       " File v" CACHE_FILE_FORMAT_VERSION ": "
                                       "c0"
#if DOC_DATA_COMPRESSION_LEVEL==0
                                       "m0"
#else
                                       "m1"
#endif
                                        "\n";

#define CACHE_FILE_MAGIC_SIZE 40

enum CacheFileBlockType {
    CBT_FREE = 0,
    CBT_INDEX = 1,
    CBT_TEXT_DATA,
    CBT_ELEM_DATA,
    CBT_RECT_DATA, //4
    CBT_ELEM_STYLE_DATA,
    CBT_MAPS_DATA,
    CBT_PAGE_DATA, //7
    CBT_PROP_DATA,
    CBT_NODE_INDEX,
    CBT_ELEM_NODE,
    CBT_TEXT_NODE,
    CBT_REND_PARAMS, //12
    CBT_TOC_DATA,
    CBT_STYLE_DATA,
    CBT_BLOB_INDEX, //15
    CBT_BLOB_DATA,
};


#include <stdlib.h>
#include <string.h>
#include "../include/crsetup.h"
#include "../include/lvstring.h"
#include "../include/lvtinydom.h"
#include "../include/fb2def.h"
#if BUILD_LITE!=1
#include "../include/lvrend.h"
#include "../include/chmfmt.h"
#endif
#include "../include/crtest.h"
#include <stddef.h>
#include <math.h>
#include <zlib.h>

// define to store new text nodes as persistent text, instead of mutable
#define USE_PERSISTENT_TEXT 1


static int _nextDocumentIndex = 0;
ldomDocument * ldomNode::_documentInstances[MAX_DOCUMENT_INSTANCE_COUNT] = {NULL,};

/// adds document to list, returns ID of allocated document, -1 if no space in instance array
int ldomNode::registerDocument( ldomDocument * doc )
{
    for ( int i=0; i<MAX_DOCUMENT_INSTANCE_COUNT; i++ ) {
        if ( _nextDocumentIndex<0 || _nextDocumentIndex>=MAX_DOCUMENT_INSTANCE_COUNT )
            _nextDocumentIndex = 0;
        if ( _documentInstances[_nextDocumentIndex]==NULL) {
            _documentInstances[_nextDocumentIndex] = doc;
            CRLog::info("ldomNode::registerDocument() - new index = %d", _nextDocumentIndex);
            return _nextDocumentIndex++;
        }
        _nextDocumentIndex++;
    }
    return -1;
}

/// removes document from list
void ldomNode::unregisterDocument( ldomDocument * doc )
{
    for ( int i=0; i<MAX_DOCUMENT_INSTANCE_COUNT; i++ ) {
        if ( _documentInstances[i]==doc ) {
            CRLog::info("ldomNode::unregisterDocument() - for index %d", i);
            _documentInstances[i] = NULL;
        }
    }
}

/// mutable text node
class ldomTextNode
{
    lUInt32 _parentIndex;
    lString8 _text;
public:

    lUInt32 getParentIndex()
    {
        return _parentIndex;
    }

    void setParentIndex( lUInt32 n )
    {
        _parentIndex = n;
    }

    ldomTextNode( lUInt32 parentIndex, const lString8 & text )
    : _parentIndex(parentIndex), _text(text)
    {
    }

    lString8 getText()
    {
        return _text;
    }

    lString16 getText16()
    {
        return Utf8ToUnicode(_text);
    }

    void setText( const lString8 & s )
    {
        _text = s;
    }

    void setText( const lString16 & s )
    {
        _text = UnicodeToUtf8(s);
    }
};

#define LASSERT(x) \
    if (!(x)) crFatalError(1111, "assertion failed: " #x)

//#define INDEX1 94
//#define INDEX2 96

//#define INDEX1 105
//#define INDEX2 106

/// pack data from _buf to _compbuf
bool ldomPack( const lUInt8 * buf, int bufsize, lUInt8 * &dstbuf, lUInt32 & dstsize );
/// unpack data from _compbuf to _buf
bool ldomUnpack( const lUInt8 * compbuf, int compsize, lUInt8 * &dstbuf, lUInt32 & dstsize  );


#if BUILD_LITE!=1

//static lUInt32 calcHash32( const lUInt8 * s, int len )
//{
//    lUInt32 res = 0;
//    for ( int i=0; i<len; i++ ) {
//        // res*31 + s
//        res = (((((((res<<1)+res)<<1)+res)<<1)+res)<<1)+res + s[i];
//    }
//    return res;
//}

// FNV 64bit hash function
// from http://isthe.com/chongo/tech/comp/fnv/#gcc-O3

#define NO_FNV_GCC_OPTIMIZATION
#define FNV_64_PRIME ((lUInt64)0x100000001b3ULL)
static lUInt64 calcHash64( const lUInt8 * s, int len )
{
    const lUInt8 * endp = s + len;
    // 64 bit FNV hash function
    lUInt64 hval = 14695981039346656037ULL;
    for ( ; s<endp; s++ ) {
#if defined(NO_FNV_GCC_OPTIMIZATION)
        hval *= FNV_64_PRIME;
#else /* NO_FNV_GCC_OPTIMIZATION */
        hval += (hval << 1) + (hval << 4) + (hval << 5) +
            (hval << 7) + (hval << 8) + (hval << 40);
#endif /* NO_FNV_GCC_OPTIMIZATION */
        hval ^= *s;
    }
    return hval;
}

lUInt32 calcGlobalSettingsHash()
{
    lUInt32 hash = 0;
    if ( fontMan->getKerning() )
        hash += 127365;
    hash = hash * 31 + fontMan->GetFontListHash();
    if ( LVRendGetFontEmbolden() )
        hash = hash * 75 + 2384761;
    if ( gFlgFloatingPunctuationEnabled )
        hash = hash * 75 + 1761;
    hash = hash * 31 + (HyphMan::getSelectedDictionary()!=NULL ? HyphMan::getSelectedDictionary()->getHash() : 123 );
    return hash;
}

static void dumpRendMethods( ldomNode * node, lString16 prefix )
{
    lString16 name = prefix;
    if ( node->isText() )
        name << node->getText();
    else
        name << L"<" << node->getNodeName() << L">   " << lString16::itoa(node->getRendMethod());
    CRLog::trace( "%s ",LCSTR(name) );
    for ( unsigned i=0; i<node->getChildCount(); i++ ) {
        dumpRendMethods( node->getChildNode(i), prefix + L"   ");
    }
}





#define CACHE_FILE_ITEM_MAGIC 0xC007B00C
struct CacheFileItem
{
    lUInt32 _magic;    // magic number
    lUInt16 _dataType;     // data type
    lUInt16 _dataIndex;    // additional data index, for internal usage for data type
    int _blockIndex;   // sequential number of block
    int _blockFilePos; // start of block
    int _blockSize;    // size of block within file
    int _dataSize;     // used data size inside block (<= block size)
    lUInt64 _dataHash; // additional hash of data
    lUInt64 _packedHash; // additional hash of packed data
    lUInt32 _uncompressedSize;   // size of uncompressed block, if compression is applied, 0 if no compression
    bool validate( int fsize )
    {
        if ( _magic!=CACHE_FILE_ITEM_MAGIC ) {
            CRLog::error("CacheFileItem::validate: block magic doesn't match");
            return false;
        }
        if ( _dataSize>_blockSize || _blockSize<0 || _dataSize<0 || _blockFilePos+_dataSize>fsize || _blockFilePos<CACHE_FILE_SECTOR_SIZE) {
            CRLog::error("CacheFileItem::validate: invalid block size or position");
            return false;
        }
        return true;
    }
    CacheFileItem()
    {
    }
    CacheFileItem( lUInt16 dataType, lUInt16 dataIndex )
    : _magic(CACHE_FILE_ITEM_MAGIC)
    , _dataType(dataType)   // data type
    , _dataIndex(dataIndex) // additional data index, for internal usage for data type
    , _blockIndex(0)        // sequential number of block
    , _blockFilePos(0)      // start of block
    , _blockSize(0)         // size of block within file
    , _dataSize(0)          // used data size inside block (<= block size)
    , _dataHash(0)          // hash of data
    , _packedHash(0) // additional hash of packed data
    , _uncompressedSize(0)  // size of uncompressed block, if compression is applied, 0 if no compression
    {
    }
};


struct SimpleCacheFileHeader
{
    char _magic[CACHE_FILE_MAGIC_SIZE]; // magic
    lUInt32 _dirty;
    SimpleCacheFileHeader( lUInt32 dirtyFlag ) {
        memset( _magic, 0, sizeof(_magic));
        memcpy( _magic, CACHE_FILE_MAGIC, CACHE_FILE_MAGIC_SIZE );
        _dirty = dirtyFlag;
    }
};

struct CacheFileHeader : public SimpleCacheFileHeader
{
    lUInt32 _fsize;
    CacheFileItem _indexBlock; // index array block parameters,
    // duplicate of one of index records which contains
    bool validate()
    {
        if (memcmp(_magic, CACHE_FILE_MAGIC, CACHE_FILE_MAGIC_SIZE) != 0) {
            CRLog::error("CacheFileHeader::validate: magic doesn't match");
            return false;
        }
        if ( _dirty!=0 ) {
            CRLog::error("CacheFileHeader::validate: dirty flag is set");
            return false;
        }
        return true;
    }
    CacheFileHeader( CacheFileItem * indexRec, int fsize, lUInt32 dirtyFlag )
    : SimpleCacheFileHeader(dirtyFlag), _indexBlock(0,0)
    {
        if ( indexRec ) {
            memcpy( &_indexBlock, indexRec, sizeof(CacheFileItem));
        } else
            memset( &_indexBlock, 0, sizeof(CacheFileItem));
        _fsize = fsize;
    }
};

/**
 * Cache file implementation.
 */
class CacheFile
{
    int _sectorSize; // block position and size granularity
    int _size;
    bool _indexChanged;
    bool _dirty;
    LVStreamRef _stream; // file stream
    LVPtrVector<CacheFileItem, true> _index; // full file block index
    LVPtrVector<CacheFileItem, false> _freeIndex; // free file block index
    LVHashTable<lUInt32, CacheFileItem*> _map; // hash map for fast search
    // searches for existing block
    CacheFileItem * findBlock( lUInt16 type, lUInt16 index );
    // alocates block at index, reuses existing one, if possible
    CacheFileItem * allocBlock( lUInt16 type, lUInt16 index, int size );
    // mark block as free, for later reusing
    void freeBlock( CacheFileItem * block );
    // writes file header
    bool updateHeader();
    // writes index block
    bool writeIndex();
    // reads index from file
    bool readIndex();
    // reads all blocks of index and checks CRCs
    bool validateContents();
public:
    // return current file size
    int getSize() { return _size; }
    // create uninitialized cache file, call open or create to initialize
    CacheFile();
    // free resources
    ~CacheFile();
    // try open existing cache file
    bool open( lString16 filename );
    // try open existing cache file from stream
    bool open( LVStreamRef stream );
    // create new cache file
    bool create( lString16 filename );
    // create new cache file in stream
    bool create( LVStreamRef stream );
    /// writes block to file
    bool write( lUInt16 type, lUInt16 dataIndex, const lUInt8 * buf, int size, bool compress );
    /// reads and allocates block in memory
    bool read( lUInt16 type, lUInt16 dataIndex, lUInt8 * &buf, int &size );
    /// reads and validates block
    bool validate( CacheFileItem * block );
    /// writes content of serial buffer
    bool write( lUInt16 type, lUInt16 index, SerialBuf & buf, bool compress );
    /// reads content of serial buffer
    bool read( lUInt16 type, lUInt16 index, SerialBuf & buf );
    /// writes content of serial buffer
    bool write( lUInt16 type, SerialBuf & buf, bool compress )
    {
        return write( type, 0, buf, compress);
    }
    /// reads content of serial buffer
    bool read( lUInt16 type, SerialBuf & buf )
    {
        return read(type, 0, buf);
    }
    /// reads block as a stream
    LVStreamRef readStream(lUInt16 type, lUInt16 index);

    /// sets dirty flag value, returns true if value is changed
    bool setDirtyFlag( bool dirty );
    // flushes index
    bool flush( bool clearDirtyFlag, CRTimerUtil & maxTime );
    int roundSector( int n )
    {
        return (n + (_sectorSize-1)) & ~(_sectorSize-1);
    }
    void setAutoSyncSize(int sz) {
        _stream->setAutoSyncSize(sz);
    }
};


// create uninitialized cache file, call open or create to initialize
CacheFile::CacheFile()
: _sectorSize( CACHE_FILE_SECTOR_SIZE ), _size(0), _indexChanged(false), _dirty(true), _map(1024)
{
}

// free resources
CacheFile::~CacheFile()
{
    if ( !_stream.isNull() ) {
        // don't flush -- leave file dirty
        //CRTimerUtil infinite;
        //flush( true, infinite );
    }
}

/// sets dirty flag value, returns true if value is changed
bool CacheFile::setDirtyFlag( bool dirty )
{
    if ( _dirty==dirty )
        return false;
    if ( !dirty ) {
        CRLog::info("CacheFile::clearing Dirty flag");
        _stream->Flush(true);
    } else {
        CRLog::info("CacheFile::setting Dirty flag");
    }
    _dirty = dirty;
    SimpleCacheFileHeader hdr(_dirty?1:0);
    _stream->SetPos(0);
    lvsize_t bytesWritten = 0;
    _stream->Write(&hdr, sizeof(hdr), &bytesWritten );
    if ( bytesWritten!=sizeof(hdr) )
        return false;
    _stream->Flush(true);
    //CRLog::trace("setDirtyFlag : hdr is saved with Dirty flag = %d", hdr._dirty);
    return true;
}

// flushes index
bool CacheFile::flush( bool clearDirtyFlag, CRTimerUtil & maxTime )
{
    if ( clearDirtyFlag ) {
        //setDirtyFlag(true);
        if ( !writeIndex() )
            return false;
        setDirtyFlag(false);
    } else {
        _stream->Flush(false, maxTime);
        //CRLog::trace("CacheFile->flush() took %d ms ", (int)timer.elapsed());
    }
    return true;
}

// reads all blocks of index and checks CRCs
bool CacheFile::validateContents()
{
    CRLog::info("Started validation of cache file contents");
    LVHashTable<lUInt32, CacheFileItem*>::pair * pair;
    for ( LVHashTable<lUInt32, CacheFileItem*>::iterator p = _map.forwardIterator(); (pair=p.next())!=NULL; ) {
        if ( pair->value->_dataType==CBT_INDEX )
            continue;
        if ( !validate(pair->value) ) {
            CRLog::error("Contents validation is failed for block type=%d index=%d", (int)pair->value->_dataType, pair->value->_dataIndex );
            return false;
        }
    }
    CRLog::info("Finished validation of cache file contents -- successful");
    return true;
}

// reads index from file
bool CacheFile::readIndex()
{
    CacheFileHeader hdr(NULL, _size, 0);
    _stream->SetPos(0);
    lvsize_t bytesRead = 0;
    _stream->Read(&hdr, sizeof(hdr), &bytesRead );
    if ( bytesRead!=sizeof(hdr) )
        return false;
    CRLog::info("Header read: DirtyFlag=%d", hdr._dirty);
    if ( !hdr.validate() )
        return false;
    if ( (int)hdr._fsize > _size + 4096-1 ) {
        CRLog::error("CacheFile::readIndex: file size doesn't match with header");
        return false;
    }
    if ( !hdr._indexBlock._blockFilePos )
        return true; // empty index is ok
    if ( hdr._indexBlock._blockFilePos>=(int)hdr._fsize || hdr._indexBlock._blockFilePos+hdr._indexBlock._blockSize>(int)hdr._fsize+4096-1 ) {
        CRLog::error("CacheFile::readIndex: Wrong index file position specified in header");
        return false;
    }
    if ((int)_stream->SetPos(hdr._indexBlock._blockFilePos)!=hdr._indexBlock._blockFilePos ) {
        CRLog::error("CacheFile::readIndex: cannot move file position to index block");
        return false;
    }
    int count = hdr._indexBlock._dataSize / sizeof(CacheFileItem);
    if ( count<0 || count>100000 ) {
        CRLog::error("CacheFile::readIndex: invalid number of blocks in index");
        return false;
    }
    CacheFileItem * index = new CacheFileItem[count];
    bytesRead = 0;
    lvsize_t  sz = sizeof(CacheFileItem)*count;
    _stream->Read(index, sz, &bytesRead );
    if ( bytesRead!=sz )
        return false;
    // check CRC
    lUInt64 hash = calcHash64( (lUInt8*)index, sz );
    if ( hdr._indexBlock._dataHash!=hash ) {
        CRLog::error("CacheFile::readIndex: CRC doesn't match found %08x expected %08x", hash, hdr._indexBlock._dataHash);
        delete[] index;
        return false;
    }
    for ( int i=0; i<count; i++ ) {
        if (index[i]._dataType == CBT_INDEX)
            index[i] = hdr._indexBlock;
        if ( !index[i].validate(_size) ) {
            delete[] index;
            return false;
        }
        CacheFileItem * item = new CacheFileItem();
        memcpy(item, &index[i], sizeof(CacheFileItem));
        _index.add( item );
        lUInt32 key = ((lUInt32)item->_dataType)<<16 | item->_dataIndex;
        if ( key==0 )
            _freeIndex.add( item );
        else
            _map.set( key, item );
    }
    delete[] index;
    CacheFileItem * indexitem = findBlock(CBT_INDEX, 0);
    if ( !indexitem ) {
        CRLog::error("CacheFile::readIndex: index block info doesn't match header");
        return false;
    }
    _dirty = hdr._dirty;
    return true;
}

// writes index block
bool CacheFile::writeIndex()
{
    if ( !_indexChanged )
        return true; // no changes: no writes

    if ( _index.length()==0 )
        return updateHeader();

    // create copy of index in memory
    int count = _index.length();
    CacheFileItem * indexItem = findBlock(CBT_INDEX, 0);
    if (!indexItem) {
        int sz = sizeof(CacheFileItem) * (count * 2 + 100);
        allocBlock(CBT_INDEX, 0, sz);
        indexItem = findBlock(CBT_INDEX, 0);
        count = _index.length();
    }
    CacheFileItem * index = new CacheFileItem[count];
    int sz = count * sizeof(CacheFileItem);
    memset(index, 0, sz);
    for ( int i = 0; i < count; i++ ) {
        memcpy( &index[i], _index[i], sizeof(CacheFileItem) );
        if (index[i]._dataType == CBT_INDEX) {
            index[i]._dataHash = 0;
            index[i]._packedHash = 0;
            index[i]._dataSize = 0;
        }
    }
    bool res = write(CBT_INDEX, 0, (const lUInt8*)index, sz, false);
    delete[] index;

    indexItem = findBlock(CBT_INDEX, 0);
    if ( !res || !indexItem ) {
        CRLog::error("CacheFile::writeIndex: error while writing index!!!");
        return false;
    }

    updateHeader();
    _indexChanged = false;
    return true;
}

// writes file header
bool CacheFile::updateHeader()
{
    CacheFileItem * indexItem = NULL;
    indexItem = findBlock(CBT_INDEX, 0);
    CacheFileHeader hdr(indexItem, _size, _dirty?1:0);
    _stream->SetPos(0);
    lvsize_t bytesWritten = 0;
    _stream->Write(&hdr, sizeof(hdr), &bytesWritten );
    if ( bytesWritten!=sizeof(hdr) )
        return false;
    //CRLog::trace("updateHeader finished: Dirty flag = %d", hdr._dirty);
    return true;
}

//
void CacheFile::freeBlock( CacheFileItem * block )
{
    lUInt32 key = ((lUInt32)block->_dataType)<<16 | block->_dataIndex;
    _map.remove(key);
    block->_dataIndex = 0;
    block->_dataType = 0;
    block->_dataSize = 0;
    _freeIndex.add( block );
}

/// reads block as a stream
LVStreamRef CacheFile::readStream(lUInt16 type, lUInt16 index)
{
    CacheFileItem * block = findBlock(type, index);
    if (block && block->_dataSize) {
#if 0
        lUInt8 * buf = NULL;
        int size = 0;
        if (read(type, index, buf, size))
            return LVCreateMemoryStream(buf, size);
#else
        return LVStreamRef(new LVStreamFragment(_stream, block->_blockFilePos, block->_dataSize));
#endif
    }
    return LVStreamRef();
}

// searches for existing block
CacheFileItem * CacheFile::findBlock( lUInt16 type, lUInt16 index )
{
    lUInt32 key = ((lUInt32)type)<<16 | index;
    CacheFileItem * existing = _map.get( key );
    return existing;
}

// allocates index record for block, sets its new size
CacheFileItem * CacheFile::allocBlock( lUInt16 type, lUInt16 index, int size )
{
    lUInt32 key = ((lUInt32)type)<<16 | index;
    CacheFileItem * existing = _map.get( key );
    if ( existing ) {
        if ( existing->_blockSize >= size ) {
            if ( existing->_dataSize != size ) {
                existing->_dataSize = size;
                _indexChanged = true;
            }
            return existing;
        }
        // old block has not enough space: free it
        freeBlock( existing );
        existing = NULL;
    }
    // search for existing free block of proper size
    int bestSize = -1;
    int bestIndex = -1;
    for ( int i=0; i<_freeIndex.length(); i++ ) {
        if ( _freeIndex[i] && (_freeIndex[i]->_blockSize>=size) && (bestSize==-1 || _freeIndex[i]->_blockSize<bestSize) ) {
            bestSize = _freeIndex[i]->_blockSize;
            bestIndex = -1;
            existing = _freeIndex[i];
        }
    }
    if ( existing ) {
        _freeIndex.remove( existing );
        existing->_dataType = type;
        existing->_dataIndex = index;
        existing->_dataSize = size;
        _map.set( key, existing );
        _indexChanged = true;
        return existing;
    }
    // allocate new block
    CacheFileItem * block = new CacheFileItem( type, index );
    _map.set( key, block );
    block->_blockSize = roundSector(size);
    block->_dataSize = size;
    block->_blockIndex = _index.length();
    _index.add(block);
    block->_blockFilePos = _size;
    _size += block->_blockSize;
    _indexChanged = true;
    // really, file size is not extended
    return block;
}

/// reads and validates block
bool CacheFile::validate( CacheFileItem * block )
{
    lUInt8 * buf = NULL;
    int size = 0;

    if ( (int)_stream->SetPos( block->_blockFilePos )!=block->_blockFilePos ) {
        CRLog::error("CacheFile::validate: Cannot set position for block %d:%d of size %d", block->_dataType, block->_dataIndex, (int)size);
        return false;
    }

    // read block from file
    size = block->_dataSize;
    buf = (lUInt8 *)malloc(size);
    lvsize_t bytesRead = 0;
    _stream->Read(buf, size, &bytesRead );
    if ( bytesRead!=size ) {
        CRLog::error("CacheFile::validate: Cannot read block %d:%d of size %d", block->_dataType, block->_dataIndex, (int)size);
        free(buf);
        return false;
    }

    // check CRC for file block
    lUInt64 packedhash = calcHash64( buf, size );
    if ( packedhash!=block->_packedHash ) {
        CRLog::error("CacheFile::validate: packed data CRC doesn't match for block %d:%d of size %d", block->_dataType, block->_dataIndex, (int)size);
        free(buf);
        return false;
    }
    free(buf);
    return true;
}

// reads and allocates block in memory
bool CacheFile::read( lUInt16 type, lUInt16 dataIndex, lUInt8 * &buf, int &size )
{
    buf = NULL;
    size = 0;
    CacheFileItem * block = findBlock( type, dataIndex );
    if ( !block ) {
        CRLog::error("CacheFile::read: Block %d:%d not found in file", type, dataIndex);
        return false;
    }
    if ( (int)_stream->SetPos( block->_blockFilePos )!=block->_blockFilePos )
        return false;

    // read block from file
    size = block->_dataSize;
    buf = (lUInt8 *)malloc(size);
    lvsize_t bytesRead = 0;
    _stream->Read(buf, size, &bytesRead );
    if ( (int)bytesRead!=size ) {
        CRLog::error("CacheFile::read: Cannot read block %d:%d of size %d", type, dataIndex, (int)size);
        free(buf);
        buf = NULL;
        size = 0;
        return false;
    }

    bool compress = block->_uncompressedSize!=0;

    if ( compress ) {
        // block is compressed

        // check crc separately only for compressed data
        lUInt64 packedhash = calcHash64( buf, size );
        if ( packedhash!=block->_packedHash ) {
            CRLog::error("CacheFile::read: packed data CRC doesn't match for block %d:%d of size %d", type, dataIndex, (int)size);
            free(buf);
            buf = NULL;
            size = 0;
            return false;
        }

        // uncompress block data
        lUInt8 * uncomp_buf = NULL;
        lUInt32 uncomp_size = 0;
        if ( ldomUnpack(buf, size, uncomp_buf, uncomp_size) && uncomp_size==block->_uncompressedSize ) {
            free( buf );
            buf = uncomp_buf;
            size = uncomp_size;
        } else {
            CRLog::error("CacheFile::read: error while uncompressing data for block %d:%d of size %d", type, dataIndex, (int)size);
            free(buf);
            buf = NULL;
            size = 0;
            return false;
        }
    }

    // check CRC
    lUInt64 hash = calcHash64( buf, size );
    if (hash != block->_dataHash) {
        CRLog::error("CacheFile::read: CRC doesn't match for block %d:%d of size %d", type, dataIndex, (int)size);
        free(buf);
        buf = NULL;
        size = 0;
        return false;
    }
    // Success. Don't forget to free allocated block externally
    return true;
}

// writes block to file
bool CacheFile::write( lUInt16 type, lUInt16 dataIndex, const lUInt8 * buf, int size, bool compress )
{
    // check whether data is changed
    lUInt64 newhash = calcHash64( buf, size );
    CacheFileItem * existingblock = findBlock( type, dataIndex );

    if (existingblock) {
        bool sameSize = ((int)existingblock->_uncompressedSize==size) || (existingblock->_uncompressedSize==0 && (int)existingblock->_dataSize==size);
        if (sameSize && existingblock->_dataHash == newhash ) {
            return true;
        }
    }

#if 1
    if (existingblock)
        CRLog::trace("*    oldsz=%d oldhash=%08x", (int)existingblock->_uncompressedSize, (int)existingblock->_dataHash);
    CRLog::trace("* wr block t=%d[%d] sz=%d hash=%08x", type, dataIndex, size, newhash);
#endif
    setDirtyFlag(true);

    lUInt32 uncompressedSize = 0;
    lUInt64 newpackedhash = newhash;
#if DOC_DATA_COMPRESSION_LEVEL==0
    compress = false;
#else
    if ( compress ) {
        lUInt8 * dstbuf = NULL;
        lUInt32 dstsize = 0;
        if ( !ldomPack( buf, size, dstbuf, dstsize ) ) {
            compress = false;
        } else {
            uncompressedSize = size;
            size = dstsize;
            buf = dstbuf;
            newpackedhash = calcHash64( buf, size );
#if DEBUG_DOM_STORAGE==1
            //CRLog::trace("packed block %d:%d : %d to %d bytes (%d%%)", type, dataIndex, srcsize, dstsize, srcsize>0?(100*dstsize/srcsize):0 );
#endif
        }
    }
#endif

    CacheFileItem * block = NULL;
    if ( existingblock && existingblock->_dataSize>=size ) {
        // reuse existing block
        block = existingblock;
    } else {
        // allocate new block
        if ( existingblock )
            freeBlock( existingblock );
        block = allocBlock( type, dataIndex, size );
    }
    if ( !block )
        return false;
    if ( (int)_stream->SetPos( block->_blockFilePos )!=block->_blockFilePos )
        return false;
    // assert: size == block->_dataSize
    // actual writing of data
    block->_dataSize = size;
    lvsize_t bytesWritten = 0;
    _stream->Write(buf, size, &bytesWritten );
    if ( (int)bytesWritten!=size )
        return false;
#if CACHE_FILE_WRITE_BLOCK_PADDING==1
    int paddingSize = block->_blockSize - size; //roundSector( size ) - size
    if ( paddingSize ) {
        if ((int)block->_blockFilePos + (int)block->_dataSize >= (int)_stream->GetSize() - _sectorSize) {
            LASSERT(size + paddingSize == block->_blockSize );
//            if (paddingSize > 16384) {
//                CRLog::error("paddingSize > 16384");
//            }
//            LASSERT(paddingSize <= 16384);
            lUInt8 tmp[16384];//paddingSize];
            memset(tmp, 0xFF, paddingSize < 16384 ? paddingSize : 16384);
            do {
                int blkSize = paddingSize < 16384 ? paddingSize : 16384;
                _stream->Write(tmp, blkSize, &bytesWritten );
                paddingSize -= blkSize;
            } while (paddingSize > 0);
        }
    }
#endif
    //_stream->Flush(true);
    // update CRC
    block->_dataHash = newhash;
    block->_packedHash = newpackedhash;
    block->_uncompressedSize = uncompressedSize;

#if DOC_DATA_COMPRESSION_LEVEL!=0
    if ( compress ) {
        free( (void*)buf );
    }
#endif
    _indexChanged = true;

    //CRLog::error("CacheFile::write: block %d:%d (pos %ds, size %ds) is written (crc=%08x)", type, dataIndex, (int)block->_blockFilePos/_sectorSize, (int)(size+_sectorSize-1)/_sectorSize, block->_dataCRC);
    // success
    return true;
}

/// writes content of serial buffer
bool CacheFile::write( lUInt16 type, lUInt16 index, SerialBuf & buf, bool compress )
{
    return write( type, index, buf.buf(), buf.pos(), compress );
}

/// reads content of serial buffer
bool CacheFile::read( lUInt16 type, lUInt16 index, SerialBuf & buf )
{
    lUInt8 * tmp = NULL;
    int size = 0;
    bool res = read( type, index, tmp, size );
    if ( res ) {
        buf.set( tmp, size );
    }
    buf.setPos(0);
    return res;
}

// try open existing cache file
bool CacheFile::open( lString16 filename )
{
    LVStreamRef stream = LVOpenFileStream( filename.c_str(), LVOM_APPEND );
    if ( !stream ) {
        CRLog::error( "CacheFile::open: cannot open file %s", LCSTR(filename));
        return false;
    }
    return open(stream);
}


// try open existing cache file
bool CacheFile::open( LVStreamRef stream )
{
    _stream = stream;
    _size = _stream->GetSize();
    //_stream->setAutoSyncSize(STREAM_AUTO_SYNC_SIZE);

    if ( !readIndex() ) {
        CRLog::error("CacheFile::open : cannot read index from file");
        return false;
    }
#if ENABLE_CACHE_FILE_CONTENTS_VALIDATION==1
    if ( !validateContents() ) {
        CRLog::error("CacheFile::open : file contents validation failed");
        return false;
    }
#endif
    return true;
}

bool CacheFile::create( lString16 filename )
{
    LVStreamRef stream = LVOpenFileStream( filename.c_str(), LVOM_APPEND );
    if ( _stream.isNull() ) {
        CRLog::error( "CacheFile::create: cannot create file %s", LCSTR(filename));
        return false;
    }
    return create(stream);
}

bool CacheFile::create( LVStreamRef stream )
{
    _stream = stream;
    //_stream->setAutoSyncSize(STREAM_AUTO_SYNC_SIZE);
    if ( _stream->SetPos(0)!=0 ) {
        CRLog::error( "CacheFile::create: cannot seek file");
        _stream.Clear();
        return false;
    }

    _size = _sectorSize;
    LVAutoPtr<lUInt8> sector0( new lUInt8[_sectorSize] );
    memset(sector0.get(), 0, _sectorSize);
    lvsize_t bytesWritten = 0;
    _stream->Write(sector0.get(), _sectorSize, &bytesWritten );
    if ( (int)bytesWritten!=_sectorSize ) {
        _stream.Clear();
        return false;
    }
    if (!updateHeader()) {
        _stream.Clear();
        return false;
    }
    return true;
}

// BLOB storage

class ldomBlobItem {
    int _storageIndex;
    lString16 _name;
    int _size;
    lUInt8 * _data;
public:
    ldomBlobItem( lString16 name ) : _storageIndex(-1), _name(name), _size(0), _data(NULL) {

    }
    ~ldomBlobItem() {
        if ( _data )
            delete[] _data;
    }
    int getSize() { return _size; }
    int getIndex() { return _storageIndex; }
    lUInt8 * getData() { return _data; }
    lString16 getName() { return _name; }
    void setIndex(int index, int size) {
        if ( _data )
            delete[] _data;
        _data = NULL;
        _storageIndex = index;
        _size = size;
    }
    void setData( const lUInt8 * data, int size ) {
        if ( _data )
            delete[] _data;
        if (data && size>0) {
            _data = new lUInt8[size];
            memcpy(_data, data, size);
            _size = size;
        } else {
            _data = NULL;
            _size = -1;
        }
    }
};

ldomBlobCache::ldomBlobCache() : _cacheFile(NULL), _changed(false)
{

}

#define BLOB_INDEX_MAGIC "BLOBINDX"

bool ldomBlobCache::loadIndex()
{
    bool res;
    SerialBuf buf(0,true);
    res = _cacheFile->read(CBT_BLOB_INDEX, buf);
    if (!res) {
        _list.clear();
        return true; // missing blob index: treat as empty list of blobs
    }
    if (!buf.checkMagic(BLOB_INDEX_MAGIC))
        return false;
    lUInt32 len;
    buf >> len;
    for ( lUInt32 i = 0; i<len; i++ ) {
        lString16 name;
        buf >> name;
        lUInt32 size;
        buf >> size;
        if (buf.error())
            break;
        ldomBlobItem * item = new ldomBlobItem(name);
        item->setIndex(i, size);
        _list.add(item);
    }
    res = !buf.error();
    return res;
}

bool ldomBlobCache::saveIndex()
{
    bool res;
    SerialBuf buf(0,true);
    buf.putMagic(BLOB_INDEX_MAGIC);
    lUInt32 len = _list.length();
    buf << len;
    for ( lUInt32 i = 0; i<len; i++ ) {
        ldomBlobItem * item = _list[i];
        buf << item->getName();
        buf << (lUInt32)item->getSize();
    }
    res = _cacheFile->write( CBT_BLOB_INDEX, buf, false );
    return res;
}

ContinuousOperationResult ldomBlobCache::saveToCache(CRTimerUtil & timeout)
{
    if (!_list.length() || !_changed || _cacheFile==NULL)
        return CR_DONE;
    bool res = true;
    for ( int i=0; i<_list.length(); i++ ) {
        ldomBlobItem * item = _list[i];
        if ( item->getData() ) {
            res = _cacheFile->write(CBT_BLOB_DATA, i, item->getData(), item->getSize(), false) && res;
            if (res)
                item->setIndex(i, item->getSize());
        }
        if (timeout.expired())
            return CR_TIMEOUT;
    }
    res = saveIndex() && res;
    if ( res )
        _changed = false;
    return res ? CR_DONE : CR_ERROR;
}

void ldomBlobCache::setCacheFile( CacheFile * cacheFile )
{
    _cacheFile = cacheFile;
    CRTimerUtil infinite;
    if (_list.empty())
        loadIndex();
    else
        saveToCache(infinite);
}

bool ldomBlobCache::addBlob( const lUInt8 * data, int size, lString16 name )
{
    CRLog::debug("ldomBlobCache::addBlob( %s, size=%d, [%02x,%02x,%02x,%02x] )", LCSTR(name), size, data[0], data[1], data[2], data[3]);
    int index = _list.length();
    ldomBlobItem * item = new ldomBlobItem(name);
    if (_cacheFile != NULL) {
        _cacheFile->write(CBT_BLOB_DATA, index, data, size, false);
        item->setIndex(index, size);
    } else {
        item->setData(data, size);
    }
    _list.add(item);
    _changed = true;
    return true;
}

LVStreamRef ldomBlobCache::getBlob( lString16 name )
{
    ldomBlobItem * item = NULL;
    lUInt16 index = 0;
    for ( int i=0; i<_list.length(); i++ ) {
        if (_list[i]->getName() == name) {
            item = _list[i];
            index = i;
            break;
        }
    }
    if (item) {
        if (item->getData()) {
            // RAM
            return LVCreateMemoryStream(item->getData(), item->getSize(), true);
        } else {
            // CACHE FILE
            return _cacheFile->readStream(CBT_BLOB_DATA, index);
        }
    }
    return LVStreamRef();
}

#if BUILD_LITE!=1
//#define DEBUG_RENDER_RECT_ACCESS
#ifdef DEBUG_RENDER_RECT_ACCESS
  static signed char render_rect_flags[200000]={0};
  static void rr_lock( ldomNode * node )
  {
    int index = node->getDataIndex()>>4;
    CRLog::debug("RenderRectAccessor(%d) lock", index );
    if ( render_rect_flags[index] )
        crFatalError(123, "render rect accessor: cannot get lock");
    render_rect_flags[index] = 1;
  }
  static void rr_unlock( ldomNode * node )
  {
    int index = node->getDataIndex()>>4;
    CRLog::debug("RenderRectAccessor(%d) lock", index );
    if ( !render_rect_flags[index] )
        crFatalError(123, "render rect accessor: unlock w/o lock");
    render_rect_flags[index] = 0;
  }
#endif

RenderRectAccessor::RenderRectAccessor( ldomNode * node )
: _node(node), _modified(false), _dirty(false)
{
#ifdef DEBUG_RENDER_RECT_ACCESS
    rr_lock( _node );
#endif
    _node->getRenderData(*this);
}

RenderRectAccessor::~RenderRectAccessor()
{
    if ( _modified )
        _node->setRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
    if ( !_dirty )
        rr_unlock( _node );
#endif
}

void RenderRectAccessor::push()
{
    if ( _modified ) {
        _node->setRenderData(*this);
        _modified = false;
        _dirty = true;
        #ifdef DEBUG_RENDER_RECT_ACCESS
            rr_unlock( _node );
        #endif
    }
}

void RenderRectAccessor::setX( int x )
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    if ( _x != x ) {
        _x = x;
        _modified = true;
    }
}
void RenderRectAccessor::setY( int y )
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    if ( _y != y ) {
        _y = y;
        _modified = true;
    }
}
void RenderRectAccessor::setWidth( int w )
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    if ( _width != w ) {
        _width = w;
        _modified = true;
    }
}
void RenderRectAccessor::setHeight( int h )
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    if ( _height != h ) {
        _height = h;
        _modified = true;
    }
}

int RenderRectAccessor::getX()
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    return _x;
}
int RenderRectAccessor::getY()
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    return _y;
}
int RenderRectAccessor::getWidth()
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    return _width;
}
int RenderRectAccessor::getHeight()
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    return _height;
}
void RenderRectAccessor::getRect( lvRect & rc )
{
    if ( _dirty ) {
        _dirty = false;
        _node->getRenderData(*this);
#ifdef DEBUG_RENDER_RECT_ACCESS
        rr_lock( _node );
#endif
    }
    rc.left = _x;
    rc.top = _y;
    rc.right = _x + _width;
    rc.bottom = _y + _height;
}
#endif


class ldomPersistentText;
class ldomPersistentElement;

/// common header for data storage items
struct DataStorageItemHeader {
    /// item type: LXML_TEXT_NODE, LXML_ELEMENT_NODE, LXML_NO_DATA
    lUInt16 type;
    /// size of item / 16
    lUInt16 sizeDiv16;
    /// data index of this node in document
    lInt32 dataIndex;
    /// data index of parent node in document, 0 means no parent
    lInt32 parentIndex;
};

/// text node storage implementation
struct TextDataStorageItem : public DataStorageItemHeader {
    /// utf8 text length, characters
    lUInt16 length;
    /// utf8 text, w/o zero
    lChar8 text[2]; // utf8 text follows here, w/o zero byte at end
    /// return text
    inline lString16 getText() { return Utf8ToUnicode( text, length ); }
    inline lString8 getText8() { return lString8( text, length ); }
};

/// element node data storage
struct ElementDataStorageItem : public DataStorageItemHeader {
    lUInt16 id;
    lUInt16 nsid;
    lInt16  attrCount;
    lUInt8  rendMethod;
    lUInt8  reserved8;
    lInt32  childCount;
    lInt32  children[1];
    lUInt16 * attrs() { return (lUInt16 *)(children + childCount); }
    lxmlAttribute * attr( int index ) { return (lxmlAttribute *)&(((lUInt16 *)(children + childCount))[index*3]); }
    lUInt16 getAttrValueId( lUInt16 ns, lUInt16 id )
    {
        lUInt16 * a = attrs();
        for ( int i=0; i<attrCount; i++ ) {
            lxmlAttribute * attr = (lxmlAttribute *)(&a[i*3]);
            if ( !attr->compare( ns, id ) )
                continue;
            return  attr->index;
        }
        return LXML_ATTR_VALUE_NONE;
    }
    lxmlAttribute * findAttr( lUInt16 ns, lUInt16 id )
    {
        lUInt16 * a = attrs();
        for ( int i=0; i<attrCount; i++ ) {
            lxmlAttribute * attr = (lxmlAttribute *)(&a[i*3]);
            if ( attr->compare( ns, id ) )
                return attr;
        }
        return NULL;
    }
    // TODO: add items here
    //css_style_ref_t _style;
    //font_ref_t      _font;
};

#endif


//=================================================================
// tinyNodeCollection implementation
//=================================================================

tinyNodeCollection::tinyNodeCollection()
: _textCount(0)
, _textNextFree(0)
, _elemCount(0)
, _elemNextFree(0)
, _styles(STYLE_HASH_TABLE_SIZE)
, _fonts(FONT_HASH_TABLE_SIZE)
, _tinyElementCount(0)
, _itemCount(0)
#if BUILD_LITE!=1
, _renderedBlockCache( 32 )
, _cacheFile(NULL)
, _mapped(false)
, _maperror(false)
, _mapSavingStage(0)
, _minSpaceCondensingPercent(DEF_MIN_SPACE_CONDENSING_PERCENT)
#endif
, _textStorage(this, 't', TEXT_CACHE_UNPACKED_SPACE, TEXT_CACHE_CHUNK_SIZE ) // persistent text node data storage
, _elemStorage(this, 'e', ELEM_CACHE_UNPACKED_SPACE, ELEM_CACHE_CHUNK_SIZE ) // persistent element data storage
, _rectStorage(this, 'r', RECT_CACHE_UNPACKED_SPACE, RECT_CACHE_CHUNK_SIZE ) // element render rect storage
, _styleStorage(this, 's', STYLE_CACHE_UNPACKED_SPACE, STYLE_CACHE_CHUNK_SIZE ) // element style info storage
,_docProps(LVCreatePropsContainer())
,_docFlags(DOC_FLAG_DEFAULTS)
,_fontMap(113)
{
    memset( _textList, 0, sizeof(_textList) );
    memset( _elemList, 0, sizeof(_elemList) );
    _docIndex = ldomNode::registerDocument((ldomDocument*)this);
}

tinyNodeCollection::tinyNodeCollection( tinyNodeCollection & v )
: _textCount(0)
, _textNextFree(0)
, _elemCount(0)
, _elemNextFree(0)
, _styles(STYLE_HASH_TABLE_SIZE)
, _fonts(FONT_HASH_TABLE_SIZE)
, _tinyElementCount(0)
, _itemCount(0)
#if BUILD_LITE!=1
, _renderedBlockCache( 32 )
, _cacheFile(NULL)
, _mapped(false)
, _maperror(false)
, _mapSavingStage(0)
, _minSpaceCondensingPercent(DEF_MIN_SPACE_CONDENSING_PERCENT)
#endif
, _textStorage(this, 't', TEXT_CACHE_UNPACKED_SPACE, TEXT_CACHE_CHUNK_SIZE ) // persistent text node data storage
, _elemStorage(this, 'e', ELEM_CACHE_UNPACKED_SPACE, ELEM_CACHE_CHUNK_SIZE ) // persistent element data storage
, _rectStorage(this, 'r', RECT_CACHE_UNPACKED_SPACE, RECT_CACHE_CHUNK_SIZE ) // element render rect storage
, _styleStorage(this, 's', STYLE_CACHE_UNPACKED_SPACE, STYLE_CACHE_CHUNK_SIZE ) // element style info storage
,_docProps(LVCreatePropsContainer())
,_docFlags(v._docFlags)
,_stylesheet(v._stylesheet)
,_fontMap(113)
{
    _docIndex = ldomNode::registerDocument((ldomDocument*)this);
}



#if BUILD_LITE!=1
bool tinyNodeCollection::openCacheFile()
{
    if ( _cacheFile )
        return true;
    CacheFile * f = new CacheFile();
    //lString16 cacheFileName("/tmp/cr3swap.tmp");

    lString16 fname = getProps()->getStringDef( DOC_PROP_FILE_NAME, "noname" );
    //lUInt32 sz = (lUInt32)getProps()->getInt64Def(DOC_PROP_FILE_SIZE, 0);
    lUInt32 crc = getProps()->getIntDef(DOC_PROP_FILE_CRC32, 0);

    if ( !ldomDocCache::enabled() ) {
        CRLog::error("Cannot open cached document: cache dir is not initialized");
        return false;
    }

    CRLog::info("ldomDocument::openCacheFile() - looking for cache file", UnicodeToUtf8(fname).c_str() );

    LVStreamRef map = ldomDocCache::openExisting( fname, crc, getPersistenceFlags() );
    if ( map.isNull() ) {
        delete f;
        return false;
    }
    CRLog::info("ldomDocument::openCacheFile() - cache file found, trying to read index", UnicodeToUtf8(fname).c_str() );

    if ( !f->open( map ) ) {
        delete f;
        return false;
    }
    CRLog::info("ldomDocument::openCacheFile() - index read successfully", UnicodeToUtf8(fname).c_str() );
    _cacheFile = f;
    _textStorage.setCache( f );
    _elemStorage.setCache( f );
    _rectStorage.setCache( f );
    _styleStorage.setCache( f );
    _blobCache.setCacheFile( f );
    return true;
}

bool tinyNodeCollection::swapToCacheIfNecessary()
{
    if ( !_cacheFile || _mapped || _maperror)
        return false;
    return createCacheFile();
    //return swapToCache();
}

bool tinyNodeCollection::createCacheFile()
{
    if ( _cacheFile )
        return true;
    CacheFile * f = new CacheFile();
    //lString16 cacheFileName("/tmp/cr3swap.tmp");

    lString16 fname = getProps()->getStringDef( DOC_PROP_FILE_NAME, "noname" );
    lUInt32 sz = (lUInt32)getProps()->getInt64Def(DOC_PROP_FILE_SIZE, 0);
    lUInt32 crc = getProps()->getIntDef(DOC_PROP_FILE_CRC32, 0);

    if ( !ldomDocCache::enabled() ) {
        CRLog::error("Cannot swap: cache dir is not initialized");
        return false;
    }

    CRLog::info("ldomDocument::createCacheFile() - initialized swapping of document %s to cache file", UnicodeToUtf8(fname).c_str() );

    LVStreamRef map = ldomDocCache::createNew( fname, crc, getPersistenceFlags(), sz );
    if ( map.isNull() ) {
        delete f;
        return false;
    }

    if ( !f->create( map ) ) {
        delete f;
        return false;
    }
    _cacheFile = f;
    _mapped = true;
    _textStorage.setCache( f );
    _elemStorage.setCache( f );
    _rectStorage.setCache( f );
    _styleStorage.setCache( f );
    _blobCache.setCacheFile( f );
    return true;
}

void tinyNodeCollection::clearNodeStyle( lUInt32 dataIndex )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    _styles.release( info._styleIndex );
    _fonts.release( info._fontIndex );
    info._fontIndex = info._styleIndex = 0;
    _styleStorage.setStyleData( dataIndex, &info );
}

void tinyNodeCollection::setNodeStyleIndex( lUInt32 dataIndex, lUInt16 index )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    if ( info._styleIndex!=index ) {
        info._styleIndex = index;
        _styleStorage.setStyleData( dataIndex, &info );
    }
}

void tinyNodeCollection::setNodeFontIndex( lUInt32 dataIndex, lUInt16 index )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    if ( info._fontIndex!=index ) {
        info._fontIndex = index;
        _styleStorage.setStyleData( dataIndex, &info );
    }
}

lUInt16 tinyNodeCollection::getNodeStyleIndex( lUInt32 dataIndex )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    return info._styleIndex;
}

css_style_ref_t tinyNodeCollection::getNodeStyle( lUInt32 dataIndex )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    css_style_ref_t res =  _styles.get( info._styleIndex );
#if DEBUG_DOM_STORAGE==1
    if ( res.isNull() && info._styleIndex!=0 ) {
        CRLog::error("Null style returned for index %d", (int)info._styleIndex);
    }
#endif
    return res;
}

font_ref_t tinyNodeCollection::getNodeFont( lUInt32 dataIndex )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    return _fonts.get( info._fontIndex );
}

void tinyNodeCollection::setNodeStyle( lUInt32 dataIndex, css_style_ref_t & v )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    _styles.cache( info._styleIndex, v );
#if DEBUG_DOM_STORAGE==1
    if ( info._styleIndex==0 ) {
        CRLog::error("tinyNodeCollection::setNodeStyle() styleIndex is 0 after caching");
    }
#endif
    _styleStorage.setStyleData( dataIndex, &info );
}

void tinyNodeCollection::setNodeFont( lUInt32 dataIndex, font_ref_t & v )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    _fonts.cache( info._fontIndex, v );
    _styleStorage.setStyleData( dataIndex, &info );
}

lUInt16 tinyNodeCollection::getNodeFontIndex( lUInt32 dataIndex )
{
    ldomNodeStyleInfo info;
    _styleStorage.getStyleData( dataIndex, &info );
    return info._fontIndex;
}

bool tinyNodeCollection::loadNodeData( lUInt16 type, ldomNode ** list, int nodecount )
{
    int count = ((nodecount+TNC_PART_LEN-1) >> TNC_PART_SHIFT);
    for ( int i=0; i<count; i++ ) {
        int offs = i*TNC_PART_LEN;
        int sz = TNC_PART_LEN;
        if ( offs + sz > nodecount ) {
            sz = nodecount - offs;
        }

        lUInt8 * p;
        int buflen;
        if ( !_cacheFile->read( type, i, p, buflen ) )
            return false;
        ldomNode * buf = (ldomNode *)p;
        if ( !buf || (int)buflen != sizeof(ldomNode)*sz )
            return false;
        list[i] = buf;
        for ( int j=0; j<sz; j++ ) {
            buf[j].setDocumentIndex( _docIndex );
            if ( buf[j].isElement() ) {
                // will be set by loadStyles/updateStyles
                //buf[j]._data._pelem._styleIndex = 0;
                setNodeFontIndex( buf[j]._handle._dataIndex, 0 );
                //buf[j]._data._pelem._fontIndex = 0;
            }
        }
    }
    return true;
}

bool tinyNodeCollection::saveNodeData( lUInt16 type, ldomNode ** list, int nodecount )
{
    int count = ((nodecount+TNC_PART_LEN-1) >> TNC_PART_SHIFT);
    for ( int i=0; i<count; i++ ) {
        if ( !list[i] )
            continue;
        int offs = i*TNC_PART_LEN;
        int sz = TNC_PART_LEN;
        if ( offs + sz > nodecount ) {
            sz = nodecount - offs;
        }
        ldomNode buf[TNC_PART_LEN];
        memcpy( buf, list[i], sizeof(ldomNode)*sz );
        for ( int j=0; j<sz; j++ )
            buf[j].setDocumentIndex( _docIndex );
        if ( !_cacheFile->write( type, i, (lUInt8*)buf, sizeof(ldomNode)*sz, COMPRESS_NODE_DATA ) )
            crFatalError(-1, "Cannot write node data");
    }
    return true;
}

#define NODE_INDEX_MAGIC 0x19283746
bool tinyNodeCollection::saveNodeData()
{
    SerialBuf buf(12, true);
    buf << (lUInt32)NODE_INDEX_MAGIC << (lUInt32)_elemCount << (lUInt32)_textCount;
    if ( !saveNodeData( CBT_ELEM_NODE, _elemList, _elemCount+1 ) )
        return false;
    if ( !saveNodeData( CBT_TEXT_NODE, _textList, _textCount+1 ) )
        return false;
    if ( !_cacheFile->write(CBT_NODE_INDEX, buf, COMPRESS_NODE_DATA) )
        return false;
    return true;
}

bool tinyNodeCollection::loadNodeData()
{
    SerialBuf buf(0, true);
    if ( !_cacheFile->read((lUInt16)CBT_NODE_INDEX, buf) )
        return false;
    lUInt32 magic;
    lInt32 elemcount;
    lInt32 textcount;
    buf >> magic >> elemcount >> textcount;
    if ( magic != NODE_INDEX_MAGIC ) {
        return false;
    }
    if ( elemcount<=0 || elemcount>200000 )
        return false;
    if ( textcount<=0 || textcount>200000 )
        return false;
    ldomNode * elemList[TNC_PART_COUNT];
    memset( elemList, 0, sizeof(elemList) );
    ldomNode * textList[TNC_PART_COUNT];
    memset( textList, 0, sizeof(textList) );
    if ( !loadNodeData( CBT_ELEM_NODE, elemList, elemcount+1 ) ) {
        for ( int i=0; i<TNC_PART_COUNT; i++ )
            if ( elemList[i] )
                free( elemList[i] );
        return false;
    }
    if ( !loadNodeData( CBT_TEXT_NODE, textList, textcount+1 ) ) {
        for ( int i=0; i<TNC_PART_COUNT; i++ )
            if ( textList[i] )
                free( textList[i] );
        return false;
    }
    for ( int i=0; i<TNC_PART_COUNT; i++ ) {
        if ( _elemList[i] )
            free( _elemList[i] );
        if ( _textList[i] )
            free( _textList[i] );
    }
    memcpy( _elemList, elemList, sizeof(elemList) );
    memcpy( _textList, textList, sizeof(textList) );
    _elemCount = elemcount;
    _textCount = textcount;
    return true;
}
#endif

/// get ldomNode instance pointer
ldomNode * tinyNodeCollection::getTinyNode( lUInt32 index )
{
    if ( !index )
        return NULL;
    if ( index & 1 ) // element
        return &(_elemList[index>>TNC_PART_INDEX_SHIFT][(index>>4)&TNC_PART_MASK]);
    else // text
        return &(_textList[index>>TNC_PART_INDEX_SHIFT][(index>>4)&TNC_PART_MASK]);
}

/// allocate new tiny node
ldomNode * tinyNodeCollection::allocTinyNode( int type )
{
    ldomNode * res;
    if ( type & 1 ) {
        // allocate Element
        if ( _elemNextFree ) {
            // reuse existing free item
            int index = (_elemNextFree << 4) | type;
            res = getTinyNode(index);
            res->_handle._dataIndex = index;
            _elemNextFree = res->_data._nextFreeIndex;
        } else {
            // create new item
            _elemCount++;
            ldomNode * part = _elemList[_elemCount >> TNC_PART_SHIFT];
            if ( !part ) {
                part = (ldomNode*)malloc( sizeof(ldomNode) * TNC_PART_LEN );
                memset( part, 0, sizeof(ldomNode) * TNC_PART_LEN );
                _elemList[ _elemCount >> TNC_PART_SHIFT ] = part;
            }
            res = &part[_elemCount & TNC_PART_MASK];
            res->setDocumentIndex( _docIndex );
            res->_handle._dataIndex = (_elemCount << 4) | type;
        }
        _itemCount++;
    } else {
        // allocate Text
        if ( _textNextFree ) {
            // reuse existing free item
            int index = (_textNextFree << 4) | type;
            res = getTinyNode(index);
            res->_handle._dataIndex = index;
            _textNextFree = res->_data._nextFreeIndex;
        } else {
            // create new item
            _textCount++;
            ldomNode * part = _textList[_textCount >> TNC_PART_SHIFT];
            if ( !part ) {
                part = (ldomNode*)malloc( sizeof(ldomNode) * TNC_PART_LEN );
                memset( part, 0, sizeof(ldomNode) * TNC_PART_LEN );
                _textList[ _textCount >> TNC_PART_SHIFT ] = part;
            }
            res = &part[_textCount & TNC_PART_MASK];
            res->setDocumentIndex( _docIndex );
            res->_handle._dataIndex = (_textCount << 4) | type;
        }
        _itemCount++;
    }
    return res;
}

void tinyNodeCollection::recycleTinyNode( lUInt32 index )
{
    if ( index & 1 ) {
        // element
        index >>= 4;
        ldomNode * part = _elemList[index >> TNC_PART_SHIFT];
        ldomNode * p = &part[index & TNC_PART_MASK];
        p->_handle._dataIndex = 0; // indicates NULL node
        p->_data._nextFreeIndex = _elemNextFree;
        _elemNextFree = index;
        _itemCount--;
    } else {
        // text
        index >>= 4;
        ldomNode * part = _textList[index >> TNC_PART_SHIFT];
        ldomNode * p = &part[index & TNC_PART_MASK];
        p->_handle._dataIndex = 0; // indicates NULL node
        p->_data._nextFreeIndex = _textNextFree;
        _textNextFree = index;
        _itemCount--;
    }
}

tinyNodeCollection::~tinyNodeCollection()
{
#if BUILD_LITE!=1
    if ( _cacheFile )
        delete _cacheFile;
#endif
    // clear all elem parts
    for ( int partindex = 0; partindex<=(_elemCount>>TNC_PART_SHIFT); partindex++ ) {
        ldomNode * part = _elemList[partindex];
        if ( part ) {
            int n0 = TNC_PART_LEN * partindex;
            for ( int i=0; i<TNC_PART_LEN && n0+i<=_elemCount; i++ )
                part[i].onCollectionDestroy();
            free(part);
            _elemList[partindex] = NULL;
        }
    }
    // clear all text parts
    for ( int partindex = 0; partindex<=(_textCount>>TNC_PART_SHIFT); partindex++ ) {
        ldomNode * part = _textList[partindex];
        if ( part ) {
            int n0 = TNC_PART_LEN * partindex;
            for ( int i=0; i<TNC_PART_LEN && n0+i<=_textCount; i++ )
                part[i].onCollectionDestroy();
            free(part);
            _textList[partindex] = NULL;
        }
    }
    ldomNode::unregisterDocument((ldomDocument*)this);
}

#if BUILD_LITE!=1
/// put all objects into persistent storage
void tinyNodeCollection::persist( CRTimerUtil & maxTime )
{
    CRLog::info("lxmlDocBase::persist() invoked - converting all nodes to persistent objects");
    // elements
    for ( int partindex = 0; partindex<=(_elemCount>>TNC_PART_SHIFT); partindex++ ) {
        ldomNode * part = _elemList[partindex];
        if ( part ) {
            int n0 = TNC_PART_LEN * partindex;
            for ( int i=0; i<TNC_PART_LEN && n0+i<=_elemCount; i++ )
                if ( !part[i].isNull() && !part[i].isPersistent() ) {
                    part[i].persist();
                    if (maxTime.expired())
                        return;
                }
        }
    }
    //_cacheFile->flush(false); // intermediate flush
    if ( maxTime.expired() )
        return;
    // texts
    for ( int partindex = 0; partindex<=(_textCount>>TNC_PART_SHIFT); partindex++ ) {
        ldomNode * part = _textList[partindex];
        if ( part ) {
            int n0 = TNC_PART_LEN * partindex;
            for ( int i=0; i<TNC_PART_LEN && n0+i<=_textCount; i++ )
                if ( !part[i].isNull() && !part[i].isPersistent() ) {
                    //CRLog::trace("before persist");
                    part[i].persist();
                    //CRLog::trace("after persist");
                    if (maxTime.expired())
                        return;
                }
        }
    }
    //_cacheFile->flush(false); // intermediate flush
}
#endif


/*

  Struct Node
  { document, nodeid&type, address }

  Data Offset format

  Chunk index, offset, type.

  getDataPtr( lUInt32 address )
  {
     return (address & TYPE_MASK) ? textStorage.get( address & ~TYPE_MASK ) : elementStorage.get( address & ~TYPE_MASK );
  }

  index->instance, data
  >
  [index] { vtable, doc, id, dataptr } // 16 bytes per node


 */


/// saves all unsaved chunks to cache file
bool ldomDataStorageManager::save( CRTimerUtil & maxTime )
{
    bool res = true;
#if BUILD_LITE!=1
    if ( !_cache )
        return true;
    for ( int i=0; i<_chunks.length(); i++ ) {
        if ( !_chunks[i]->save() ) {
            res = false;
            break;
        }
        //CRLog::trace("time elapsed: %d", (int)maxTime.elapsed());
        if (maxTime.expired())
            return res;
//        if ( (i&3)==3 &&  maxTime.expired() )
//            return res;
    }
    if (!maxTime.infinite())
        _cache->flush(false, maxTime); // intermediate flush
    if ( maxTime.expired() )
        return res;
    if ( !res )
        return false;
    // save chunk index
    int n = _chunks.length();
    SerialBuf buf(n*4+4, true);
    buf << (lUInt32)n;
    for ( int i=0; i<n; i++ ) {
        buf << (lUInt32)_chunks[i]->_bufpos;
    }
    res = _cache->write( cacheType(), 0xFFFF, buf, COMPRESS_NODE_STORAGE_DATA );
    if ( !res ) {
        CRLog::error("ldomDataStorageManager::save() - Cannot write chunk index");
    }
#endif
    return res;
}

/// load chunk index from cache file
bool ldomDataStorageManager::load()
{
#if BUILD_LITE!=1
    if ( !_cache )
        return false;
    //load chunk index
    SerialBuf buf(0, true);
    if ( !_cache->read( cacheType(), 0xFFFF, buf ) ) {
        CRLog::error("ldomDataStorageManager::load() - Cannot read chunk index");
        return false;
    }
    lUInt32 n;
    buf >> n;
    if ( n<0 || n > 10000 )
        return false; // invalid
    _recentChunk = NULL;
    _chunks.clear();
    lUInt32 compsize = 0;
    lUInt32 uncompsize = 0;
    for ( unsigned i=0; i<n; i++ ) {
        buf >> uncompsize;
        if ( buf.error() ) {
            _chunks.clear();
            return false;
        }
        _chunks.add( new ldomTextStorageChunk( this, i,compsize, uncompsize ) );
    }
    return true;
#else
    return false;
#endif
}

/// get chunk pointer and update usage data
ldomTextStorageChunk * ldomDataStorageManager::getChunk( lUInt32 address )
{
    ldomTextStorageChunk * chunk = _chunks[address>>16];
    if ( chunk!=_recentChunk ) {
        if ( chunk->_prevRecent )
            chunk->_prevRecent->_nextRecent = chunk->_nextRecent;
        if ( chunk->_nextRecent )
            chunk->_nextRecent->_prevRecent = chunk->_prevRecent;
        chunk->_prevRecent = NULL;
        if ( (chunk->_nextRecent = _recentChunk) )
            _recentChunk->_prevRecent = chunk;
        _recentChunk = chunk;
    }
    chunk->ensureUnpacked();
    return chunk;
}

void ldomDataStorageManager::setCache( CacheFile * cache )
{
    _cache = cache;
}

/// type
lUInt16 ldomDataStorageManager::cacheType()
{
    switch ( _type ) {
    case 't':
        return CBT_TEXT_DATA;
    case 'e':
        return CBT_ELEM_DATA;
    case 'r':
        return CBT_RECT_DATA;
    case 's':
        return CBT_ELEM_STYLE_DATA;
    }
    return 0;
}

/// get or allocate space for element style data item
void ldomDataStorageManager::getStyleData( lUInt32 elemDataIndex, ldomNodeStyleInfo * dst )
{
    // assume storage has raw data chunks
    int index = elemDataIndex>>4; // element sequential index
    int chunkIndex = index >> STYLE_DATA_CHUNK_ITEMS_SHIFT;
    while ( _chunks.length() <= chunkIndex ) {
        //if ( _chunks.length()>0 )
        //    _chunks[_chunks.length()-1]->compact();
        _chunks.add( new ldomTextStorageChunk(STYLE_DATA_CHUNK_SIZE, this, _chunks.length()) );
        getChunk( (_chunks.length()-1)<<16 );
        compact( 0 );
    }
    ldomTextStorageChunk * chunk = getChunk( chunkIndex<<16 );
    int offsetIndex = index & STYLE_DATA_CHUNK_MASK;
    chunk->getRaw( offsetIndex * sizeof(ldomNodeStyleInfo), sizeof(ldomNodeStyleInfo), (lUInt8 *)dst );
}

/// set element style data item
void ldomDataStorageManager::setStyleData( lUInt32 elemDataIndex, const ldomNodeStyleInfo * src )
{
    // assume storage has raw data chunks
    int index = elemDataIndex>>4; // element sequential index
    int chunkIndex = index >> STYLE_DATA_CHUNK_ITEMS_SHIFT;
    while ( _chunks.length() < chunkIndex ) {
        //if ( _chunks.length()>0 )
        //    _chunks[_chunks.length()-1]->compact();
        _chunks.add( new ldomTextStorageChunk(STYLE_DATA_CHUNK_SIZE, this, _chunks.length()) );
        getChunk( (_chunks.length()-1)<<16 );
        compact( 0 );
    }
    ldomTextStorageChunk * chunk = getChunk( chunkIndex<<16 );
    int offsetIndex = index & STYLE_DATA_CHUNK_MASK;
    chunk->setRaw( offsetIndex * sizeof(ldomNodeStyleInfo), sizeof(ldomNodeStyleInfo), (const lUInt8 *)src );
}


/// get or allocate space for rect data item
void ldomDataStorageManager::getRendRectData( lUInt32 elemDataIndex, lvdomElementFormatRec * dst )
{
    // assume storage has raw data chunks
    int index = elemDataIndex>>4; // element sequential index
    int chunkIndex = index >> RECT_DATA_CHUNK_ITEMS_SHIFT;
    while ( _chunks.length() <= chunkIndex ) {
        //if ( _chunks.length()>0 )
        //    _chunks[_chunks.length()-1]->compact();
        _chunks.add( new ldomTextStorageChunk(RECT_DATA_CHUNK_SIZE, this, _chunks.length()) );
        getChunk( (_chunks.length()-1)<<16 );
        compact( 0 );
    }
    ldomTextStorageChunk * chunk = getChunk( chunkIndex<<16 );
    int offsetIndex = index & RECT_DATA_CHUNK_MASK;
    chunk->getRaw( offsetIndex * sizeof(lvdomElementFormatRec), sizeof(lvdomElementFormatRec), (lUInt8 *)dst );
}

/// set rect data item
void ldomDataStorageManager::setRendRectData( lUInt32 elemDataIndex, const lvdomElementFormatRec * src )
{
    // assume storage has raw data chunks
    int index = elemDataIndex>>4; // element sequential index
    int chunkIndex = index >> RECT_DATA_CHUNK_ITEMS_SHIFT;
    while ( _chunks.length() < chunkIndex ) {
        //if ( _chunks.length()>0 )
        //    _chunks[_chunks.length()-1]->compact();
        _chunks.add( new ldomTextStorageChunk(RECT_DATA_CHUNK_SIZE, this, _chunks.length()) );
        getChunk( (_chunks.length()-1)<<16 );
        compact( 0 );
    }
    ldomTextStorageChunk * chunk = getChunk( chunkIndex<<16 );
    int offsetIndex = index & RECT_DATA_CHUNK_MASK;
    chunk->setRaw( offsetIndex * sizeof(lvdomElementFormatRec), sizeof(lvdomElementFormatRec), (const lUInt8 *)src );
}

#if BUILD_LITE!=1
lUInt32 ldomDataStorageManager::allocText( lUInt32 dataIndex, lUInt32 parentIndex, const lString8 & text )
{
    if ( !_activeChunk ) {
        _activeChunk = new ldomTextStorageChunk(this, _chunks.length());
        _chunks.add( _activeChunk );
        getChunk( (_chunks.length()-1)<<16 );
        compact( 0 );
    }
    int offset = _activeChunk->addText( dataIndex, parentIndex, text );
    if ( offset<0 ) {
        // no space in current chunk, add one more chunk
        //_activeChunk->compact();
        _activeChunk = new ldomTextStorageChunk(this, _chunks.length());
        _chunks.add( _activeChunk );
        getChunk( (_chunks.length()-1)<<16 );
        compact( 0 );
        offset = _activeChunk->addText( dataIndex, parentIndex, text );
        if ( offset<0 )
            crFatalError(1001, "Unexpected error while allocation of text");
    }
    return offset | (_activeChunk->getIndex()<<16);
}

lUInt32 ldomDataStorageManager::allocElem( lUInt32 dataIndex, lUInt32 parentIndex, int childCount, int attrCount )
{
    if ( !_activeChunk ) {
        _activeChunk = new ldomTextStorageChunk(this, _chunks.length());
        _chunks.add( _activeChunk );
        getChunk( (_chunks.length()-1)<<16 );
        compact( 0 );
    }
    int offset = _activeChunk->addElem( dataIndex, parentIndex, childCount, attrCount );
    if ( offset<0 ) {
        // no space in current chunk, add one more chunk
        //_activeChunk->compact();
        _activeChunk = new ldomTextStorageChunk(this, _chunks.length());
        _chunks.add( _activeChunk );
        getChunk( (_chunks.length()-1)<<16 );
        compact( 0 );
        offset = _activeChunk->addElem( dataIndex, parentIndex, childCount, attrCount );
        if ( offset<0 )
            crFatalError(1002, "Unexpected error while allocation of element");
    }
    return offset | (_activeChunk->getIndex()<<16);
}

/// call to invalidate chunk if content is modified
void ldomDataStorageManager::modified( lUInt32 addr )
{
    ldomTextStorageChunk * chunk = getChunk(addr);
    chunk->modified();
}

/// change node's parent
bool ldomDataStorageManager::setParent( lUInt32 address, lUInt32 parent )
{
    ldomTextStorageChunk * chunk = getChunk(address);
    return chunk->setParent(address&0xFFFF, parent);
}

/// free data item
void ldomDataStorageManager::freeNode( lUInt32 addr )
{
    ldomTextStorageChunk * chunk = getChunk(addr);
    chunk->freeNode(addr&0xFFFF);
}


lString8 ldomDataStorageManager::getText( lUInt32 address )
{
    ldomTextStorageChunk * chunk = getChunk(address);
    return chunk->getText(address&0xFFFF);
}

/// get pointer to element data
ElementDataStorageItem * ldomDataStorageManager::getElem( lUInt32 addr )
{
    ldomTextStorageChunk * chunk = getChunk(addr);
    return chunk->getElem(addr&0xFFFF);
}

/// returns node's parent by address
lUInt32 ldomDataStorageManager::getParent( lUInt32 addr )
{
    ldomTextStorageChunk * chunk = getChunk(addr);
    return chunk->getElem(addr&0xFFFF)->parentIndex;
}
#endif

void ldomDataStorageManager::compact( int reservedSpace )
{
#if BUILD_LITE!=1
    if ( _uncompressedSize + reservedSpace > _maxUncompressedSize + _maxUncompressedSize/10 ) { // allow +10% overflow
        // do compacting
        int sumsize = reservedSpace;
        for ( ldomTextStorageChunk * p = _recentChunk; p; p = p->_nextRecent ) {
            if ( p->_bufsize >= 0 ) {
                if ( (int)p->_bufsize + sumsize < _maxUncompressedSize || (p==_activeChunk && reservedSpace<0xFFFFFFF)) {
                    // fits
                    sumsize += p->_bufsize;
                } else {
                    if ( !_cache )
                        _owner->createCacheFile();
                    if ( _cache ) {
                        if ( !p->swapToCache(true) ) {
                            crFatalError(111, "Swap file writing error!");
                        }
                    }
                }
            }

        }

    }
#endif
}

// max 512K of uncompressed data (~8 chunks)
#define DEF_MAX_UNCOMPRESSED_SIZE 0x80000
ldomDataStorageManager::ldomDataStorageManager( tinyNodeCollection * owner, char type, int maxUnpackedSize, int chunkSize )
: _owner( owner )
, _activeChunk(NULL)
, _recentChunk(NULL)
, _cache(NULL)
, _uncompressedSize(0)
, _maxUncompressedSize(maxUnpackedSize)
, _chunkSize(chunkSize)
, _type(type)
{
}

ldomDataStorageManager::~ldomDataStorageManager()
{
}

/// create chunk to be read from cache file
ldomTextStorageChunk::ldomTextStorageChunk( ldomDataStorageManager * manager, int index, int compsize, int uncompsize )
: _manager(manager)
, _buf(NULL)   /// buffer for uncompressed data
, _bufsize(0)    /// _buf (uncompressed) area size, bytes
, _bufpos(uncompsize)     /// _buf (uncompressed) data write position (for appending of new data)
, _index(index)      /// ? index of chunk in storage
, _type( manager->_type )
, _nextRecent(NULL)
, _prevRecent(NULL)
, _saved(true)
{
}

ldomTextStorageChunk::ldomTextStorageChunk( int preAllocSize, ldomDataStorageManager * manager, int index )
: _manager(manager)
, _buf(NULL)   /// buffer for uncompressed data
, _bufsize(preAllocSize)    /// _buf (uncompressed) area size, bytes
, _bufpos(preAllocSize)     /// _buf (uncompressed) data write position (for appending of new data)
, _index(index)      /// ? index of chunk in storage
, _type( manager->_type )
, _nextRecent(NULL)
, _prevRecent(NULL)
, _saved(false)
{
    _buf = (lUInt8*)malloc(preAllocSize);
    memset(_buf, 0, preAllocSize);
    _manager->_uncompressedSize += _bufsize;
}

ldomTextStorageChunk::ldomTextStorageChunk( ldomDataStorageManager * manager, int index )
: _manager(manager)
, _buf(NULL)   /// buffer for uncompressed data
, _bufsize(0)    /// _buf (uncompressed) area size, bytes
, _bufpos(0)     /// _buf (uncompressed) data write position (for appending of new data)
, _index(index)      /// ? index of chunk in storage
, _type( manager->_type )
, _nextRecent(NULL)
, _prevRecent(NULL)
, _saved(false)
{
}

#if BUILD_LITE!=1
/// saves data to cache file, if unsaved
bool ldomTextStorageChunk::save()
{
    if ( !_saved )
        return swapToCache(false);
    return true;
}
#endif

ldomTextStorageChunk::~ldomTextStorageChunk()
{
    setunpacked(NULL, 0);
}


#if BUILD_LITE!=1
/// pack data, and remove unpacked, put packed data to cache file
bool ldomTextStorageChunk::swapToCache( bool removeFromMemory )
{
    if ( !_manager->_cache )
        return true;
    if ( _buf ) {
        if ( !_saved && _manager->_cache) {
#if DEBUG_DOM_STORAGE==1
            CRLog::debug("Writing %d bytes of chunk %c%d to cache", _bufpos, _type, _index);
#endif
            if ( !_manager->_cache->write( _manager->cacheType(), _index, _buf, _bufpos, COMPRESS_NODE_STORAGE_DATA) ) {
                CRLog::error("Error while swapping of chunk %c%d to cache file", _type, _index);
                crFatalError(-1, "Error while swapping of chunk to cache file");
                return false;
            }
            _saved = true;
        }
    }
    if ( removeFromMemory ) {
        setunpacked(NULL, 0);
    }
    return true;
}

/// read packed data from cache
bool ldomTextStorageChunk::restoreFromCache()
{
    if ( _buf )
        return true;
    if ( !_saved )
        return false;
    int size;
    if ( !_manager->_cache->read( _manager->cacheType(), _index, _buf, size ) )
        return false;
    _bufsize = size;
    _manager->_uncompressedSize += _bufsize;
#if DEBUG_DOM_STORAGE==1
    CRLog::debug("Read %d bytes of chunk %c%d from cache", _bufsize, _type, _index);
#endif
    return true;
}
#endif

/// get raw data bytes
void ldomTextStorageChunk::getRaw( int offset, int size, lUInt8 * buf )
{
#ifdef _DEBUG
    if ( !_buf || offset+size>(int)_bufpos || offset+size>(int)_bufsize )
        crFatalError(123, "ldomTextStorageChunk: Invalid raw data buffer position");
#endif
    memcpy( buf, _buf+offset, size );
}

/// set raw data bytes
void ldomTextStorageChunk::setRaw( int offset, int size, const lUInt8 * buf )
{
#ifdef _DEBUG
    if ( !_buf || offset+size>(int)_bufpos || offset+size>(int)_bufsize )
        crFatalError(123, "ldomTextStorageChunk: Invalid raw data buffer position");
#endif
    if (memcmp(_buf+offset, buf, size) != 0) {
        memcpy(_buf+offset, buf, size);
        modified();
    }
}


/// returns free space in buffer
int ldomTextStorageChunk::space()
{
    return _bufsize - _bufpos;
}

#if BUILD_LITE!=1
/// returns free space in buffer
int ldomTextStorageChunk::addText( lUInt32 dataIndex, lUInt32 parentIndex, const lString8 & text )
{
    int itemsize = (sizeof(TextDataStorageItem)+text.length()-2 + 15) & 0xFFFFFFF0;
    if ( !_buf ) {
        // create new buffer, if necessary
        _bufsize = _manager->_chunkSize > itemsize ? _manager->_chunkSize : itemsize;
        _buf = (lUInt8*)malloc(sizeof(lUInt8) * _bufsize);
        memset(_buf, 0, _bufsize );
        _bufpos = 0;
        _manager->_uncompressedSize += _bufsize;
    }
    if ( (int)_bufsize - (int)_bufpos < itemsize )
        return -1;
    TextDataStorageItem * p = (TextDataStorageItem*)(_buf + _bufpos);
    p->sizeDiv16 = itemsize>>4;
    p->dataIndex = dataIndex;
    p->parentIndex = parentIndex;
    p->type = LXML_TEXT_NODE;
    p->length = text.length();
    memcpy(p->text, text.c_str(), p->length);
    int res = _bufpos >> 4;
    _bufpos += itemsize;
    return res;
}

/// adds new element item to buffer, returns offset inside chunk of stored data
int ldomTextStorageChunk::addElem( lUInt32 dataIndex, lUInt32 parentIndex, int childCount, int attrCount )
{
    int itemsize = (sizeof(ElementDataStorageItem) + attrCount*sizeof(lUInt16)*3 + childCount*sizeof(lUInt32) - sizeof(lUInt32) + 15) & 0xFFFFFFF0;
    if ( !_buf ) {
        // create new buffer, if necessary
        _bufsize = _manager->_chunkSize > itemsize ? _manager->_chunkSize : itemsize;
        _buf = (lUInt8*)malloc(sizeof(lUInt8) * _bufsize);
        memset(_buf, 0, _bufsize );
        _bufpos = 0;
        _manager->_uncompressedSize += _bufsize;
    }
    if ( _bufsize - _bufpos < (unsigned)itemsize )
        return -1;
    ElementDataStorageItem *item = (ElementDataStorageItem *)(_buf + _bufpos);
    if ( item ) {
        item->sizeDiv16 = itemsize>>4;
        item->dataIndex = dataIndex;
        item->parentIndex = parentIndex;
        item->type = LXML_ELEMENT_NODE;
        item->parentIndex = parentIndex;
        item->attrCount = attrCount;
        item->childCount = childCount;
    }
    int res = _bufpos >> 4;
    _bufpos += itemsize;
    return res;
}

/// set node parent by offset
bool ldomTextStorageChunk::setParent( int offset, lUInt32 parentIndex )
{
    offset <<= 4;
    if ( offset>=0 && offset<(int)_bufpos ) {
        TextDataStorageItem * item = (TextDataStorageItem *)(_buf+offset);
        if ( (int)parentIndex!=item->parentIndex ) {
            item->parentIndex = parentIndex;
            modified();
            return true;
        } else
            return false;
    }
    CRLog::error("Offset %d is out of bounds (%d) for storage chunk %c%d, chunkCount=%d", offset, this->_bufpos, this->_type, this->_index, _manager->_chunks.length() );
    return false;
}


/// get text node parent by offset
lUInt32 ldomTextStorageChunk::getParent( int offset )
{
    offset <<= 4;
    if ( offset>=0 && offset<(int)_bufpos ) {
        TextDataStorageItem * item = (TextDataStorageItem *)(_buf+offset);
        return item->parentIndex;
    }
    CRLog::error("Offset %d is out of bounds (%d) for storage chunk %c%d, chunkCount=%d", offset, this->_bufpos, this->_type, this->_index, _manager->_chunks.length() );
    return 0;
}

/// get pointer to element data
ElementDataStorageItem * ldomTextStorageChunk::getElem( int offset  )
{
    offset <<= 4;
    if ( offset>=0 && offset<(int)_bufpos ) {
        ElementDataStorageItem * item = (ElementDataStorageItem *)(_buf+offset);
        return item;
    }
    CRLog::error("Offset %d is out of bounds (%d) for storage chunk %c%d, chunkCount=%d", offset, this->_bufpos, this->_type, this->_index, _manager->_chunks.length() );
    return NULL;
}
#endif


/// call to invalidate chunk if content is modified
void ldomTextStorageChunk::modified()
{
    if ( !_buf ) {
        CRLog::error("Modified is called for node which is not in memory");
    }
    _saved = false;
}

#if BUILD_LITE!=1
/// free data item
void ldomTextStorageChunk::freeNode( int offset )
{
    offset <<= 4;
    if ( offset>=0 && offset<(int)_bufpos ) {
        TextDataStorageItem * item = (TextDataStorageItem *)(_buf+offset);
        if ( (item->type==LXML_TEXT_NODE || item->type==LXML_ELEMENT_NODE) && item->dataIndex ) {
            item->type = LXML_NO_DATA;
            item->dataIndex = 0;
            modified();
        }
    }
}

/// get text item from buffer by offset
lString8 ldomTextStorageChunk::getText( int offset )
{
    offset <<= 4;
    if ( offset>=0 && offset<(int)_bufpos ) {
        TextDataStorageItem * item = (TextDataStorageItem *)(_buf+offset);
        return item->getText8();
    }
    return lString8();
}
#endif


/// pack data from _buf to _compbuf
bool ldomPack( const lUInt8 * buf, int bufsize, lUInt8 * &dstbuf, lUInt32 & dstsize )
{
    lUInt8 tmp[PACK_BUF_SIZE]; // 64K buffer for compressed data
    int ret;
    z_stream z;
    z.zalloc = Z_NULL;
    z.zfree = Z_NULL;
    z.opaque = Z_NULL;
    ret = deflateInit( &z, DOC_DATA_COMPRESSION_LEVEL );
    if ( ret != Z_OK )
        return false;
    z.avail_in = bufsize;
    z.next_in = (unsigned char *)buf;
    z.avail_out = PACK_BUF_SIZE;
    z.next_out = tmp;
    ret = deflate( &z, Z_FINISH );
    int have = PACK_BUF_SIZE - z.avail_out;
    deflateEnd(&z);
    if ( ret!=Z_STREAM_END || have==0 || have>=PACK_BUF_SIZE || z.avail_in!=0 ) {
        // some error occured while packing, leave unpacked
        //setpacked( buf, bufsize );
        return false;
    }
    dstsize = have;
    dstbuf = (lUInt8 *)malloc(have);
    memcpy( dstbuf, tmp, have );
    return true;
}

/// unpack data from _compbuf to _buf
bool ldomUnpack( const lUInt8 * compbuf, int compsize, lUInt8 * &dstbuf, lUInt32 & dstsize  )
{
    lUInt8 tmp[UNPACK_BUF_SIZE]; // 64K buffer for compressed data
    int ret;
    z_stream z;
    memset( &z, 0, sizeof(z) );
    z.zalloc = Z_NULL;
    z.zfree = Z_NULL;
    z.opaque = Z_NULL;
    ret = inflateInit( &z );
    if ( ret != Z_OK )
        return false;
    z.avail_in = compsize;
    z.next_in = (unsigned char *)compbuf;
    z.avail_out = UNPACK_BUF_SIZE;
    z.next_out = tmp;
    ret = inflate( &z, Z_FINISH );
    int have = UNPACK_BUF_SIZE - z.avail_out;
    inflateEnd(&z);
    if ( ret!=Z_STREAM_END || have==0 || have>=UNPACK_BUF_SIZE || z.avail_in!=0 ) {
        // some error occured while unpacking
        return false;
    }
    dstsize = have;
    dstbuf = (lUInt8 *)malloc(have);
    memcpy( dstbuf, tmp, have );
    return true;
}

void ldomTextStorageChunk::setunpacked( const lUInt8 * buf, int bufsize )
{
    if ( _buf ) {
        _manager->_uncompressedSize -= _bufsize;
        free(_buf);
        _buf = NULL;
        _bufsize = 0;
    }
    if ( buf && bufsize ) {
        _bufsize = bufsize;
        _bufpos = bufsize;
        _buf = (lUInt8 *)malloc( sizeof(lUInt8) * bufsize );
        _manager->_uncompressedSize += _bufsize;
        memcpy( _buf, buf, bufsize );
    }
}

/// unpacks chunk, if packed; checks storage space, compact if necessary
void ldomTextStorageChunk::ensureUnpacked()
{
#if BUILD_LITE!=1
    if ( !_buf ) {
        if ( _saved ) {
            if ( !restoreFromCache() ) {
                CRLog::error( "restoreFromCache() failed for chunk %c%d", _type, _index);
                crFatalError( 111, "restoreFromCache() failed for chunk");
            }
            _manager->compact( 0 );
        }
    } else {
        // compact
    }
#endif
}









// moved to .cpp to hide implementation
// fastDOM
class ldomAttributeCollection
{
private:
    lUInt16 _len;
    lUInt16 _size;
    lxmlAttribute * _list;
public:
    ldomAttributeCollection()
    : _len(0), _size(0), _list(NULL)
    {
    }
    ~ldomAttributeCollection()
    {
        if (_list)
            free(_list);
    }
    lxmlAttribute * operator [] (int index) { return &_list[index]; }
    const lxmlAttribute * operator [] (int index) const { return &_list[index]; }
    lUInt16 length() const
    {
        return _len;
    }
    lUInt16 get( lUInt16 nsId, lUInt16 attrId ) const
    {
        for (lUInt16 i=0; i<_len; i++)
        {
            if (_list[i].compare( nsId, attrId ))
                return _list[i].index;
        }
        return LXML_ATTR_VALUE_NONE;
    }
    void set( lUInt16 nsId, lUInt16 attrId, lUInt16 valueIndex )
    {
        // find existing
        for (lUInt16 i=0; i<_len; i++)
        {
            if (_list[i].compare( nsId, attrId ))
            {
                _list[i].index = valueIndex;
                return;
            }
        }
        // add
        if (_len>=_size)
        {
            _size += 4;
            _list = cr_realloc( _list, _size );
        }
        _list[ _len++ ].setData(nsId, attrId, valueIndex);
    }
    void add( lUInt16 nsId, lUInt16 attrId, lUInt16 valueIndex )
    {
        // find existing
        if (_len>=_size)
        {
            _size += 4;
            _list = cr_realloc( _list, _size );
        }
        _list[ _len++ ].setData(nsId, attrId, valueIndex);
    }
    void add( const lxmlAttribute * v )
    {
        // find existing
        if (_len>=_size)
        {
            _size += 4;
            _list = cr_realloc( _list, _size );
        }
        _list[ _len++ ] = *v;
    }
};


/*
class simpleLogFile
{
public:
    FILE * f;
    simpleLogFile(const char * fname) { f = fopen( fname, "wt" ); }
    ~simpleLogFile() { if (f) fclose(f); }
    simpleLogFile & operator << ( const char * str ) { fprintf( f, "%s", str ); fflush( f ); return *this; }
    simpleLogFile & operator << ( int d ) { fprintf( f, "%d(0x%X) ", d, d ); fflush( f ); return *this; }
    simpleLogFile & operator << ( const wchar_t * str )
    {
        if (str)
        {
            for (; *str; str++ )
            {
                fputc( *str >= 32 && *str<127 ? *str : '?', f );
            }
        }
        fflush( f );
        return *this;
    }
};

simpleLogFile logfile("logfile.log");
*/



/////////////////////////////////////////////////////////////////
/// lxmlDocument


lxmlDocBase::lxmlDocBase( int dataBufSize )
: _elementNameTable(MAX_ELEMENT_TYPE_ID)
, _attrNameTable(MAX_ATTRIBUTE_TYPE_ID)
, _nsNameTable(MAX_NAMESPACE_TYPE_ID)
, _nextUnknownElementId(UNKNOWN_ELEMENT_TYPE_ID)
, _nextUnknownAttrId(UNKNOWN_ATTRIBUTE_TYPE_ID)
, _nextUnknownNsId(UNKNOWN_NAMESPACE_TYPE_ID)
, _attrValueTable( DOC_STRING_HASH_SIZE )
,_idNodeMap(8192)
,_urlImageMap(1024)
,_idAttrId(0)
,_nameAttrId(0)
#if BUILD_LITE!=1
//,_keepData(false)
//,_mapped(false)
#endif
#if BUILD_LITE!=1
,_pagesData(8192)
#endif
{
    // create and add one data buffer
    _stylesheet.setDocument( this );
}

/// Destructor
lxmlDocBase::~lxmlDocBase()
{
}

void lxmlDocBase::onAttributeSet( lUInt16 attrId, lUInt16 valueId, ldomNode * node )
{
    if ( _idAttrId==0 )
        _idAttrId = _attrNameTable.idByName("id");
    if ( _nameAttrId==0 )
        _nameAttrId = _attrNameTable.idByName("name");
    if (attrId == _idAttrId) {
        _idNodeMap.set( valueId, node->getDataIndex() );
    } else if ( attrId==_nameAttrId ) {
        lString16 nodeName = node->getNodeName();
        if ( nodeName==L"a" )
            _idNodeMap.set( valueId, node->getDataIndex() );
    }
}

lUInt16 lxmlDocBase::getNsNameIndex( const lChar16 * name )
{
    const LDOMNameIdMapItem * item = _nsNameTable.findItem( name );
    if (item)
        return item->id;
    _nsNameTable.AddItem( _nextUnknownNsId, lString16(name), NULL );
    return _nextUnknownNsId++;
}

lUInt16 lxmlDocBase::getAttrNameIndex( const lChar16 * name )
{
    const LDOMNameIdMapItem * item = _attrNameTable.findItem( name );
    if (item)
        return item->id;
    _attrNameTable.AddItem( _nextUnknownAttrId, lString16(name), NULL );
    return _nextUnknownAttrId++;
}

lUInt16 lxmlDocBase::getElementNameIndex( const lChar16 * name )
{
    const LDOMNameIdMapItem * item = _elementNameTable.findItem( name );
    if (item)
        return item->id;
    _elementNameTable.AddItem( _nextUnknownElementId, lString16(name), NULL );
    return _nextUnknownElementId++;
}


/// create formatted text object with options set
LFormattedText * lxmlDocBase::createFormattedText()
{
    LFormattedText * p = new LFormattedText();
    p->setImageScalingOptions(&_imgScalingOptions);
    p->setMinSpaceCondensingPercent(_minSpaceCondensingPercent);
    return p;
}

/// returns main element (i.e. FictionBook for FB2)
ldomNode * lxmlDocBase::getRootNode()
{
    return getTinyNode(17);
}

ldomDocument::ldomDocument()
: m_toc(this)
#if BUILD_LITE!=1
, _last_docflags(0)
, _page_height(0)
, _page_width(0)
, _rendered(false)
#endif
, lists(100)
{
    allocTinyElement(NULL, 0, 0);
    //new ldomElement( this, NULL, 0, 0, 0 );
    //assert( _instanceMapCount==2 );
}

/// Copy constructor - copies ID tables contents
lxmlDocBase::lxmlDocBase( lxmlDocBase & doc )
:    tinyNodeCollection(doc)
,   _elementNameTable(doc._elementNameTable)    // Element Name<->Id map
,   _attrNameTable(doc._attrNameTable)       // Attribute Name<->Id map
,   _nsNameTable(doc._nsNameTable)           // Namespace Name<->Id map
,   _nextUnknownElementId(doc._nextUnknownElementId) // Next Id for unknown element
,   _nextUnknownAttrId(doc._nextUnknownAttrId)    // Next Id for unknown attribute
,   _nextUnknownNsId(doc._nextUnknownNsId)      // Next Id for unknown namespace
    //lvdomStyleCache _styleCache;         // Style cache
,   _attrValueTable(doc._attrValueTable)
,   _idNodeMap(doc._idNodeMap)
,   _urlImageMap(1024)
,   _idAttrId(doc._idAttrId) // Id for "id" attribute name
//,   _docFlags(doc._docFlags)
#if BUILD_LITE!=1
,   _pagesData(8192)
#endif
{
}

/// creates empty document which is ready to be copy target of doc partial contents
ldomDocument::ldomDocument( ldomDocument & doc )
: lxmlDocBase(doc)
#if BUILD_LITE!=1
, _def_font(doc._def_font) // default font
, _def_style(doc._def_style)
, _last_docflags(doc._last_docflags)
, _page_height(doc._page_height)
, _page_width(doc._page_width)
#endif
, _container(doc._container)
, lists(100)
, m_toc(this)
{
}

static void writeNode( LVStream * stream, ldomNode * node, bool treeLayout )
{
    int level = 0;
    if ( treeLayout ) {
        level = node->getNodeLevel();
        for (int i=0; i<level; i++ )
            *stream << "  ";
    }
    if ( node->isText() )
    {
        lString8 txt = node->getText8();
        *stream << txt;
        if ( treeLayout )
            *stream << "\n";
    }
    else if (  node->isElement() )
    {
        lString8 elemName = UnicodeToUtf8(node->getNodeName());
        lString8 elemNsName = UnicodeToUtf8(node->getNodeNsName());
        if (!elemNsName.empty())
            elemName = elemNsName + ":" + elemName;
        if (!elemName.empty())
            *stream << "<" << elemName;
        int i;
        for (i=0; i<(int)node->getAttrCount(); i++)
        {
            const lxmlAttribute * attr = node->getAttribute(i);
            if (attr)
            {
                lString8 attrName( UnicodeToUtf8(node->getDocument()->getAttrName(attr->id)) );
                lString8 nsName( UnicodeToUtf8(node->getDocument()->getNsName(attr->nsid)) );
                lString8 attrValue( UnicodeToUtf8(node->getDocument()->getAttrValue(attr->index)) );
                *stream << " ";
                if ( nsName.length() > 0 )
                    *stream << nsName << ":";
                *stream << attrName << "=\"" << attrValue << "\"";
            }
        }

#if 0
            if (!elemName.empty())
            {
                ldomNode * elem = node;
                lvdomElementFormatRec * fmt = elem->getRenderData();
                css_style_ref_t style = elem->getStyle();
                if ( fmt ) {
                    lvRect rect;
                    elem->getAbsRect( rect );
                    *stream << L" fmt=\"";
                    *stream << L"rm:" << lString16::itoa( (int)elem->getRendMethod() ) << L" ";
                    if ( style.isNull() )
                        *stream << L"style: NULL ";
                    else {
                        *stream << L"disp:" << lString16::itoa( (int)style->display ) << L" ";
                    }
                    *stream << L"y:" << lString16::itoa( (int)fmt->getY() ) << L" ";
                    *stream << L"h:" << lString16::itoa( (int)fmt->getHeight() ) << L" ";
                    *stream << L"ay:" << lString16::itoa( (int)rect.top ) << L" ";
                    *stream << L"ah:" << lString16::itoa( (int)rect.height() ) << L" ";
                    *stream << L"\"";
                }
            }
#endif

        if ( node->getChildCount() == 0 ) {
            if (!elemName.empty())
            {
                if ( elemName[0] == '?' )
                    *stream << "?>";
                else
                    *stream << "/>";
            }
            if ( treeLayout )
                *stream << "\n";
        } else {
            if (!elemName.empty())
                *stream << ">";
            if ( treeLayout )
                *stream << "\n";
            for (i=0; i<(int)node->getChildCount(); i++)
            {
                writeNode( stream, node->getChildNode(i), treeLayout );
            }
            if ( treeLayout ) {
                for (int i=0; i<level; i++ )
                    *stream << "  ";
            }
            if (!elemName.empty())
                *stream << "</" << elemName << ">";
            if ( treeLayout )
                *stream << "\n";
        }
    }
}

bool ldomDocument::saveToStream( LVStreamRef stream, const char *, bool treeLayout )
{
    //CRLog::trace("ldomDocument::saveToStream()");
    if (!stream || !getRootNode()->getChildCount())
        return false;

    *stream.get() << UnicodeToLocal(lString16(L"\xFEFF"));
    writeNode( stream.get(), getRootNode(), treeLayout );
    return true;
}

ldomDocument::~ldomDocument()
{
#if BUILD_LITE!=1
    updateMap();
#endif
}

#if BUILD_LITE!=1

/// renders (formats) document in memory
bool ldomDocument::setRenderProps( int width, int dy, bool showCover, int y0, font_ref_t def_font, int def_interline_space, CRPropRef props )
{
    bool changed = false;
    _renderedBlockCache.clear();
    changed = _imgScalingOptions.update(props, def_font->getSize()) || changed;
    css_style_ref_t s( new css_style_rec_t );
    s->display = css_d_block;
    s->white_space = css_ws_normal;
    s->text_align = css_ta_left;
    s->text_align_last = css_ta_left;
    s->text_decoration = css_td_none;
    s->hyphenate = css_hyph_auto;
    s->color.type = css_val_unspecified;
    s->color.value = 0x000000;
    s->background_color.type = css_val_unspecified;
    s->background_color.value = 0xFFFFFF;
    //_def_style->background_color.type = color;
    //_def_style->background_color.value = 0xFFFFFF;
    s->page_break_before = css_pb_auto;
    s->page_break_after = css_pb_auto;
    s->page_break_inside = css_pb_auto;
    s->list_style_type = css_lst_disc;
    s->list_style_position = css_lsp_outside;
    s->vertical_align = css_va_baseline;
    s->font_family = def_font->getFontFamily();
    s->font_size.type = css_val_px;
    s->font_size.value = def_font->getSize();
    s->font_name = def_font->getTypeFace();
    s->font_weight = css_fw_400;
    s->font_style = css_fs_normal;
    s->text_indent.type = css_val_px;
    s->text_indent.value = 0;
    s->line_height.type = css_val_percent;
    s->line_height.value = def_interline_space;
    //lUInt32 defStyleHash = (((_stylesheet.getHash() * 31) + calcHash(_def_style))*31 + calcHash(_def_font));
    //defStyleHash = defStyleHash * 31 + getDocFlags();
    if ( _last_docflags != getDocFlags() ) {
        CRLog::trace("ldomDocument::setRenderProps() - doc flags changed");
        _last_docflags = getDocFlags();
        changed = true;
    }
    if ( calcHash(_def_style) != calcHash(s) ) {
        CRLog::trace("ldomDocument::setRenderProps() - style is changed");
        _def_style = s;
        changed = true;
    }
    if ( calcHash(_def_font) != calcHash(def_font)) {
        CRLog::trace("ldomDocument::setRenderProps() - font is changed");
        _def_font = def_font;
        changed = true;
    }
    if ( _page_height != dy ) {
        CRLog::trace("ldomDocument::setRenderProps() - page height is changed");
        _page_height = dy;
        changed = true;
    }
    if ( _page_width != width ) {
        CRLog::trace("ldomDocument::setRenderProps() - page width is changed");
        _page_width = width;
        changed = true;
    }
//    {
//        lUInt32 styleHash = calcStyleHash();
//        styleHash = styleHash * 31 + calcGlobalSettingsHash();
//        CRLog::debug("Style hash before set root style: %x", styleHash);
//    }
//    getRootNode()->setFont( _def_font );
//    getRootNode()->setStyle( _def_style );
//    {
//        lUInt32 styleHash = calcStyleHash();
//        styleHash = styleHash * 31 + calcGlobalSettingsHash();
//        CRLog::debug("Style hash after set root style: %x", styleHash);
//    }
    return changed;
}

void tinyNodeCollection::dropStyles()
{
    _styles.clear(-1);
    _fonts.clear(-1);
    resetNodeNumberingProps();

    int count = ((_elemCount+TNC_PART_LEN-1) >> TNC_PART_SHIFT);
    for ( int i=0; i<count; i++ ) {
        int offs = i*TNC_PART_LEN;
        int sz = TNC_PART_LEN;
        if ( offs + sz > _elemCount+1 ) {
            sz = _elemCount+1 - offs;
        }
        ldomNode * buf = _elemList[i];
        for ( int j=0; j<sz; j++ ) {
            if ( buf[j].isElement() ) {
                setNodeStyleIndex( buf[j]._handle._dataIndex, 0 );
                setNodeFontIndex( buf[j]._handle._dataIndex, 0 );
            }
        }
    }
}

int tinyNodeCollection::calcFinalBlocks()
{
    int cnt = 0;
    int count = ((_elemCount+TNC_PART_LEN-1) >> TNC_PART_SHIFT);
    for ( int i=0; i<count; i++ ) {
        int offs = i*TNC_PART_LEN;
        int sz = TNC_PART_LEN;
        if ( offs + sz > _elemCount+1 ) {
            sz = _elemCount+1 - offs;
        }
        ldomNode * buf = _elemList[i];
        for ( int j=0; j<sz; j++ ) {
            if ( buf[j].isElement() ) {
                int rm = buf[j].getRendMethod();
                if ( rm==erm_final )
                    cnt++;
            }
        }
    }
    return cnt;
}

void ldomDocument::applyDocumentStyleSheet()
{
    if ( !getDocFlag(DOC_FLAG_ENABLE_INTERNAL_STYLES) ) {
        CRLog::trace("applyDocumentStyleSheet() : DOC_FLAG_ENABLE_INTERNAL_STYLES is disabled");
        return;
    }
    if ( !_docStylesheetFileName.empty() ) {
        if ( getContainer().isNull() )
            return;
        LVStreamRef cssStream = getContainer()->OpenStream(_docStylesheetFileName.c_str(), LVOM_READ);
        if ( !cssStream.isNull() ) {
            lString16 css;
            css << LVReadTextFile( cssStream );
            if ( !css.empty() ) {
                CRLog::debug("applyDocumentStyleSheet() : Using document stylesheet from link/stylesheet from %s", LCSTR(_docStylesheetFileName));
                _stylesheet.parse(LCSTR(css));
                return;
            }
        }
        CRLog::error("applyDocumentStyleSheet() : cannot load link/stylesheet from %s", LCSTR(_docStylesheetFileName));
    } else {
        ldomXPointer ss = createXPointer(lString16(L"/FictionBook/stylesheet"));
        if ( !ss.isNull() ) {
            lString16 css = ss.getText('\n');
            if ( !css.empty() ) {
                CRLog::debug("applyDocumentStyleSheet() : Using internal FB2 document stylesheet:\n%s", LCSTR(css));
                _stylesheet.parse(LCSTR(css));
            } else {
                CRLog::trace("applyDocumentStyleSheet() : stylesheet under /FictionBook/stylesheet is empty");
            }
        } else {
            CRLog::trace("applyDocumentStyleSheet() : No internal FB2 stylesheet found under /FictionBook/stylesheet");
        }
    }
}


int ldomDocument::render( LVRendPageList * pages, LVDocViewCallback * callback, int width, int dy, bool showCover, int y0, font_ref_t def_font, int def_interline_space, CRPropRef props )
{
    CRLog::info("Render is called for width %d, pageHeight=%d, fontFace=%s", width, dy, def_font->getTypeFace().c_str() );
    CRLog::trace("initializing default style...");
    //persist();
//    {
//        lUInt32 styleHash = calcStyleHash();
//        styleHash = styleHash * 31 + calcGlobalSettingsHash();
//        CRLog::debug("Style hash before setRenderProps: %x", styleHash);
//    } //bool propsChanged =
    setRenderProps( width, dy, showCover, y0, def_font, def_interline_space, props );

    // update styles
//    if ( getRootNode()->getStyle().isNull() || getRootNode()->getFont().isNull()
//        || _docFlags != _hdr.render_docflags
//        || width!=_hdr.render_dx || dy!=_hdr.render_dy || defStyleHash!=_hdr.stylesheet_hash ) {
//        CRLog::trace("init format data...");
//        getRootNode()->recurseElements( initFormatData );
//    } else {
//        CRLog::trace("reusing existing format data...");
//    }

    if ( !checkRenderContext() ) {
        CRLog::info("rendering context is changed - full render required...");
        CRLog::trace("init format data...");
        //CRLog::trace("validate 1...");
        //validateDocument();
        CRLog::trace("Dropping existing styles...");
        //CRLog::debug( "root style before drop style %d", getNodeStyleIndex(getRootNode()->getDataIndex()));
        dropStyles();
        //CRLog::debug( "root style after drop style %d", getNodeStyleIndex(getRootNode()->getDataIndex()));

        //ldomNode * root = getRootNode();
        //css_style_ref_t roots = root->getStyle();
        //CRLog::trace("validate 2...");
        //validateDocument();

        CRLog::trace("Save stylesheet...");
        _stylesheet.push();
        CRLog::trace("Init node styles...");
        applyDocumentStyleSheet();
        getRootNode()->initNodeStyleRecursive();
        CRLog::trace("Restoring stylesheet...");
        _stylesheet.pop();

        CRLog::trace("init render method...");
        getRootNode()->initNodeRendMethodRecursive();

//        getRootNode()->setFont( _def_font );
//        getRootNode()->setStyle( _def_style );
        updateRenderContext();

        // DEBUG dump of render methods
        //dumpRendMethods( getRootNode(), lString16(" - ") );
//        lUInt32 styleHash = calcStyleHash();
//        styleHash = styleHash * 31 + calcGlobalSettingsHash();
//        CRLog::debug("Style hash: %x", styleHash);

        _rendered = false;
    }
    if ( !_rendered ) {
        pages->clear();
        if ( showCover )
            pages->add( new LVRendPageInfo( _page_height ) );
        LVRendPageContext context( pages, _page_height );
        int numFinalBlocks = calcFinalBlocks();
        CRLog::info("Final block count: %d", numFinalBlocks);
        context.setCallback(callback, numFinalBlocks);
        //updateStyles();
        CRLog::trace("rendering...");
        int height = renderBlockElement( context, getRootNode(),
            0, y0, width ) + y0;
        _rendered = true;
    #if 0 //def _DEBUG
        LVStreamRef ostream = LVOpenFileStream( "test_save_after_init_rend_method.xml", LVOM_WRITE );
        saveToStream( ostream, "utf-16" );
    #endif
        gc();
        CRLog::trace("finalizing... fonts.length=%d", _fonts.length());
        context.Finalize();
        updateRenderContext();
        _pagesData.reset();
        pages->serialize( _pagesData );

        if ( callback ) {
            callback->OnFormatEnd();
        }

        //saveChanges();

        //persist();
        dumpStatistics();
        return height;
    } else {
        CRLog::info("rendering context is not changed - no render!");
        if ( _pagesData.pos() ) {
            _pagesData.setPos(0);
            pages->deserialize( _pagesData );
        }
        CRLog::info("%d rendered pages found", pages->length() );
        return getFullHeight();
    }

}
#endif

void lxmlDocBase::setNodeTypes( const elem_def_t * node_scheme )
{
    if ( !node_scheme )
        return;
    for ( ; node_scheme && node_scheme->id != 0; ++node_scheme )
    {
        _elementNameTable.AddItem(
            node_scheme->id,               // ID
            lString16(node_scheme->name),  // Name
            &node_scheme->props );  // ptr
    }
}

// set attribute types from table
void lxmlDocBase::setAttributeTypes( const attr_def_t * attr_scheme )
{
    if ( !attr_scheme )
        return;
    for ( ; attr_scheme && attr_scheme->id != 0; ++attr_scheme )
    {
        _attrNameTable.AddItem(
            attr_scheme->id,               // ID
            lString16(attr_scheme->name),  // Name
            NULL);
    }
    _idAttrId = _attrNameTable.idByName("id");
}

// set namespace types from table
void lxmlDocBase::setNameSpaceTypes( const ns_def_t * ns_scheme )
{
    if ( !ns_scheme )
        return;
    for ( ; ns_scheme && ns_scheme->id != 0; ++ns_scheme )
    {
        _nsNameTable.AddItem(
            ns_scheme->id,                 // ID
            lString16(ns_scheme->name),    // Name
            NULL);
    }
}

void lxmlDocBase::dumpUnknownEntities( const char * fname )
{
    FILE * f = fopen( fname, "wt" );
    if ( !f )
        return;
    fprintf(f, "Unknown elements:\n");
    _elementNameTable.dumpUnknownItems(f, UNKNOWN_ELEMENT_TYPE_ID);
    fprintf(f, "-------------------------------\n");
    fprintf(f, "Unknown attributes:\n");
    _attrNameTable.dumpUnknownItems(f, UNKNOWN_ATTRIBUTE_TYPE_ID);
    fprintf(f, "-------------------------------\n");
    fprintf(f, "Unknown namespaces:\n");
    _nsNameTable.dumpUnknownItems(f, UNKNOWN_NAMESPACE_TYPE_ID);
    fprintf(f, "-------------------------------\n");
    fclose(f);
}

#if BUILD_LITE!=1
static const char * id_map_list_magic = "MAPS";
static const char * elem_id_map_magic = "ELEM";
static const char * attr_id_map_magic = "ATTR";
static const char * attr_value_map_magic = "ATTV";
static const char * ns_id_map_magic =   "NMSP";
static const char * node_by_id_map_magic = "NIDM";

/// serialize to byte array (pointer will be incremented by number of bytes written)
void lxmlDocBase::serializeMaps( SerialBuf & buf )
{
    if ( buf.error() )
        return;
    int pos = buf.pos();
    buf.putMagic( id_map_list_magic );
    buf.putMagic( elem_id_map_magic );
    _elementNameTable.serialize( buf );
    buf << _nextUnknownElementId; // Next Id for unknown element
    buf.putMagic( attr_id_map_magic );
    _attrNameTable.serialize( buf );
    buf << _nextUnknownAttrId;    // Next Id for unknown attribute
    buf.putMagic( ns_id_map_magic );
    _nsNameTable.serialize( buf );
    buf << _nextUnknownNsId;      // Next Id for unknown namespace
    buf.putMagic( attr_value_map_magic );
    _attrValueTable.serialize( buf );

    int start = buf.pos();
    buf.putMagic( node_by_id_map_magic );
    lUInt32 cnt = 0;
    {
        LVHashTable<lUInt16,lInt32>::iterator ii = _idNodeMap.forwardIterator();
        for ( LVHashTable<lUInt16,lInt32>::pair * p = ii.next(); p!=NULL; p = ii.next() ) {
            cnt++;
        }
    }
    // TODO: investigate why length() doesn't work as count
    if ( (int)cnt!=_idNodeMap.length() )
        CRLog::error("_idNodeMap.length=%d doesn't match real item count %d", _idNodeMap.length(), cnt);
    buf << cnt;
    {
        LVHashTable<lUInt16,lInt32>::iterator ii = _idNodeMap.forwardIterator();
        for ( LVHashTable<lUInt16,lInt32>::pair * p = ii.next(); p!=NULL; p = ii.next() ) {
            buf << (lUInt16)p->key << (lUInt32)p->value;
        }
    }
    buf.putMagic( node_by_id_map_magic );
    buf.putCRC( buf.pos() - start );

    buf.putCRC( buf.pos() - pos );
}

/// deserialize from byte array (pointer will be incremented by number of bytes read)
bool lxmlDocBase::deserializeMaps( SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    int pos = buf.pos();
    buf.checkMagic( id_map_list_magic );
    buf.checkMagic( elem_id_map_magic );
    _elementNameTable.deserialize( buf );
    buf >> _nextUnknownElementId; // Next Id for unknown element

    if ( buf.error() ) {
        CRLog::error("Error while deserialization of Element ID map");
        return false;
    }

    buf.checkMagic( attr_id_map_magic );
    _attrNameTable.deserialize( buf );
    buf >> _nextUnknownAttrId;    // Next Id for unknown attribute

    if ( buf.error() ) {
        CRLog::error("Error while deserialization of Attr ID map");
        return false;
    }


    buf.checkMagic( ns_id_map_magic );
    _nsNameTable.deserialize( buf );
    buf >> _nextUnknownNsId;      // Next Id for unknown namespace

    if ( buf.error() ) {
        CRLog::error("Error while deserialization of NS ID map");
        return false;
    }

    buf.checkMagic( attr_value_map_magic );
    _attrValueTable.deserialize( buf );

    if ( buf.error() ) {
        CRLog::error("Error while deserialization of AttrValue map");
        return false;
    }

    int start = buf.pos();
    buf.checkMagic( node_by_id_map_magic );
    lUInt32 idmsize;
    buf >> idmsize;
    _idNodeMap.clear();
    if ( idmsize < 20000 )
        _idNodeMap.resize( idmsize*2 );
    for ( unsigned i=0; i<idmsize; i++ ) {
        lUInt16 key;
        lUInt32 value;
        buf >> key;
        buf >> value;
        _idNodeMap.set( key, value );
        if ( buf.error() )
            return false;
    }
    buf.checkMagic( node_by_id_map_magic );

    if ( buf.error() ) {
        CRLog::error("Error while deserialization of ID->Node map");
        return false;
    }

    buf.checkCRC( buf.pos() - start );

    if ( buf.error() ) {
        CRLog::error("Error while deserialization of ID->Node map - CRC check failed");
        return false;
    }

    buf.checkCRC( buf.pos() - pos );

    return !buf.error();
}
#endif

bool IsEmptySpace( const lChar16 * text, int len )
{
   for (int i=0; i<len; i++)
      if ( text[i]!=' ' && text[i]!='\r' && text[i]!='\n' && text[i]!='\t')
         return false;
   return true;
}


/////////////////////////////////////////////////////////////////
/// lxmlElementWriter

static bool IS_FIRST_BODY = false;

ldomElementWriter::ldomElementWriter(ldomDocument * document, lUInt16 nsid, lUInt16 id, ldomElementWriter * parent)
    : _parent(parent), _document(document), _tocItem(NULL), _isBlock(true), _isSection(false), _stylesheetIsSet(false), _bodyEnterCalled(false)
{
    //logfile << "{c";
    _typeDef = _document->getElementTypePtr( id );
    _flags = 0;
    if ( (_typeDef && _typeDef->white_space==css_ws_pre) || (_parent && _parent->getFlags()&TXTFLG_PRE) )
        _flags |= TXTFLG_PRE;
    _isSection = (id==el_section);
    _allowText = _typeDef ? _typeDef->allow_text : (_parent?true:false);
    if (_parent)
        _element = _parent->getElement()->insertChildElement( (lUInt32)-1, nsid, id );
    else
        _element = _document->getRootNode(); //->insertChildElement( (lUInt32)-1, nsid, id );
    //CRLog::trace("ldomElementWriter created for element 0x%04x %s", _element->getDataIndex(), LCSTR(_element->getNodeName()));
    if ( IS_FIRST_BODY && id==el_body ) {
        _tocItem = _document->getToc();
        //_tocItem->clear();
        IS_FIRST_BODY = false;
    }
    //logfile << "}";
}

lUInt32 ldomElementWriter::getFlags()
{
    return _flags;
}

static bool isBlockNode( ldomNode * node )
{
    if ( !node->isElement() )
        return false;
#if BUILD_LITE!=1
    switch ( node->getStyle()->display )
    {
    case css_d_block:
    case css_d_list_item:
    case css_d_table:
    case css_d_table_row:
    case css_d_inline_table:
    case css_d_table_row_group:
    case css_d_table_header_group:
    case css_d_table_footer_group:
    case css_d_table_column_group:
    case css_d_table_column:
    case css_d_table_cell:
    case css_d_table_caption:
        return true;

    case css_d_inherit:
    case css_d_inline:
    case css_d_run_in:
    case css_d_compact:
    case css_d_marker:
    case css_d_none:
        break;
    }
    return false;
#else
    return true;
#endif
}

static bool isInlineNode( ldomNode * node )
{
    if ( node->isText() )
        return true;
    //int d = node->getStyle()->display;
    //return ( d==css_d_inline || d==css_d_run_in );
    int m = node->getRendMethod();
    return m==erm_inline || m==erm_runin;
}

static lString16 getSectionHeader( ldomNode * section )
{
    lString16 header;
    if ( !section || section->getChildCount() == 0 )
        return header;
    ldomNode * child = section->getChildElementNode(0, L"title");
    if ( !child )
        return header;
    header = child->getText(L' ', 1024);
    return header;
}

lString16 ldomElementWriter::getPath()
{
    if ( !_path.empty() || _element->isRoot() )
        return _path;
    _path = _parent->getPath() + L"/" + _element->getXPathSegment();
    return _path;
}

void ldomElementWriter::updateTocItem()
{
    if ( !_isSection )
        return;
    // TODO: update item
    if ( _parent && _parent->_tocItem ) {
        lString16 title = getSectionHeader( _element );
        //CRLog::trace("TOC ITEM: %s", LCSTR(title));
        _tocItem = _parent->_tocItem->addChild(title, ldomXPointer(_element,0), getPath() );
    }
    _isSection = false;
}

void ldomElementWriter::onBodyEnter()
{
    _bodyEnterCalled = true;
#if BUILD_LITE!=1
    //CRLog::trace("onBodyEnter() for node %04x %s", _element->getDataIndex(), LCSTR(_element->getNodeName()));
    if ( _document->isDefStyleSet() ) {
        _element->initNodeStyle();
//        if ( _element->getStyle().isNull() ) {
//            CRLog::error("error while style initialization of element %x %s", _element->getNodeIndex(), LCSTR(_element->getNodeName()) );
//            crFatalError();
//        }
        _isBlock = isBlockNode(_element);
    } else {
    }
    if ( _isSection ) {
        if ( _parent && _parent->_isSection ) {
            _parent->updateTocItem();
        }

    }
#endif
}

void ldomNode::removeChildren( int startIndex, int endIndex )
{
    for ( int i=endIndex; i>=startIndex; i-- ) {
        removeChild(i)->destroy();
    }
}

void ldomNode::autoboxChildren( int startIndex, int endIndex )
{
#if BUILD_LITE!=1
    if ( !isElement() )
        return;
    css_style_ref_t style = getStyle();
    bool pre = ( style->white_space==css_ws_pre );
    int firstNonEmpty = startIndex;
    int lastNonEmpty = endIndex;

    bool hasInline = pre;
    if ( !pre ) {
        while ( firstNonEmpty<=endIndex && getChildNode(firstNonEmpty)->isText() ) {
            lString16 s = getChildNode(firstNonEmpty)->getText();
            if ( !IsEmptySpace(s.c_str(), s.length() ) )
                break;
            firstNonEmpty++;
        }
        while ( lastNonEmpty>=endIndex && getChildNode(lastNonEmpty)->isText() ) {
            lString16 s = getChildNode(lastNonEmpty)->getText();
            if ( !IsEmptySpace(s.c_str(), s.length() ) )
                break;
            lastNonEmpty--;
        }

        for ( int i=firstNonEmpty; i<=lastNonEmpty; i++ ) {
            ldomNode * node = getChildNode(i);
            if ( isInlineNode( node ) )
                hasInline = true;
        }
    }

    if ( hasInline ) { //&& firstNonEmpty<=lastNonEmpty

#ifdef TRACE_AUTOBOX
        CRLog::trace("Autobox children %d..%d of node <%s>  childCount=%d", firstNonEmpty, lastNonEmpty, LCSTR(getNodeName()), getChildCount());

        for ( int i=firstNonEmpty; i<=lastNonEmpty; i++ ) {
            ldomNode * node = getChildNode(i);
            if ( node->isText() )
                CRLog::trace("    text: %d '%s'", node->getDataIndex(), LCSTR(node->getText()));
            else
                CRLog::trace("    elem: %d <%s> rendMode=%d  display=%d", node->getDataIndex(), LCSTR(node->getNodeName()), node->getRendMethod(), node->getStyle()->display);
        }
#endif
        // remove starting empty
        removeChildren(lastNonEmpty+1, endIndex);

        // inner inline
        ldomNode * abox = insertChildElement( firstNonEmpty, LXML_NS_NONE, el_autoBoxing );
        abox->initNodeStyle();
        abox->setRendMethod( erm_final );
        moveItemsTo( abox, firstNonEmpty+1, lastNonEmpty+1 );
        // remove trailing empty
        removeChildren(startIndex, firstNonEmpty-1);
    } else {
        // only empty items: remove them instead of autoboxing
        removeChildren(startIndex, endIndex);
    }
#endif
}

#if BUILD_LITE!=1
static void resetRendMethodToInline( ldomNode * node )
{
    node->setRendMethod(erm_inline);
}

static void resetRendMethodToInvisible( ldomNode * node )
{
    node->setRendMethod(erm_invisible);
}

static void detectChildTypes( ldomNode * parent, bool & hasBlockItems, bool & hasInline )
{
    hasBlockItems = false;
    hasInline = false;
    int len = parent->getChildCount();
    for ( int i=len-1; i>=0; i-- ) {
        ldomNode * node = parent->getChildNode(i);
        if ( !node->isElement() ) {
            // text
            hasInline = true;
        } else {
            // element
            int d = node->getStyle()->display;
            int m = node->getRendMethod();
            if ( d==css_d_none || m==erm_invisible )
                continue;
            if ( m==erm_inline || m==erm_runin) { //d==css_d_inline || d==css_d_run_in
                hasInline = true;
            } else {
                hasBlockItems = true;
            }
        }
    }
}


// init table element render methods
// states: 0=table, 1=colgroup, 2=rowgroup, 3=row, 4=cell
// returns table cell count
int initTableRendMethods( ldomNode * enode, int state )
{
    //main node: table
    if ( state==0 && enode->getStyle()->display==css_d_table )
        enode->setRendMethod( erm_table ); // for table
    int cellCount = 0;
    int cnt = enode->getChildCount();
    int i;
    for (i=0; i<cnt; i++)
    {
        ldomNode * child = enode->getChildNode( i );
        if ( child->isElement() )
        {
            switch( child->getStyle()->display )
            {
            case css_d_table_caption:
                if ( state==0 ) {
                    child->setRendMethod( erm_table_caption );
                } else {
                    child->setRendMethod( erm_invisible );
                }
                break;
            case css_d_inline:
                {
                }
                break;
            case css_d_table_row_group:
                if ( state==0 ) {
                    child->setRendMethod( erm_table_row_group );
                    cellCount += initTableRendMethods( child, 2 );
                } else {
                    child->setRendMethod( erm_invisible );
                }
                break;
            case css_d_table_header_group:
                if ( state==0 ) {
                    child->setRendMethod( erm_table_header_group );
                    cellCount += initTableRendMethods( child, 2);
                } else {
                    child->setRendMethod( erm_invisible );
                }
                break;
            case css_d_table_footer_group:
                if ( state==0 ) {
                    child->setRendMethod( erm_table_footer_group );
                    cellCount += initTableRendMethods( child, 2 );
                } else {
                    child->setRendMethod( erm_invisible );
                }
                break;
            case css_d_table_row:
                if ( state==0 || state==2 ) {
                    child->setRendMethod( erm_table_row );
                    cellCount += initTableRendMethods( child, 3 );
                } else {
                    child->setRendMethod( erm_invisible );
                }
                break;
            case css_d_table_column_group:
                if ( state==0 ) {
                    child->setRendMethod( erm_table_column_group );
                    cellCount += initTableRendMethods( child, 1 );
                } else {
                    child->setRendMethod( erm_invisible );
                }
                break;
            case css_d_table_column:
                if ( state==0 || state==1 ) {
                    child->setRendMethod( erm_table_column );
                } else {
                    child->setRendMethod( erm_invisible );
                }
                break;
            case css_d_table_cell:
                if ( state==3 ) {
                    child->setRendMethod( erm_table_cell );
                    cellCount++;
                    // will be translated to block or final below
                    //child->initNodeRendMethod();
                    child->initNodeRendMethodRecursive();
                    //child->setRendMethod( erm_table_cell );
                    //initRendMethod( child, true, true );
                } else {
                    child->setRendMethod( erm_invisible );
                }
                break;
            default:
                // ignore
                break;
            }
        }
    }
//    if ( state==0 ) {
//        dumpRendMethods( enode, lString16(L"   ") );
//    }
    return cellCount;
}

bool hasInvisibleParent( ldomNode * node )
{
    for ( ; !node->isRoot(); node = node->getParentNode() )
        if ( node->getStyle()->display==css_d_none )
            return true;
    return false;
}

void ldomNode::initNodeRendMethod()
{
    if ( !isElement() )
        return;
    if ( isRoot() ) {
        setRendMethod(erm_block);
        return;
    }

    // DEBUG TEST
//    if ( getParentNode()->getChildIndex( getDataIndex() )<0 ) {
//        CRLog::error("Invalid parent->child relation for nodes %d->%d", getParentNode()->getDataIndex(), getDataIndex() );
//    }
//    if ( getNodeName()==L"image" ) {
//        CRLog::trace("Init log for image");
//    }

    int d = getStyle()->display;

    if ( hasInvisibleParent(this) ) {
        // invisible
        //recurseElements( resetRendMethodToInvisible );
        setRendMethod(erm_invisible);
    } else if ( d==css_d_inline ) {
        // inline
        //CRLog::trace("switch all children elements of <%s> to inline", LCSTR(getNodeName()));
        recurseElements( resetRendMethodToInline );
    } else if ( d==css_d_run_in ) {
        // runin
        //CRLog::trace("switch all children elements of <%s> to inline", LCSTR(getNodeName()));
        recurseElements( resetRendMethodToInline );
        setRendMethod(erm_runin);
    } else if ( d==css_d_list_item ) {
        // list item
        setRendMethod(erm_list_item);
    } else if (d == css_d_table) {
        // table
        initTableRendMethods( this, 0 );
    } else {
        // block or final
        // remove last empty space text nodes
        bool hasBlockItems = false;
        bool hasInline = false;
        detectChildTypes( this, hasBlockItems, hasInline );
        const css_elem_def_props_t * ntype = getElementTypePtr();
        if (ntype && ntype->is_object) {
            switch ( d )
            {
            case css_d_block:
            case css_d_inline:
            case css_d_run_in:
                setRendMethod( erm_final );
                break;
            default:
                //setRendMethod( erm_invisible );
                recurseElements( resetRendMethodToInvisible );
                break;
            }
        } else if ( hasBlockItems && !hasInline ) {
            // only blocks inside
            setRendMethod( erm_block );
        } else if ( !hasBlockItems && hasInline ) {
            setRendMethod( erm_final );
        } else if ( !hasBlockItems && !hasInline ) {
            setRendMethod( erm_block );
            //setRendMethod( erm_invisible );
        } else if ( hasBlockItems && hasInline ) {
            if ( getParentNode()->getNodeId()==el_autoBoxing ) {
                // already autoboxed
                setRendMethod( erm_final );
            } else {
                // cleanup or autobox
                int i=getChildCount()-1;
                for ( ; i>=0; i-- ) {
                    ldomNode * node = getChildNode(i);

                    // DEBUG TEST
//                    if ( getParentNode()->getChildIndex( getDataIndex() )<0 ) {
//                        CRLog::error("Invalid parent->child relation for nodes %d->%d", getParentNode()->getDataIndex(), getDataIndex() );
//                    }

                    if ( isInlineNode(node) ) {
                        int j = i-1;
                        for ( ; j>=0; j-- ) {
                            node = getChildNode(j);
                            if ( !isInlineNode(node) )
                                break;
                        }
                        j++;
                        // j..i are inline
                        if ( j>0 || i<(int)getChildCount()-1 )
                            autoboxChildren( j, i );
                        i = j;
                    } else if ( i>0 ) {
                        ldomNode * prev = getChildNode(i-1);
                        if ( prev->isElement() && prev->getRendMethod()==erm_runin ) {
                            // autobox run-in
                            if ( getChildCount()!=2 ) {
                                CRLog::debug("Autoboxing run-in items");
                                autoboxChildren( i-1, i );
                            }
                            i--;
                        }
                    }
                }
                // check types after autobox
                detectChildTypes( this, hasBlockItems, hasInline );
                if ( hasInline ) {
                    // Final
                    setRendMethod( erm_final );
                } else {
                    // Block
                    setRendMethod( erm_block );
                }
            }
        }
    }
}
#endif

void ldomElementWriter::onBodyExit()
{
    if ( _isSection )
        updateTocItem();

#if BUILD_LITE!=1
    if ( !_document->isDefStyleSet() )
        return;
    if ( !_bodyEnterCalled ) {
        onBodyEnter();
    }
//    if ( _element->getStyle().isNull() ) {
//        lString16 path;
//        ldomNode * p = _element->getParentNode();
//        while (p) {
//            path = p->getNodeName() + L"/" + path;
//            p = p->getParentNode();
//        }
//        //CRLog::error("style not initialized for element 0x%04x %s path %s", _element->getDataIndex(), LCSTR(_element->getNodeName()), LCSTR(path));
//        crFatalError();
//    }
    _element->initNodeRendMethod();

    if ( _stylesheetIsSet )
        _document->getStyleSheet()->pop();
#endif
}

void ldomElementWriter::onText( const lChar16 * text, int len, lUInt32 )
{
    //logfile << "{t";
    {
        // normal mode: store text copy
        // add text node, if not first empty space string of block node
        if ( !_isBlock || _element->getChildCount()!=0 || !IsEmptySpace( text, len ) || (_flags&TXTFLG_PRE) )
            _element->insertChildText(lString16(text, len));
        else {
            //CRLog::trace("ldomElementWriter::onText: Ignoring first empty space of block item");
        }
    }
    //logfile << "}";
}

//#define DISABLE_STYLESHEET_REL
#if BUILD_LITE!=1
/// if stylesheet file name is set, and file is found, set stylesheet to its value
bool ldomNode::applyNodeStylesheet()
{
#ifndef DISABLE_STYLESHEET_REL
    if ( getNodeId()!=el_DocFragment || !hasAttribute(attr_StyleSheet) )
        return false;
    lString16 v = getAttributeValue(attr_StyleSheet);
    if ( v.empty() )
        return false;
    if ( getDocument()->getContainer().isNull() )
        return false;
    LVStreamRef cssStream = getDocument()->getContainer()->OpenStream(v.c_str(), LVOM_READ);
    if ( !cssStream.isNull() ) {
        lString16 css;
        css << LVReadTextFile( cssStream );
        if ( !css.empty() ) {
            getDocument()->_stylesheet.push();
            getDocument()->_stylesheet.parse(UnicodeToUtf8(css).c_str());
            return true;
        }
    }
#endif
    return false;
}
#endif

void ldomElementWriter::addAttribute( lUInt16 nsid, lUInt16 id, const wchar_t * value )
{
    getElement()->setAttributeValue(nsid, id, value);
#if BUILD_LITE!=1
    if ( id==attr_StyleSheet ) {
        _stylesheetIsSet = _element->applyNodeStylesheet();
    }
#endif
}

ldomElementWriter * ldomDocumentWriter::pop( ldomElementWriter * obj, lUInt16 id )
{
    //logfile << "{p";
    ldomElementWriter * tmp = obj;
    for ( ; tmp; tmp = tmp->_parent )
    {
        //logfile << "-";
        if (tmp->getElement()->getNodeId() == id)
            break;
    }
    //logfile << "1";
    if (!tmp)
    {
        //logfile << "-err}";
        return obj; // error!!!
    }
    ldomElementWriter * tmp2 = NULL;
    //logfile << "2";
    for ( tmp = obj; tmp; tmp = tmp2 )
    {
        //logfile << "-";
        tmp2 = tmp->_parent;
        bool stop = (tmp->getElement()->getNodeId() == id);
        ElementCloseHandler( tmp->getElement() );
        tmp->getElement()->persist();
        delete tmp;
        if ( stop )
            return tmp2;
    }
    /*
    logfile << "3 * ";
    logfile << (int)tmp << " - " << (int)tmp2 << " | cnt=";
    logfile << (int)tmp->getElement()->childCount << " - "
            << (int)tmp2->getElement()->childCount;
    */
    //logfile << "}";
    return tmp2;
}

ldomElementWriter::~ldomElementWriter()
{
    //CRLog::trace("~ldomElementWriter for element 0x%04x %s", _element->getDataIndex(), LCSTR(_element->getNodeName()));
    //getElement()->persist();
    onBodyExit();
}




/////////////////////////////////////////////////////////////////
/// ldomDocumentWriter

// overrides
void ldomDocumentWriter::OnStart(LVFileFormatParser * parser)
{
    //logfile << "ldomDocumentWriter::OnStart()\n";
    // add document root node
    //CRLog::trace("ldomDocumentWriter::OnStart()");
    if ( !_headerOnly )
        _stopTagId = 0xFFFE;
    else {
        _stopTagId = _document->getElementNameIndex(L"description");
        //CRLog::trace( "ldomDocumentWriter() : header only, tag id=%d", _stopTagId );
    }
    LVXMLParserCallback::OnStart( parser );
    _currNode = new ldomElementWriter(_document, 0, 0, NULL);
}

void ldomDocumentWriter::OnStop()
{
    //logfile << "ldomDocumentWriter::OnStop()\n";
    while (_currNode)
        _currNode = pop( _currNode, _currNode->getElement()->getNodeId() );
}

/// called after > of opening tag (when entering tag body)
void ldomDocumentWriter::OnTagBody()
{
    // init element style
    if ( _currNode ) {
        _currNode->onBodyEnter();
    }
}

ldomNode * ldomDocumentWriter::OnTagOpen( const lChar16 * nsname, const lChar16 * tagname )
{
    //logfile << "ldomDocumentWriter::OnTagOpen() [" << nsname << ":" << tagname << "]";
    //CRLog::trace("OnTagOpen(%s)", UnicodeToUtf8(lString16(tagname)).c_str());
    lUInt16 id = _document->getElementNameIndex(tagname);
    lUInt16 nsid = (nsname && nsname[0]) ? _document->getNsNameIndex(nsname) : 0;

    //if ( id==_stopTagId ) {
        //CRLog::trace("stop tag found, stopping...");
    //    _parser->Stop();
    //}
    _currNode = new ldomElementWriter( _document, nsid, id, _currNode );
    _flags = _currNode->getFlags();
    //logfile << " !o!\n";
    //return _currNode->getElement();
    return _currNode->getElement();
}

ldomDocumentWriter::~ldomDocumentWriter()
{
    while (_currNode)
        _currNode = pop( _currNode, _currNode->getElement()->getNodeId() );
#if BUILD_LITE!=1
    if ( _document->isDefStyleSet() ) {
        if ( _popStyleOnFinish )
            _document->getStyleSheet()->pop();
        _document->getRootNode()->initNodeStyle();
        _document->getRootNode()->initNodeFont();
        //if ( !_document->validateDocument() )
        //    CRLog::error("*** document style validation failed!!!");
        _document->updateRenderContext();
        _document->dumpStatistics();
    }
#endif
}

void ldomDocumentWriter::OnTagClose( const lChar16 *, const lChar16 * tagname )
{
    //logfile << "ldomDocumentWriter::OnTagClose() [" << nsname << ":" << tagname << "]";
    if (!_currNode)
    {
        _errFlag = true;
        //logfile << " !c-err!\n";
        return;
    }
    if ( tagname[0]=='l' && _currNode && !lStr_cmp(tagname, L"link") ) {
        // link node
        if ( _currNode && _currNode->getElement() && _currNode->getElement()->getNodeName()==L"link" &&
             _currNode->getElement()->getParentNode() && _currNode->getElement()->getParentNode()->getNodeName()==L"head" &&
             _currNode->getElement()->getAttributeValue(L"rel")==L"stylesheet" &&
             _currNode->getElement()->getAttributeValue(L"type")==L"text/css" ) {
            lString16 href = _currNode->getElement()->getAttributeValue(L"href");
            lString16 stylesheetFile = LVCombinePaths( _document->getCodeBase(), href );
            CRLog::debug("Internal stylesheet file: %s", LCSTR(stylesheetFile));
            _document->setDocStylesheetFileName(stylesheetFile);
            _document->applyDocumentStyleSheet();
        }
    }
    lUInt16 id = _document->getElementNameIndex(tagname);
    //lUInt16 nsid = (nsname && nsname[0]) ? _document->getNsNameIndex(nsname) : 0;
    _errFlag |= (id != _currNode->getElement()->getNodeId());
    _currNode = pop( _currNode, id );

    if ( _currNode )
        _flags = _currNode->getFlags();

    if ( id==_stopTagId ) {
        //CRLog::trace("stop tag found, stopping...");
        _parser->Stop();
    }

    if ( !lStr_cmp(tagname, L"stylesheet") ) {
        //CRLog::trace("</stylesheet> found");
#if BUILD_LITE!=1
        if ( !_popStyleOnFinish ) {
            //CRLog::trace("saving current stylesheet before applying of document stylesheet");
            _document->getStyleSheet()->push();
            _popStyleOnFinish = true;
            _document->applyDocumentStyleSheet();
        }
#endif
    }
    //logfile << " !c!\n";
}

void ldomDocumentWriter::OnAttribute( const lChar16 * nsname, const lChar16 * attrname, const lChar16 * attrvalue )
{
    //logfile << "ldomDocumentWriter::OnAttribute() [" << nsname << ":" << attrname << "]";
    lUInt16 attr_ns = (nsname && nsname[0]) ? _document->getNsNameIndex( nsname ) : 0;
    lUInt16 attr_id = (attrname && attrname[0]) ? _document->getAttrNameIndex( attrname ) : 0;
    _currNode->addAttribute( attr_ns, attr_id, attrvalue );

    //logfile << " !a!\n";
}

void ldomDocumentWriter::OnText( const lChar16 * text, int len, lUInt32 flags )
{
    //logfile << "ldomDocumentWriter::OnText() fpos=" << fpos;
    if (_currNode)
    {
        if ( (_flags & XML_FLAG_NO_SPACE_TEXT)
             && IsEmptySpace(text, len)  && !(flags & TXTFLG_PRE))
             return;
        if (_currNode->_allowText)
            _currNode->onText( text, len, flags );
    }
    //logfile << " !t!\n";
}

void ldomDocumentWriter::OnEncoding( const lChar16 *, const lChar16 *)
{
}

ldomDocumentWriter::ldomDocumentWriter(ldomDocument * document, bool headerOnly)
    : _document(document), _currNode(NULL), _errFlag(false), _headerOnly(headerOnly), _popStyleOnFinish(false), _flags(0)
{
    _stopTagId = 0xFFFE;
    IS_FIRST_BODY = true;

#if BUILD_LITE!=1
    if ( _document->isDefStyleSet() ) {
        _document->getRootNode()->initNodeStyle();
        _document->getRootNode()->setRendMethod(erm_block);
    }
#endif

    //CRLog::trace("ldomDocumentWriter() headerOnly=%s", _headerOnly?"true":"false");
}








bool FindNextNode( ldomNode * & node, ldomNode * root )
{
    if ( node->getChildCount()>0 ) {
        // first child
        node = node->getChildNode(0);
        return true;
    }
    if (node->isRoot() || node == root )
        return false; // root node reached
    int index = node->getNodeIndex();
    ldomNode * parent = node->getParentNode();
    while (parent)
    {
        if ( index < (int)parent->getChildCount()-1 ) {
            // next sibling
            node = parent->getChildNode( index + 1 );
            return true;
        }
        if (parent->isRoot() || parent == root )
            return false; // root node reached
        // up one level
        index = parent->getNodeIndex();
        parent = parent->getParentNode();
    }
    //if ( node->getNodeType() == LXML_TEXT_NODE )
    return false;
}

// base64 decode table
static const signed char base64_decode_table[] = {
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //0..15
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, //16..31   10
   -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63, //32..47   20
   52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1, //48..63   30
   -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, //64..79   40
   15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1, //80..95   50
   -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40, //INDEX2..111  60
   41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1  //112..127 70
};

#define BASE64_BUF_SIZE 128
class LVBase64NodeStream : public LVNamedStream
{
private:
    ldomNode *  m_elem;
    ldomNode *  m_curr_node;
    lString16   m_curr_text;
    int         m_text_pos;
    lvsize_t    m_size;
    lvpos_t     m_pos;

    int         m_iteration;
    lUInt32     m_value;

    lUInt8      m_bytes[BASE64_BUF_SIZE];
    int         m_bytes_count;
    int         m_bytes_pos;

    int readNextBytes()
    {
        int bytesRead = 0;
        bool flgEof = false;
        while ( bytesRead == 0 && !flgEof )
        {
            while ( m_text_pos >= (int)m_curr_text.length() )
            {
                if ( !findNextTextNode() )
                    return bytesRead;
            }
            int len = m_curr_text.length();
            const lChar16 * txt = m_curr_text.c_str();
            for ( ; m_text_pos<len && m_bytes_count < BASE64_BUF_SIZE - 3; m_text_pos++ )
            {
                lChar16 ch = txt[ m_text_pos ];
                if ( ch < 128 )
                {
                    if ( ch == '=' )
                    {
                        // end of stream
                        if ( m_iteration == 2 )
                        {
                            m_bytes[m_bytes_count++] = (lUInt8)((m_value>>4) & 0xFF);
                            bytesRead++;
                        }
                        else if ( m_iteration == 3 )
                        {
                            m_bytes[m_bytes_count++] = (lUInt8)((m_value>>10) & 0xFF);
                            m_bytes[m_bytes_count++] = (lUInt8)((m_value>>2) & 0xFF);
                            bytesRead += 2;
                        }
                        // stop!!!
                        m_text_pos--;
                        flgEof = true;
                        break;
                    }
                    else
                    {
                        int k = base64_decode_table[ch];
                        if ( !(k & 0x80) )
                        {
                            // next base-64 digit
                            m_value = (m_value << 6) | (k);
                            m_iteration++;
                            if (m_iteration==4)
                            {
                                //
                                m_bytes[m_bytes_count++] = (lUInt8)((m_value>>16) & 0xFF);
                                m_bytes[m_bytes_count++] = (lUInt8)((m_value>>8) & 0xFF);
                                m_bytes[m_bytes_count++] = (lUInt8)((m_value>>0) & 0xFF);
                                m_iteration = 0;
                                m_value = 0;
                                bytesRead+=3;
                            }
                        }
                    }
                }
            }
        }
        return bytesRead;
    }

    bool findNextTextNode()
    {
        while ( FindNextNode( m_curr_node, m_elem ) ) {
            if ( m_curr_node->isText() ) {
                m_curr_text = m_curr_node->getText();
                m_text_pos = 0;
                return true;
            }
        }
        return false;
    }

    int bytesAvailable() { return m_bytes_count - m_bytes_pos; }

    bool rewind()
    {
        m_curr_node = m_elem;
        m_pos = 0;
        m_bytes_count = 0;
        m_bytes_pos = 0;
        m_iteration = 0;
        m_value = 0;
        return findNextTextNode();
    }

    bool skip( lvsize_t count )
    {
        while ( count )
        {
            if ( m_bytes_pos >= m_bytes_count )
            {
                m_bytes_pos = 0;
                m_bytes_count = 0;
                int bytesRead = readNextBytes();
                if ( bytesRead == 0 )
                    return false;
            }
            int diff = (int) (m_bytes_count - m_bytes_pos);
            if (diff > (int)count)
                diff = (int)count;
            m_pos += diff;
            count -= diff;
        }
        return true;
    }

public:
    virtual ~LVBase64NodeStream() { }
    LVBase64NodeStream( ldomNode * element )
        : m_elem(element), m_curr_node(element), m_size(0), m_pos(0)
    {
        // calculate size
        rewind();
        m_size = bytesAvailable();
        for (;;) {
            int bytesRead = readNextBytes();
            if ( !bytesRead )
                break;
            m_bytes_count = 0;
            m_bytes_pos = 0;
            m_size += bytesRead;
        }
        // rewind
        rewind();
    }
    virtual bool Eof()
    {
        return m_pos >= m_size;
    }
    virtual lvsize_t  GetSize()
    {
        return m_size;
    }

    virtual lvpos_t GetPos()
    {
        return m_pos;
    }

    virtual lverror_t GetPos( lvpos_t * pos )
    {
        if (pos)
            *pos = m_pos;
        return LVERR_OK;
    }

    virtual lverror_t Seek(lvoffset_t offset, lvseek_origin_t origin, lvpos_t* newPos)
    {
        lvpos_t npos = 0;
        lvpos_t currpos = GetPos();
        switch (origin) {
        case LVSEEK_SET:
            npos = offset;
            break;
        case LVSEEK_CUR:
            npos = currpos + offset;
            break;
        case LVSEEK_END:
            npos = m_size + offset;
            break;
        }
        if (npos > m_size)
            return LVERR_FAIL;
        if ( npos != currpos )
        {
            if (npos < currpos)
            {
                if ( !rewind() || !skip(npos) )
                    return LVERR_FAIL;
            }
            else
            {
                skip( npos - currpos );
            }
        }
        if (newPos)
            *newPos = npos;
        return LVERR_OK;
    }
    virtual lverror_t Write(const void*, lvsize_t, lvsize_t*)
    {
        return LVERR_NOTIMPL;
    }
    virtual lverror_t Read(void* buf, lvsize_t size, lvsize_t* pBytesRead)
    {
        lvsize_t bytesRead = 0;
        //fprintf( stderr, "Read()\n" );

        lUInt8 * out = (lUInt8 *)buf;

        while (size>0)
        {
            int sz = bytesAvailable();
            if (!sz) {
                m_bytes_pos = m_bytes_count = 0;
                sz = readNextBytes();
                if (!sz) {
                    if ( !bytesRead || m_pos!=m_size) //
                        return LVERR_FAIL;
                    break;
                }
            }
            if (sz>(int)size)
                sz = (int)size;
            for (int i=0; i<sz; i++)
                *out++ = m_bytes[m_bytes_pos++];
            size -= sz;
            bytesRead += sz;
            m_pos += sz;
        }

        if (pBytesRead)
            *pBytesRead = bytesRead;
        //fprintf( stderr, "    %d bytes read...\n", (int)bytesRead );
        return LVERR_OK;
    }
    virtual lverror_t SetSize(lvsize_t)
    {
        return LVERR_NOTIMPL;
    }
};

img_scaling_option_t::img_scaling_option_t()
{
    mode = (MAX_IMAGE_SCALE_MUL>1) ? (ARBITRARY_IMAGE_SCALE_ENABLED==1 ? IMG_FREE_SCALING : IMG_INTEGER_SCALING) : IMG_NO_SCALE;
    max_scale = (MAX_IMAGE_SCALE_MUL>1) ? MAX_IMAGE_SCALE_MUL : 1;
}

img_scaling_options_t::img_scaling_options_t()
{
    img_scaling_option_t option;
    zoom_in_inline = option;
    zoom_in_block = option;
    zoom_out_inline = option;
    zoom_out_block = option;
}

#define FONT_SIZE_BIG 32
#define FONT_SIZE_VERY_BIG 50
static bool updateScalingOption( img_scaling_option_t & v, CRPropRef props, int fontSize, bool zoomin, bool isInline )
{
    lString8 propName("crengine.image.scaling.");
    propName << (zoomin ? "zoomin." : "zoomout.");
    propName << (isInline ? "inline." : "block.");
    lString8 propNameMode = propName + "mode";
    lString8 propNameScale = propName + "scale";
    img_scaling_option_t def;
    int currMode = props->getIntDef(propNameMode.c_str(), (int)def.mode);
    int currScale = props->getIntDef(propNameScale.c_str(), (int)def.max_scale);
    if ( currScale==0 ) {
        if ( fontSize>=FONT_SIZE_VERY_BIG )
            currScale = 3;
        else if ( fontSize>=FONT_SIZE_BIG )
            currScale = 2;
        else
            currScale = 1;
    }
    if ( currScale==1 )
        currMode = 0;
    int updated = false;
    if ( v.max_scale!=currScale ) {
        updated = true;
        v.max_scale = currScale;
    }
    if ( v.mode!=(img_scaling_mode_t)currMode ) {
        updated = true;
        v.mode = (img_scaling_mode_t)currMode;
    }
    props->setIntDef(propNameMode.c_str(), currMode);
    props->setIntDef(propNameScale.c_str(), currScale);
    return updated;
}

/// returns true if any changes occured
bool img_scaling_options_t::update( CRPropRef props, int fontSize )
{
    bool updated = false;
    updated = updateScalingOption( zoom_in_inline, props, fontSize, true, true ) || updated;
    updated = updateScalingOption( zoom_in_block, props, fontSize, true, false ) || updated;
    updated = updateScalingOption( zoom_out_inline, props, fontSize, false, true ) || updated;
    updated = updateScalingOption( zoom_out_block, props, fontSize, false, false ) || updated;
    return updated;
}

xpath_step_t ParseXPathStep( const lChar16 * &path, lString16 & name, int & index )
{
    int pos = 0;
    const lChar16 * s = path;
    //int len = path.GetLength();
    name.clear();
    index = -1;
    int flgPrefix = 0;
    if (s && s[pos]) {
        lChar16 ch = s[pos];
        // prefix: none, '/' or '.'
        if (ch=='/') {
            flgPrefix = 1;
            ch = s[++pos];
        } else if (ch=='.') {
            flgPrefix = 2;
            ch = s[++pos];
        }
        int nstart = pos;
        if (ch>='0' && ch<='9') {
            // node or point index
            pos++;
            while (s[pos]>='0' && s[pos]<='9')
                pos++;
            if (s[pos] && s[pos!='/'] && s[pos]!='.')
                return xpath_step_error;
            lString16 sindex( path+nstart, pos-nstart );
            index = sindex.atoi();
            if (index<((flgPrefix==2)?0:1))
                return xpath_step_error;
            path += pos;
            return (flgPrefix==2) ? xpath_step_point : xpath_step_nodeindex;
        }
        while (s[pos] && s[pos]!='[' && s[pos]!='/' && s[pos]!='.')
            pos++;
        if (pos==nstart)
            return xpath_step_error;
        name = lString16( path+ nstart, pos-nstart );
        if (s[pos]=='[') {
            // index
            pos++;
            int istart = pos;
            while (s[pos] && s[pos]!=']' && s[pos]!='/' && s[pos]!='.')
                pos++;
            if (!s[pos] || pos==istart)
                return xpath_step_error;

            lString16 sindex( path+istart, pos-istart );
            index = sindex.atoi();
            pos++;
        }
        if (!s[pos] || s[pos]=='/' || s[pos]=='.') {
            path += pos;
            return (name==L"text()") ? xpath_step_text : xpath_step_element; // OK!
        }
        return xpath_step_error; // error
    }
    return xpath_step_error;
}


/// get pointer for relative path
ldomXPointer ldomXPointer::relative( lString16 relativePath )
{
    return getDocument()->createXPointer( getNode(), relativePath );
}
/// create xpointer from pointer string
ldomXPointer ldomDocument::createXPointer( const lString16 & xPointerStr )
{
    if ( xPointerStr[0]=='#' ) {
        lString16 id = xPointerStr.substr(1);
        lUInt16 idid = getAttrValueIndex(id.c_str());
        lInt32 nodeIndex;
        if ( _idNodeMap.get(idid, nodeIndex) ) {
            ldomNode * node = getTinyNode(nodeIndex);
            if ( node && node->isElement() ) {
                return ldomXPointer(node, -1);
            }
        }
        return ldomXPointer();
    }
    return createXPointer( getRootNode(), xPointerStr );
}

#if BUILD_LITE!=1

/// return parent final node, if found
ldomNode * ldomXPointer::getFinalNode() const
{
    ldomNode * node = getNode();
    for (;;) {
        if ( !node )
            return NULL;
        if ( node->getRendMethod()==erm_final )
            return node;
        node = node->getParentNode();
    }
}

/// create xpointer from doc point
ldomXPointer ldomDocument::createXPointer( lvPoint pt, int direction )
{
    //
    ldomXPointer ptr;
    if ( !getRootNode() )
        return ptr;
    ldomNode * finalNode = getRootNode()->elementFromPoint( pt, direction );
    if ( !finalNode ) {
        if ( pt.y >= getFullHeight()) {
            ldomNode * node = getRootNode()->getLastTextChild();
            return ldomXPointer(node,node ? node->getText().length() : 0);
        } else if ( pt.y <= 0 ) {
            ldomNode * node = getRootNode()->getFirstTextChild();
            return ldomXPointer(node, 0);
        }
        CRLog::trace("not final node");
        return ptr;
    }
    lvRect rc;
    finalNode->getAbsRect( rc );
    //CRLog::debug("ldomDocument::createXPointer point = (%d, %d), finalNode %08X rect = (%d,%d,%d,%d)", pt.x, pt.y, (lUInt32)finalNode, rc.left, rc.top, rc.right, rc.bottom );
    pt.x -= rc.left;
    pt.y -= rc.top;
    //if ( !r )
    //    return ptr;
    if ( finalNode->getRendMethod() != erm_final && finalNode->getRendMethod() !=  erm_list_item) {
        // not final, use as is
        if ( pt.y < (rc.bottom + rc.top) / 2 )
            return ldomXPointer( finalNode, 0 );
        else
            return ldomXPointer( finalNode, finalNode->getChildCount() );
    }
    // final, format and search
    LFormattedTextRef txtform;
    {
        RenderRectAccessor r( finalNode );
        finalNode->renderFinalBlock( txtform, &r, r.getWidth() );
    }
    int lcount = txtform->GetLineCount();
    for ( int l = 0; l<lcount; l++ ) {
        const formatted_line_t * frmline = txtform->GetLineInfo(l);
        if ( pt.y >= (int)(frmline->y + frmline->height) && l<lcount-1 )
            continue;
        //CRLog::debug("  point (%d, %d) line found [%d]: (%d..%d)", pt.x, pt.y, l, frmline->y, frmline->y+frmline->height);
        // found line, searching for word
        int wc = (int)frmline->word_count;
        int x = pt.x - frmline->x;
        for ( int w=0; w<wc; w++ ) {
            const formatted_word_t * word = &frmline->words[w];
            if ( x < word->x + word->width || w==wc-1 ) {
                const src_text_fragment_t * src = txtform->GetSrcInfo(word->src_text_index);
                //CRLog::debug(" word found [%d]: x=%d..%d, start=%d, len=%d  %08X", w, word->x, word->x + word->width, word->t.start, word->t.len, src->object);
                // found word, searching for letters
                ldomNode * node = (ldomNode *)src->object;
                if ( !node )
                    continue;
                if ( src->flags & LTEXT_SRC_IS_OBJECT ) {
                    // object (image)
#if 1
                    // return image object itself
                    return ldomXPointer(node, 0);
#else
                    return ldomXPointer( node->getParentNode(),
                        node->getNodeIndex() + (( x < word->x + word->width/2 ) ? 0 : 1) );
#endif
                }
                LVFont * font = (LVFont *) src->t.font;
                lUInt16 w[512];
                lUInt8 flg[512];

                lString16 str = node->getText();
                font->measureText( str.c_str()+word->t.start, word->t.len, w, flg, word->width+50, '?', src->letter_spacing);
                for ( int i=0; i<word->t.len; i++ ) {
                    int xx = ( i>0 ) ? (w[i-1] + w[i])/2 : w[i]/2;
                    if ( x < word->x + xx ) {
                        return ldomXPointer( node, src->t.offset + word->t.start + i );
                    }
                }
                return ldomXPointer( node, src->t.offset + word->t.start + word->t.len );
            }
        }
    }
    return ptr;
}

/// returns coordinates of pointer inside formatted document
lvPoint ldomXPointer::toPoint() const
{
    lvRect rc;
    if ( !getRect( rc ) )
        return lvPoint(-1, -1);
    return rc.topLeft();
}

/// returns caret rectangle for pointer inside formatted document
bool ldomXPointer::getRect(lvRect & rect) const
{
    //CRLog::trace("ldomXPointer::getRect()");
    if ( isNull() )
        return false;
    ldomNode * p = isElement() ? getNode() : getNode()->getParentNode();
    ldomNode * p0 = p;
    ldomNode * finalNode = NULL;
    if ( !p ) {
        //CRLog::trace("ldomXPointer::getRect() - p==NULL");
    }
    //printf("getRect( p=%08X type=%d )\n", (unsigned)p, (int)p->getNodeType() );
    if ( !p->getDocument() ) {
        //CRLog::trace("ldomXPointer::getRect() - p->getDocument()==NULL");
    }
    ldomNode * mainNode = p->getDocument()->getRootNode();
    for ( ; p; p = p->getParentNode() ) {
        if ( p->getRendMethod() == erm_final ) {
            finalNode = p; // found final block
        } else if ( p->getRendMethod() == erm_invisible ) {
            return false; // invisible !!!
        }
        if ( p==mainNode )
            break;
    }

    if ( finalNode==NULL ) {
        lvRect rc;
        p0->getAbsRect( rc );
        CRLog::debug("node w/o final parent: %d..%d", rc.top, rc.bottom);

    }

    if ( finalNode!=NULL ) {
        lvRect rc;
        finalNode->getAbsRect( rc );
        RenderRectAccessor r( finalNode );
        //if ( !r )
        //    return false;
        LFormattedTextRef txtform;
        finalNode->renderFinalBlock( txtform, &r, r.getWidth() );

        ldomNode * node = getNode();
        int offset = getOffset();
////        ldomXPointerEx xp(node, offset);
////        if ( !node->isText() ) {
////            //ldomXPointerEx xp(node, offset);
////            xp.nextVisibleText();
////            node = xp.getNode();
////            offset = xp.getOffset();
////        }
//        if ( node->isElement() ) {
//            if ( offset>=0 ) {
//                //
//                if ( offset>= (int)node->getChildCount() ) {
//                    node = node->getLastTextChild();
//                    if ( node )
//                        offset = node->getText().length();
//                    else
//                        return false;
//                } else {
//                    for ( int ci=offset; ci<(int)node->getChildCount(); ci++ ) {
//                        ldomNode * child = node->getChildNode( offset );
//                        ldomNode * txt = txt = child->getFirstTextChild( true );
//                        if ( txt ) {
//                            node = txt;
////                            lString16 s = txt->getText();
////                            CRLog::debug("text: [%d] '%s'", s.length(), LCSTR(s));
//                            break;
//                        }
//                    }
//                    if ( !node->isText() )
//                        return false;
//                    offset = 0;
//                }
//            }
//        }

        // text node
        int srcIndex = -1;
        int srcLen = -1;
        int lastIndex = -1;
        int lastLen = -1;
        int lastOffset = -1;
        ldomXPointerEx xp(node, offset);
        for ( int i=0; i<txtform->GetSrcCount(); i++ ) {
            const src_text_fragment_t * src = txtform->GetSrcInfo(i);
            bool isObject = (src->flags&LTEXT_SRC_IS_OBJECT)!=0;
            if ( src->object == node ) {
                srcIndex = i;
                srcLen = isObject ? 0 : src->t.len;
                break;
            }
            lastIndex = i;
            lastLen =  isObject ? 0 : src->t.len;
            lastOffset = isObject ? 0 : src->t.offset;
            ldomXPointerEx xp2((ldomNode*)src->object, lastOffset);
            if ( xp2.compare(xp)>0 ) {
                srcIndex = i;
                srcLen = lastLen;
                offset = lastOffset;
                break;
            }
        }
        if ( srcIndex == -1 ) {
            if ( lastIndex<0 )
                return false;
            srcIndex = lastIndex;
            srcLen = lastLen;
            offset = lastOffset;
        }
        for ( int l = 0; l<txtform->GetLineCount(); l++ ) {
            const formatted_line_t * frmline = txtform->GetLineInfo(l);
            for ( int w=0; w<(int)frmline->word_count; w++ ) {
                const formatted_word_t * word = &frmline->words[w];
                bool lastWord = (l==txtform->GetLineCount()-1 && w==frmline->word_count-1);
                if ( word->src_text_index>=srcIndex || lastWord ) {
                    // found word from same src line
                    if ( word->src_text_index>srcIndex || offset<=word->t.start ) {
                        // before this word
                        rect.left = word->x + rc.left + frmline->x;
                        //rect.top = word->y + rc.top + frmline->y + frmline->baseline;
                        rect.top = rc.top + frmline->y;
                        rect.right = rect.left + 1;
                        rect.bottom = rect.top + frmline->height;
                        return true;
                    } else if ( (offset<word->t.start+word->t.len) || (offset==srcLen && offset==word->t.start+word->t.len) ) {
                        // pointer inside this word
                        LVFont * font = (LVFont *) txtform->GetSrcInfo(srcIndex)->t.font;
                        lUInt16 w[512];
                        lUInt8 flg[512];
                        lString16 str = node->getText();
                        font->measureText( str.c_str()+word->t.start, offset - word->t.start, w, flg, word->width+50, '?', txtform->GetSrcInfo(srcIndex)->letter_spacing);
                        int chx = w[ offset - word->t.start - 1 ];
                        rect.left = word->x + chx + rc.left + frmline->x;
                        //rect.top = word->y + rc.top + frmline->y + frmline->baseline;
                        rect.top = rc.top + frmline->y;
                        rect.right = rect.left + 1;
                        rect.bottom = rect.top + frmline->height;
                        return true;
                    } else if (lastWord) {
                        // after last word
                        rect.left = word->x + rc.left + frmline->x + word->width;
                        //rect.top = word->y + rc.top + frmline->y + frmline->baseline;
                        rect.top = rc.top + frmline->y;
                        rect.right = rect.left + 1;
                        rect.bottom = rect.top + frmline->height;
                        return true;
                    }
                }
            }
        }
        return false;
    } else {
        // no base final node, using blocks
        //lvRect rc;
        ldomNode * node = getNode();
        int offset = getOffset();
        if ( offset<0 || node->getChildCount()==0 ) {
            node->getAbsRect( rect );
            return true;
            //return rc.topLeft();
        }
        if ( offset < (int)node->getChildCount() ) {
            node->getChildNode(offset)->getAbsRect( rect );
            return true;
            //return rc.topLeft();
        }
        node->getChildNode(node->getChildCount()-1)->getAbsRect( rect );
        return true;
        //return rc.bottomRight();
    }
}
#endif

/// create xpointer from relative pointer string
ldomXPointer ldomDocument::createXPointer( ldomNode * baseNode, const lString16 & xPointerStr )
{
    //CRLog::trace( "ldomDocument::createXPointer(%s)", UnicodeToUtf8(xPointerStr).c_str() );
    if ( xPointerStr.empty() )
        return ldomXPointer();
    const lChar16 * str = xPointerStr.c_str();
    int index = -1;
    ldomNode * currNode = baseNode;
    lString16 name;
    lString8 ptr8 = UnicodeToUtf8(xPointerStr);
    //const char * ptr = ptr8.c_str();
    xpath_step_t step_type;

    while ( *str ) {
        //CRLog::trace( "    %s", UnicodeToUtf8(lString16(str)).c_str() );
        step_type = ParseXPathStep( str, name, index );
        //CRLog::trace( "        name=%s index=%d", UnicodeToUtf8(lString16(name)).c_str(), index );
        switch (step_type ) {
        case xpath_step_error:
            // error
            //CRLog::trace("    xpath_step_error");
            return ldomXPointer();
        case xpath_step_element:
            // element of type 'name' with 'index'        /elemname[N]/
            {
                lUInt16 id = getElementNameIndex( name.c_str() );
                ldomNode * foundItem = NULL;
                int foundCount = 0;
                for (unsigned i=0; i<currNode->getChildCount(); i++) {
                    ldomNode * p = currNode->getChildNode(i);
                    //CRLog::trace( "        node[%d] = %d %s", i, p->getNodeId(), LCSTR(p->getNodeName()) );
                    if ( p && p->isElement() && p->getNodeId()==id ) {
                        foundCount++;
                        if ( foundCount==index || index==-1 ) {
                            foundItem = p;
                            break; // DON'T CHECK WHETHER OTHER ELEMENTS EXIST
                        }
                    }
                }
                if ( foundItem==NULL || (index==-1 && foundCount>1) ) {
                    //CRLog::trace("    Element %d is not found. foundCount=%d", id, foundCount);
                    return ldomXPointer(); // node not found
                }
                // found element node
                currNode = foundItem;
            }
            break;
        case xpath_step_text:
            // text node with 'index'                     /text()[N]/
            {
                ldomNode * foundItem = NULL;
                int foundCount = 0;
                for (unsigned i=0; i<currNode->getChildCount(); i++) {
                    ldomNode * p = currNode->getChildNode(i);
                    if ( p->isText() ) {
                        foundCount++;
                        if ( foundCount==index || index==-1 ) {
                            foundItem = p;
                        }
                    }
                }
                if ( foundItem==NULL || (index==-1 && foundCount>1) )
                    return ldomXPointer(); // node not found
                // found text node
                currNode = foundItem;
            }
            break;
        case xpath_step_nodeindex:
            // node index                                 /N/
            if ( index<=0 || index>(int)currNode->getChildCount() )
                return ldomXPointer(); // node not found: invalid index
            currNode = currNode->getChildNode( index-1 );
            break;
        case xpath_step_point:
            // point index                                .N
            if (*str)
                return ldomXPointer(); // not at end of string
            if ( currNode->isElement() ) {
                // element point
                if ( index<0 || index>(int)currNode->getChildCount() )
                    return ldomXPointer();
                return ldomXPointer(currNode, index);
            } else {
                // text point
                if ( index<0 || index>(int)currNode->getText().length() )
                    return ldomXPointer();
                return ldomXPointer(currNode, index);
            }
            break;
        }
    }
    return ldomXPointer( currNode, -1 ); // XPath: index==-1
}

/// returns XPath segment for this element relative to parent element (e.g. "p[10]")
lString16 ldomNode::getXPathSegment()
{
    if ( isNull() || isRoot() )
        return lString16::empty_str;
    ldomNode * parent = getParentNode();
    int cnt = parent->getChildCount();
    int index = 0;
    if ( isElement() ) {
        int id = getNodeId();
        for ( int i=0; i<cnt; i++ ) {
            ldomNode * node = parent->getChildNode(i);
            if ( node == this ) {
                return getNodeName() + L"[" + lString16::itoa(index+1) + L"]";
            }
            if ( node->isElement() && node->getNodeId()==id )
                index++;
        }
    } else {
        for ( int i=0; i<cnt; i++ ) {
            ldomNode * node = parent->getChildNode(i);
            if ( node == this ) {
                return L"text()[" + lString16::itoa(index+1) + L"]";
            }
            if ( node->isText() )
                index++;
        }
    }
    return lString16::empty_str;
}

lString16 ldomXPointer::toString()
{
    lString16 path;
    if ( isNull() )
        return path;
    ldomNode * node = getNode();
    int offset = getOffset();
    if ( offset >= 0 ) {
        path << L"." << lString16::itoa(offset);
    }
    ldomNode * p = node;
    ldomNode * mainNode = node->getDocument()->getRootNode();
    while (p && p!=mainNode) {
        ldomNode * parent = p->getParentNode();
        if ( p->isElement() ) {
            // element
            lString16 name = p->getNodeName();
            int id = p->getNodeId();
            if ( !parent )
                return lString16(L"/") + name + path;
            int index = -1;
            int count = 0;
            for ( unsigned i=0; i<parent->getChildCount(); i++ ) {
                ldomNode * node = parent->getChildElementNode( i, id );
                if ( node ) {
                    count++;
                    if ( node==p )
                        index = count;
                }
            }
            if ( count>1 )
                path = lString16(L"/") + name + L"[" + lString16::itoa(index) + L"]" + path;
            else
                path = lString16(L"/") + name + path;
        } else {
            // text
            if ( !parent )
                return lString16(L"/text()") + path;
            int index = -1;
            int count = 0;
            for ( unsigned i=0; i<parent->getChildCount(); i++ ) {
                ldomNode * node = parent->getChildNode( i );
                if ( node->isText() ) {
                    count++;
                    if ( node==p )
                        index = count;
                }
            }
            if ( count>1 )
                path = lString16(L"/text()") + L"[" + lString16::itoa(index) + L"]" + path;
            else
                path = lString16(L"/text()") + path;
        }
        p = parent;
    }
    return path;
}

#if BUILD_LITE!=1
int ldomDocument::getFullHeight()
{
    RenderRectAccessor rd( this->getRootNode() );
    return rd.getHeight() + rd.getY();
}
#endif




lString16 extractDocAuthors( ldomDocument * doc, lString16 delimiter, bool shortMiddleName )
{
    if ( delimiter.empty() )
        delimiter = L", ";
    lString16 authors;
    for ( int i=0; i<16; i++) {
        lString16 path = lString16(L"/FictionBook/description/title-info/author[") + lString16::itoa(i+1) + L"]";
        ldomXPointer pauthor = doc->createXPointer(path);
        if ( !pauthor ) {
            //CRLog::trace( "xpath not found: %s", UnicodeToUtf8(path).c_str() );
            break;
        }
        lString16 firstName = pauthor.relative( L"/first-name" ).getText();
        lString16 lastName = pauthor.relative( L"/last-name" ).getText();
        lString16 middleName = pauthor.relative( L"/middle-name" ).getText();
        lString16 author = firstName;
        if ( !author.empty() )
            author += L" ";
        if ( !middleName.empty() )
            author += shortMiddleName ? lString16(middleName, 0, 1) + L"." : middleName;
        if ( !lastName.empty() && !author.empty() )
            author += L" ";
        author += lastName;
        if ( !authors.empty() )
            authors += delimiter;
        authors += author;
    }
    return authors;
}

lString16 extractDocTitle( ldomDocument * doc )
{
    return doc->createXPointer(L"/FictionBook/description/title-info/book-title").getText();
}

lString16 extractDocSeries( ldomDocument * doc, int * pSeriesNumber )
{
    lString16 res;
    ldomNode * series = doc->createXPointer(L"/FictionBook/description/title-info/sequence").getNode();
    if ( series ) {
        lString16 sname = series->getAttributeValue( attr_name );
        lString16 snumber = series->getAttributeValue( attr_number );
        if ( !sname.empty() ) {
            if ( pSeriesNumber ) {
                *pSeriesNumber = snumber.atoi();
                res = sname;
            } else {
                res << L"(" << sname;
                if ( !snumber.empty() )
                    res << L" #" << snumber << L")";
            }
        }
    }
    return res;
}

void ldomXPointerEx::initIndex()
{
    int m[MAX_DOM_LEVEL];
    ldomNode * p = getNode();
    _level = 0;
    while ( p ) {
        m[_level] = p->getNodeIndex();
        _level++;
        p = p->getParentNode();
    }
    for ( int i=0; i<_level; i++ ) {
        _indexes[ i ] = m[ _level - i - 1 ];
    }
}

/// move to sibling #
bool ldomXPointerEx::sibling( int index )
{
    if ( _level <= 1 )
        return false;
    ldomNode * p = getNode()->getParentNode();
    if ( !p || index < 0 || index >= (int)p->getChildCount() )
        return false;
    setNode( p->getChildNode( index ) );
    setOffset(0);
    _indexes[ _level-1 ] = index;
    return true;
}

/// move to next sibling
bool ldomXPointerEx::nextSibling()
{
    return sibling( _indexes[_level-1] + 1 );
}

/// move to previous sibling
bool ldomXPointerEx::prevSibling()
{
    if ( _level <= 1 )
        return false;
    return sibling( _indexes[_level-1] - 1 );
}

/// move to next sibling element
bool ldomXPointerEx::nextSiblingElement()
{
    if ( _level <= 1 )
        return false;
    ldomNode * node = getNode();
    ldomNode * p = node->getParentNode();
    for ( int i=_indexes[_level-1] + 1; i<(int)p->getChildCount(); i++ ) {
        if ( p->getChildNode( i )->isElement() )
            return sibling( i );
    }
    return false;
}

/// move to previous sibling element
bool ldomXPointerEx::prevSiblingElement()
{
    if ( _level <= 1 )
        return false;
    ldomNode * node = getNode();
    ldomNode * p = node->getParentNode();
    for ( int i=_indexes[_level-1] - 1; i>=0; i-- ) {
        if ( p->getChildNode( i )->isElement() )
            return sibling( i );
    }
    return false;
}

/// move to parent
bool ldomXPointerEx::parent()
{
    if ( _level<=1 )
        return false;
    setNode( getNode()->getParentNode() );
    setOffset(0);
    _level--;
    return true;
}

/// move to child #
bool ldomXPointerEx::child( int index )
{
    if ( _level >= MAX_DOM_LEVEL )
        return false;
    int count = getNode()->getChildCount();
    if ( index<0 || index>=count )
        return false;
    _indexes[ _level++ ] = index;
    setNode( getNode()->getChildNode( index ) );
    setOffset(0);
    return true;
}

/// compare two pointers, returns -1, 0, +1
int ldomXPointerEx::compare( const ldomXPointerEx& v ) const
{
    int i;
    for ( i=0; i<_level && i<v._level; i++ ) {
        if ( _indexes[i] < v._indexes[i] )
            return -1;
        if ( _indexes[i] > v._indexes[i] )
            return 1;
    }
    if ( _level < v._level ) {
        return -1;
//        if ( getOffset() < v._indexes[i] )
//            return -1;
//        if ( getOffset() > v._indexes[i] )
//            return 1;
//        return -1;
    }
    if ( _level > v._level ) {
        if ( _indexes[i] < v.getOffset() )
            return -1;
        if ( _indexes[i] > v.getOffset() )
            return 1;
        return 1;
    }
    if ( getOffset() < v.getOffset() )
        return -1;
    if ( getOffset() > v.getOffset() )
        return 1;
    return 0;
}

/// calls specified function recursively for all elements of DOM tree
void ldomXPointerEx::recurseElements( void (*pFun)( ldomXPointerEx & node ) )
{
    if ( !isElement() )
        return;
    pFun( *this );
    if ( child( 0 ) ) {
        do {
            recurseElements( pFun );
        } while ( nextSibling() );
        parent();
    }
}

/// calls specified function recursively for all nodes of DOM tree
void ldomXPointerEx::recurseNodes( void (*pFun)( ldomXPointerEx & node ) )
{
    if ( !isElement() )
        return;
    pFun( *this );
    if ( child( 0 ) ) {
        do {
            recurseElements( pFun );
        } while ( nextSibling() );
        parent();
    }
}

/// returns true if this interval intersects specified interval
bool ldomXRange::checkIntersection( ldomXRange & v )
{
    if ( isNull() || v.isNull() )
        return false;
    if ( _end.compare( v._start ) < 0 )
        return false;
    if ( _start.compare( v._end ) > 0 )
        return false;
    return true;
}

/// create list by filtering existing list, to get only values which intersect filter range
ldomXRangeList::ldomXRangeList( ldomXRangeList & srcList, ldomXRange & filter )
{
    for ( int i=0; i<srcList.length(); i++ ) {
        if ( srcList[i]->checkIntersection( filter ) )
            LVPtrVector<ldomXRange>::add( new ldomXRange( *srcList[i] ) );
    }
}

/// copy constructor of full node range
ldomXRange::ldomXRange( ldomNode * p )
: _start( p, 0 ), _end( p, p->isText() ? p->getText().length() : p->getChildCount() ), _flags(1)
{
}

static const ldomXPointerEx & _max( const ldomXPointerEx & v1,  const ldomXPointerEx & v2 )
{
    int c = v1.compare( v2 );
    if ( c>=0 )
        return v1;
    else
        return v2;
}

static const ldomXPointerEx & _min( const ldomXPointerEx & v1,  const ldomXPointerEx & v2 )
{
    int c = v1.compare( v2 );
    if ( c<=0 )
        return v1;
    else
        return v2;
}

/// create intersection of two ranges
ldomXRange::ldomXRange( const ldomXRange & v1,  const ldomXRange & v2 )
    : _start( _max( v1._start, v2._start ) ), _end( _min( v1._end, v2._end ) )
{
}

/// create list splittiny existing list into non-overlapping ranges
ldomXRangeList::ldomXRangeList( ldomXRangeList & srcList, bool splitIntersections )
{
    if ( srcList.empty() )
        return;
    int i;
    if ( splitIntersections ) {
        ldomXRange * maxRange = new ldomXRange( *srcList[0] );
        for ( i=1; i<srcList.length(); i++ ) {
            if ( srcList[i]->getStart().compare( maxRange->getStart() ) < 0 )
                maxRange->setStart( srcList[i]->getStart() );
            if ( srcList[i]->getEnd().compare( maxRange->getEnd() ) > 0 )
                maxRange->setEnd( srcList[i]->getEnd() );
        }
        maxRange->setFlags(0);
        add( maxRange );
        for ( i=0; i<srcList.length(); i++ )
            split( srcList[i] );
        for ( int i=length()-1; i>=0; i-- ) {
            if ( get(i)->getFlags()==0 )
                erase( i, 1 );
        }
    } else {
        for ( i=0; i<srcList.length(); i++ )
            add( new ldomXRange( *srcList[i] ) );
    }
}

/// split into subranges using intersection
void ldomXRangeList::split( ldomXRange * r )
{
    int i;
    for ( i=0; i<length(); i++ ) {
        if ( r->checkIntersection( *get(i) ) ) {
            ldomXRange * src = remove( i );
            int cmp1 = src->getStart().compare( r->getStart() );
            int cmp2 = src->getEnd().compare( r->getEnd() );
            //TODO: add intersections
            if ( cmp1 < 0 && cmp2 < 0 ) {
                //   0====== src ======0
                //        X======= r=========X
                //   1111122222222222222
                ldomXRange * r1 = new ldomXRange( src->getStart(), r->getStart(), src->getFlags() );
                ldomXRange * r2 = new ldomXRange( r->getStart(), src->getEnd(), src->getFlags() | r->getFlags() );
                insert( i++, r1 );
                insert( i, r2 );
                delete src;
            } else if ( cmp1 > 0 && cmp2 > 0 ) {
                //           0====== src ======0
                //     X======= r=========X
                //           2222222222222233333
                ldomXRange * r2 = new ldomXRange( src->getStart(), r->getEnd(), src->getFlags() | r->getFlags() );
                ldomXRange * r3 = new ldomXRange( r->getEnd(), src->getEnd(), src->getFlags() );
                insert( i++, r2 );
                insert( i, r3 );
                delete src;
            } else if ( cmp1 < 0 && cmp2 > 0 ) {
                // 0====== src ================0
                //     X======= r=========X
                ldomXRange * r1 = new ldomXRange( src->getStart(), r->getStart(), src->getFlags() );
                ldomXRange * r2 = new ldomXRange( r->getStart(), r->getEnd(), src->getFlags() | r->getFlags() );
                ldomXRange * r3 = new ldomXRange( r->getEnd(), src->getEnd(), src->getFlags() );
                insert( i++, r1 );
                insert( i++, r2 );
                insert( i, r3 );
                delete src;
            } else if ( cmp1 == 0 && cmp2 > 0 ) {
                //   0====== src ========0
                //   X====== r=====X
                ldomXRange * r1 = new ldomXRange( src->getStart(), r->getEnd(), src->getFlags() | r->getFlags() );
                ldomXRange * r2 = new ldomXRange( r->getEnd(), src->getEnd(), src->getFlags() );
                insert( i++, r1 );
                insert( i, r2 );
                delete src;
            } else if ( cmp1 < 0 && cmp2 == 0 ) {
                //   0====== src =====0
                //      X====== r=====X
                ldomXRange * r1 = new ldomXRange( src->getStart(), r->getStart(), src->getFlags() );
                ldomXRange * r2 = new ldomXRange( r->getStart(), r->getEnd(), src->getFlags() | r->getFlags() );
                insert( i++, r1 );
                insert( i, r2 );
                delete src;
            } else {
                //        0====== src =====0
                //   X============== r===========X
                //
                //        0====== src =====0
                //   X============== r=====X
                //
                //   0====== src =====0
                //   X============== r=====X
                //
                //   0====== src ========0
                //   X========== r=======X
                src->setFlags( src->getFlags() | r->getFlags() );
                insert( i, src );
            }
        }
    }
}

#if BUILD_LITE!=1

bool ldomDocument::findText( lString16 pattern, bool caseInsensitive, bool reverse, int minY, int maxY, LVArray<ldomWord> & words, int maxCount, int maxHeight )
{
    if ( minY<0 )
        minY = 0;
    int fh = getFullHeight();
    if ( maxY<=0 || maxY>fh )
        maxY = fh;
    ldomXPointer start = createXPointer( lvPoint(0, minY), reverse?-1:1 );
    ldomXPointer end = createXPointer( lvPoint(10000, maxY), reverse?-1:1 );
    if ( start.isNull() || end.isNull() )
        return false;
    ldomXRange range( start, end );
    CRLog::debug("ldomDocument::findText() for Y %d..%d, range %d..%d", minY, maxY, start.toPoint().y, end.toPoint().y);
    if ( range.getStart().toPoint().y==-1 ) {
        range.getStart().nextVisibleText();
        CRLog::debug("ldomDocument::findText() updated range %d..%d", range.getStart().toPoint().y, range.getEnd().toPoint().y);
    }
    if ( range.getEnd().toPoint().y==-1 ) {
        range.getEnd().prevVisibleText();
        CRLog::debug("ldomDocument::findText() updated range %d..%d", range.getStart().toPoint().y, range.getEnd().toPoint().y);
    }
    if ( range.isNull() ) {
        CRLog::debug("No text found: Range is empty");
        return false;
    }
    return range.findText( pattern, caseInsensitive, reverse, words, maxCount, maxHeight );
}

static bool findText( const lString16 & str, int & pos, const lString16 & pattern )
{
    int len = pattern.length();
    if ( pos < 0 || pos + len > (int)str.length() )
        return false;
    const lChar16 * s1 = str.c_str() + pos;
    const lChar16 * s2 = pattern.c_str();
    int nlen = str.length() - pos - len;
    for ( int j=0; j<nlen; j++ ) {
        bool matched = true;
        for ( int i=0; i<len; i++ ) {
            if ( s1[i] != s2[i] ) {
                matched = false;
                break;
            }
        }
        if ( matched )
            return true;
        s1++;
        pos++;
    }
    return false;
}

static bool findTextRev( const lString16 & str, int & pos, const lString16 & pattern )
{
    int len = pattern.length();
    if ( pos+len>(int)str.length() )
        pos = str.length()-len;
    if ( pos < 0 )
        return false;
    const lChar16 * s1 = str.c_str() + pos;
    const lChar16 * s2 = pattern.c_str();
    int nlen = pos - len;
    for ( int j=nlen-1; j>=0; j-- ) {
        bool matched = true;
        for ( int i=0; i<len; i++ ) {
            if ( s1[i] != s2[i] ) {
                matched = false;
                break;
            }
        }
        if ( matched )
            return true;
        s1--;
        pos--;
    }
    return false;
}

/// searches for specified text inside range
bool ldomXRange::findText( lString16 pattern, bool caseInsensitive, bool reverse, LVArray<ldomWord> & words, int maxCount, int maxHeight, bool checkMaxFromStart )
{
    if ( caseInsensitive )
        pattern.lowercase();
    words.clear();
    if ( pattern.empty() )
        return false;
    if ( reverse ) {
        // reverse search
        if ( !_end.isText() ) {
            _end.prevVisibleText();
            lString16 txt = _end.getNode()->getText();
            _end.setOffset(txt.length());
        }
        int firstFoundTextY = -1;
        while ( !isNull() ) {

            lString16 txt = _end.getNode()->getText();
            int offs = _end.getOffset();

            if ( firstFoundTextY!=-1 && maxHeight>0 ) {
                ldomXPointer p( _start.getNode(), offs );
                int currentTextY = p.toPoint().y;
                if ( currentTextY<firstFoundTextY-maxHeight )
                    return words.length()>0;
            }

            if ( caseInsensitive )
                txt.lowercase();

            while ( ::findTextRev( txt, offs, pattern ) ) {
                if ( !words.length() && maxHeight>0 ) {
                    ldomXPointer p( _end.getNode(), offs );
                    firstFoundTextY = p.toPoint().y;
                }
                words.add( ldomWord(_end.getNode(), offs, offs + pattern.length() ) );
                offs--;
            }
            if ( !_end.prevVisibleText() )
                break;
            txt = _end.getNode()->getText();
            _end.setOffset(txt.length());
            if ( words.length() >= maxCount )
                break;
        }
    } else {
        // direct search
        if ( !_start.isText() )
            _start.nextVisibleText();
        int firstFoundTextY = -1;
        if (checkMaxFromStart) {
			ldomXPointer p( _start.getNode(), _start.getOffset() );
			firstFoundTextY = p.toPoint().y;
		}
        while ( !isNull() ) {
            int offs = _start.getOffset();

            if ( firstFoundTextY!=-1 && maxHeight>0 ) {
                ldomXPointer p( _start.getNode(), offs );
                int currentTextY = p.toPoint().y;
                if ( (checkMaxFromStart && currentTextY>=firstFoundTextY+maxHeight) ||
					currentTextY>firstFoundTextY+maxHeight )
                    return words.length()>0;
            }

            lString16 txt = _start.getNode()->getText();
            if ( caseInsensitive )
                txt.lowercase();

            while ( ::findText( txt, offs, pattern ) ) {
                if ( !words.length() && maxHeight>0 ) {
                    ldomXPointer p( _start.getNode(), offs );
                    int currentTextY = p.toPoint().y;
                    if (checkMaxFromStart) {
						if ( currentTextY>=firstFoundTextY+maxHeight )
							return words.length()>0;
					} else
						firstFoundTextY = currentTextY;
                }
                words.add( ldomWord(_start.getNode(), offs, offs + pattern.length() ) );
                offs++;
            }
            if ( !_start.nextVisibleText() )
                break;
            if ( words.length() >= maxCount )
                break;
        }
    }
    return words.length() > 0;
}

/// fill marked ranges list
void ldomXRangeList::getRanges( ldomMarkedRangeList &dst )
{
    dst.clear();
    if ( empty() )
        return;
    for ( int i=0; i<length(); i++ ) {
        ldomXRange * range = get(i);
        lvPoint ptStart = range->getStart().toPoint();
        lvPoint ptEnd = range->getEnd().toPoint();
//        // LVE:DEBUG
//        CRLog::trace("selectRange( %d,%d : %d,%d : %s, %s )", ptStart.x, ptStart.y, ptEnd.x, ptEnd.y, LCSTR(range->getStart().toString()), LCSTR(range->getEnd().toString()) );
        ldomMarkedRange * item = new ldomMarkedRange( ptStart, ptEnd, range->getFlags() );
        if ( !item->empty() )
            dst.add( item );
        else
            delete item;
    }
}

/// fill text selection list by splitting text into monotonic flags ranges
void ldomXRangeList::splitText( ldomMarkedTextList &dst, ldomNode * textNodeToSplit )
{
    lString16 text = textNodeToSplit->getText();
    if ( length()==0 ) {
        dst.add( new ldomMarkedText( text, 0, 0 ) );
        return;
    }
    ldomXRange textRange( textNodeToSplit );
    ldomXRangeList ranges;
    ranges.add( new ldomXRange(textRange) );
    int i;
    for ( i=0; i<length(); i++ ) {
        ranges.split( get(i) );
    }
    for ( i=0; i<ranges.length(); i++ ) {
        ldomXRange * r = ranges[i];
        int start = r->getStart().getOffset();
        int end = r->getEnd().getOffset();
        if ( end>start )
            dst.add( new ldomMarkedText( text.substr(start, end-start), r->getFlags(), start ) );
    }
    /*
    if ( dst.length() ) {
        CRLog::debug(" splitted: ");
        for ( int k=0; k<dst.length(); k++ ) {
            CRLog::debug("    (%d, %d) %s", dst[k]->offset, dst[k]->flags, UnicodeToUtf8(dst[k]->text).c_str());
        }
    }
    */
}

/// returns rectangle (in doc coordinates) for range. Returns true if found.
bool ldomXRange::getRect( lvRect & rect )
{
    if ( isNull() )
        return false;
    // get start and end rects
    lvRect rc1;
    lvRect rc2;
    if ( !getStart().getRect(rc1) || !getEnd().getRect(rc2) )
        return false;
    if ( rc1.top == rc2.top && rc1.bottom == rc2.bottom ) {
        // on same line
        rect.left = rc1.left;
        rect.top = rc1.top;
        rect.right = rc2.right;
        rect.bottom = rc2.bottom;
        return !rect.isEmpty();
    }
    // on different lines
    ldomNode * parent = getNearestCommonParent();
    if ( !parent )
        return false;
    parent->getAbsRect(rect);
    rect.top = rc1.top;
    rect.bottom = rc2.bottom;
    return !rect.isEmpty();
}

/// sets range to nearest word bounds, returns true if success
bool ldomXRange::getWordRange( ldomXRange & range, ldomXPointer & p )
{
    ldomNode * node = p.getNode();
    if ( !node->isText() )
        return false;
    int pos = p.getOffset();
    lString16 txt = node->getText();
    if ( pos<0 )
        pos = 0;
    if ( pos>(int)txt.length() )
        pos = txt.length();
    int endpos = pos;
    for (;;) {
        lChar16 ch = txt[endpos];
        if ( ch==0 || ch==' ' )
            break;
        endpos++;
    }
    /*
    // include trailing space
    for (;;) {
        lChar16 ch = txt[endpos];
        if ( ch==0 || ch!=' ' )
            break;
        endpos++;
    }
    */
    for ( ;; ) {
        if ( pos==0 )
            break;
        if ( txt[pos]!=' ' )
            break;
        pos--;
    }
    for ( ;; ) {
        if ( pos==0 )
            break;
        if ( txt[pos-1]==' ' )
            break;
        pos--;
    }
    ldomXRange r( ldomXPointer( node, pos ), ldomXPointer( node, endpos ) );
    range = r;
    return true;
}
#endif

/// returns true if intersects specified line rectangle
bool ldomMarkedRange::intersects( lvRect & rc, lvRect & intersection )
{
    if ( start.y>=rc.bottom )
        return false;
    if ( end.y<rc.top )
        return false;
    intersection = rc;
    if ( start.y>=rc.top && start.y<rc.bottom ) {
        if ( start.x > rc.right )
            return false;
        intersection.left = rc.left > start.x ? rc.left : start.x;
    }
    if ( end.y>=rc.top && end.y<rc.bottom ) {
        if ( end.x < rc.left )
            return false;
        intersection.right = rc.right < end.x ? rc.right : end.x;
    }
    return true;
}

/// create bounded by RC list, with (0,0) coordinates at left top corner
ldomMarkedRangeList::ldomMarkedRangeList( const ldomMarkedRangeList * list, lvRect & rc )
{
    if ( !list || list->empty() )
        return;
//    if ( list->get(0)->start.y>rc.bottom )
//        return;
//    if ( list->get( list->length()-1 )->end.y < rc.top )
//        return;
    for ( int i=0; i<list->length(); i++ ) {
        ldomMarkedRange * src = list->get(i);
        if ( src->start.y>=rc.bottom || src->end.y<rc.top )
            continue;
        add( new ldomMarkedRange(
            lvPoint(src->start.x-rc.left, src->start.y-rc.top ),
            lvPoint(src->end.x-rc.left, src->end.y-rc.top ),
            src->flags ) );
    }
}

/// returns nearest common element for start and end points
ldomNode * ldomXRange::getNearestCommonParent()
{
    ldomXPointerEx start(getStart());
    ldomXPointerEx end(getEnd());
    while ( start.getLevel() > end.getLevel() && start.parent() )
        ;
    while ( start.getLevel() < end.getLevel() && end.parent() )
        ;
    while ( start.getIndex()!=end.getIndex() && start.parent() && end.parent() )
        ;
    if ( start.getNode()==end.getNode() )
        return start.getNode();
    return NULL;
}

/// searches path for element with specific id, returns level at which element is founs, 0 if not found
int ldomXPointerEx::findElementInPath( lUInt16 id )
{
    if ( !ensureElement() )
        return 0;
    ldomNode * e = getNode();
    for ( ; e!=NULL; e = e->getParentNode() ) {
        if ( e->getNodeId()==id ) {
            return e->getNodeLevel();
        }
    }
    return 0;
}

bool ldomXPointerEx::ensureFinal()
{
    if ( !ensureElement() )
        return false;
    int cnt = 0;
    int foundCnt = -1;
    ldomNode * e = getNode();
    for ( ; e!=NULL; e = e->getParentNode() ) {
        if ( e->getRendMethod() == erm_final ) {
            foundCnt = cnt;
        }
        cnt++;
    }
    if ( foundCnt<0 )
        return false;
    for ( int i=0; i<foundCnt; i++ )
        parent();
    // curent node is final formatted element (e.g. paragraph)
    return true;
}

/// ensure that current node is element (move to parent, if not - from text node to element)
bool ldomXPointerEx::ensureElement()
{
    ldomNode * node = getNode();
    if ( !node )
        return false;
    if ( node->isText()) {
        if (!parent())
            return false;
        node = getNode();
    }
    if ( !node || !node->isElement() )
        return false;
    return true;
}

/// move to first child of current node
bool ldomXPointerEx::firstChild()
{
    return child(0);
}

/// move to last child of current node
bool ldomXPointerEx::lastChild()
{
    int count = getNode()->getChildCount();
    if ( count <=0 )
        return false;
    return child( count - 1 );
}

/// move to first element child of current node
bool ldomXPointerEx::firstElementChild()
{
    ldomNode * node = getNode();
    int count = node->getChildCount();
    for ( int i=0; i<count; i++ ) {
        if ( node->getChildNode( i )->isElement() )
            return child( i );
    }
    return false;
}

/// move to last element child of current node
bool ldomXPointerEx::lastElementChild()
{
    ldomNode * node = getNode();
    int count = node->getChildCount();
    for ( int i=count-1; i>=0; i-- ) {
        if ( node->getChildNode( i )->isElement() )
            return child( i );
    }
    return false;
}

/// forward iteration by elements of DOM three
bool ldomXPointerEx::nextElement()
{
    if ( !ensureElement() )
        return false;
    if ( firstElementChild() )
        return true;
    for (;;) {
        if ( nextSiblingElement() )
            return true;
        if ( !parent() )
            return false;
    }
}

/// returns true if current node is visible element with render method == erm_final
bool ldomXPointerEx::isVisibleFinal()
{
    if ( !isElement() )
        return false;
    int cnt = 0;
    int foundCnt = -1;
    ldomNode * e = getNode();
    for ( ; e!=NULL; e = e->getParentNode() ) {
        switch ( e->getRendMethod() ) {
        case erm_final:
            foundCnt = cnt;
            break;
        case erm_invisible:
            foundCnt = -1;
            break;
        default:
            break;
        }
        cnt++;
    }
    if ( foundCnt != 0 )
        return false;
    // curent node is visible final formatted element (e.g. paragraph)
    return true;
}

/// move to next visible text node
bool ldomXPointerEx::nextVisibleText( bool thisBlockOnly )
{
    ldomXPointerEx backup;
    if ( thisBlockOnly )
        backup = *this;
    while ( nextText(thisBlockOnly) ) {
        if ( isVisible() )
            return true;
    }
    if ( thisBlockOnly )
        *this = backup;
    return false;
}

/// returns true if current node is visible element or text
bool ldomXPointerEx::isVisible()
{
    ldomNode * p;
    ldomNode * node = getNode();
    if ( node && node->isText() )
        p = node->getParentNode();
    else
        p = node;
    while ( p ) {
        if ( p->getRendMethod() == erm_invisible )
            return false;
        p = p->getParentNode();
    }
    return true;
}

/// move to next text node
bool ldomXPointerEx::nextText( bool thisBlockOnly )
{
    ldomNode * block = NULL;
    if ( thisBlockOnly )
        block = getThisBlockNode();
    setOffset( 0 );
    while ( firstChild() ) {
        if ( isText() )
            return (!thisBlockOnly || getThisBlockNode()==block);
    }
    for (;;) {
        while ( nextSibling() ) {
            if ( isText() )
                return (!thisBlockOnly || getThisBlockNode()==block);
            while ( firstChild() ) {
                if ( isText() )
                    return (!thisBlockOnly || getThisBlockNode()==block);
            }
        }
        if ( !parent() )
            return false;
    }
}

/// move to previous text node
bool ldomXPointerEx::prevText( bool thisBlockOnly )
{
    ldomNode * block = NULL;
    if ( thisBlockOnly )
        block = getThisBlockNode();
    setOffset( 0 );
    for (;;) {
        while ( prevSibling() ) {
            if ( isText() )
                return  (!thisBlockOnly || getThisBlockNode()==block);
            while ( lastChild() ) {
                if ( isText() )
                    return (!thisBlockOnly || getThisBlockNode()==block);
            }
        }
        if ( !parent() )
            return false;
    }
}

/// move to previous visible text node
bool ldomXPointerEx::prevVisibleText( bool thisBlockOnly )
{
    ldomXPointerEx backup;
    if ( thisBlockOnly )
        backup = *this;
    while ( prevText( thisBlockOnly ) )
        if ( isVisible() )
            return true;
    if ( thisBlockOnly )
        *this = backup;
    return false;
}

// TODO: implement better behavior
inline bool IsUnicodeSpace( lChar16 ch )
{
    return ch==' ';
}

// TODO: implement better behavior
inline bool IsUnicodeSpaceOrNull( lChar16 ch )
{
    return ch==0 || ch==' ';
}

inline bool canWrapWordBefore( lChar16 ch ) {
    return ch>=0x2e80 && ch<0xa640;
}

inline bool canWrapWordAfter( lChar16 ch ) {
    return ch>=0x2e80 && ch<0xa640;
}

/// move to previous visible word beginning
bool ldomXPointerEx::prevVisibleWordStart( bool thisBlockOnly )
{
    if ( isNull() )
        return false;
    ldomNode * node = NULL;
    lString16 text;
    int textLen = 0;
    for ( ;; ) {
        if ( !isText() || !isVisible() || _data->getOffset()==0 ) {
            // move to previous text
            if ( !prevVisibleText(thisBlockOnly) )
                return false;
            node = getNode();
            text = node->getText();
            textLen = text.length();
            _data->setOffset( textLen );
        } else {
            node = getNode();
            text = node->getText();
            textLen = text.length();
        }
        bool foundNonSpace = false;
        while ( _data->getOffset() > 0 && IsUnicodeSpace(text[_data->getOffset()-1]) )
            _data->addOffset(-1);
        while ( _data->getOffset()>0 ) {
            if ( IsUnicodeSpace(text[ _data->getOffset()-1 ]) )
                break;
            foundNonSpace = true;
            _data->addOffset(-1);
        }
        if ( foundNonSpace )
            return true;
    }
}

/// move to previous visible word end
bool ldomXPointerEx::prevVisibleWordEnd( bool thisBlockOnly )
{
    if ( isNull() )
        return false;
    ldomNode * node = NULL;
    lString16 text;
    int textLen = 0;
    bool moved = false;
    for ( ;; ) {
        if ( !isText() || !isVisible() || _data->getOffset()==0 ) {
            // move to previous text
            if ( !prevVisibleText(thisBlockOnly) )
                return false;
            node = getNode();
            text = node->getText();
            textLen = text.length();
            _data->setOffset( textLen );
            moved = true;
        } else {
            node = getNode();
            text = node->getText();
            textLen = text.length();
        }
        // skip spaces
        while ( _data->getOffset() > 0 && IsUnicodeSpace(text[_data->getOffset()-1]) ) {
            _data->addOffset(-1);
            moved = true;
        }
        if ( moved && _data->getOffset()>0 )
            return true; // found!
        // skip non-spaces
        while ( _data->getOffset()>0 ) {
            if ( IsUnicodeSpace(text[ _data->getOffset()-1 ]) )
                break;
            _data->addOffset(-1);
        }
        // skip spaces
        while ( _data->getOffset() > 0 && IsUnicodeSpace(text[_data->getOffset()-1]) ) {
            _data->addOffset(-1);
            moved = true;
        }
        if ( moved && _data->getOffset()>0 )
            return true; // found!
    }
}

/// move to next visible word beginning
bool ldomXPointerEx::nextVisibleWordStart( bool thisBlockOnly )
{
    if ( isNull() )
        return false;
    ldomNode * node = NULL;
    lString16 text;
    int textLen = 0;
    bool moved = false;
    for ( ;; ) {
        if ( !isText() || !isVisible() ) {
            // move to previous text
            if ( !nextVisibleText(thisBlockOnly) )
                return false;
            node = getNode();
            text = node->getText();
            textLen = text.length();
            _data->setOffset( 0 );
            moved = true;
        } else {
            for (;;) {
                node = getNode();
                text = node->getText();
                textLen = text.length();
                if ( _data->getOffset() < textLen )
                    break;
                if ( !nextVisibleText(thisBlockOnly) )
                    return false;
                _data->setOffset( 0 );
            }
        }
        // skip spaces
        while ( _data->getOffset()<textLen && IsUnicodeSpace(text[ _data->getOffset() ]) ) {
            _data->addOffset(1);
            moved = true;
        }
        if ( moved && _data->getOffset()<textLen )
            return true;
        // skip non-spaces
        while ( _data->getOffset()<textLen ) {
            if ( IsUnicodeSpace(text[ _data->getOffset() ]) )
                break;
            moved = true;
            _data->addOffset(1);
        }
        // skip spaces
        while ( _data->getOffset()<textLen && IsUnicodeSpace(text[ _data->getOffset() ]) ) {
            _data->addOffset(1);
            moved = true;
        }
        if ( moved && _data->getOffset()<textLen )
            return true;
    }
}

/// move to next visible word end
bool ldomXPointerEx::nextVisibleWordEnd( bool thisBlockOnly )
{
    if ( isNull() )
        return false;
    ldomNode * node = NULL;
    lString16 text;
    int textLen = 0;
    bool moved = false;
    for ( ;; ) {
        if ( !isText() || !isVisible() ) {
            // move to previous text
            if ( !nextVisibleText(thisBlockOnly) )
                return false;
            node = getNode();
            text = node->getText();
            textLen = text.length();
            _data->setOffset( 0 );
            moved = true;
        } else {
            for (;;) {
                node = getNode();
                text = node->getText();
                textLen = text.length();
                if ( _data->getOffset() < textLen )
                    break;
                if ( !nextVisibleText(thisBlockOnly) )
                    return false;
                _data->setOffset( 0 );
            }
        }
        bool nonSpaceFound = false;
        // skip non-spaces
        while ( _data->getOffset()<textLen ) {
            if ( IsUnicodeSpace(text[ _data->getOffset() ]) )
                break;
            nonSpaceFound = true;
            _data->addOffset(1);
        }
        if ( nonSpaceFound )
            return true;
        // skip spaces
        while ( _data->getOffset()<textLen && IsUnicodeSpace(text[ _data->getOffset() ]) ) {
            _data->addOffset(1);
            moved = true;
        }
        // skip non-spaces
        while ( _data->getOffset()<textLen ) {
            if ( IsUnicodeSpace(text[ _data->getOffset() ]) )
                break;
            nonSpaceFound = true;
            _data->addOffset(1);
        }
        if ( nonSpaceFound )
            return true;
    }
}

/// returns true if current position is visible word beginning
bool ldomXPointerEx::isVisibleWordStart()
{
   if ( isNull() )
        return false;
    if ( !isText() || !isVisible() )
        return false;
    ldomNode * node = getNode();
    lString16 text = node->getText();
    int textLen = text.length();
    int i = _data->getOffset();
    lChar16 currCh = i<textLen ? text[i] : 0;
    lChar16 prevCh = i<textLen && i>0 ? text[i-1] : 0;
    if ( canWrapWordBefore(currCh) || IsUnicodeSpaceOrNull(prevCh) && !IsUnicodeSpace(currCh) )
        return true;
    return false;
 }

/// returns true if current position is visible word end
bool ldomXPointerEx::isVisibleWordEnd()
{
    if ( isNull() )
        return false;
    if ( !isText() || !isVisible() )
        return false;
    ldomNode * node = getNode();
    lString16 text = node->getText();
    int textLen = text.length();
    int i = _data->getOffset();
    lChar16 currCh = i>0 ? text[i-1] : 0;
    lChar16 nextCh = i<textLen ? text[i] : 0;
    if ( canWrapWordAfter(currCh) || !IsUnicodeSpace(currCh) && IsUnicodeSpaceOrNull(nextCh) )
        return true;
    return false;
}

/// returns block owner node of current node (or current node if it's block)
ldomNode * ldomXPointerEx::getThisBlockNode()
{
    if ( isNull() )
        return NULL;
    ldomNode * node = getNode();
    if ( node->isText() )
        node = node->getParentNode();
    for (;;) {
        if ( !node )
            return NULL;
        lvdom_element_render_method rm = node->getRendMethod();
        switch ( rm ) {
        case erm_runin: // treat as separate block
        case erm_block:
        case erm_final:
        case erm_mixed:
        case erm_list_item:
        case erm_table:
        case erm_table_row_group:
        case erm_table_row:
        case erm_table_caption:
            return node;
        default:
            break; // ignore
        }
        node = node->getParentNode();
    }
}

/// returns true if points to last visible text inside block element
bool ldomXPointerEx::isLastVisibleTextInBlock()
{
    if ( !isText() )
        return false;
    ldomXPointerEx pos(*this);
    return !pos.nextVisibleText(true);
}

/// returns true if points to first visible text inside block element
bool ldomXPointerEx::isFirstVisibleTextInBlock()
{
    if ( !isText() )
        return false;
    ldomXPointerEx pos(*this);
    return !pos.prevVisibleText(true);
}

// sentence navigation

/// returns true if points to beginning of sentence
bool ldomXPointerEx::isSentenceStart()
{
    if ( isNull() )
        return false;
    if ( !isText() || !isVisible() )
        return false;
    ldomNode * node = getNode();
    lString16 text = node->getText();
    int textLen = text.length();
    int i = _data->getOffset();
    lChar16 currCh = i<textLen ? text[i] : 0;
    lChar16 prevCh = i>0 ? text[i-1] : 0;
    lChar16 prevNonSpace = 0;
    for ( ;i>0; i-- ) {
        lChar16 ch = text[i-1];
        if ( !IsUnicodeSpace(ch) ) {
            prevNonSpace = ch;
            break;
        }
    }
    if ( !prevNonSpace ) {
        ldomXPointerEx pos(*this);
        while ( !prevNonSpace && pos.prevVisibleText(true) ) {
            lString16 prevText = pos.getText();
            for ( int j=prevText.length()-1; j>=0; j-- ) {
                lChar16 ch = prevText[j];
                if ( !IsUnicodeSpace(ch) ) {
                    prevNonSpace = ch;
                    break;
                }
            }
        }
    }

    if ( !IsUnicodeSpace(currCh) && IsUnicodeSpaceOrNull(prevCh) ) {
        switch (prevNonSpace) {
        case 0:
        case '.':
        case '?':
        case '!':
        case L'\x2026': // horizontal ellypsis
            return true;
        default:
            return false;
        }
    }
    return false;
}

/// returns true if points to end of sentence
bool ldomXPointerEx::isSentenceEnd()
{
    if ( isNull() )
        return false;
    if ( !isText() || !isVisible() )
        return false;
    ldomNode * node = getNode();
    lString16 text = node->getText();
    int textLen = text.length();
    int i = _data->getOffset();
    lChar16 currCh = i<textLen ? text[i] : 0;
    lChar16 prevCh = i>0 ? text[i-1] : 0;
    if ( IsUnicodeSpaceOrNull(currCh) ) {
        switch (prevCh) {
        case 0:
        case '.':
        case '?':
        case '!':
        case L'\x2026': // horizontal ellypsis
            return true;
        default:
            break;
        }
    }
    // word is not ended with . ! ?
    // check whether it's last word of block
    ldomXPointerEx pos(*this);
    return !pos.nextVisibleWordStart(true);
}

/// move to beginning of current visible text sentence
bool ldomXPointerEx::thisSentenceStart()
{
    if ( isNull() )
        return false;
    if ( !isText() && !nextVisibleText() && !prevVisibleText() )
        return false;
    for (;;) {
        if ( isSentenceStart() )
            return true;
        if ( !prevVisibleWordStart(true) )
            return false;
    }
}

/// move to end of current visible text sentence
bool ldomXPointerEx::thisSentenceEnd()
{
    if ( isNull() )
        return false;
    if ( !isText() && !nextVisibleText() && !prevVisibleText() )
        return false;
    for (;;) {
        if ( isSentenceEnd() )
            return true;
        if ( !nextVisibleWordEnd(true) )
            return false;
    }
}

/// move to beginning of next visible text sentence
bool ldomXPointerEx::nextSentenceStart()
{
    if ( !isSentenceStart() && !thisSentenceEnd() )
        return false;
    for (;;) {
        if ( !nextVisibleWordStart() )
            return false;
        if ( isSentenceStart() )
            return true;
    }
}

/// move to beginning of prev visible text sentence
bool ldomXPointerEx::prevSentenceStart()
{
    if ( !thisSentenceStart() )
        return false;
    for (;;) {
        if ( !prevVisibleWordStart() )
            return false;
        if ( isSentenceStart() )
            return true;
    }
}

/// move to end of next visible text sentence
bool ldomXPointerEx::nextSentenceEnd()
{
    if ( !nextSentenceStart() )
        return false;
    return thisSentenceEnd();
}

/// move to end of next visible text sentence
bool ldomXPointerEx::prevSentenceEnd()
{
    if ( !thisSentenceStart() )
        return false;
    for (;;) {
        if ( !prevVisibleWordEnd() )
            return false;
        if ( isSentenceEnd() )
            return true;
    }
}

/// if start is after end, swap start and end
void ldomXRange::sort()
{
    if ( _start.isNull() || _end.isNull() )
        return;
    if ( _start.compare(_end) > 0 ) {
        ldomXPointer p1( _start );
        ldomXPointer p2( _end );
        _start = p2;
        _end = p1;
    }
}

/// backward iteration by elements of DOM three
bool ldomXPointerEx::prevElement()
{
    if ( !ensureElement() )
        return false;
    for (;;) {
        if ( prevSiblingElement() ) {
            while ( lastElementChild() )
                ;
            return true;
        }
        if ( !parent() )
            return false;
        return true;
    }
}

/// move to next final visible node (~paragraph)
bool ldomXPointerEx::nextVisibleFinal()
{
    for ( ;; ) {
        if ( !nextElement() )
            return false;
        if ( isVisibleFinal() )
            return true;
    }
}

/// move to previous final visible node (~paragraph)
bool ldomXPointerEx::prevVisibleFinal()
{
    for ( ;; ) {
        if ( !prevElement() )
            return false;
        if ( isVisibleFinal() )
            return true;
    }
}

/// run callback for each node in range
void ldomXRange::forEach( ldomNodeCallback * callback )
{
    if ( isNull() )
        return;
    ldomXRange pos( _start, _end, 0 );
    bool allowGoRecurse = true;
    while ( !pos._start.isNull() && pos._start.compare( _end ) < 0 ) {
        // do something
        ldomNode * node = pos._start.getNode();
        //lString16 path = pos._start.toString();
        //CRLog::trace( "%s", UnicodeToUtf8(path).c_str() );
        if ( node->isElement() ) {
            allowGoRecurse = callback->onElement( &pos.getStart() );
        } else if ( node->isText() ) {
            lString16 txt = node->getText();
            pos._end = pos._start;
            pos._start.setOffset( 0 );
            pos._end.setOffset( txt.length() );
            if ( _start.getNode() == node ) {
                pos._start.setOffset( _start.getOffset() );
            }
            if ( _end.getNode() == node && pos._end.getOffset() > _end.getOffset()) {
                pos._end.setOffset( _end.getOffset() );
            }
            callback->onText( &pos );
            allowGoRecurse = false;
        }
        // move to next item
        bool stop = false;
        if ( !allowGoRecurse || !pos._start.child(0) ) {
             while ( !pos._start.nextSibling() ) {
                if ( !pos._start.parent() ) {
                    stop = true;
                    break;
                }
            }
        }
        if ( stop )
            break;
    }
}

class ldomWordsCollector : public ldomNodeCallback {
    LVArray<ldomWord> & _list;
public:
    ldomWordsCollector( LVArray<ldomWord> & list )
        : _list( list )
    {
    }
    /// called for each found text fragment in range
    virtual void onText( ldomXRange * nodeRange )
    {
        ldomNode * node = nodeRange->getStart().getNode();
        lString16 text = node->getText();
        int len = text.length();
        int end = nodeRange->getEnd().getOffset();
        if ( len>end )
            len = end;
        int beginOfWord = -1;
        for ( int i=nodeRange->getStart().getOffset(); i <= len; i++ ) {
            int alpha = lGetCharProps(text[i]) & CH_PROP_ALPHA;
            if (alpha && beginOfWord<0 ) {
                beginOfWord = i;
            }
            if ( !alpha && beginOfWord>=0) {
                _list.add( ldomWord( node, beginOfWord, i ) );
                beginOfWord = -1;
            }
        }
    }
    /// called for each found node in range
    virtual bool onElement( ldomXPointerEx * ptr )
    {
        ldomNode * elem = ptr->getNode();
        if ( elem->getRendMethod()==erm_invisible )
            return false;
        return true;
    }
};

/// get all words from specified range
void ldomXRange::getRangeWords( LVArray<ldomWord> & list )
{
    ldomWordsCollector collector( list );
    forEach( &collector );
}

/// adds all visible words from range, returns number of added words
int ldomWordExList::addRangeWords( ldomXRange & range, bool trimPunctuation ) {
    LVArray<ldomWord> list;
    range.getRangeWords( list );
    for ( int i=0; i<list.length(); i++ )
        add( new ldomWordEx(list[i]) );
    init();
    return list.length();
}

lvPoint ldomMarkedRange::getMiddlePoint() {
    if ( start.y==end.y ) {
        return lvPoint( ((start.x + end.x)>>1), start.y );
    } else {
        return start;
    }
}

/// returns distance (dx+dy) from specified point to middle point
int ldomMarkedRange::calcDistance( int x, int y, MoveDirection dir ) {
    lvPoint middle = getMiddlePoint();
    int dx = middle.x - x;
    int dy = middle.y - y;
    if ( dx<0 ) dx = -dx;
    if ( dy<0 ) dy = -dy;
    switch (dir) {
    case DIR_LEFT:
    case DIR_RIGHT:
        return dx + dy;
    case DIR_UP:
    case DIR_DOWN:
        return dx + dy*100;
    }


    return dx + dy;
}

/// select word
void ldomWordExList::selectWord( ldomWordEx * word, MoveDirection dir )
{
    selWord = word;
    if ( selWord ) {
        lvPoint middle = word->getMark().getMiddlePoint();
        if ( x==-1 || (dir!=DIR_UP && dir!=DIR_DOWN) )
            x = middle.x;
        y = middle.y;
    } else {
        x = y = -1;
    }
}

/// select next word in specified direction
ldomWordEx * ldomWordExList::selectNextWord( MoveDirection dir, int moveBy )
{
    if ( !selWord )
        return selectMiddleWord();
    pattern.clear();
    for ( int i=0; i<moveBy; i++ ) {
        ldomWordEx * word = findNearestWord( x, y, dir );
        if ( word )
            selectWord( word, dir );
    }
    return selWord;
}

/// select middle word in range
ldomWordEx * ldomWordExList::selectMiddleWord() {
    if ( minx==-1 )
        init();
    ldomWordEx * word = findNearestWord( (maxx+minx)/2, (miny+maxy)/2, DIR_ANY );
    selectWord(word, DIR_ANY);
    return word;
}

ldomWordEx * ldomWordExList::findWordByPattern()
{
    ldomWordEx * lastBefore = NULL;
    ldomWordEx * firstAfter = NULL;
    bool selReached = false;
    for ( int i=0; i<length(); i++ ) {
        ldomWordEx * item = get(i);
        if ( item==selWord )
            selReached = true;
        lString16 text = item->getText();
        text.lowercase();
        bool flg = true;
        for ( unsigned j=0; j<pattern.length(); j++ ) {
            if ( j>=text.length() ) {
                flg = false;
                break;
            }
            lString16 chars = pattern[j];
            chars.lowercase();
            bool charFound = false;
            for ( unsigned k=0; k<chars.length(); k++ ) {
                if ( chars[k]==text[j] ) {
                    charFound = true;
                    break;
                }
            }
            if ( !charFound ) {
                flg = false;
                break;
            }
        }
        if ( !flg )
            continue;
        if ( selReached ) {
            if ( firstAfter==NULL )
                firstAfter = item;
        } else {
            lastBefore = item;
        }
    }

    if ( firstAfter )
        return firstAfter;
    else
        return lastBefore;
}

/// try append search pattern and find word
ldomWordEx * ldomWordExList::appendPattern(lString16 chars)
{
    pattern.add(chars);
    ldomWordEx * foundWord = findWordByPattern();

    if ( foundWord ) {
        selectWord(foundWord, DIR_ANY);
    } else {
        pattern.erase(pattern.length()-1, 1);
    }
    return foundWord;
}

/// remove last character from pattern and try to search
ldomWordEx * ldomWordExList::reducePattern()
{
    if ( pattern.length()==0 )
        return NULL;
    pattern.erase(pattern.length()-1, 1);
    ldomWordEx * foundWord = findWordByPattern();

    if ( foundWord )
        selectWord(foundWord, DIR_ANY);
    return foundWord;
}

/// find word nearest to specified point
ldomWordEx * ldomWordExList::findNearestWord( int x, int y, MoveDirection dir ) {
    if ( !length() )
        return NULL;
    int bestDistance = -1;
    ldomWordEx * bestWord = NULL;
    ldomWordEx * defWord = (dir==DIR_LEFT || dir==DIR_UP) ? get(length()-1) : get(0);
    int i;
    if ( dir==DIR_LEFT || dir==DIR_RIGHT ) {
        int thisLineY = -1;
        int thisLineDy = -1;
        for ( i=0; i<length(); i++ ) {
            ldomWordEx * item = get(i);
            lvPoint middle = item->getMark().getMiddlePoint();
            int dy = middle.y - y;
            if ( dy<0 ) dy = -dy;
            if ( thisLineY==-1 || thisLineDy>dy ) {
                thisLineY = middle.y;
                thisLineDy = dy;
            }
        }
        ldomWordEx * nextLineWord = NULL;
        for ( i=0; i<length(); i++ ) {
            ldomWordEx * item = get(i);
            if ( dir!=DIR_ANY && item==selWord )
                continue;
            ldomMarkedRange * mark = &item->getMark();
            lvPoint middle = mark->getMiddlePoint();
            switch ( dir ) {
            case DIR_LEFT:
                if ( middle.y<thisLineY )
                    nextLineWord = item; // last word of prev line
                if ( middle.x>=x )
                    continue;
                break;
            case DIR_RIGHT:
                if ( nextLineWord==NULL && middle.y>thisLineY )
                    nextLineWord = item; // first word of next line
                if ( middle.x<=x )
                    continue;
                break;
            }
            if ( middle.y!=thisLineY )
                continue;
            int dist = mark->calcDistance(x, y, dir);
            if ( bestDistance==-1 || dist<bestDistance ) {
                bestWord = item;
                bestDistance = dist;
            }
        }
        if ( bestWord!=NULL )
            return bestWord; // found in the same line
        if ( nextLineWord!=NULL  )
            return nextLineWord;
        return defWord;
    }
    for ( i=0; i<length(); i++ ) {
        ldomWordEx * item = get(i);
        if ( dir!=DIR_ANY && item==selWord )
            continue;
        ldomMarkedRange * mark = &item->getMark();
        lvPoint middle = mark->getMiddlePoint();
        if ( dir==DIR_UP && middle.y >= y )
            continue;
        if ( dir==DIR_DOWN && middle.y <= y )
            continue;

        int dist = mark->calcDistance(x, y, dir);
        if ( bestDistance==-1 || dist<bestDistance ) {
            bestWord = item;
            bestDistance = dist;
        }
    }
    if ( bestWord!=NULL )
        return bestWord;
    return defWord;
}

void ldomWordExList::init()
{
    if ( !length() )
        return;
    for ( int i=0; i<length(); i++ ) {
        ldomWordEx * item = get(i);
        lvPoint middle = item->getMark().getMiddlePoint();
        if ( i==0 || minx > middle.x )
            minx = middle.x;
        if ( i==0 || maxx < middle.x )
            maxx = middle.x;
        if ( i==0 || miny > middle.y )
            miny = middle.y;
        if ( i==0 || maxy < middle.y )
            maxy = middle.y;
    }
}


class ldomTextCollector : public ldomNodeCallback
{
private:
    bool lastText;
    bool newBlock;
    int  delimiter;
    int  maxLen;
    lString16 text;
public:
    ldomTextCollector( lChar16 blockDelimiter, int maxTextLen )
        : lastText(false), newBlock(true), delimiter( blockDelimiter), maxLen( maxTextLen )
    {
    }
    /// destructor
    virtual ~ldomTextCollector() { }
    /// called for each found text fragment in range
    virtual void onText( ldomXRange * nodeRange )
    {
        if ( newBlock && !text.empty()) {
            text << delimiter;
        }
        lString16 txt = nodeRange->getStart().getNode()->getText();
        int start = nodeRange->getStart().getOffset();
        int end = nodeRange->getEnd().getOffset();
        if ( start < end ) {
            text << txt.substr( start, end-start );
        }
        lastText = true;
        newBlock = false;
    }
    /// called for each found node in range
    virtual bool onElement( ldomXPointerEx * ptr )
    {
#if BUILD_LITE!=1
        ldomNode * elem = (ldomNode *)ptr->getNode();
        if ( elem->getRendMethod()==erm_invisible )
            return false;
        switch ( elem->getStyle()->display ) {
        /*
        case css_d_inherit:
        case css_d_block:
        case css_d_list_item:
        case css_d_compact:
        case css_d_marker:
        case css_d_table:
        case css_d_inline_table:
        case css_d_table_row_group:
        case css_d_table_header_group:
        case css_d_table_footer_group:
        case css_d_table_row:
        case css_d_table_column_group:
        case css_d_table_column:
        case css_d_table_cell:
        case css_d_table_caption:
        */
        default:
            newBlock = true;
            return true;
        case css_d_none:
            return false;
        case css_d_inline:
        case css_d_run_in:
            newBlock = false;
            return true;
        }
#else
        newBlock = true;
        return true;
#endif
    }
    /// get collected text
    lString16 getText() { return text; }
};

/// returns text between two XPointer positions
lString16 ldomXRange::getRangeText( lChar16 blockDelimiter, int maxTextLen )
{
    ldomTextCollector callback( blockDelimiter, maxTextLen );
    forEach( &callback );
    return callback.getText();
}

/// returns href attribute of <A> element, null string if not found
lString16 ldomXPointer::getHRef()
{
    if ( isNull() )
        return lString16();
    ldomNode * node = getNode();
    while ( node && !node->isElement() )
        node = node->getParentNode();
    while ( node && node->getNodeId()!=el_a )
        node = node->getParentNode();
    if ( !node )
        return lString16();
    lString16 ref = node->getAttributeValue( LXML_NS_ANY, attr_href );
    if ( !ref.empty() && ref[0]!='#' )
        ref = DecodeHTMLUrlString(ref);
    return ref;
}


/// returns href attribute of <A> element, null string if not found
lString16 ldomXRange::getHRef()
{
    if ( isNull() )
        return lString16();
    return _start.getHRef();
}


ldomDocument * LVParseXMLStream( LVStreamRef stream,
                              const elem_def_t * elem_table,
                              const attr_def_t * attr_table,
                              const ns_def_t * ns_table )
{
    if ( stream.isNull() )
        return NULL;
    bool error = true;
    ldomDocument * doc;
    doc = new ldomDocument();
    doc->setDocFlags( 0 );

    ldomDocumentWriter writer(doc);
    doc->setNodeTypes( elem_table );
    doc->setAttributeTypes( attr_table );
    doc->setNameSpaceTypes( ns_table );

    /// FB2 format
    LVFileFormatParser * parser = new LVXMLParser(stream, &writer);
    if ( parser->CheckFormat() ) {
        if ( parser->Parse() ) {
            error = false;
        }
    }
    delete parser;
    if ( error ) {
        delete doc;
        doc = NULL;
    }
    return doc;
}

ldomDocument * LVParseHTMLStream( LVStreamRef stream,
                              const elem_def_t * elem_table,
                              const attr_def_t * attr_table,
                              const ns_def_t * ns_table )
{
    if ( stream.isNull() )
        return NULL;
    bool error = true;
    ldomDocument * doc;
    doc = new ldomDocument();
    doc->setDocFlags( 0 );

    ldomDocumentWriterFilter writerFilter(doc, false, HTML_AUTOCLOSE_TABLE);
    doc->setNodeTypes( elem_table );
    doc->setAttributeTypes( attr_table );
    doc->setNameSpaceTypes( ns_table );

    /// FB2 format
    LVFileFormatParser * parser = new LVHTMLParser(stream, &writerFilter);
    if ( parser->CheckFormat() ) {
        if ( parser->Parse() ) {
            error = false;
        }
    }
    delete parser;
    if ( error ) {
        delete doc;
        doc = NULL;
    }
    return doc;
}

#if 0
static lString16 escapeDocPath( lString16 path )
{
    for ( unsigned i=0; i<path.length(); i++ ) {
        lChar16 ch = path[i];
        if ( ch=='/' || ch=='\\')
            path[i] = '_';
    }
    return path;
}
#endif

lString16 ldomDocumentFragmentWriter::convertId( lString16 id )
{
    if ( !codeBasePrefix.empty() ) {
        return codeBasePrefix + L"_" + id;
    }
    return id;
}

lString16 ldomDocumentFragmentWriter::convertHref( lString16 href )
{
    if ( href.pos(L"://")>=0 )
        return href; // fully qualified href: no conversion

    href = LVCombinePaths(codeBase, href);

    // resolve relative links
    lString16 p, id;
    if ( !href.split2(lString16("#"), p, id) )
        p = href;
    if ( p.empty() ) {
        if ( codeBasePrefix.empty() )
            return href;
        p = codeBasePrefix;
    } else {
        lString16 replacement = pathSubstitutions.get(p);
        if ( !replacement.empty() )
            p = replacement;
        else
            return href;
        //else
        //    p = codeBasePrefix;
        //p = LVCombinePaths( codeBase, p ); // relative to absolute path
    }
    if ( !id.empty() )
        p = p + L"_" + id;

    p = lString16("#") + p;

    //CRLog::debug("converted href=%s to %s", LCSTR(href), LCSTR(p) );

    return p;
}

void ldomDocumentFragmentWriter::setCodeBase( lString16 fileName )
{
    filePathName = fileName;
    codeBasePrefix = pathSubstitutions.get(fileName);
    codeBase = LVExtractPath(filePathName);
    if ( codeBasePrefix.empty() ) {
        CRLog::trace("codeBasePrefix is empty for path %s", LCSTR(fileName));
        codeBasePrefix = pathSubstitutions.get(fileName);
    }
    stylesheetFile.clear();
}

/// called on attribute
void ldomDocumentFragmentWriter::OnAttribute( const lChar16 * nsname, const lChar16 * attrname, const lChar16 * attrvalue )
{
    if ( insideTag ) {
        if ( !lStr_cmp(attrname, L"href") || !lStr_cmp(attrname, L"src") ) {
            parent->OnAttribute(nsname, attrname, convertHref(lString16(attrvalue)).c_str() );
        } else if ( !lStr_cmp(attrname, L"id") ) {
            parent->OnAttribute(nsname, attrname, convertId(lString16(attrvalue)).c_str() );
        } else if ( !lStr_cmp(attrname, L"name") ) {
            //CRLog::trace("name attribute = %s", LCSTR(lString16(attrvalue)));
            parent->OnAttribute(nsname, attrname, convertId(lString16(attrvalue)).c_str() );
        } else {
            parent->OnAttribute(nsname, attrname, attrvalue);
        }
    } else {
        if ( styleDetectionState ) {
            if ( !lStr_cmp(attrname, L"rel") && !lStr_cmp(attrvalue, L"stylesheet") )
                styleDetectionState |= 2;
            else if ( !lStr_cmp(attrname, L"type") && !lStr_cmp(attrvalue, L"text/css") )
                styleDetectionState |= 4;
            else if ( !lStr_cmp(attrname, L"href") ) {
                styleDetectionState |= 8;
                lString16 href = attrvalue;
                tmpStylesheetFile = LVCombinePaths( codeBase, href );
            }
            if ( styleDetectionState==15 ) {
                stylesheetFile = tmpStylesheetFile;
                styleDetectionState = 0;
                CRLog::trace("CSS file href: %s", LCSTR(stylesheetFile));
            }
        }
    }
}

/// called on opening tag
ldomNode * ldomDocumentFragmentWriter::OnTagOpen( const lChar16 * nsname, const lChar16 * tagname )
{
    if ( insideTag ) {
        return parent->OnTagOpen(nsname, tagname);
    } else {
        if ( !lStr_cmp(tagname, L"link") )
            styleDetectionState = 1;
    }
    if ( !insideTag && baseTag==tagname ) {
        insideTag = true;
        if ( !baseTagReplacement.empty() ) {
            baseElement = parent->OnTagOpen(L"", baseTagReplacement.c_str());
            lastBaseElement = baseElement;
            if ( !stylesheetFile.empty() ) {
                parent->OnAttribute(L"", L"StyleSheet", stylesheetFile.c_str() );
                CRLog::debug("Setting StyleSheet attribute to %s for document fragment", LCSTR(stylesheetFile) );
            }
            if ( !codeBasePrefix.empty() )
                parent->OnAttribute(L"", L"id", codeBasePrefix.c_str() );
            parent->OnTagBody();
            return baseElement;
        }
    }
    return NULL;
}

/// called on closing tag
void ldomDocumentFragmentWriter::OnTagClose( const lChar16 * nsname, const lChar16 * tagname )
{
    styleDetectionState = 0;
    if ( insideTag && baseTag==tagname ) {
        insideTag = false;
        if ( !baseTagReplacement.empty() ) {
            parent->OnTagClose(L"", baseTagReplacement.c_str());
        }
        baseElement = NULL;
        return;
    }
    if ( insideTag )
        parent->OnTagClose(nsname, tagname);
}

/// called after > of opening tag (when entering tag body)
void ldomDocumentFragmentWriter::OnTagBody()
{
    if ( insideTag ) {
        parent->OnTagBody();
    }
    styleDetectionState = 0;
}



/** \brief callback object to fill DOM tree

    To be used with XML parser as callback object.

    Creates document according to incoming events.

    Autoclose HTML tags.
*/

void ldomDocumentWriterFilter::setClass( const lChar16 * className, bool overrideExisting )
{
    ldomNode * node = _currNode->_element;
    if ( _classAttrId==0 ) {
        _classAttrId = _document->getAttrNameIndex(L"class");
    }
    if ( overrideExisting || !node->hasAttribute(_classAttrId) ) {
        node->setAttributeValue(LXML_NS_NONE, _classAttrId, className);
    }
}

void ldomDocumentWriterFilter::appendStyle( const lChar16 * style )
{
    ldomNode * node = _currNode->_element;
    if ( _styleAttrId==0 ) {
        _styleAttrId = _document->getAttrNameIndex(L"style");
    }
    lString16 oldStyle = node->getAttributeValue(_styleAttrId);
    if ( !oldStyle.empty() && oldStyle.at(oldStyle.length()-1)!=';' )
        oldStyle << L"; ";
    oldStyle << style;
    node->setAttributeValue(LXML_NS_NONE, _styleAttrId, oldStyle.c_str());
}

void ldomDocumentWriterFilter::AutoClose( lUInt16 tag_id, bool open )
{
    lUInt16 * rule = _rules[tag_id];
    if ( !rule )
        return;
    if ( open ) {
        ldomElementWriter * found = NULL;
        ldomElementWriter * p = _currNode;
        while ( p && !found ) {
            lUInt16 id = p->_element->getNodeId();
            for ( int i=0; rule[i]; i++ ) {
                if ( rule[i]==id ) {
                    found = p;
                    break;
                }
            }
            p = p->_parent;
        }
        // found auto-close target
        if ( found != NULL ) {
            bool done = false;
            while ( !done && _currNode ) {
                if ( _currNode == found )
                    done = true;
                ldomNode * closedElement = _currNode->getElement();
                _currNode = pop( _currNode, closedElement->getNodeId() );
                //ElementCloseHandler( closedElement );
            }
        }
    } else {
        if ( !rule[0] )
            _currNode = pop( _currNode, _currNode->getElement()->getNodeId() );
    }
}

ldomNode * ldomDocumentWriterFilter::OnTagOpen( const lChar16 * nsname, const lChar16 * tagname )
{
    if ( !_tagBodyCalled ) {
        CRLog::error("OnTagOpen w/o parent's OnTagBody : %s", LCSTR(lString16(tagname)));
        crFatalError();
    }
    _tagBodyCalled = false;
    //logfile << "lxmlDocumentWriter::OnTagOpen() [" << nsname << ":" << tagname << "]";
//    if ( nsname && nsname[0] )
//        lStr_lowercase( const_cast<lChar16 *>(nsname), lStr_len(nsname) );
//    lStr_lowercase( const_cast<lChar16 *>(tagname), lStr_len(tagname) );

    // Patch for bad LIB.RU books - BR delimited paragraphs in "Fine HTML" format
    if ( (tagname[0]=='b' && tagname[1]=='r' && tagname[2]==0)
        || tagname[0]=='d' && tagname[1]=='d' && tagname[2]==0 ) {
        // substitute to P
        tagname = L"p";
        _libRuParagraphStart = true; // to trim leading &nbsp;
    } else {
        _libRuParagraphStart = false;
    }

    lUInt16 id = _document->getElementNameIndex(tagname);
    lUInt16 nsid = (nsname && nsname[0]) ? _document->getNsNameIndex(nsname) : 0;
    AutoClose( id, true );
    _currNode = new ldomElementWriter( _document, nsid, id, _currNode );
    _flags = _currNode->getFlags();
    if ( _libRuDocumentDetected && (_flags & TXTFLG_PRE) )
        _flags |= TXTFLG_PRE_PARA_SPLITTING | TXTFLG_TRIM; // convert preformatted text into paragraphs
    //logfile << " !o!\n";
    //return _currNode->getElement();
    return _currNode->getElement();
}

void ldomDocumentWriterFilter::OnTagBody()
{
    _tagBodyCalled = true;
    if ( _currNode ) {
        _currNode->onBodyEnter();
    }
}

void ldomDocumentWriterFilter::ElementCloseHandler( ldomNode * node )
{
    ldomNode * parent = node->getParentNode();
    lUInt16 id = node->getNodeId();
    if ( parent ) {
        if ( parent->getLastChild() != node )
            return;
        if ( id==el_table ) {
            if ( node->getAttributeValue(attr_align)==L"right" && node->getAttributeValue(attr_width)==L"30%" ) {
                // LIB.RU TOC detected: remove it
                parent->removeLastChild();
            }
        } else if ( id==el_pre && _libRuDocumentDetected ) {
            // for LIB.ru - replace PRE element with DIV (section?)
            if ( node->getChildCount()==0 )
                parent->removeLastChild(); // remove empty PRE element
            //else if ( node->getLastChild()->getNodeId()==el_div && node->getLastChild()->getChildCount() &&
            //          ((ldomElement*)node->getLastChild())->getLastChild()->getNodeId()==el_form )
            //    parent->removeLastChild(); // remove lib.ru final section
            else
                node->setNodeId( el_div );
        } else if ( id==el_div ) {
            if ( node->getAttributeValue(attr_align)==L"right" ) {
                ldomNode * child = node->getLastChild();
                if ( child && child->getNodeId()==el_form )  {
                    // LIB.RU form detected: remove it
                    parent->removeLastChild();
                    _libRuDocumentDetected = true;
                }
            }
        }
    }
}

void ldomDocumentWriterFilter::OnAttribute( const lChar16 * nsname, const lChar16 * attrname, const lChar16 * attrvalue )
{
    //logfile << "ldomDocumentWriter::OnAttribute() [" << nsname << ":" << attrname << "]";
    //if ( nsname && nsname[0] )
    //    lStr_lowercase( const_cast<lChar16 *>(nsname), lStr_len(nsname) );
    //lStr_lowercase( const_cast<lChar16 *>(attrname), lStr_len(attrname) );

    //CRLog::trace("OnAttribute(%s, %s)", LCSTR(lString16(attrname)), LCSTR(lString16(attrvalue)));

    if ( !lStr_cmp(attrname, L"align") ) {
        if ( !lStr_cmp(attrvalue, L"justify") )
            appendStyle( L"text-align: justify" );
        else if ( !lStr_cmp(attrvalue, L"left") )
            appendStyle( L"text-align: left" );
        else if ( !lStr_cmp(attrvalue, L"right") )
            appendStyle( L"text-align: right" );
        else if ( !lStr_cmp(attrvalue, L"center") )
            appendStyle( L"text-align: center" );
       return;
    }
    lUInt16 attr_ns = (nsname && nsname[0]) ? _document->getNsNameIndex( nsname ) : 0;
    lUInt16 attr_id = (attrname && attrname[0]) ? _document->getAttrNameIndex( attrname ) : 0;

    _currNode->addAttribute( attr_ns, attr_id, attrvalue );

    //logfile << " !a!\n";
}

/// called on closing tag
void ldomDocumentWriterFilter::OnTagClose( const lChar16 * nsname, const lChar16 * tagname )
{
    if ( !_tagBodyCalled ) {
        CRLog::error("OnTagClose w/o parent's OnTagBody : %s", LCSTR(lString16(tagname)));
        crFatalError();
    }
    //logfile << "ldomDocumentWriter::OnTagClose() [" << nsname << ":" << tagname << "]";
//    if ( nsname && nsname[0] )
//        lStr_lowercase( const_cast<lChar16 *>(nsname), lStr_len(nsname) );
//    lStr_lowercase( const_cast<lChar16 *>(tagname), lStr_len(tagname) );
    if (!_currNode)
    {
        _errFlag = true;
        //logfile << " !c-err!\n";
        return;
    }


    if ( tagname[0]=='l' && _currNode && !lStr_cmp(tagname, L"link") ) {
        // link node
        if ( _currNode && _currNode->getElement() && _currNode->getElement()->getNodeName()==L"link" &&
             _currNode->getElement()->getParentNode() && _currNode->getElement()->getParentNode()->getNodeName()==L"head" &&
             _currNode->getElement()->getAttributeValue(L"rel")==L"stylesheet" &&
             _currNode->getElement()->getAttributeValue(L"type")==L"text/css" ) {
            lString16 href = _currNode->getElement()->getAttributeValue(L"href");
            lString16 stylesheetFile = LVCombinePaths( _document->getCodeBase(), href );
            CRLog::debug("Internal stylesheet file: %s", LCSTR(stylesheetFile));
            _document->setDocStylesheetFileName(stylesheetFile);
            _document->applyDocumentStyleSheet();
        }
    }

    lUInt16 id = _document->getElementNameIndex(tagname);

    // HTML title detection
    if ( id==el_title && _currNode->_element->getParentNode()!= NULL && _currNode->_element->getParentNode()->getNodeId()==el_head ) {
        lString16 s = _currNode->_element->getText();
        s.trim();
        if ( !s.empty() ) {
            // TODO: split authors, title & series
            _document->getProps()->setString( DOC_PROP_TITLE, s );
        }
    }
    //======== START FILTER CODE ============
    AutoClose( _currNode->_element->getNodeId(), false );
    //======== END FILTER CODE ==============
    //lUInt16 nsid = (nsname && nsname[0]) ? _document->getNsNameIndex(nsname) : 0;
    // save closed element
    ldomNode * closedElement = _currNode->getElement();
    _errFlag |= (id != closedElement->getNodeId());
    _currNode = pop( _currNode, id );


    if ( _currNode ) {
        _flags = _currNode->getFlags();
        if ( _libRuDocumentDetected && (_flags & TXTFLG_PRE) )
            _flags |= TXTFLG_PRE_PARA_SPLITTING | TXTFLG_TRIM; // convert preformatted text into paragraphs
    }

    //=============================================================
    // LIB.RU patch: remove table of contents
    //ElementCloseHandler( closedElement );
    //=============================================================

    if ( id==_stopTagId ) {
        //CRLog::trace("stop tag found, stopping...");
        _parser->Stop();
    }
    //logfile << " !c!\n";
}

/// called on text
void ldomDocumentWriterFilter::OnText( const lChar16 * text, int len, lUInt32 flags )
{
    //logfile << "lxmlDocumentWriter::OnText() fpos=" << fpos;
    if (_currNode)
    {
        AutoClose( _currNode->_element->getNodeId(), false );
        if ( (_flags & XML_FLAG_NO_SPACE_TEXT)
             && IsEmptySpace(text, len) && !(flags & TXTFLG_PRE))
             return;
        bool autoPara = _libRuDocumentDetected && (flags & TXTFLG_PRE);
        if (_currNode->_allowText) {
            if ( _libRuParagraphStart ) {
                bool cleaned = false;
                while ( *text==160 && len > 0 ) {
                    cleaned = true;
                    text++;
                    len--;
                    while ( *text==' ' && len > 0 ) {
                        text++;
                        len--;
                    }
                }
                if ( cleaned ) {
                    setClass(L"justindent");
                    //appendStyle(L"text-indent: 1.3em; text-align: justify");
                }
                _libRuParagraphStart = false;
            }
            int leftSpace = 0;
            const lChar16 * paraTag = NULL;
            bool isHr = false;
            if ( autoPara ) {
                while ( (*text==' ' || *text=='\t' || *text==160) && len > 0 ) {
                    text++;
                    len--;
                    leftSpace += (*text == '\t') ? 8 : 1;
                }
                paraTag = leftSpace > 8 ? L"h2" : L"p";
                lChar16 ch = 0;
                bool sameCh = true;
                for ( int i=0; i<len; i++ ) {
                    if ( !ch )
                        ch = text[i];
                    else if ( ch != text[i] ) {
                        sameCh = false;
                        break;
                    }
                }
                if ( !ch )
                    sameCh = false;
                if ( (ch=='-' || ch=='=' || ch=='_' || ch=='*' || ch=='#') && sameCh )
                    isHr = true;
            }
            if ( isHr ) {
                OnTagOpen( NULL, L"hr" );
                OnTagClose( NULL, L"hr" );
            } else if ( len > 0 ) {
                if ( autoPara )
                    OnTagOpen( NULL, paraTag );
                _currNode->onText( text, len, flags );
                if ( autoPara )
                    OnTagClose( NULL, paraTag );
            }
        }
    }
    //logfile << " !t!\n";
}

ldomDocumentWriterFilter::ldomDocumentWriterFilter(ldomDocument * document, bool headerOnly, const char *** rules )
: ldomDocumentWriter( document, headerOnly )
, _libRuDocumentDetected(false)
, _libRuParagraphStart(false)
, _styleAttrId(0)
, _classAttrId(0)
, _tagBodyCalled(true)
{
    lUInt16 i;
    for ( i=0; i<MAX_ELEMENT_TYPE_ID; i++ )
        _rules[i] = NULL;
    lUInt16 items[MAX_ELEMENT_TYPE_ID];
    for ( i=0; rules[i]; i++ ) {
        const char ** rule = rules[i];
        lUInt16 j;
        for ( j=0; rule[j] && j<MAX_ELEMENT_TYPE_ID; j++ ) {
            const char * s = rule[j];
            items[j] = _document->getElementNameIndex( lString16(s).c_str() );
        }
        if ( j>=1 ) {
            lUInt16 id = items[0];
            _rules[ id ] = new lUInt16[j];
            for ( int k=0; k<j; k++ ) {
                _rules[id][k] = k==j-1 ? 0 : items[k+1];
            }
        }
    }
}

ldomDocumentWriterFilter::~ldomDocumentWriterFilter()
{

    for ( int i=0; i<MAX_ELEMENT_TYPE_ID; i++ ) {
        if ( _rules[i] )
            delete[] _rules[i];
    }
}

#if BUILD_LITE!=1
static const char * doc_file_magic = "CR3\n";


bool lxmlDocBase::DocFileHeader::serialize( SerialBuf & hdrbuf )
{
    int start = hdrbuf.pos();
    hdrbuf.putMagic( doc_file_magic );
    //CRLog::trace("Serializing render data: %d %d %d %d", render_dx, render_dy, render_docflags, render_style_hash);
    hdrbuf << render_dx << render_dy << render_docflags << render_style_hash << stylesheet_hash;

    hdrbuf.putCRC( hdrbuf.pos() - start );

#if 0
    {
        lString8 s;
        s<<"SERIALIZED HDR BUF: ";
        for ( int i=0; i<hdrbuf.pos(); i++ ) {
            char tmp[20];
            sprintf(tmp, "%02x ", hdrbuf.buf()[i]);
            s<<tmp;
        }
        CRLog::trace(s.c_str());
    }
#endif
    return !hdrbuf.error();
}

bool lxmlDocBase::DocFileHeader::deserialize( SerialBuf & hdrbuf )
{
    int start = hdrbuf.pos();
    hdrbuf.checkMagic( doc_file_magic );
    if ( hdrbuf.error() ) {
        CRLog::error("Swap file Magic signature doesn't match");
        return false;
    }
    hdrbuf >> render_dx >> render_dy >> render_docflags >> render_style_hash >> stylesheet_hash;
    //CRLog::trace("Deserialized render data: %d %d %d %d", render_dx, render_dy, render_docflags, render_style_hash);
    hdrbuf.checkCRC( hdrbuf.pos() - start );
    if ( hdrbuf.error() ) {
        CRLog::error("Swap file - header unpack error");
        return false;
    }
    return true;
}
#endif

void tinyNodeCollection::setDocFlag( lUInt32 mask, bool value )
{
    CRLog::debug("setDocFlag(%04x, %s)", mask, value?"true":"false");
    if ( value )
        _docFlags |= mask;
    else
        _docFlags &= ~mask;
}

void tinyNodeCollection::setDocFlags( lUInt32 value )
{
    CRLog::debug("setDocFlags(%04x)", value);
    _docFlags = value;
}

int tinyNodeCollection::getPersistenceFlags()
{
    int format = getProps()->getIntDef(DOC_PROP_FILE_FORMAT, 0);
    int flag = ( format==2 && getDocFlag(DOC_FLAG_PREFORMATTED_TEXT) ) ? 1 : 0;
    return flag;
}

void ldomDocument::clear()
{
#if BUILD_LITE!=1
    clearRendBlockCache();
    _rendered = false;
    _urlImageMap.clear();
#endif
    //TODO: implement clear
    //_elemStorage.
}

#if BUILD_LITE!=1
bool ldomDocument::openFromCache( CacheLoadingCallback * formatCallback )
{
    if ( !openCacheFile() ) {
        CRLog::info("Cannot open document from cache. Need to read fully");
        clear();
        return false;
    }
    if ( !loadCacheFileContent(formatCallback) ) {
        CRLog::info("Error while loading document content from cache file.");
        clear();
        return false;
    }
#if 0
    LVStreamRef s = LVOpenFileStream("/tmp/test.xml", LVOM_WRITE);
    if ( !s.isNull() )
        saveToStream(s, "UTF8");
#endif
    _mapped = true;
    _rendered = true;
    return true;
}

/// load document cache file content, @see saveChanges()
bool ldomDocument::loadCacheFileContent(CacheLoadingCallback * formatCallback)
{

    CRLog::trace("ldomDocument::loadCacheFileContent()");
    {
        SerialBuf propsbuf(0, true);
        if ( !_cacheFile->read( CBT_PROP_DATA, propsbuf ) ) {
            CRLog::error("Error while reading props data");
            return false;
        }
        getProps()->deserialize( propsbuf );
        if ( propsbuf.error() ) {
            CRLog::error("Cannot decode property table for document");
            return false;
        }

        if ( formatCallback ) {
            int fmt = getProps()->getIntDef(DOC_PROP_FILE_FORMAT_ID,
                    doc_format_fb2);
            if (fmt < doc_format_fb2 || fmt > doc_format_max)
                fmt = doc_format_fb2;
            // notify about format detection, to allow setting format-specific CSS
            formatCallback->OnCacheFileFormatDetected((doc_format_t)fmt);
        }

        SerialBuf idbuf(0, true);
        if ( !_cacheFile->read( CBT_MAPS_DATA, idbuf ) ) {
            CRLog::error("Error while reading Id data");
            return false;
        }
        deserializeMaps( idbuf );
        if ( idbuf.error() ) {
            CRLog::error("Cannot decode ID table for document");
            return false;
        }

        CRLog::trace("ldomDocument::loadCacheFileContent() - page data");
        SerialBuf pagebuf(0, true);
        if ( !_cacheFile->read( CBT_PAGE_DATA, pagebuf ) ) {
            CRLog::error("Error while reading pages data");
            return false;
        }
        pagebuf.swap( _pagesData );
        _pagesData.setPos( 0 );
        LVRendPageList pages;
        pages.deserialize(_pagesData);
        if ( _pagesData.error() ) {
            CRLog::error("Page data deserialization is failed");
            return false;
        }
        CRLog::info("%d pages read from cache file", pages.length());
        //_pagesData.setPos( 0 );

        DocFileHeader h;
        memset(&h, 0, sizeof(h));
        SerialBuf hdrbuf(0,true);
        if ( !_cacheFile->read( CBT_REND_PARAMS, hdrbuf ) ) {
            CRLog::error("Error while reading header data");
            return false;
        } else if ( !h.deserialize(hdrbuf) ) {
            CRLog::error("Header data deserialization is failed");
            return false;
        }
        _hdr = h;
        CRLog::info("Loaded render properties: styleHash=%x, stylesheetHash=%x, docflags=%x, width=%x, height=%x",
                _hdr.render_style_hash, _hdr.stylesheet_hash, _hdr.render_docflags, _hdr.render_dx, _hdr.render_dy);

    }

    CRLog::trace("ldomDocument::loadCacheFileContent() - node data");
    if ( !loadNodeData() ) {
        CRLog::error("Error while reading node instance data");
        return false;
    }


    CRLog::trace("ldomDocument::loadCacheFileContent() - element storage");
    if ( !_elemStorage.load() ) {
        CRLog::error("Error while loading element data");
        return false;
    }
    CRLog::trace("ldomDocument::loadCacheFileContent() - text storage");
    if ( !_textStorage.load() ) {
        CRLog::error("Error while loading text data");
        return false;
    }
    CRLog::trace("ldomDocument::loadCacheFileContent() - rect storage");
    if ( !_rectStorage.load() ) {
        CRLog::error("Error while loading rect data");
        return false;
    }
    CRLog::trace("ldomDocument::loadCacheFileContent() - node style storage");
    if ( !_styleStorage.load() ) {
        CRLog::error("Error while loading node style data");
        return false;
    }

    CRLog::trace("ldomDocument::loadCacheFileContent() - TOC");
    {
        SerialBuf tocbuf(0,true);
        if ( !_cacheFile->read( CBT_TOC_DATA, tocbuf ) ) {
            CRLog::error("Error while reading TOC data");
            return false;
        } else if ( !m_toc.deserialize(this, tocbuf) ) {
            CRLog::error("TOC data deserialization is failed");
            return false;
        }
    }

    if ( loadStylesData() ) {
        CRLog::trace("ldomDocument::loadCacheFileContent() - using loaded styles");
        updateLoadedStyles( true );
//        lUInt32 styleHash = calcStyleHash();
//        styleHash = styleHash * 31 + calcGlobalSettingsHash();
//        CRLog::debug("Loaded style hash: %x", styleHash);
//        lUInt32 styleHash = calcStyleHash();
//        CRLog::info("Loaded style hash = %08x", styleHash);
    } else {
        CRLog::trace("ldomDocument::loadCacheFileContent() - style loading failed: will reinit ");
        updateLoadedStyles( false );
    }

    CRLog::trace("ldomDocument::loadCacheFileContent() - completed successfully");

    return true;
}

static const char * styles_magic = "CRSTYLES";

#define CHECK_EXPIRATION(s) \
    if ( maxTime.expired() ) { CRLog::info("timer expired while " s); return CR_TIMEOUT; }

/// saves changes to cache file, limited by time interval (can be called again to continue after TIMEOUT)
ContinuousOperationResult ldomDocument::saveChanges( CRTimerUtil & maxTime )
{
    if ( !_cacheFile )
        return CR_DONE;

    if (maxTime.infinite()) {
        _mapSavingStage = 0; // all stages from the beginning
        _cacheFile->setAutoSyncSize(0);
    } else {
        //CRLog::trace("setting autosync");
        _cacheFile->setAutoSyncSize(STREAM_AUTO_SYNC_SIZE);
        //CRLog::trace("setting autosync - done");
    }

    CRLog::trace("ldomDocument::saveChanges(timeout=%d stage=%d)", maxTime.interval(), _mapSavingStage);

    switch (_mapSavingStage) {
    default:
    case 0:

        if (!maxTime.infinite())
            _cacheFile->flush(false, maxTime);
        CHECK_EXPIRATION("flushing of stream")

        persist( maxTime );
        CHECK_EXPIRATION("persisting of node data")

        // fall through
    case 1:
        _mapSavingStage = 1;
        CRLog::trace("ldomDocument::saveChanges() - element storage");

        if ( !_elemStorage.save(maxTime) ) {
            CRLog::error("Error while saving element data");
            return CR_ERROR;
        }
        CHECK_EXPIRATION("saving element storate")
        // fall through
    case 2:
        _mapSavingStage = 2;
        CRLog::trace("ldomDocument::saveChanges() - text storage");
        if ( !_textStorage.save(maxTime) ) {
            CRLog::error("Error while saving text data");
            return CR_ERROR;
        }
        CHECK_EXPIRATION("saving text storate")
        // fall through
    case 3:
        _mapSavingStage = 3;
        CRLog::trace("ldomDocument::saveChanges() - rect storage");

        if ( !_rectStorage.save(maxTime) ) {
            CRLog::error("Error while saving rect data");
            return CR_ERROR;
        }
        CHECK_EXPIRATION("saving rect storate")
        // fall through
    case 41:
        _mapSavingStage = 41;
        CRLog::trace("ldomDocument::saveChanges() - blob storage data");

        if ( _blobCache.saveToCache(maxTime) == CR_ERROR ) {
            CRLog::error("Error while saving blob storage data");
            return CR_ERROR;
        }
        if (!maxTime.infinite())
            _cacheFile->flush(false, maxTime); // intermediate flush
        CHECK_EXPIRATION("saving blob storage data")
        // fall through
    case 4:
        _mapSavingStage = 4;
        CRLog::trace("ldomDocument::saveChanges() - node style storage");

        if ( !_styleStorage.save(maxTime) ) {
            CRLog::error("Error while saving node style data");
            return CR_ERROR;
        }
        if (!maxTime.infinite())
            _cacheFile->flush(false, maxTime); // intermediate flush
        CHECK_EXPIRATION("saving node style storage")
        // fall through
    case 5:
        _mapSavingStage = 5;
        CRLog::trace("ldomDocument::saveChanges() - misc data");
        {
            SerialBuf propsbuf(4096);
            getProps()->serialize( propsbuf );
            if ( !_cacheFile->write( CBT_PROP_DATA, propsbuf, COMPRESS_MISC_DATA ) ) {
                CRLog::error("Error while saving props data");
                return CR_ERROR;
            }
        }
        if (!maxTime.infinite())
            _cacheFile->flush(false, maxTime); // intermediate flush
        CHECK_EXPIRATION("saving props data")
        // fall through
    case 6:
        _mapSavingStage = 6;
        CRLog::trace("ldomDocument::saveChanges() - ID data");
        {
            SerialBuf idbuf(4096);
            serializeMaps( idbuf );
            if ( !_cacheFile->write( CBT_MAPS_DATA, idbuf, COMPRESS_MISC_DATA ) ) {
                CRLog::error("Error while saving Id data");
                return CR_ERROR;
            }
        }
        if (!maxTime.infinite())
            _cacheFile->flush(false, maxTime); // intermediate flush
        CHECK_EXPIRATION("saving ID data")
        // fall through
    case 7:
        _mapSavingStage = 7;
        if ( _pagesData.pos() ) {
            CRLog::trace("ldomDocument::saveChanges() - page data (%d bytes)", _pagesData.pos());
            if ( !_cacheFile->write( CBT_PAGE_DATA, _pagesData, COMPRESS_PAGES_DATA  ) ) {
                CRLog::error("Error while saving pages data");
                return CR_ERROR;
            }
        } else {
            CRLog::trace("ldomDocument::saveChanges() - no page data");
        }
        if (!maxTime.infinite())
            _cacheFile->flush(false, maxTime); // intermediate flush
        CHECK_EXPIRATION("saving page data")
        // fall through
    case 8:
        _mapSavingStage = 8;

        CRLog::trace("ldomDocument::saveChanges() - node data");
        if ( !saveNodeData() ) {
            CRLog::error("Error while node instance data");
            return CR_ERROR;
        }
        if (!maxTime.infinite())
            _cacheFile->flush(false, maxTime); // intermediate flush
        CHECK_EXPIRATION("saving node data")
        // fall through
    case 9:
        _mapSavingStage = 9;
        CRLog::trace("ldomDocument::saveChanges() - render info");
        {
            SerialBuf hdrbuf(0,true);
            if ( !_hdr.serialize(hdrbuf) ) {
                CRLog::error("Header data serialization is failed");
                return CR_ERROR;
            } else if ( !_cacheFile->write( CBT_REND_PARAMS, hdrbuf, false ) ) {
                CRLog::error("Error while writing header data");
                return CR_ERROR;
            }
        }
        CRLog::info("Saving render properties: styleHash=%x, stylesheetHash=%x, docflags=%x, width=%x, height=%x",
                    _hdr.render_style_hash, _hdr.stylesheet_hash, _hdr.render_docflags, _hdr.render_dx, _hdr.render_dy);


        CRLog::trace("ldomDocument::saveChanges() - TOC");
        {
            SerialBuf tocbuf(0,true);
            if ( !m_toc.serialize(tocbuf) ) {
                CRLog::error("TOC data serialization is failed");
                return CR_ERROR;
            } else if ( !_cacheFile->write( CBT_TOC_DATA, tocbuf, COMPRESS_TOC_DATA ) ) {
                CRLog::error("Error while writing TOC data");
                return CR_ERROR;
            }
        }
        if (!maxTime.infinite())
            _cacheFile->flush(false, maxTime); // intermediate flush
        CHECK_EXPIRATION("saving TOC data")
        // fall through
    case 10:
        _mapSavingStage = 10;

        if ( !saveStylesData() ) {
            CRLog::error("Error while writing style data");
            return CR_ERROR;
        }
        CRLog::trace("ldomDocument::saveChanges() - flush");
        {
            CRTimerUtil infinite;
            if ( !_cacheFile->flush(true, infinite) ) {
                CRLog::error("Error while updating index of cache file");
                return CR_ERROR;
            }
        }
        // fall through
    case 11:
        _mapSavingStage = 11;
    }
    CRLog::trace("ldomDocument::saveChanges() - done");
    return CR_DONE;
}

/// save changes to cache file, @see loadCacheFileContent()
bool ldomDocument::saveChanges()
{
    if ( !_cacheFile )
        return true;
    CRLog::debug("ldomDocument::saveChanges() - infinite");
    CRTimerUtil timerNoLimit;
    ContinuousOperationResult res = saveChanges(timerNoLimit);
    return res!=CR_ERROR;
}

bool tinyNodeCollection::saveStylesData()
{
    SerialBuf stylebuf(0, true);
    lUInt32 stHash = _stylesheet.getHash();
    LVArray<css_style_ref_t> * list = _styles.getIndex();
    stylebuf.putMagic(styles_magic);
    stylebuf << stHash;
    stylebuf << (lUInt32)list->length(); // index
    for ( int i=0; i<list->length(); i++ ) {
        css_style_ref_t rec = list->get(i);
        if ( !rec.isNull() ) {
            stylebuf << (lUInt32)i; // index
            rec->serialize( stylebuf ); // style
        }
    }
    stylebuf << (lUInt32)0; // index=0 is end list mark
    stylebuf.putMagic(styles_magic);
    delete list;
    if ( stylebuf.error() )
        return false;
    CRLog::trace("Writing style data: %d bytes", stylebuf.pos());
    if ( !_cacheFile->write( CBT_STYLE_DATA, stylebuf, COMPRESS_STYLE_DATA) ) {
        return false;
    }
    return !stylebuf.error();
}

bool tinyNodeCollection::loadStylesData()
{
    SerialBuf stylebuf(0, true);
    if ( !_cacheFile->read( CBT_STYLE_DATA, stylebuf ) ) {
        CRLog::error("Error while reading style data");
        return false;
    }
    lUInt32 stHash = 0;
    lInt32 len = 0;
    lUInt32 myHash = _stylesheet.getHash();

    //LVArray<css_style_ref_t> * list = _styles.getIndex();
    stylebuf.checkMagic(styles_magic);
    stylebuf >> stHash;
    if ( stHash != myHash ) {
        CRLog::info("tinyNodeCollection::loadStylesData() - stylesheet hash is changed: skip loading styles");
        return false;
    }
    stylebuf >> len; // index
    if ( stylebuf.error() )
        return false;
    LVArray<css_style_ref_t> list(len, css_style_ref_t());
    for ( int i=0; i<list.length(); i++ ) {
        lUInt32 index = 0;
        stylebuf >> index; // index
        if ( index<=0 || (int)index>=len || stylebuf.error() )
            break;
        css_style_ref_t rec( new css_style_rec_t() );
        if ( !rec->deserialize(stylebuf) )
            break;
        list.set( index, rec );
    }
    stylebuf.checkMagic(styles_magic);
    if ( stylebuf.error() )
        return false;

    CRLog::trace("Setting style data: %d bytes", stylebuf.size());
    _styles.setIndex( list );

    return !stylebuf.error();
}

lUInt32 tinyNodeCollection::calcStyleHash()
{
//    int maxlog = 20;
    int count = ((_elemCount+TNC_PART_LEN-1) >> TNC_PART_SHIFT);
    lUInt32 res = 0; //_elemCount;
    lUInt32 globalHash = calcGlobalSettingsHash();
    lUInt32 docFlags = getDocFlags();
//    CRLog::info("Calculating style hash...  elemCount=%d, globalHash=%08x, docFlags=%08x", _elemCount, globalHash, docFlags);
    for ( int i=0; i<count; i++ ) {
        int offs = i*TNC_PART_LEN;
        int sz = TNC_PART_LEN;
        if ( offs + sz > _elemCount+1 ) {
            sz = _elemCount+1 - offs;
        }
        ldomNode * buf = _elemList[i];
        for ( int j=0; j<sz; j++ ) {
            if ( buf[j].isElement() ) {
                css_style_ref_t style = buf[j].getStyle();
                lUInt32 sh = calcHash( style );
                res = res * 31 + sh;
                LVFontRef font = buf[j].getFont();
                lUInt32 fh = calcHash( font );
                res = res * 31 + fh;
//                if ( maxlog>0 && sh==0 ) {
//                    style = buf[j].getStyle();
//                    CRLog::trace("[%06d] : s=%08x f=%08x  res=%08x", offs+j, sh, fh, res);
//                    maxlog--;
//                }
            }
        }
    }
    res = res * 31 + _imgScalingOptions.getHash();
    res = res * 31 + _minSpaceCondensingPercent;
    res = (res * 31 + globalHash) * 31 + docFlags;
//    CRLog::info("Calculated style hash = %08x", res);
    return res;
}

static void validateChild( ldomNode * node )
{
    // DEBUG TEST
    if ( !node->isRoot() && node->getParentNode()->getChildIndex( node->getDataIndex() )<0 ) {
        CRLog::error("Invalid parent->child relation for nodes %d->%d", node->getParentNode()->getDataIndex(), node->getParentNode()->getDataIndex() );
    }
}

/// called on document loading end
bool tinyNodeCollection::validateDocument()
{
    ((ldomDocument*)this)->getRootNode()->recurseElements(validateChild);
    int count = ((_elemCount+TNC_PART_LEN-1) >> TNC_PART_SHIFT);
    bool res = true;
    for ( int i=0; i<count; i++ ) {
        int offs = i*TNC_PART_LEN;
        int sz = TNC_PART_LEN;
        if ( offs + sz > _elemCount+1 ) {
            sz = _elemCount+1 - offs;
        }
        ldomNode * buf = _elemList[i];
        for ( int j=0; j<sz; j++ ) {
            buf[j].setDocumentIndex( _docIndex );
            if ( buf[j].isElement() ) {
                lUInt16 style = getNodeStyleIndex( buf[j]._handle._dataIndex );
                lUInt16 font = getNodeFontIndex( buf[j]._handle._dataIndex );;
                if ( !style ) {
                    if ( !buf[j].isRoot() ) {
                        CRLog::error("styleId=0 for node <%s> %d", LCSTR(buf[j].getNodeName()), buf[j].getDataIndex());
                        res = false;
                    }
                } else if ( _styles.get(style).isNull() ) {
                    CRLog::error("styleId!=0, but absent in cache for node <%s> %d", LCSTR(buf[j].getNodeName()), buf[j].getDataIndex());
                    res = false;
                }
                if ( !font ) {
                    if ( !buf[j].isRoot() ) {
                        CRLog::error("fontId=0 for node <%s>", LCSTR(buf[j].getNodeName()));
                        res = false;
                    }
                } else if ( _fonts.get(font).isNull() ) {
                    CRLog::error("fontId!=0, but absent in cache for node <%s>", LCSTR(buf[j].getNodeName()));
                    res = false;
                }
            }
        }
    }
    return res;
}

bool tinyNodeCollection::updateLoadedStyles( bool enabled )
{
    int count = ((_elemCount+TNC_PART_LEN-1) >> TNC_PART_SHIFT);
    bool res = true;
    LVArray<css_style_ref_t> * list = _styles.getIndex();

    _fontMap.clear(); // style index to font index

    for ( int i=0; i<count; i++ ) {
        int offs = i*TNC_PART_LEN;
        int sz = TNC_PART_LEN;
        if ( offs + sz > _elemCount+1 ) {
            sz = _elemCount+1 - offs;
        }
        ldomNode * buf = _elemList[i];
        for ( int j=0; j<sz; j++ ) {
            buf[j].setDocumentIndex( _docIndex );
            if ( buf[j].isElement() ) {
                lUInt16 style = getNodeStyleIndex( buf[j]._handle._dataIndex );
                if ( enabled && style!=0 ) {
                    css_style_ref_t s = list->get( style );
                    if ( !s.isNull() ) {
                        lUInt16 fntIndex = _fontMap.get( style );
                        if ( fntIndex==0 ) {
                            LVFontRef fnt = getFont( s.get() );
                            fntIndex = _fonts.cache( fnt );
                            if ( fnt.isNull() ) {
                                CRLog::error("font not found for style!");
                            } else {
                                _fontMap.set(style, fntIndex);
                            }
                        } else {
                            _fonts.addIndexRef( fntIndex );
                        }
                        if ( fntIndex<=0 ) {
                            CRLog::error("font caching failed for style!");
                            res = false;
                        } else {
                            setNodeFontIndex( buf[j]._handle._dataIndex, fntIndex );
                            //buf[j]._data._pelem._fontIndex = fntIndex;
                        }
                    } else {
                        CRLog::error("Loaded style index %d not found in style collection", (int)style);
                        setNodeFontIndex( buf[j]._handle._dataIndex, 0 );
                        setNodeStyleIndex( buf[j]._handle._dataIndex, 0 );
//                        buf[j]._data._pelem._styleIndex = 0;
//                        buf[j]._data._pelem._fontIndex = 0;
                        res = false;
                    }
                } else {
                    setNodeFontIndex( buf[j]._handle._dataIndex, 0 );
                    setNodeStyleIndex( buf[j]._handle._dataIndex, 0 );
//                    buf[j]._data._pelem._styleIndex = 0;
//                    buf[j]._data._pelem._fontIndex = 0;
                }
            }
        }
    }
#ifdef TODO_INVESTIGATE
    if ( enabled && res) {
        //_styles.setIndex( *list );
        // correct list reference counters

        for ( int i=0; i<list->length(); i++ ) {
            if ( !list->get(i).isNull() ) {
                // decrease reference counter
                // TODO:
                //_styles.release( list->get(i) );
            }
        }
    }
#endif
    delete list;
//    getRootNode()->setFont( _def_font );
//    getRootNode()->setStyle( _def_style );
    return res;
}

/// swaps to cache file or saves changes, limited by time interval
ContinuousOperationResult ldomDocument::swapToCache( CRTimerUtil & maxTime )
{
    if ( _maperror )
        return CR_ERROR;
    if ( !_mapped ) {
        if ( !createCacheFile() ) {
            CRLog::error("ldomDocument::swapToCache: failed: cannot create cache file");
            _maperror = true;
            return CR_ERROR;
        }
    }
    _mapped = true;
    if (!maxTime.infinite()) {
        CRLog::info("Cache file is created, but document saving is postponed");
        return CR_TIMEOUT;
    }
    ContinuousOperationResult res = saveChanges(maxTime);
    if ( res==CR_ERROR )
    {
        CRLog::error("Error while saving changes to cache file");
        _maperror = true;
        return CR_ERROR;
    }
    CRLog::info("Successfully saved document to cache file: %dK", _cacheFile->getSize()/1024 );
    return res;
}

/// saves recent changes to mapped file
ContinuousOperationResult ldomDocument::updateMap(CRTimerUtil & maxTime)
{
    if ( !_cacheFile || !_mapped )
        return CR_DONE;

    ContinuousOperationResult res = saveChanges(maxTime);
    if ( res==CR_ERROR )
    {
        CRLog::error("Error while saving changes to cache file");
        return CR_ERROR;
    }

    if ( res==CR_DONE ) {
        CRLog::info("Cache file updated successfully");
        dumpStatistics();
    }
    return res;
}

#endif

static const char * doccache_magic = "CoolReader3 Document Cache Directory Index\nV1.00\n";

/// document cache
class ldomDocCacheImpl : public ldomDocCache
{
    lString16 _cacheDir;
    lvsize_t _maxSize;

    struct FileItem {
        lString16 filename;
        lUInt32 size;
    };
    LVPtrVector<FileItem> _files;
public:
    ldomDocCacheImpl( lString16 cacheDir, lvsize_t maxSize )
        : _cacheDir( cacheDir ), _maxSize( maxSize )
    {
        LVAppendPathDelimiter( _cacheDir );
    }

    bool writeIndex()
    {
        lString16 filename = _cacheDir + L"cr3cache.inx";
        LVStreamRef stream = LVOpenFileStream( filename.c_str(), LVOM_WRITE );
        if ( !stream )
            return false;
        SerialBuf buf( 16384, true );
        buf.putMagic( doccache_magic );

        lUInt32 start = buf.pos();
        lUInt32 count = _files.length();
        buf << count;
        for ( unsigned i=0; i<count && !buf.error(); i++ ) {
            FileItem * item = _files[i];
            buf << item->filename;
            buf << item->size;
        }
        buf.putCRC( buf.pos() - start );

        if ( buf.error() )
            return false;
        if ( stream->Write( buf.buf(), buf.pos(), NULL )!=LVERR_OK )
            return false;
        return true;
    }

    bool readIndex(  )
    {
        lString16 filename = _cacheDir + L"cr3cache.inx";
        // read index
        lUInt32 totalSize = 0;
        LVStreamRef instream = LVOpenFileStream( filename.c_str(), LVOM_READ );
        if ( !instream.isNull() ) {
            LVStreamBufferRef sb = instream->GetReadBuffer(0, instream->GetSize() );
            if ( !sb )
                return false;
            SerialBuf buf( sb->getReadOnly(), sb->getSize() );
            if ( !buf.checkMagic( doccache_magic ) ) {
                CRLog::error("wrong cache index file format");
                return false;
            }

            lUInt32 start = buf.pos();
            lUInt32 count;
            buf >> count;
            for ( unsigned i=0; i<count && !buf.error(); i++ ) {
                FileItem * item = new FileItem();
                _files.add( item );
                buf >> item->filename;
                buf >> item->size;
                CRLog::trace("cache %d: %s [%d]", i, UnicodeToUtf8(item->filename).c_str(), (int)item->size );
                totalSize += item->size;
            }
            if ( !buf.checkCRC( buf.pos() - start ) ) {
                CRLog::error("CRC32 doesn't match in cache index file");
                return false;
            }

            if ( buf.error() )
                return false;

            CRLog::info( "Document cache index file read ok, %d files in cache, %d bytes", _files.length(), totalSize );
            return true;
        } else {
            CRLog::error( "Document cache index file cannot be read" );
            return false;
        }
    }

    /// remove all .cr3 files which are not listed in index
    bool removeExtraFiles( )
    {
        LVContainerRef container;
        container = LVOpenDirectory( _cacheDir.c_str(), L"*.cr3" );
        if ( container.isNull() ) {
            if ( !LVCreateDirectory( _cacheDir ) ) {
                CRLog::error("Cannot create directory %s", UnicodeToUtf8(_cacheDir).c_str() );
                return false;
            }
            container = LVOpenDirectory( _cacheDir.c_str(), L"*.cr3" );
            if ( container.isNull() ) {
                CRLog::error("Cannot open directory %s", UnicodeToUtf8(_cacheDir).c_str() );
                return false;
            }
        }
        for ( int i=0; i<container->GetObjectCount(); i++ ) {
            const LVContainerItemInfo * item = container->GetObjectInfo( i );
            if ( !item->IsContainer() ) {
                lString16 fn = item->GetName();
                if ( !fn.endsWith(L".cr3") )
                    continue;
                if ( findFileIndex(fn)<0 ) {
                    // delete file
                    CRLog::info("Removing cache file not specified in index: %s", UnicodeToUtf8(fn).c_str() );
                    if ( !LVDeleteFile( _cacheDir + fn ) ) {
                        CRLog::error("Error while removing cache file not specified in index: %s", UnicodeToUtf8(fn).c_str() );
                    }
                }
            }
        }
        return true;
    }

    // remove all extra files to add new one of specified size
    bool reserve( lvsize_t allocSize )
    {
        bool res = true;
        // remove extra files specified in list
        lvsize_t dirsize = allocSize;
        for ( int i=0; i<_files.length(); ) {
            if ( LVFileExists( _cacheDir + _files[i]->filename ) ) {
                if ( (i>0 || allocSize>0) && dirsize+_files[i]->size > _maxSize ) {
                    if ( LVDeleteFile( _cacheDir + _files[i]->filename ) ) {
                        _files.erase(i, 1);
                    } else {
                        CRLog::error("Cannot delete cache file %s", UnicodeToUtf8(_files[i]->filename).c_str() );
                        dirsize += _files[i]->size;
                        res = false;
                        i++;
                    }
                } else {
                    dirsize += _files[i]->size;
                    i++;
                }
            } else {
                CRLog::error("File %s is found in cache index, but does not exist", UnicodeToUtf8(_files[i]->filename).c_str() );
                _files.erase(i, 1);
            }
        }
        return res;
    }

    int findFileIndex( lString16 filename )
    {
        for ( int i=0; i<_files.length(); i++ ) {
            if ( _files[i]->filename == filename )
                return i;
        }
        return -1;
    }

    bool moveFileToTop( lString16 filename, lUInt32 size )
    {
        int index = findFileIndex( filename );
        if ( index<0 ) {
            FileItem * item = new FileItem();
            item->filename = filename;
            item->size = size;
            _files.insert( 0, item );
        } else {
            _files.move( 0, index );
            _files[0]->size = size;
        }
        return writeIndex();
    }

    bool init()
    {
        CRLog::info("Initialize document cache in directory %s", UnicodeToUtf8(_cacheDir).c_str() );
        // read index
        if ( readIndex(  ) ) {
            // read successfully
            // remove files not specified in list
            removeExtraFiles( );
        } else {
            if ( !LVCreateDirectory( _cacheDir ) ) {
                CRLog::error("Document Cache: cannot create cache directory %s, disabling cache", UnicodeToUtf8(_cacheDir).c_str() );
                return false;
            }
            _files.clear();

        }
        reserve(0);
        if ( !writeIndex() )
            return false; // cannot write index: read only?
        return true;
    }

    /// remove all files
    bool clear()
    {
        for ( int i=0; i<_files.length(); i++ )
            LVDeleteFile( _files[i]->filename );
        _files.clear();
        return writeIndex();
    }

    // dir/filename.{crc32}.cr3
    lString16 makeFileName( lString16 filename, lUInt32 crc, lUInt32 docFlags )
    {
        char s[16];
        sprintf(s, ".%08x.%d.cr3", (unsigned)crc, (int)docFlags);
        return filename + lString16( s ); //_cacheDir + 
    }

    /// open existing cache file stream
    LVStreamRef openExisting( lString16 filename, lUInt32 crc, lUInt32 docFlags )
    {
        lString16 fn = makeFileName( filename, crc, docFlags );
        LVStreamRef res;
        if ( findFileIndex( fn ) < 0 ) {
            CRLog::error( "ldomDocCache::openExisting - File %s is not found in cache index", UnicodeToUtf8(fn).c_str() );
            return res;
        }
        res = LVOpenFileStream( (_cacheDir+fn).c_str(), LVOM_APPEND|LVOM_FLAG_SYNC );
        if ( !res ) {
            CRLog::error( "ldomDocCache::openExisting - File %s is listed in cache index, but cannot be opened", UnicodeToUtf8(fn).c_str() );
            return res;
        }

#if ENABLED_BLOCK_WRITE_CACHE
        res = LVCreateBlockWriteStream( res, WRITE_CACHE_BLOCK_SIZE, WRITE_CACHE_BLOCK_COUNT );
#if TEST_BLOCK_STREAM

        LVStreamRef stream2 = LVOpenFileStream( (_cacheDir+fn+L"_c").c_str(), LVOM_APPEND );
        if ( !stream2 ) {
            CRLog::error( "ldomDocCache::createNew - file %s is cannot be created", UnicodeToUtf8(fn).c_str() );
            return stream2;
        }
        res = LVCreateCompareTestStream(res, stream2);
#endif
#endif

        lUInt32 fileSize = (lUInt32) res->GetSize();
        moveFileToTop( fn, fileSize );
        return res;
    }

    /// create new cache file
    LVStreamRef createNew( lString16 filename, lUInt32 crc, lUInt32 docFlags, lUInt32 fileSize )
    {
        lString16 fn = makeFileName( filename, crc, docFlags );
        LVStreamRef res;
        lString16 pathname( _cacheDir+fn );
        if ( findFileIndex( pathname ) >= 0 )
            LVDeleteFile( pathname );
        reserve( fileSize/10 );
        //res = LVMapFileStream( (_cacheDir+fn).c_str(), LVOM_APPEND, fileSize );
        LVDeleteFile( pathname ); // try to delete, ignore errors
        res = LVOpenFileStream( pathname.c_str(), LVOM_APPEND|LVOM_FLAG_SYNC );
        if ( !res ) {
            CRLog::error( "ldomDocCache::createNew - file %s is cannot be created", UnicodeToUtf8(fn).c_str() );
            return res;
        }
#if ENABLED_BLOCK_WRITE_CACHE
        res = LVCreateBlockWriteStream( res, WRITE_CACHE_BLOCK_SIZE, WRITE_CACHE_BLOCK_COUNT );
#if TEST_BLOCK_STREAM
        LVStreamRef stream2 = LVOpenFileStream( (pathname+L"_c").c_str(), LVOM_APPEND );
        if ( !stream2 ) {
            CRLog::error( "ldomDocCache::createNew - file %s is cannot be created", UnicodeToUtf8(fn).c_str() );
            return stream2;
        }
        res = LVCreateCompareTestStream(res, stream2);
#endif
#endif
        moveFileToTop( fn, fileSize );
        return res;
    }

    virtual ~ldomDocCacheImpl()
    {
    }
};

static ldomDocCacheImpl * _cacheInstance = NULL;

bool ldomDocCache::init( lString16 cacheDir, lvsize_t maxSize )
{
    if ( _cacheInstance )
        delete _cacheInstance;
    CRLog::info("Initialize document cache at %s (max size = %d)", UnicodeToUtf8(cacheDir).c_str(), (int)maxSize );
    _cacheInstance = new ldomDocCacheImpl( cacheDir, maxSize );
    if ( !_cacheInstance->init() ) {
        delete _cacheInstance;
        _cacheInstance = NULL;
        return false;
    }
    return true;
}

bool ldomDocCache::close()
{
    if ( !_cacheInstance )
        return false;
    delete _cacheInstance;
    _cacheInstance = NULL;
    return true;
}

/// open existing cache file stream
LVStreamRef ldomDocCache::openExisting( lString16 filename, lUInt32 crc, lUInt32 docFlags )
{
    if ( !_cacheInstance )
        return LVStreamRef();
    return _cacheInstance->openExisting( filename, crc, docFlags );
}

/// create new cache file
LVStreamRef ldomDocCache::createNew( lString16 filename, lUInt32 crc, lUInt32 docFlags, lUInt32 fileSize )
{
    if ( !_cacheInstance )
        return LVStreamRef();
    return _cacheInstance->createNew( filename, crc, docFlags, fileSize );
}

/// delete all cache files
bool ldomDocCache::clear()
{
    if ( !_cacheInstance )
        return false;
    return _cacheInstance->clear();
}

/// returns true if cache is enabled (successfully initialized)
bool ldomDocCache::enabled()
{
    return _cacheInstance!=NULL;
}

//void calcStyleHash( ldomNode * node, lUInt32 & value )
//{
//    if ( !node )
//        return;
//
//    if ( node->isText() || node->getRendMethod()==erm_invisible ) {
//        value = value * 75 + 1673251;
//        return; // don't go through invisible nodes
//    }
//
//    css_style_ref_t style = node->getStyle();
//    font_ref_t font = node->getFont();
//    lUInt32 styleHash = (!style) ? 4324324 : calcHash( style );
//    lUInt32 fontHash = (!font) ? 256371 : calcHash( font );
//    value = (value*75 + styleHash) * 75 + fontHash;
//
//    int cnt = node->getChildCount();
//    for ( int i=0; i<cnt; i++ ) {
//        calcStyleHash( node->getChildNode(i), value );
//    }
//}


#if BUILD_LITE!=1

/// save document formatting parameters after render
void ldomDocument::updateRenderContext()
{
    int dx = _page_width;
    int dy = _page_height;
    lUInt32 styleHash = calcStyleHash();
    lUInt32 stylesheetHash = (((_stylesheet.getHash() * 31) + calcHash(_def_style))*31 + calcHash(_def_font));
    //calcStyleHash( getRootNode(), styleHash );
    _hdr.render_style_hash = styleHash;
    _hdr.stylesheet_hash = stylesheetHash;
    _hdr.render_dx = dx;
    _hdr.render_dy = dy;
    _hdr.render_docflags = _docFlags;
    CRLog::info("Updating render properties: styleHash=%x, stylesheetHash=%x, docflags=%x, width=%x, height=%x",
                _hdr.render_style_hash, _hdr.stylesheet_hash, _hdr.render_docflags, _hdr.render_dx, _hdr.render_dy);
}

/// check document formatting parameters before render - whether we need to reformat; returns false if render is necessary
bool ldomDocument::checkRenderContext()
{
    int dx = _page_width;
    int dy = _page_height;
    lUInt32 styleHash = calcStyleHash();
    lUInt32 stylesheetHash = (((_stylesheet.getHash() * 31) + calcHash(_def_style))*31 + calcHash(_def_font));
    //calcStyleHash( getRootNode(), styleHash );
    bool res = true;
    if ( styleHash != _hdr.render_style_hash ) {
        CRLog::info("checkRenderContext: Style hash doesn't match %x!=%x", styleHash, _hdr.render_style_hash);
        res = false;
    } else if ( stylesheetHash != _hdr.stylesheet_hash ) {
        CRLog::info("checkRenderContext: Stylesheet hash doesn't match %x!=%x", stylesheetHash, _hdr.stylesheet_hash);
        res = false;
    } else if ( _docFlags != _hdr.render_docflags ) {
        CRLog::info("checkRenderContext: Doc flags don't match %x!=%x", _docFlags, _hdr.render_docflags);
        res = false;
    } else if ( dx != (int)_hdr.render_dx ) {
        CRLog::info("checkRenderContext: Width doesn't match %x!=%x", dx, (int)_hdr.render_dx);
        res = false;
    } else if ( dy != (int)_hdr.render_dy ) {
        CRLog::info("checkRenderContext: Page height doesn't match %x!=%x", dy, (int)_hdr.render_dy);
        res = false;
    }
    if ( res ) {

        //if ( pages->length()==0 ) {
//            _pagesData.reset();
//            pages->deserialize( _pagesData );
        //}

        return true;
    }
//    _hdr.render_style_hash = styleHash;
//    _hdr.stylesheet_hash = stylesheetHash;
//    _hdr.render_dx = dx;
//    _hdr.render_dy = dy;
//    _hdr.render_docflags = _docFlags;
//    CRLog::info("New render properties: styleHash=%x, stylesheetHash=%x, docflags=%x, width=%x, height=%x",
//                _hdr.render_style_hash, _hdr.stylesheet_hash, _hdr.render_docflags, _hdr.render_dx, _hdr.render_dy);
    return false;
}

#endif

void lxmlDocBase::setStyleSheet( const char * css, bool replace )
{
    lUInt32 oldHash = _stylesheet.getHash();
    if ( replace ) {
        //CRLog::debug("cleaning stylesheet contents");
        _stylesheet.clear();
    }
    if ( css && *css ) {
        //CRLog::debug("appending stylesheet contents: \n%s", css);
        _stylesheet.parse( css );
    }
    lUInt32 newHash = _stylesheet.getHash();
    if (oldHash != newHash) {
        CRLog::debug("New stylesheet hash: %08x", newHash);
    }
}






//=====================================================
// ldomElement declaration placed here to hide DOM implementation
// use ldomNode rich interface instead
class tinyElement
{
    friend class ldomNode;
private:
    ldomDocument * _document;
    ldomNode * _parentNode;
    lUInt16 _id;
    lUInt16 _nsid;
    LVArray < lInt32 > _children;
    ldomAttributeCollection _attrs;
    lvdom_element_render_method _rendMethod;
public:
    tinyElement( ldomDocument * document, ldomNode * parentNode, lUInt16 nsid, lUInt16 id )
    : _document(document), _parentNode(parentNode), _id(id), _nsid(nsid), _rendMethod(erm_invisible)
    { _document->_tinyElementCount++; }
    /// destructor
    ~tinyElement() { _document->_tinyElementCount--; }
};


#define NPELEM _data._elem_ptr
#define NPTEXT _data._text_ptr._str

//=====================================================

/// minimize memory consumption
void tinyNodeCollection::compact()
{
    _textStorage.compact(0xFFFFFF);
    _elemStorage.compact(0xFFFFFF);
    _rectStorage.compact(0xFFFFFF);
    _styleStorage.compact(0xFFFFFF);
}

/// allocate new tinyElement
ldomNode * tinyNodeCollection::allocTinyElement( ldomNode * parent, lUInt16 nsid, lUInt16 id )
{
    ldomNode * node = allocTinyNode( ldomNode::NT_ELEMENT );
    tinyElement * elem = new tinyElement( (ldomDocument*)this, parent, nsid, id );
    node->NPELEM = elem;
    return node;
}

static void readOnlyError()
{
    crFatalError( 125, "Text node is persistent (read-only)! Call modify() to get r/w instance." );
}

//=====================================================

// shortcut for dynamic element accessor
#ifdef _DEBUG
  #define ASSERT_NODE_NOT_NULL \
    if ( isNull() ) \
		crFatalError( 1313, "Access to null node" )
#else
  #define ASSERT_NODE_NOT_NULL
#endif

/// returns node level, 0 is root node
lUInt8 ldomNode::getNodeLevel() const
{
    const ldomNode * node = this;
    int level = 0;
    for ( ; node; node = node->getParentNode() )
        level++;
    return level;
}

void ldomNode::onCollectionDestroy()
{
    if ( isNull() )
        return;
    //CRLog::trace("ldomNode::onCollectionDestroy(%d) type=%d", this->_handle._dataIndex, TNTYPE);
    switch ( TNTYPE ) {
    case NT_TEXT:
        delete _data._text_ptr;
        _data._text_ptr = NULL;
        break;
    case NT_ELEMENT:
        // ???
#if BUILD_LITE!=1
        getDocument()->clearNodeStyle( _handle._dataIndex );
#endif
        delete NPELEM;
        NPELEM = NULL;
        break;
#if BUILD_LITE!=1
    case NT_PTEXT:      // immutable (persistent) text node
        // do nothing
        break;
    case NT_PELEMENT:   // immutable (persistent) element node
        // do nothing
        break;
#endif
    }
}

void ldomNode::destroy()
{
    if ( isNull() )
        return;
    //CRLog::trace("ldomNode::destroy(%d) type=%d", this->_handle._dataIndex, TNTYPE);
    switch ( TNTYPE ) {
    case NT_TEXT:
        delete _data._text_ptr;
        break;
    case NT_ELEMENT:
        {
#if BUILD_LITE!=1
            getDocument()->clearNodeStyle(_handle._dataIndex);
#endif
            tinyElement * me = NPELEM;
            // delete children
            for ( int i=0; i<me->_children.length(); i++ ) {
                ldomNode * child = getDocument()->getTinyNode(me->_children[i]);
                if ( child )
                    child->destroy();
            }
            delete me;
            NPELEM = NULL;
        }
        delete NPELEM;
        break;
#if BUILD_LITE!=1
    case NT_PTEXT:
        // disable removing from storage: to minimize modifications
        //_document->_textStorage.freeNode( _data._ptext_addr._addr );
        break;
    case NT_PELEMENT:   // immutable (persistent) element node
        {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            for ( int i=0; i<me->childCount; i++ )
                getDocument()->getTinyNode( me->children[i] )->destroy();
            getDocument()->clearNodeStyle( _handle._dataIndex );
//            getDocument()->_styles.release( _data._pelem._styleIndex );
//            getDocument()->_fonts.release( _data._pelem._fontIndex );
//            _data._pelem._styleIndex = 0;
//            _data._pelem._fontIndex = 0;
            getDocument()->_elemStorage.freeNode( _data._pelem_addr );
        }
        break;
#endif
    }
    getDocument()->recycleTinyNode( _handle._dataIndex );
}

/// returns index of child node by dataIndex
int ldomNode::getChildIndex( lUInt32 dataIndex ) const
{
    dataIndex &= 0xFFFFFFF0;
    ASSERT_NODE_NOT_NULL;
    int parentIndex = -1;
    switch ( TNTYPE ) {
    case NT_ELEMENT:
        {
            tinyElement * me = NPELEM;
            for ( int i=0; i<me->_children.length(); i++ ) {
                if ( (me->_children[i] & 0xFFFFFFF0) == dataIndex ) {
                    // found
                    parentIndex = i;
                    break;
                }
            }
        }
        break;
#if BUILD_LITE!=1
    case NT_PELEMENT:
        {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            for ( int i=0; i<me->childCount; i++ ) {
                if ( (me->children[i] & 0xFFFFFFF0) == dataIndex ) {
                    // found
                    parentIndex = i;
                    break;
                }
            }
        }
        break;
    case NT_PTEXT:      // immutable (persistent) text node
#endif
    case NT_TEXT:
        break;
    }
    return parentIndex;
}

/// returns index of node inside parent's child collection
int ldomNode::getNodeIndex() const
{
    ASSERT_NODE_NOT_NULL;
    ldomNode * parent = getParentNode();
    if ( parent )
        return parent->getChildIndex( getDataIndex() );
    return 0;
}

/// returns true if node is document's root
bool ldomNode::isRoot() const
{
    ASSERT_NODE_NOT_NULL;
    switch ( TNTYPE ) {
    case NT_ELEMENT:
        return !NPELEM->_parentNode;
#if BUILD_LITE!=1
    case NT_PELEMENT:   // immutable (persistent) element node
        {
             ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
             return me->parentIndex==0;
        }
        break;
    case NT_PTEXT:      // immutable (persistent) text node
        {
            return getDocument()->_textStorage.getParent( _data._ptext_addr )==0;
        }
#endif
    case NT_TEXT:
        return _data._text_ptr->getParentIndex()==0;
    }
    return false;
}

/// call to invalidate cache if persistent node content is modified
void ldomNode::modified()
{
#if BUILD_LITE!=1
    if ( isPersistent() ) {
        if ( isElement() )
            getDocument()->_elemStorage.modified( _data._pelem_addr );
        else
            getDocument()->_textStorage.modified( _data._ptext_addr );
    }
#endif
}

/// changes parent of item
void ldomNode::setParentNode( ldomNode * parent )
{
    ASSERT_NODE_NOT_NULL;
#ifdef TRACE_AUTOBOX
    if ( getParentNode()!=NULL && parent != NULL )
        CRLog::trace("Changing parent of %d from %d to %d", getDataIndex(), getParentNode()->getDataIndex(), parent->getDataIndex());
#endif
    switch ( TNTYPE ) {
    case NT_ELEMENT:
        NPELEM->_parentNode = parent;
        break;
#if BUILD_LITE!=1
    case NT_PELEMENT:   // immutable (persistent) element node
        {
            lUInt32 parentIndex = parent->_handle._dataIndex;
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            if ( me->parentIndex != (int)parentIndex ) {
                me->parentIndex = parentIndex;
                modified();
            }
        }
        break;
    case NT_PTEXT:      // immutable (persistent) text node
        {
            lUInt32 parentIndex = parent->_handle._dataIndex;
            getDocument()->_textStorage.setParent(_data._ptext_addr, parentIndex);
            //_data._ptext_addr._parentIndex = parentIndex;
            //_document->_textStorage.setTextParent( _data._ptext_addr._addr, parentIndex );
        }
        break;
#endif
    case NT_TEXT:
        {
            lUInt32 parentIndex = parent->_handle._dataIndex;
            _data._text_ptr->setParentIndex( parentIndex );
        }
        break;
    }
}

/// returns dataIndex of node's parent, 0 if no parent
int ldomNode::getParentIndex() const
{
    ASSERT_NODE_NOT_NULL;

    switch ( TNTYPE ) {
    case NT_ELEMENT:
        return NPELEM->_parentNode ? NPELEM->_parentNode->getDataIndex() : 0;
#if BUILD_LITE!=1
    case NT_PELEMENT:   // immutable (persistent) element node
        {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            return me->parentIndex;
        }
        break;
    case NT_PTEXT:      // immutable (persistent) text node
        return getDocument()->_textStorage.getParent(_data._ptext_addr);
#endif
    case NT_TEXT:
        return _data._text_ptr->getParentIndex();
    }
    return 0;
}

/// returns pointer to parent node, NULL if node has no parent
ldomNode * ldomNode::getParentNode() const
{
    ASSERT_NODE_NOT_NULL;
    int parentIndex = 0;
    switch ( TNTYPE ) {
    case NT_ELEMENT:
        return NPELEM->_parentNode;
#if BUILD_LITE!=1
    case NT_PELEMENT:   // immutable (persistent) element node
        {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            parentIndex = me->parentIndex;
        }
        break;
    case NT_PTEXT:      // immutable (persistent) text node
        parentIndex = getDocument()->_textStorage.getParent(_data._ptext_addr);
        break;
#endif
    case NT_TEXT:
        parentIndex = _data._text_ptr->getParentIndex();
        break;
    }
    return parentIndex ? getTinyNode(parentIndex) : NULL;
}

/// returns true child node is element
bool ldomNode::isChildNodeElement( lUInt32 index ) const
{
    ASSERT_NODE_NOT_NULL;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        int n = me->_children[index];
        return ( (n & 1)==1 );
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        int n = me->children[index];
        return ( (n & 1)==1 );
    }
#endif
}

/// returns true child node is text
bool ldomNode::isChildNodeText( lUInt32 index ) const
{
    ASSERT_NODE_NOT_NULL;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        int n = me->_children[index];
        return ( (n & 1)==0 );
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        int n = me->children[index];
        return ( (n & 1)==0 );
    }
#endif
}

/// returns child node by index, NULL if node with this index is not element or nodeTag!=0 and element node name!=nodeTag
ldomNode * ldomNode::getChildElementNode( lUInt32 index, const lChar16 * nodeTag ) const
{
    lUInt16 nodeId = getDocument()->getElementNameIndex(nodeTag);
    return getChildElementNode( index, nodeId );
}

/// returns child node by index, NULL if node with this index is not element or nodeId!=0 and element node id!=nodeId
ldomNode * ldomNode::getChildElementNode( lUInt32 index, lUInt16 nodeId ) const
{
    ASSERT_NODE_NOT_NULL;
    ldomNode * res = NULL;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        int n = me->_children[index];
        if ( (n & 1)==0 ) // not element
            return NULL;
        res = getTinyNode( n );
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        int n = me->children[index];
        if ( (n & 1)==0 ) // not element
            return NULL;
        res = getTinyNode( n );
    }
#endif
    if ( res && nodeId!=0 && res->getNodeId()!=nodeId )
        res = NULL;
    return res;
}

/// returns child node by index
ldomNode * ldomNode::getChildNode( lUInt32 index ) const
{
    ASSERT_NODE_NOT_NULL;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        return getTinyNode( me->_children[index] );
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        return getTinyNode( me->children[index] );
    }
#endif
}

/// returns element child count
lUInt32 ldomNode::getChildCount() const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return 0;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        return me->_children.length();
#if BUILD_LITE!=1
    } else {
        // persistent element
        // persistent element
        {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
//            if ( me==NULL ) { // DEBUG
//                me = _document->_elemStorage.getElem( _data._pelem_addr );
//            }
            return me->childCount;
        }
    }
    return 0; // TODO
#endif
}

/// returns element attribute count
lUInt32 ldomNode::getAttrCount() const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return 0;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        return me->_attrs.length();
#if BUILD_LITE!=1
    } else {
        // persistent element
        {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            return me->attrCount;
        }
    }
    return 0;
#endif
}

/// returns attribute value by attribute name id and namespace id
const lString16 & ldomNode::getAttributeValue( lUInt16 nsid, lUInt16 id ) const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return lString16::empty_str;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        lUInt16 valueId = me->_attrs.get( nsid, id );
        if ( valueId==LXML_ATTR_VALUE_NONE )
            return lString16::empty_str;
        return getDocument()->getAttrValue(valueId);
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        lUInt16 valueId = me->getAttrValueId( nsid, id );
        if ( valueId==LXML_ATTR_VALUE_NONE )
            return lString16::empty_str;
        return getDocument()->getAttrValue(valueId);
    }
    return lString16::empty_str;
#endif
}

/// returns attribute value by attribute name and namespace
const lString16 & ldomNode::getAttributeValue( const lChar16 * nsName, const lChar16 * attrName ) const
{
    ASSERT_NODE_NOT_NULL;
    lUInt16 nsId = (nsName&&nsName[0]) ? getDocument()->getNsNameIndex( nsName ) : LXML_NS_ANY;
    lUInt16 attrId = getDocument()->getAttrNameIndex( attrName );
    return getAttributeValue( nsId, attrId );
}

/// returns attribute by index
const lxmlAttribute * ldomNode::getAttribute( lUInt32 index ) const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return NULL;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        return me->_attrs[index];
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        return me->attr( index );
    }
#endif
}

/// returns true if element node has attribute with specified name id and namespace id
bool ldomNode::hasAttribute( lUInt16 nsid, lUInt16 id ) const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return false;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        tinyElement * me = NPELEM;
        lUInt16 valueId = me->_attrs.get( nsid, id );
        return ( valueId!=LXML_ATTR_VALUE_NONE );
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        return (me->findAttr( nsid, id ) != NULL);
    }
#endif
}

/// returns attribute name by index
const lString16 & ldomNode::getAttributeName( lUInt32 index ) const
{
    ASSERT_NODE_NOT_NULL;
    const lxmlAttribute * attr = getAttribute( index );
    if ( attr )
        return getDocument()->getAttrName( attr->id );
    return lString16::empty_str;
}

/// sets attribute value
void ldomNode::setAttributeValue( lUInt16 nsid, lUInt16 id, const lChar16 * value )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return;
    int valueIndex = getDocument()->getAttrValueIndex(value);
#if BUILD_LITE!=1
    if ( isPersistent() ) {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        lxmlAttribute * attr = me->findAttr( nsid, id );
        if ( attr ) {
            attr->index = valueIndex;
            modified();
            return;
        }
        // else: convert to modifable and continue as non-persistent
        modify();
    }
#endif
    // element
    tinyElement * me = NPELEM;
    me->_attrs.set(nsid, id, valueIndex);
    if (nsid == LXML_NS_NONE)
        getDocument()->onAttributeSet( id, valueIndex, this );
}

/// returns element type structure pointer if it was set in document for this element name
const css_elem_def_props_t * ldomNode::getElementTypePtr()
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return NULL;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        const css_elem_def_props_t * res = getDocument()->getElementTypePtr(NPELEM->_id);
//        if ( res && res->is_object ) {
//            CRLog::trace("Object found");
//        }
        return res;
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        const css_elem_def_props_t * res = getDocument()->getElementTypePtr(me->id);
//        if ( res && res->is_object ) {
//            CRLog::trace("Object found");
//        }
        return res;
    }
#endif
}

/// returns element name id
lUInt16 ldomNode::getNodeId() const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return 0;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
        // element
#endif
        return NPELEM->_id;
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        return me->id;
    }
#endif
}

/// returns element namespace id
lUInt16 ldomNode::getNodeNsId() const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return 0;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
        // element
#endif
        return NPELEM->_nsid;
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        return me->nsid;
    }
#endif
}

/// replace element name id with another value
void ldomNode::setNodeId( lUInt16 id )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
        // element
#endif
        NPELEM->_id = id;
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        me->id = id;
        modified();
    }
#endif
}

/// returns element name
const lString16 & ldomNode::getNodeName() const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return lString16::empty_str;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
        // element
#endif
        return getDocument()->getElementName(NPELEM->_id);
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        return getDocument()->getElementName(me->id);
    }
#endif
}

/// returns element namespace name
const lString16 & ldomNode::getNodeNsName() const
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return lString16::empty_str;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
#endif
        // element
        return getDocument()->getNsName(NPELEM->_nsid);
#if BUILD_LITE!=1
    } else {
        // persistent element
        ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
        return getDocument()->getNsName(me->nsid);
    }
#endif
}



/// returns text node text as wide string
lString16 ldomNode::getText( lChar16 blockDelimiter, int maxSize ) const
{
    ASSERT_NODE_NOT_NULL;
    switch ( TNTYPE ) {
#if BUILD_LITE!=1
    case NT_PELEMENT:
#endif
    case NT_ELEMENT:
        {
            lString16 txt;
            unsigned cc = getChildCount();
            for ( unsigned i=0; i<cc; i++ ) {
                ldomNode * child = getChildNode(i);
                txt += child->getText(blockDelimiter, maxSize);
                if ( maxSize!=0 && txt.length()>(unsigned)maxSize )
                    break;
                if ( i>=cc-1 )
                    break;
#if BUILD_LITE!=1
                if ( blockDelimiter && child->isElement() ) {
                    if ( !child->getStyle().isNull() && child->getStyle()->display == css_d_block )
                        txt << blockDelimiter;
                }
#endif
            }
            return txt;
        }
        break;
#if BUILD_LITE!=1
    case NT_PTEXT:
        return Utf8ToUnicode(getDocument()->_textStorage.getText( _data._ptext_addr ));
#endif
    case NT_TEXT:
        return _data._text_ptr->getText16();
    }
    return lString16::empty_str;
}

/// returns text node text as utf8 string
lString8 ldomNode::getText8( lChar8 blockDelimiter, int maxSize ) const
{
    ASSERT_NODE_NOT_NULL;
    switch ( TNTYPE ) {
    case NT_ELEMENT:
#if BUILD_LITE!=1
    case NT_PELEMENT:
        {
            lString8 txt;
            unsigned cc = getChildCount();
            for ( unsigned i=0; i<cc; i++ ) {
                ldomNode * child = getChildNode(i);
                txt += child->getText8(blockDelimiter, maxSize);
                if ( maxSize!=0 && txt.length()>(unsigned)maxSize )
                    break;
                if ( i>=getChildCount()-1 )
                    break;
                if ( blockDelimiter && child->isElement() ) {
                    if ( child->getStyle()->display == css_d_block )
                        txt << blockDelimiter;
                }
            }
            return txt;
        }
        break;
    case NT_PTEXT:
        return getDocument()->_textStorage.getText( _data._ptext_addr );
#endif
    case NT_TEXT:
        return _data._text_ptr->getText();
    }
    return lString8::empty_str;
}

/// sets text node text as wide string
void ldomNode::setText( lString16 str )
{
    ASSERT_NODE_NOT_NULL;
    switch ( TNTYPE ) {
    case NT_ELEMENT:
        readOnlyError();
        break;
#if BUILD_LITE!=1
    case NT_PELEMENT:
        readOnlyError();
        break;
    case NT_PTEXT:
        {
            // convert persistent text to mutable
            lUInt32 parentIndex = getDocument()->_textStorage.getParent(_data._ptext_addr);
            getDocument()->_textStorage.freeNode( _data._ptext_addr );
            _data._text_ptr = new ldomTextNode( parentIndex, UnicodeToUtf8(str) );
            // change type from PTEXT to TEXT
            _handle._dataIndex = (_handle._dataIndex & ~0xF) | NT_TEXT;
        }
        break;
#endif
    case NT_TEXT:
        {
            _data._text_ptr->setText( str );
        }
        break;
    }
}

/// sets text node text as utf8 string
void ldomNode::setText8( lString8 utf8 )
{
    ASSERT_NODE_NOT_NULL;
    switch ( TNTYPE ) {
    case NT_ELEMENT:
        readOnlyError();
        break;
#if BUILD_LITE!=1
    case NT_PELEMENT:
        readOnlyError();
        break;
    case NT_PTEXT:
        {
            // convert persistent text to mutable
            lUInt32 parentIndex = getDocument()->_textStorage.getParent(_data._ptext_addr);
            getDocument()->_textStorage.freeNode( _data._ptext_addr );
            _data._text_ptr = new ldomTextNode( parentIndex, utf8 );
            // change type from PTEXT to TEXT
            _handle._dataIndex = (_handle._dataIndex & ~0xF) | NT_TEXT;
        }
        break;
#endif
    case NT_TEXT:
        {
            _data._text_ptr->setText( utf8 );
        }
        break;
    }
}

#if BUILD_LITE!=1
/// returns node absolute rectangle
void ldomNode::getAbsRect( lvRect & rect )
{
    ASSERT_NODE_NOT_NULL;
    ldomNode * node = this;
    RenderRectAccessor fmt( node );
    rect.left = fmt.getX();
    rect.top = fmt.getY();
    rect.right = fmt.getWidth();
    rect.bottom = fmt.getHeight();
    node = node->getParentNode();
    for (; node; node = node->getParentNode())
    {
        RenderRectAccessor fmt( node );
        rect.left += fmt.getX();
        rect.top += fmt.getY();
    }
    rect.bottom += rect.top;
    rect.right += rect.left;
}

/// returns render data structure
void ldomNode::getRenderData( lvdomElementFormatRec & dst)
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() ) {
        dst.clear();
        return;
    }
    getDocument()->_rectStorage.getRendRectData(_handle._dataIndex, &dst);
}

/// sets new value for render data structure
void ldomNode::setRenderData( lvdomElementFormatRec & newData)
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return;
    getDocument()->_rectStorage.setRendRectData(_handle._dataIndex, &newData);
}

/// sets node rendering structure pointer
void ldomNode::clearRenderData()
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return;
    lvdomElementFormatRec rec;
    getDocument()->_rectStorage.setRendRectData(_handle._dataIndex, &rec);
}
#endif

/// calls specified function recursively for all elements of DOM tree, children before parent
void ldomNode::recurseElementsDeepFirst( void (*pFun)( ldomNode * node ) )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return;
    int cnt = getChildCount();
    for (int i=0; i<cnt; i++)
    {
        ldomNode * child = getChildNode( i );
        if ( child->isElement() )
        {
            child->recurseElementsDeepFirst( pFun );
        }
    }
    pFun( this );
}

#if BUILD_LITE!=1
static void updateRendMethod( ldomNode * node )
{
    node->initNodeRendMethod();
}

/// init render method for the whole subtree
void ldomNode::initNodeRendMethodRecursive()
{
    recurseElementsDeepFirst( updateRendMethod );
}
#endif

#if 0
static void updateStyleData( ldomNode * node )
{
    if ( node->getNodeId()==el_DocFragment )
        node->applyNodeStylesheet();
    node->initNodeStyle();
}
#endif

#if BUILD_LITE!=1
static void updateStyleDataRecursive( ldomNode * node )
{
    if ( !node->isElement() )
        return;
    bool styleSheetChanged = false;
    if ( node->getNodeId()==el_DocFragment )
        styleSheetChanged = node->applyNodeStylesheet();
    node->initNodeStyle();
    int n = node->getChildCount();
    for ( int i=0; i<n; i++ ) {
        ldomNode * child = node->getChildNode(i);
        if ( child->isElement() )
            updateStyleDataRecursive( child );
    }
    if ( styleSheetChanged )
        node->getDocument()->getStyleSheet()->pop();
}

/// init render method for the whole subtree
void ldomNode::initNodeStyleRecursive()
{
    getDocument()->_fontMap.clear();
    updateStyleDataRecursive( this );
    //recurseElements( updateStyleData );
}
#endif

/// calls specified function recursively for all elements of DOM tree
void ldomNode::recurseElements( void (*pFun)( ldomNode * node ) )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return;
    pFun( this );
    int cnt = getChildCount();
    for (int i=0; i<cnt; i++)
    {
        ldomNode * child = getChildNode( i );
        if ( child->isElement() )
        {
            child->recurseElements( pFun );
        }
    }
}

/// calls specified function recursively for all nodes of DOM tree
void ldomNode::recurseNodes( void (*pFun)( ldomNode * node ) )
{
    ASSERT_NODE_NOT_NULL;
    pFun( this );
    if ( isElement() )
    {
        int cnt = getChildCount();
        for (int i=0; i<cnt; i++)
        {
            ldomNode * child = getChildNode( i );
            child->recurseNodes( pFun );
        }
    }
}

/// returns first text child element
ldomNode * ldomNode::getFirstTextChild(bool skipEmpty)
{
    ASSERT_NODE_NOT_NULL;
    if ( isText() ) {
        if ( !skipEmpty )
            return this;
        lString16 txt = getText();
        bool nonSpaceFound = false;
        for ( unsigned i=0; i<txt.length(); i++ ) {
            lChar16 ch = txt[i];
            if ( ch!=' ' && ch!='\t' && ch!='\r' && ch!='\n' ) {
                nonSpaceFound = true;
                break;
            }
        }
        if ( nonSpaceFound )
            return this;
        return NULL;
    }
    for ( int i=0; i<(int)getChildCount(); i++ ) {
        ldomNode * p = getChildNode(i)->getFirstTextChild(skipEmpty);
        if (p)
            return p;
    }
    return NULL;
}

/// returns last text child element
ldomNode * ldomNode::getLastTextChild()
{
    ASSERT_NODE_NOT_NULL;
    if ( isText() )
        return this;
    else {
        for ( int i=(int)getChildCount()-1; i>=0; i-- ) {
            ldomNode * p = getChildNode(i)->getLastTextChild();
            if (p)
                return p;
        }
    }
    return NULL;
}

#if BUILD_LITE!=1
/// find node by coordinates of point in formatted document
ldomNode * ldomNode::elementFromPoint( lvPoint pt, int direction )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return NULL;
    ldomNode * enode = this;
    RenderRectAccessor fmt( this );
    if ( enode->getRendMethod() == erm_invisible ) {
        return NULL;
    }
    if ( pt.y < fmt.getY() ) {
        if ( direction>0 && enode->getRendMethod() == erm_final )
            return this;
        return NULL;
    }
    if ( pt.y >= fmt.getY() + fmt.getHeight() ) {
        if ( direction<0 && enode->getRendMethod() == erm_final )
            return this;
        return NULL;
    }
    if ( enode->getRendMethod() == erm_final ) {
        return this;
    }
    int count = getChildCount();
    if ( direction>=0 ) {
        for ( int i=0; i<count; i++ ) {
            ldomNode * p = getChildNode( i );
            ldomNode * e = p->elementFromPoint( lvPoint( pt.x - fmt.getX(),
                    pt.y - fmt.getY() ), direction );
            if ( e )
                return e;
        }
    } else {
        for ( int i=count-1; i>=0; i-- ) {
            ldomNode * p = getChildNode( i );
            ldomNode * e = p->elementFromPoint( lvPoint( pt.x - fmt.getX(),
                    pt.y - fmt.getY() ), direction );
            if ( e )
                return e;
        }
    }
    return this;
}

/// find final node by coordinates of point in formatted document
ldomNode * ldomNode::finalBlockFromPoint( lvPoint pt )
{
    ASSERT_NODE_NOT_NULL;
    ldomNode * elem = elementFromPoint( pt, 0 );
    if ( elem && elem->getRendMethod() == erm_final )
        return elem;
    return NULL;
}
#endif

/// returns rendering method
lvdom_element_render_method ldomNode::getRendMethod()
{
    ASSERT_NODE_NOT_NULL;
    if ( isElement() ) {
#if BUILD_LITE!=1
        if ( !isPersistent() ) {
#endif
            return NPELEM->_rendMethod;
#if BUILD_LITE!=1
        } else {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            return (lvdom_element_render_method)me->rendMethod;
        }
#endif
    }
    return erm_invisible;
}

/// sets rendering method
void ldomNode::setRendMethod( lvdom_element_render_method method )
{
    ASSERT_NODE_NOT_NULL;
    if ( isElement() ) {
#if BUILD_LITE!=1
        if ( !isPersistent() ) {
#endif
            NPELEM->_rendMethod = method;
#if BUILD_LITE!=1
        } else {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            if ( me->rendMethod != method ) {
                me->rendMethod = method;
                modified();
            }
        }
#endif
    }
}

#if BUILD_LITE!=1
/// returns element style record
css_style_ref_t ldomNode::getStyle()
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return css_style_ref_t();
    css_style_ref_t res = getDocument()->getNodeStyle( _handle._dataIndex );
    return res;
}

/// returns element font
font_ref_t ldomNode::getFont()
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return font_ref_t();
    return getDocument()->getNodeFont( _handle._dataIndex );
}

/// sets element font
void ldomNode::setFont( font_ref_t font )
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
        getDocument()->setNodeFont( _handle._dataIndex, font );
    }
}

/// sets element style record
void ldomNode::setStyle( css_style_ref_t & style )
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
        getDocument()->setNodeStyle( _handle._dataIndex, style );
    }
}

bool ldomNode::initNodeFont()
{
    if ( !isElement() )
        return false;
    lUInt16 style = getDocument()->getNodeStyleIndex( _handle._dataIndex );
    lUInt16 font = getDocument()->getNodeFontIndex( _handle._dataIndex );
    lUInt16 fntIndex = getDocument()->_fontMap.get( style );
    if ( fntIndex==0 ) {
        css_style_ref_t s = getDocument()->_styles.get( style );
        if ( s.isNull() ) {
            CRLog::error("style not found for index %d", style);
            s = getDocument()->_styles.get( style );
        }
        LVFontRef fnt = ::getFont( s.get() );
        fntIndex = getDocument()->_fonts.cache( fnt );
        if ( fnt.isNull() ) {
            CRLog::error("font not found for style!");
            return false;
        } else {
            getDocument()->_fontMap.set(style, fntIndex);
        }
        if ( font != 0 ) {
            if ( font!=fntIndex ) // ???
                getDocument()->_fonts.release(font);
        }
        getDocument()->setNodeFontIndex( _handle._dataIndex, fntIndex);
        return true;
    } else {
        if ( font!=fntIndex )
            getDocument()->_fonts.addIndexRef( fntIndex );
    }
    if ( fntIndex<=0 ) {
        CRLog::error("font caching failed for style!");
        return false;;
    } else {
        getDocument()->setNodeFontIndex( _handle._dataIndex, fntIndex);
    }
    return true;
}

void ldomNode::initNodeStyle()
{
    // assume all parent styles already initialized
    if ( !getDocument()->isDefStyleSet() )
        return;
    if ( isElement() ) {
        if ( isRoot() || getParentNode()->isRoot() )
        {
            setNodeStyle( this,
                getDocument()->getDefaultStyle(),
                getDocument()->getDefaultFont()
            );
        }
        else
        {
            ldomNode * parent = getParentNode();

            // DEBUG TEST
            if ( parent->getChildIndex( getDataIndex() )<0 ) {
                CRLog::error("Invalid parent->child relation for nodes %d->%d", parent->getDataIndex(), getDataIndex() );
            }


            //lvdomElementFormatRec * parent_fmt = node->getParentNode()->getRenderData();
            css_style_ref_t style = parent->getStyle();
            LVFontRef font = parent->getFont();
#if DEBUG_DOM_STORAGE==1
            if ( style.isNull() ) {
                // for debugging
                CRLog::error("NULL style is returned for node <%s> %d level=%d  "
                             "parent <%s> %d level=%d children %d childIndex=%d",
                             LCSTR(getNodeName()), getDataIndex(), getNodeLevel(),
                             LCSTR(parent->getNodeName()), parent->getDataIndex(),
                             parent->getNodeLevel(), parent->getChildCount(), parent->getChildIndex(getDataIndex()) );

                style = parent->getStyle();
            }
#endif
            setNodeStyle( this,
                style,
                font
                );
#if DEBUG_DOM_STORAGE==1
            if ( this->getStyle().isNull() ) {
                CRLog::error("NULL style is set for <%s>", LCSTR(getNodeName()) );
                style = this->getStyle();
            }
#endif
        }
    }
}
#endif

/// for display:list-item node, get marker
bool ldomNode::getNodeListMarker( int & counterValue, lString16 & marker, int & markerWidth )
{
#if BUILD_LITE!=1
    css_style_ref_t s = getStyle();
    marker.clear();
    markerWidth = 0;
    if ( s.isNull() )
        return false;
    css_list_style_type_t st = s->list_style_type;
    switch ( st ) {
    default:
        // treat default as disc
    case css_lst_disc:
        marker = L"\x25CF";
        break;
    case css_lst_circle:
        marker = L"\x25CB";
        break;
    case css_lst_square:
        marker = L"\x25A0";
        break;
    case css_lst_decimal:
    case css_lst_lower_roman:
    case css_lst_upper_roman:
    case css_lst_lower_alpha:
    case css_lst_upper_alpha:
        if ( counterValue<=0 ) {
            // calculate counter
            ldomNode * parent = getParentNode();
            counterValue = 0;
            for ( unsigned i=0; i<parent->getChildCount(); i++ ) {
                ldomNode * child = parent->getChildNode(i);
                css_style_ref_t cs = child->getStyle();
                if ( cs.isNull() )
                    continue;
                switch ( cs->list_style_type ) {
                case css_lst_decimal:
                case css_lst_lower_roman:
                case css_lst_upper_roman:
                case css_lst_lower_alpha:
                case css_lst_upper_alpha:
                    counterValue++;
                    break;
                default:
                    // do nothing
                    ;
                }
                if ( child==this )
                    break;
            }
        } else {
            counterValue++;
        }
        static const char * lower_roman[] = {"i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix",
                                             "x", "xi", "xii", "xiii", "xiv", "xv", "xvi", "xvii", "xviii", "xix",
                                         "xx", "xxi", "xxii", "xxiii"};
        if ( counterValue>0 ) {
            switch (st) {
            case css_lst_decimal:
                marker = lString16::itoa(counterValue);
                break;
            case css_lst_lower_roman:
                if ( counterValue-1<sizeof(lower_roman)/sizeof(lower_roman[0]) )
                    marker = lString16(lower_roman[counterValue-1]);
                else
                    marker = lString16::itoa(counterValue); // fallback to simple counter
                break;
            case css_lst_upper_roman:
                if ( counterValue-1<sizeof(lower_roman)/sizeof(lower_roman[0]) )
                    marker = lString16(lower_roman[counterValue-1]);
                else
                    marker = lString16::itoa(counterValue); // fallback to simple digital counter
                marker.uppercase();
                break;
            case css_lst_lower_alpha:
                if ( counterValue<=26 )
                    marker.append(1, 'a' + counterValue-1);
                else
                    marker = lString16::itoa(counterValue); // fallback to simple digital counter
                break;
            case css_lst_upper_alpha:
                if ( counterValue<=26 )
                    marker.append(1, 'A' + counterValue-1);
                else
                    marker = lString16::itoa(counterValue); // fallback to simple digital counter
                break;
            }
        }
        break;
    }
    bool res = false;
    if ( !marker.empty() ) {
        LVFont * font = getFont().get();
        if ( font ) {
            markerWidth = font->getTextWidth((marker + L"  ").c_str(), marker.length()+2) + s->font_size.value/8;
            res = true;
        } else {
            marker.clear();
        }
    }
    return res;
#else
    marker = lString16("*");
    return true;
#endif
}


/// returns first child node
ldomNode * ldomNode::getFirstChild() const
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
#if BUILD_LITE!=1
        if ( !isPersistent() ) {
#endif
            tinyElement * me = NPELEM;
            if ( me->_children.length() )
                return getDocument()->getTinyNode(me->_children[0]);
#if BUILD_LITE!=1
        } else {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            if ( me->childCount )
                return getDocument()->getTinyNode(me->children[0]);
        }
#endif
    }
    return NULL;
}

/// returns last child node
ldomNode * ldomNode::getLastChild() const
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
#if BUILD_LITE!=1
        if ( !isPersistent() ) {
#endif
            tinyElement * me = NPELEM;
            if ( me->_children.length() )
                return getDocument()->getTinyNode(me->_children[me->_children.length()-1]);
#if BUILD_LITE!=1
        } else {
            ElementDataStorageItem * me = getDocument()->_elemStorage.getElem( _data._pelem_addr );
            if ( me->childCount )
                return getDocument()->getTinyNode(me->children[me->childCount-1]);
        }
#endif
    }
    return NULL;
}

/// removes and deletes last child element
void ldomNode::removeLastChild()
{
    ASSERT_NODE_NOT_NULL;
    if ( hasChildren() ) {
        ldomNode * lastChild = removeChild( getChildCount() - 1 );
        lastChild->destroy();
    }
}

/// add child
void ldomNode::addChild( lInt32 childNodeIndex )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return;
    if ( isPersistent() )
        modify(); // convert to mutable element
    tinyElement * me = NPELEM;
    me->_children.add( childNodeIndex );
}

/// move range of children startChildIndex to endChildIndex inclusively to specified element
void ldomNode::moveItemsTo( ldomNode * destination, int startChildIndex, int endChildIndex )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return;
    if ( isPersistent() )
        modify();

#ifdef TRACE_AUTOBOX
    CRLog::debug( "moveItemsTo() invoked from %d to %d", getDataIndex(), destination->getDataIndex() );
#endif
    //if ( getDataIndex()==INDEX2 || getDataIndex()==INDEX1) {
    //    CRLog::trace("nodes from element %d are being moved", getDataIndex());
    //}
/*#ifdef _DEBUG
    if ( !_document->checkConsistency( false ) )
        CRLog::error("before moveItemsTo");
#endif*/
    int len = endChildIndex - startChildIndex + 1;
    tinyElement * me = NPELEM;
    for ( int i=0; i<len; i++ ) {
        ldomNode * item = getChildNode( startChildIndex );
        //if ( item->getDataIndex()==INDEX2 || item->getDataIndex()==INDEX1 ) {
        //    CRLog::trace("node %d is being moved", item->getDataIndex() );
        //}
        me->_children.remove( startChildIndex ); // + i
        item->setParentNode(destination);
        destination->addChild( item->getDataIndex() );
    }
    // TODO: renumber rest of children in necessary
/*#ifdef _DEBUG
    if ( !_document->checkConsistency( false ) )
        CRLog::error("after moveItemsTo");
#endif*/

}

/// find child element by tag id
ldomNode * ldomNode::findChildElement( lUInt16 nsid, lUInt16 id, int index )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return NULL;
    ldomNode * res = NULL;
    int k = 0;
    int childCount = getChildCount();
    for ( int i=0; i<childCount; i++ )
    {
        ldomNode * p = getChildNode( i );
        if ( !p->isElement() )
            continue;
        if ( p->getNodeId() == id && ( (p->getNodeNsId() == nsid) || (nsid==LXML_NS_ANY) ) )
        {
            if ( k==index || index==-1 )
                res = p;
            k++;
        }
    }
    if ( !res || (index==-1 && k>1) )
        return NULL;
    return res;
}

/// find child element by id path
ldomNode * ldomNode::findChildElement( lUInt16 idPath[] )
{
    ASSERT_NODE_NOT_NULL;
    if ( !this || !isElement() )
        return NULL;
    ldomNode * elem = this;
    for ( int i=0; idPath[i]; i++ ) {
        elem = elem->findChildElement( LXML_NS_ANY, idPath[i], -1 );
        if ( !elem )
            return NULL;
    }
    return elem;
}

/// inserts child element
ldomNode * ldomNode::insertChildElement( lUInt32 index, lUInt16 nsid, lUInt16 id )
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
        if ( isPersistent() )
            modify();
        tinyElement * me = NPELEM;
        if (index>(lUInt32)me->_children.length())
            index = me->_children.length();
        ldomNode * node = getDocument()->allocTinyElement( this, nsid, id );
        me->_children.insert( index, node->getDataIndex() );
        return node;
    }
    readOnlyError();
    return NULL;
}

/// inserts child element
ldomNode * ldomNode::insertChildElement( lUInt16 id )
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
        if ( isPersistent() )
            modify();
        ldomNode * node = getDocument()->allocTinyElement( this, LXML_NS_NONE, id );
        NPELEM->_children.insert( NPELEM->_children.length(), node->getDataIndex() );
        return node;
    }
    readOnlyError();
    return NULL;
}

/// inserts child text
ldomNode * ldomNode::insertChildText( lUInt32 index, const lString16 & value )
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
        if ( isPersistent() )
            modify();
        tinyElement * me = NPELEM;
        if (index>(lUInt32)me->_children.length())
            index = me->_children.length();
#if !defined(USE_PERSISTENT_TEXT) || BUILD_LITE==1
        ldomNode * node = getDocument()->allocTinyNode( NT_TEXT );
        lString8 s8 = UnicodeToUtf8(value);
        node->_data._text_ptr = new ldomTextNode(_handle._dataIndex, s8);
#else
        ldomNode * node = getDocument()->allocTinyNode( NT_PTEXT );
        //node->_data._ptext_addr._parentIndex = _handle._dataIndex;
        lString8 s8 = UnicodeToUtf8(value);
        node->_data._ptext_addr = getDocument()->_textStorage.allocText( node->_handle._dataIndex, _handle._dataIndex, s8 );
#endif
        me->_children.insert( index, node->getDataIndex() );
        return node;
    }
    readOnlyError();
    return NULL;
}

/// inserts child text
ldomNode * ldomNode::insertChildText( const lString16 & value )
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
        if ( isPersistent() )
            modify();
        tinyElement * me = NPELEM;
#if !defined(USE_PERSISTENT_TEXT) || BUILD_LITE==1
        ldomNode * node = getDocument()->allocTinyNode( NT_TEXT );
        lString8 s8 = UnicodeToUtf8(value);
        node->_data._text_ptr = new ldomTextNode(_handle._dataIndex, s8);
#else
        ldomNode * node = getDocument()->allocTinyNode( NT_PTEXT );
        lString8 s8 = UnicodeToUtf8(value);
        node->_data._ptext_addr = getDocument()->_textStorage.allocText( node->_handle._dataIndex, _handle._dataIndex, s8 );
#endif
        me->_children.insert( me->_children.length(), node->getDataIndex() );
        return node;
    }
    readOnlyError();
    return NULL;
}

/// remove child
ldomNode * ldomNode::removeChild( lUInt32 index )
{
    ASSERT_NODE_NOT_NULL;
    if  ( isElement() ) {
        if ( isPersistent() )
            modify();
        lUInt32 removedIndex = NPELEM->_children.remove(index);
        ldomNode * node = getTinyNode( removedIndex );
        return node;
    }
    readOnlyError();
    return NULL;
}

/// creates stream to read base64 encoded data from element
LVStreamRef ldomNode::createBase64Stream()
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return LVStreamRef();
#define DEBUG_BASE64_IMAGE 0
#if DEBUG_BASE64_IMAGE==1
    lString16 fname = getAttributeValue( attr_id );
    lString8 fname8 = UnicodeToUtf8( fname );
    LVStreamRef ostream = LVOpenFileStream( fname.empty()?L"image.png":fname.c_str(), LVOM_WRITE );
    printf("createBase64Stream(%s)\n", fname8.c_str());
#endif
    LVStream * stream = new LVBase64NodeStream( this );
    if ( stream->GetSize()==0 )
    {
#if DEBUG_BASE64_IMAGE==1
        printf("    cannot create base64 decoder stream!!!\n");
#endif
        delete stream;
        return LVStreamRef();
    }
    LVStreamRef istream( stream );

#if DEBUG_BASE64_IMAGE==1
    LVPumpStream( ostream, istream );
    istream->SetPos(0);
#endif

    return istream;
}

#if BUILD_LITE!=1

class NodeImageProxy : public LVImageSource
{
    ldomNode * _node;
    lString16 _refName;
    int _dx;
    int _dy;
public:
    NodeImageProxy( ldomNode * node, lString16 refName, int dx, int dy )
        : _node(node), _refName(refName), _dx(dx), _dy(dy)
    {

    }

    virtual ldomNode * GetSourceNode()
    {
        return NULL;
    }
    virtual LVStream * GetSourceStream()
    {
        return NULL;
    }

    virtual void   Compact() { }
    virtual int    GetWidth() { return _dx; }
    virtual int    GetHeight() { return _dy; }
    virtual bool   Decode( LVImageDecoderCallback * callback )
    {
        LVImageSourceRef img = _node->getDocument()->getObjectImageSource(_refName);
        if ( img.isNull() )
            return false;
        return img->Decode(callback);
    }
    virtual ~NodeImageProxy()
    {

    }
};

/// returns object image ref name
lString16 ldomNode::getObjectImageRefName()
{
    if ( !this || !isElement() )
        return lString16();
    //printf("ldomElement::getObjectImageSource() ... ");
    const css_elem_def_props_t * et = getDocument()->getElementTypePtr(getNodeId());
    if (!et || !et->is_object)
        return lString16();
    lUInt16 hrefId = getDocument()->getAttrNameIndex(L"href");
    lUInt16 srcId = getDocument()->getAttrNameIndex(L"src");
    lUInt16 recIndexId = getDocument()->getAttrNameIndex(L"recindex");
    lString16 refName = getAttributeValue( getDocument()->getNsNameIndex(L"xlink"),
        hrefId );

    if ( refName.empty() )
        refName = getAttributeValue( getDocument()->getNsNameIndex(L"l"), hrefId );
    if ( refName.empty() )
        refName = getAttributeValue( LXML_NS_ANY, hrefId ); //LXML_NS_NONE
    if ( refName.empty() )
        refName = getAttributeValue( LXML_NS_ANY, srcId ); //LXML_NS_NONE
    if (refName.empty()) {
        lString16 recindex = getAttributeValue( LXML_NS_ANY, recIndexId );
        if (!recindex.empty()) {
            int n;
            if (recindex.atoi(n)) {
                refName = lString16(MOBI_IMAGE_NAME_PREFIX) + lString16::itoa(n);
                //CRLog::debug("get mobi image %s", LCSTR(refName));
            }
        }
//        else {
//            for (int k=0; k<getAttrCount(); k++) {
//                CRLog::debug("attr %s=%s", LCSTR(getAttributeName(k)), LCSTR(getAttributeValue(getAttributeName(k).c_str())));
//            }
//        }
    }
    if ( refName.length()<2 )
        return lString16();
    refName = DecodeHTMLUrlString(refName);
    return refName;
}


/// returns object image stream
LVStreamRef ldomNode::getObjectImageStream()
{
    lString16 refName = getObjectImageRefName();
    if ( refName.empty() )
        return LVStreamRef();
    return getDocument()->getObjectImageStream( refName );
}


/// returns object image source
LVImageSourceRef ldomNode::getObjectImageSource()
{
    lString16 refName = getObjectImageRefName();
    LVImageSourceRef ref;
    if ( refName.empty() )
        return ref;
    ref = getDocument()->getObjectImageSource( refName );
    if ( !ref.isNull() ) {
        int dx = ref->GetWidth();
        int dy = ref->GetHeight();
        ref = LVImageSourceRef( new NodeImageProxy(this, refName, dx, dy) );
    } else {
        CRLog::error("ObjectImageSource cannot be opened by name %s", LCSTR(refName));
    }

    getDocument()->_urlImageMap.set( refName, ref );
    return ref;
}

/// returns object image stream
LVStreamRef ldomDocument::getObjectImageStream( lString16 refName )
{
    LVStreamRef ref;
    if ( refName.startsWith(lString16(BLOB_NAME_PREFIX)) ) {
        return _blobCache.getBlob(refName);
    } if ( refName[0]!='#' ) {
        if ( !getContainer().isNull() ) {
            lString16 name = refName;
            if ( !getCodeBase().empty() )
                name = getCodeBase() + refName;
            ref = getContainer()->OpenStream(name.c_str(), LVOM_READ);
            if ( ref.isNull() ) {
                lString16 fname = getProps()->getStringDef( DOC_PROP_FILE_NAME, "" );
                fname = LVExtractFilenameWithoutExtension(fname);
                if ( !fname.empty() ) {
                    lString16 fn = fname + L"_img";
//                    if ( getContainer()->GetObjectInfo(fn) ) {

//                    }
                    lString16 name = fn + L"/" + refName;
                    if ( !getCodeBase().empty() )
                        name = getCodeBase() + name;
                    ref = getContainer()->OpenStream(name.c_str(), LVOM_READ);
                }
            }
            if ( ref.isNull() )
                CRLog::error("Cannot open stream by name %s", LCSTR(name));
        }
        return ref;
    }
    lUInt16 refValueId = findAttrValueIndex( refName.c_str() + 1 );
    if ( refValueId == (lUInt16)-1 ) {
        return ref;
    }
    ldomNode * objnode = getNodeById( refValueId );
    if ( !objnode || !objnode->isElement())
        return ref;
    ref = objnode->createBase64Stream();
    return ref;
}

/// returns object image source
LVImageSourceRef ldomDocument::getObjectImageSource( lString16 refName )
{
    LVStreamRef stream = getObjectImageStream( refName );
    if ( stream.isNull() )\
         return LVImageSourceRef();
    return LVCreateStreamImageSource( stream );
}

void ldomDocument::resetNodeNumberingProps()
{
    lists.clear();
}

ListNumberingPropsRef ldomDocument::getNodeNumberingProps( lUInt32 nodeDataIndex )
{
    return lists.get(nodeDataIndex);
}

void ldomDocument::setNodeNumberingProps( lUInt32 nodeDataIndex, ListNumberingPropsRef v )
{
    lists.set(nodeDataIndex, v);
}

/// formats final block
int ldomNode::renderFinalBlock(  LFormattedTextRef & frmtext, RenderRectAccessor * fmt, int width )
{
    ASSERT_NODE_NOT_NULL;
    if ( !isElement() )
        return 0;
    //CRLog::trace("renderFinalBlock()");
    CVRendBlockCache & cache = getDocument()->getRendBlockCache();
    LFormattedTextRef f;
    lvdom_element_render_method rm = getRendMethod();
    if ( cache.get( this, f ) ) {
        frmtext = f;
        if ( rm != erm_final && rm != erm_list_item && rm != erm_table_caption )
            return 0;
        //RenderRectAccessor fmt( this );
        //CRLog::trace("Found existing formatted object for node #%08X", (lUInt32)this);
        return fmt->getHeight();
    }
    f = getDocument()->createFormattedText();
    if ( (rm != erm_final && rm != erm_list_item && rm != erm_table_caption) )
        return 0;
    //RenderRectAccessor fmt( this );
    /// render whole node content as single formatted object
    int flags = styleToTextFmtFlags( getStyle(), 0 );
    ::renderFinalBlock( this, f.get(), fmt, flags, 0, 16 );
    int page_h = getDocument()->getPageHeight();
    cache.set( this, f );
    int h = f->Format( width, page_h );
    frmtext = f;
    //CRLog::trace("Created new formatted object for node #%08X", (lUInt32)this);
    return h;
}

/// formats final block again after change, returns true if size of block is changed
bool ldomNode::refreshFinalBlock()
{
    ASSERT_NODE_NOT_NULL;
    if ( getRendMethod() != erm_final )
        return false;
    // TODO: implement reformatting of one node
    CVRendBlockCache & cache = getDocument()->getRendBlockCache();
    cache.remove( this );
    RenderRectAccessor fmt( this );
    lvRect oldRect, newRect;
    fmt.getRect( oldRect );
    LFormattedTextRef txtform;
    int width = fmt.getWidth();
    renderFinalBlock( txtform, &fmt, width );
    fmt.getRect( newRect );
    if ( oldRect == newRect )
        return false;
    // TODO: relocate other blocks
    return true;
}

#endif

/// replace node with r/o persistent implementation
ldomNode * ldomNode::persist()
{
    ASSERT_NODE_NOT_NULL;
#if BUILD_LITE!=1
    if ( !isPersistent() ) {
        if ( isElement() ) {
            // ELEM->PELEM
            tinyElement * elem = NPELEM;
            int attrCount = elem->_attrs.length();
            int childCount = elem->_children.length();
            _handle._dataIndex = (_handle._dataIndex & ~0xF) | NT_PELEMENT;
            _data._pelem_addr = getDocument()->_elemStorage.allocElem(_handle._dataIndex, elem->_parentNode ? elem->_parentNode->_handle._dataIndex : 0, elem->_children.length(), elem->_attrs.length() );
            ElementDataStorageItem * data = getDocument()->_elemStorage.getElem(_data._pelem_addr);
            data->nsid = elem->_nsid;
            data->id = elem->_id;
            lUInt16 * attrs = data->attrs();
            int i;
            for ( i=0; i<attrCount; i++ ) {
                const lxmlAttribute * attr = elem->_attrs[i];
                attrs[i * 3] = attr->nsid;     // namespace
                attrs[i * 3 + 1] = attr->id;   // id
                attrs[i * 3 + 2] = attr->index;// value
            }
            for ( i=0; i<childCount; i++ ) {
                data->children[i] = elem->_children[i];
            }
            data->rendMethod = (lUInt8)elem->_rendMethod;
            delete elem;
        } else {
            // TEXT->PTEXT
            lString8 utf8 = _data._text_ptr->getText();
            delete _data._text_ptr;
            lUInt32 parentIndex = _data._text_ptr->getParentIndex();
            _handle._dataIndex = (_handle._dataIndex & ~0xF) | NT_PTEXT;
            _data._ptext_addr = getDocument()->_textStorage.allocText(_handle._dataIndex, parentIndex, utf8 );
            // change type
        }
    }
#endif
    return this;
}

/// replace node with r/w implementation
ldomNode * ldomNode::modify()
{
    ASSERT_NODE_NOT_NULL;
#if BUILD_LITE!=1
    if ( isPersistent() ) {
        if ( isElement() ) {
            // PELEM->ELEM
            ElementDataStorageItem * data = getDocument()->_elemStorage.getElem(_data._pelem_addr);
            tinyElement * elem = new tinyElement(getDocument(), getParentNode(), data->nsid, data->id );
            for ( int i=0; i<data->childCount; i++ )
                elem->_children.add( data->children[i] );
            for ( int i=0; i<data->attrCount; i++ )
                elem->_attrs.add( data->attr(i) );
            _handle._dataIndex = (_handle._dataIndex & ~0xF) | NT_ELEMENT;
            elem->_rendMethod = (lvdom_element_render_method)data->rendMethod;
            getDocument()->_elemStorage.freeNode( _data._pelem_addr );
            NPELEM = elem;
        } else {
            // PTEXT->TEXT
            // convert persistent text to mutable
            lString8 utf8 = getDocument()->_textStorage.getText(_data._ptext_addr);
            lUInt32 parentIndex = getDocument()->_textStorage.getParent(_data._ptext_addr);
            getDocument()->_textStorage.freeNode( _data._ptext_addr );
            _data._text_ptr = new ldomTextNode( parentIndex, utf8 );
            // change type
            _handle._dataIndex = (_handle._dataIndex & ~0xF) | NT_TEXT;
        }
    }
#endif
    return this;
}


/// dumps memory usage statistics to debug log
void tinyNodeCollection::dumpStatistics()
{
    CRLog::info("*** Document memory usage: "
                "elements:%d, textNodes:%d, "
                "ptext=("
                "%d uncompressed), "
                "ptelems=("
                "%d uncompressed), "
                "rects=("
                "%d uncompressed), "
                "nodestyles=("
                "%d uncompressed), "
                "styles:%d, fonts:%d, renderedNodes:%d, "
                "totalNodes:%d(%dKb), mutableElements:%d(~%dKb)",
                _elemCount, _textCount,
                _textStorage.getUncompressedSize(),
                _elemStorage.getUncompressedSize(),
                _rectStorage.getUncompressedSize(),
                _styleStorage.getUncompressedSize(),
                _styles.length(), _fonts.length(),
#if BUILD_LITE!=1
                ((ldomDocument*)this)->_renderedBlockCache.length(),
#else
                0,
#endif
                _itemCount, _itemCount*16/1024,
                _tinyElementCount, _tinyElementCount*(sizeof(tinyElement)+8*4)/1024 );
}


/// returns position pointer
ldomXPointer LVTocItem::getXPointer()
{
    if ( _position.isNull() && !_path.empty() ) {
        _position = _doc->createXPointer( _path );
        if ( _position.isNull() ) {
            CRLog::trace("TOC node is not found for path %s", LCSTR(_path) );
        } else {
            CRLog::trace("TOC node is found for path %s", LCSTR(_path) );
        }
    }
    return _position;
}

/// returns position path
lString16 LVTocItem::getPath()
{
    if ( _path.empty() && !_position.isNull())
        _path = _position.toString();
    return _path;
}

/// returns Y position
int LVTocItem::getY()
{
#if BUILD_LITE!=1
    return getXPointer().toPoint().y;
#else
    return 0;
#endif
}

/// serialize to byte array (pointer will be incremented by number of bytes written)
bool LVTocItem::serialize( SerialBuf & buf )
{
//    LVTocItem *     _parent;
//    int             _level;
//    int             _index;
//    int             _page;
//    int             _percent;
//    lString16       _name;
//    ldomXPointer    _position;
//    LVPtrVector<LVTocItem> _children;

    buf << (lUInt32)_level << (lUInt32)_index << (lUInt32)_page << (lUInt32)_percent << (lUInt32)_children.length() << _name << getPath();
    if ( buf.error() )
        return false;
    for ( int i=0; i<_children.length(); i++ ) {
        _children[i]->serialize( buf );
        if ( buf.error() )
            return false;
    }
    return !buf.error();
}

/// deserialize from byte array (pointer will be incremented by number of bytes read)
bool LVTocItem::deserialize( ldomDocument * doc, SerialBuf & buf )
{
    if ( buf.error() )
        return false;
    lInt32 childCount = 0;
    buf >> _level >> _index >> _page >> _percent >> childCount >> _name >> _path;
//    CRLog::trace("[%d] %05d  %s  %s", _level, _page, LCSTR(_name), LCSTR(_path));
    if ( buf.error() )
        return false;
//    if ( _level>0 ) {
//        _position = doc->createXPointer( _path );
//        if ( _position.isNull() ) {
//            CRLog::error("Cannot find TOC node by path %s", LCSTR(_path) );
//            buf.seterror();
//            return false;
//        }
//    }
    for ( int i=0; i<childCount; i++ ) {
        LVTocItem * item = new LVTocItem(doc);
        if ( !item->deserialize( doc, buf ) ) {
            delete item;
            return false;
        }
        item->_parent = this;
        _children.add( item );
        if ( buf.error() )
            return false;
    }
    return true;
}

/// returns page number
//int LVTocItem::getPageNum( LVRendPageList & pages )
//{
//    return getSectionPage( _position.getNode(), pages );
//}





#if defined(_DEBUG) 

#define TEST_FILE_NAME "/tmp/test-cache-file.dat"

#include <lvdocview.h>

void testCacheFile()
{
#if BUILD_LITE!=1
    CRLog::info("Starting CacheFile unit test");
    lUInt8 data1[] = {'T', 'e', 's', 't', 'd', 'a', 't', 'a', 1, 2, 3, 4, 5, 6, 7};
    lUInt8 data2[] = {'T', 'e', 's', 't', 'd', 'a', 't', 'a', '2', 1, 2, 3, 4, 5, 6, 7};
    lUInt8 * buf1;
    lUInt8 * buf2;
    int sz1;
    int sz2;
    lString16 fn(TEST_FILE_NAME);

    {
        lUInt8 data1[] = {'T', 'e', 's', 't', 'D', 'a', 't', 'a', '1'};
        lUInt8 data2[] = {'T', 'e', 's', 't', 'D', 'a', 't', 'a', '2', 1, 2, 3, 4, 5, 6, 7};
        LVStreamRef s = LVOpenFileStream( fn.c_str(), LVOM_APPEND );
        s->SetPos(0);
        s->Write(data1, sizeof(data1), NULL);
        s->SetPos(4096);
        s->Write(data1, sizeof(data1), NULL);
        s->SetPos(8192);
        s->Write(data2, sizeof(data2), NULL);
        s->SetPos(4096);
        s->Write(data2, sizeof(data2), NULL);
        lUInt8 buf[16];
        s->SetPos(0);
        s->Read(buf, sizeof(data1), NULL);
        MYASSERT(!memcmp(buf, data1, sizeof(data1)), "read 1 content");
        s->SetPos(4096);
        s->Read(buf, sizeof(data2), NULL);
        MYASSERT(!memcmp(buf, data2, sizeof(data2)), "read 2 content");

        //return;
    }

    // write
    {
        CacheFile f;
        MYASSERT(f.open(lString16("/tmp/blabla-not-exits-file-name"))==false, "Wrong failed open result");
        MYASSERT(f.create( fn )==true, "new file created");
        MYASSERT(f.write(CBT_TEXT_DATA, 1, data1, sizeof(data1), true)==true, "write 1");
        MYASSERT(f.write(CBT_ELEM_DATA, 3, data2, sizeof(data2), false)==true, "write 2");

        MYASSERT(f.read(CBT_TEXT_DATA, 1, buf1, sz1)==true, "read 1");
        MYASSERT(f.read(CBT_ELEM_DATA, 3, buf2, sz2)==true, "read 2");
        MYASSERT(sz1==sizeof(data1), "read 1 size");
        MYASSERT(!memcmp(buf1, data1, sizeof(data1)), "read 1 content");
        MYASSERT(sz2==sizeof(data2), "read 2 size");
        MYASSERT(!memcmp(buf2, data2, sizeof(data2)), "read 2 content");
    }
    // write
    {
        CacheFile f;
        MYASSERT(f.open(fn)==true, "Wrong failed open result");
        MYASSERT(f.read(CBT_TEXT_DATA, 1, buf1, sz1)==true, "read 1");
        MYASSERT(f.read(CBT_ELEM_DATA, 3, buf2, sz2)==true, "read 2");
        MYASSERT(sz1==sizeof(data1), "read 1 size");
        MYASSERT(!memcmp(buf1, data1, sizeof(data1)), "read 1 content");
        MYASSERT(sz2==sizeof(data2), "read 2 size");
        MYASSERT(!memcmp(buf2, data2, sizeof(data2)), "read 2 content");
    }

    CRLog::info("Finished CacheFile unit test");
#endif
}

#ifdef _WIN32
#define TEST_FN_TO_OPEN "/projects/test/bibl.fb2.zip"
#else
#define TEST_FN_TO_OPEN "/home/lve/src/test/bibl.fb2.zip"
#endif

void runFileCacheTest()
{
#if BUILD_LITE!=1
    CRLog::info("====Cache test started =====");

    // init and clear cache
    ldomDocCache::init(lString16("/tmp/cr3cache"), 100);
    MYASSERT(ldomDocCache::enabled(), "clear cache");

    {
        CRLog::info("====Open document and save to cache=====");
        LVDocView view(4);
        view.Resize(600, 800);
        bool res = view.LoadDocument(TEST_FN_TO_OPEN);
        MYASSERT(res, "load document");
        view.getPageImage(0);
        view.getDocProps()->setInt(PROP_FORCED_MIN_FILE_SIZE_TO_CACHE, 30000);
        view.swapToCache();
        //MYASSERT(res, "swap to cache");
        view.getDocument()->dumpStatistics();
    }
    {
        CRLog::info("====Open document from cache=====");
        LVDocView view(4);
        view.Resize(600, 800);
        bool res = view.LoadDocument(TEST_FN_TO_OPEN);
        MYASSERT(res, "load document");
        view.getDocument()->dumpStatistics();
        view.getPageImage(0);
    }
    CRLog::info("====Cache test finished=====");
#endif
}

void runBasicTinyDomUnitTests()
{
    CRLog::info("==========================");
    CRLog::info("Starting tinyDOM unit test");
    ldomDocument * doc = new ldomDocument();
    ldomNode * root = doc->getRootNode();//doc->allocTinyElement( NULL, 0, 0 );
    MYASSERT(root!=NULL,"root != NULL");

    int el_p = doc->getElementNameIndex(L"p");
    int el_title = doc->getElementNameIndex(L"title");
    int el_strong = doc->getElementNameIndex(L"strong");
    int el_emphasis = doc->getElementNameIndex(L"emphasis");
    int attr_id = doc->getAttrNameIndex(L"id");
    int attr_name = doc->getAttrNameIndex(L"name");
    static lUInt16 path1[] = {el_title, el_p, 0};
    static lUInt16 path2[] = {el_title, el_p, el_strong, 0};

    CRLog::info("* simple DOM operations, tinyElement");
    MYASSERT(root->isRoot(),"root isRoot");
    MYASSERT(root->getParentNode()==NULL,"root parent is null");
    MYASSERT(root->getParentIndex()==0,"root parent index == 0");
    MYASSERT(root->getChildCount()==0,"empty root child count");
    ldomNode * el1 = root->insertChildElement(el_p);
    MYASSERT(root->getChildCount()==1,"root child count 1");
    MYASSERT(el1->getParentNode()==root,"element parent node");
    MYASSERT(el1->getParentIndex()==root->getDataIndex(),"element parent node index");
    MYASSERT(el1->getNodeId()==el_p, "node id");
    MYASSERT(el1->getNodeNsId()==LXML_NS_NONE, "node nsid");
    MYASSERT(!el1->isRoot(),"elem not isRoot");
    ldomNode * el2 = root->insertChildElement(el_title);
    MYASSERT(root->getChildCount()==2,"root child count 2");
    MYASSERT(el2->getNodeId()==el_title, "node id");
    MYASSERT(el2->getNodeNsId()==LXML_NS_NONE, "node nsid");
    lString16 nodename = el2->getNodeName();
    //CRLog::debug("node name: %s", LCSTR(nodename));
    MYASSERT(nodename==L"title","node name");
    ldomNode * el21 = el2->insertChildElement(el_p);
    MYASSERT(root->getNodeLevel()==1,"node level 1");
    MYASSERT(el2->getNodeLevel()==2,"node level 2");
    MYASSERT(el21->getNodeLevel()==3,"node level 3");
    MYASSERT(el21->getNodeIndex()==0,"node index single");
    MYASSERT(el1->getNodeIndex()==0,"node index first");
    MYASSERT(el2->getNodeIndex()==1,"node index last");
    MYASSERT(root->getNodeIndex()==0,"node index for root");
    MYASSERT(root->getFirstChild()==el1,"first child");
    MYASSERT(root->getLastChild()==el2,"last child");
    MYASSERT(el2->getFirstChild()==el21,"first single child");
    MYASSERT(el2->getLastChild()==el21,"last single child");
    MYASSERT(el21->getFirstChild()==NULL,"first child - no children");
    MYASSERT(el21->getLastChild()==NULL,"last child - no children");
    ldomNode * el0 = root->insertChildElement(1, LXML_NS_NONE, el_title);
    MYASSERT(el1->getNodeIndex()==0,"insert in the middle");
    MYASSERT(el0->getNodeIndex()==1,"insert in the middle");
    MYASSERT(el2->getNodeIndex()==2,"insert in the middle");
    MYASSERT(root->getChildNode(0)==el1,"child node 0");
    MYASSERT(root->getChildNode(1)==el0,"child node 1");
    MYASSERT(root->getChildNode(2)==el2,"child node 2");
    ldomNode * removedNode = root->removeChild( 1 );
    MYASSERT(removedNode==el0,"removed node");
    el0->destroy();
    MYASSERT(el0->isNull(),"destroyed node isNull");
    MYASSERT(root->getChildNode(0)==el1,"child node 0, after removal");
    MYASSERT(root->getChildNode(1)==el2,"child node 1, after removal");
    ldomNode * el02 = root->insertChildElement(5, LXML_NS_NONE, el_emphasis);
    MYASSERT(el02==el0,"removed node reusage");

    {
        ldomNode * f1 = root->findChildElement(path1);
        MYASSERT(f1==el21, "find 1 on mutable - is el21");
        MYASSERT(f1->getNodeId()==el_p, "find 1 on mutable");
        //ldomNode * f2 = root->findChildElement(path2);
        //MYASSERT(f2!=NULL, "find 2 on mutable - not null");
        //MYASSERT(f2==el21, "find 2 on mutable - is el21");
        //MYASSERT(f2->getNodeId()==el_strong, "find 2 on mutable");
    }

    CRLog::info("* simple DOM operations, mutable text");
    lString16 sampleText("Some sample text.");
    lString16 sampleText2("Some sample text 2.");
    lString16 sampleText3("Some sample text 3.");
    ldomNode * text1 = el1->insertChildText(sampleText);
    MYASSERT(text1->getText()==sampleText, "sample text 1 match unicode");
    MYASSERT(text1->getNodeLevel()==3,"text node level");
    MYASSERT(text1->getNodeIndex()==0,"text node index");
    MYASSERT(text1->isText(),"text node isText");
    MYASSERT(!text1->isElement(),"text node isElement");
    MYASSERT(!text1->isNull(),"text node isNull");
    ldomNode * text2 = el1->insertChildText(0, sampleText2);
    MYASSERT(text2->getNodeIndex()==0,"text node index, insert at beginning");
    MYASSERT(text2->getText()==sampleText2, "sample text 2 match unicode");
    MYASSERT(text2->getText8()==UnicodeToUtf8(sampleText2), "sample text 2 match utf8");
    text1->setText(sampleText2);
    MYASSERT(text1->getText()==sampleText2, "sample text 1 match unicode, changed");
    text1->setText8(UnicodeToUtf8(sampleText3));
    MYASSERT(text1->getText()==sampleText3, "sample text 1 match unicode, changed 8");
    MYASSERT(text1->getText8()==UnicodeToUtf8(sampleText3), "sample text 1 match utf8, changed");

    MYASSERT(el1->getFirstTextChild()==text2, "firstTextNode");
    MYASSERT(el1->getLastTextChild()==text1, "lastTextNode");
    MYASSERT(el21->getLastTextChild()==NULL, "lastTextNode NULL");

#if BUILD_LITE!=1
    CRLog::info("* style cache");
    {
        css_style_ref_t style1;
        style1 = css_style_ref_t( new css_style_rec_t );
        style1->display = css_d_block;
        style1->white_space = css_ws_normal;
        style1->text_align = css_ta_left;
        style1->text_align_last = css_ta_left;
        style1->text_decoration = css_td_none;
        style1->hyphenate = css_hyph_auto;
        style1->color.type = css_val_unspecified;
        style1->color.value = 0x000000;
        style1->background_color.type = css_val_unspecified;
        style1->background_color.value = 0xFFFFFF;
        style1->page_break_before = css_pb_auto;
        style1->page_break_after = css_pb_auto;
        style1->page_break_inside = css_pb_auto;
        style1->vertical_align = css_va_baseline;
        style1->font_family = css_ff_sans_serif;
        style1->font_size.type = css_val_px;
        style1->font_size.value = 24;
        style1->font_name = lString8("Arial");
        style1->font_weight = css_fw_400;
        style1->font_style = css_fs_normal;
        style1->text_indent.type = css_val_px;
        style1->text_indent.value = 0;
        style1->line_height.type = css_val_percent;
        style1->line_height.value = 100;

        css_style_ref_t style2;
        style2 = css_style_ref_t( new css_style_rec_t );
        style2->display = css_d_block;
        style2->white_space = css_ws_normal;
        style2->text_align = css_ta_left;
        style2->text_align_last = css_ta_left;
        style2->text_decoration = css_td_none;
        style2->hyphenate = css_hyph_auto;
        style2->color.type = css_val_unspecified;
        style2->color.value = 0x000000;
        style2->background_color.type = css_val_unspecified;
        style2->background_color.value = 0xFFFFFF;
        style2->page_break_before = css_pb_auto;
        style2->page_break_after = css_pb_auto;
        style2->page_break_inside = css_pb_auto;
        style2->vertical_align = css_va_baseline;
        style2->font_family = css_ff_sans_serif;
        style2->font_size.type = css_val_px;
        style2->font_size.value = 24;
        style2->font_name = lString8("Arial");
        style2->font_weight = css_fw_400;
        style2->font_style = css_fs_normal;
        style2->text_indent.type = css_val_px;
        style2->text_indent.value = 0;
        style2->line_height.type = css_val_percent;
        style2->line_height.value = 100;

        css_style_ref_t style3;
        style3 = css_style_ref_t( new css_style_rec_t );
        style3->display = css_d_block;
        style3->white_space = css_ws_normal;
        style3->text_align = css_ta_right;
        style3->text_align_last = css_ta_left;
        style3->text_decoration = css_td_none;
        style3->hyphenate = css_hyph_auto;
        style3->color.type = css_val_unspecified;
        style3->color.value = 0x000000;
        style3->background_color.type = css_val_unspecified;
        style3->background_color.value = 0xFFFFFF;
        style3->page_break_before = css_pb_auto;
        style3->page_break_after = css_pb_auto;
        style3->page_break_inside = css_pb_auto;
        style3->vertical_align = css_va_baseline;
        style3->font_family = css_ff_sans_serif;
        style3->font_size.type = css_val_px;
        style3->font_size.value = 24;
        style3->font_name = lString8("Arial");
        style3->font_weight = css_fw_400;
        style3->font_style = css_fs_normal;
        style3->text_indent.type = css_val_px;
        style3->text_indent.value = 0;
        style3->line_height.type = css_val_percent;
        style3->line_height.value = 100;

        el1->setStyle(style1);
        css_style_ref_t s1 = el1->getStyle();
        MYASSERT(!s1.isNull(), "style is set");
        el2->setStyle(style2);
        MYASSERT(*style1==*style2, "identical styles : == is true");
        MYASSERT(calcHash(*style1)==calcHash(*style2), "identical styles have the same hashes");
        MYASSERT(el1->getStyle().get()==el2->getStyle().get(), "identical styles reused");
        el21->setStyle(style3);
        MYASSERT(el1->getStyle().get()!=el21->getStyle().get(), "different styles not reused");
    }

    CRLog::info("* font cache");
    {
        font_ref_t font1 = fontMan->GetFont(24, 400, false, css_ff_sans_serif, lString8("DejaVu Sans"));
        font_ref_t font2 = fontMan->GetFont(24, 400, false, css_ff_sans_serif, lString8("DejaVu Sans"));
        font_ref_t font3 = fontMan->GetFont(28, 800, false, css_ff_serif, lString8("DejaVu Sans Condensed"));
        MYASSERT(el1->getFont().isNull(), "font is not set");
        el1->setFont(font1);
        MYASSERT(!el1->getFont().isNull(), "font is set");
        el2->setFont(font2);
        MYASSERT(*font1==*font2, "identical fonts : == is true");
        MYASSERT(calcHash(font1)==calcHash(font2), "identical styles have the same hashes");
        MYASSERT(el1->getFont().get()==el2->getFont().get(), "identical fonts reused");
        el21->setFont(font3);
        MYASSERT(el1->getFont().get()!=el21->getFont().get(), "different fonts not reused");
    }

    CRLog::info("* persistance test");

    el2->setAttributeValue(LXML_NS_NONE, attr_id, L"id1");
    el2->setAttributeValue(LXML_NS_NONE, attr_name, L"name1");
    MYASSERT(el2->getNodeId()==el_title, "mutable node id");
    MYASSERT(el2->getNodeNsId()==LXML_NS_NONE, "mutable node nsid");
    MYASSERT(el2->getAttributeValue(attr_id)==L"id1", "attr id1 mutable");
    MYASSERT(el2->getAttributeValue(attr_name)==L"name1", "attr name1 mutable");
    MYASSERT(el2->getAttrCount()==2, "attr count mutable");
    el2->persist();
    MYASSERT(el2->getAttributeValue(attr_id)==L"id1", "attr id1 pers");
    MYASSERT(el2->getAttributeValue(attr_name)==L"name1", "attr name1 pers");
    MYASSERT(el2->getNodeId()==el_title, "persistent node id");
    MYASSERT(el2->getNodeNsId()==LXML_NS_NONE, "persistent node nsid");
    MYASSERT(el2->getAttrCount()==2, "attr count persist");

    {
        ldomNode * f1 = root->findChildElement(path1);
        MYASSERT(f1==el21, "find 1 on mutable - is el21");
        MYASSERT(f1->getNodeId()==el_p, "find 1 on mutable");
    }

    el2->modify();
    MYASSERT(el2->getNodeId()==el_title, "mutable 2 node id");
    MYASSERT(el2->getNodeNsId()==LXML_NS_NONE, "mutable 2 node nsid");
    MYASSERT(el2->getAttributeValue(attr_id)==L"id1", "attr id1 mutable 2");
    MYASSERT(el2->getAttributeValue(attr_name)==L"name1", "attr name1 mutable 2");
    MYASSERT(el2->getAttrCount()==2, "attr count mutable 2");

    {
        ldomNode * f1 = root->findChildElement(path1);
        MYASSERT(f1==el21, "find 1 on mutable - is el21");
        MYASSERT(f1->getNodeId()==el_p, "find 1 on mutable");
    }

    CRLog::info("* convert to persistent");
    CRTimerUtil infinite;
    doc->persist(infinite);
    doc->dumpStatistics();

    MYASSERT(el21->getFirstChild()==NULL,"first child - no children");
    MYASSERT(el21->isPersistent(), "persistent before insertChildElement");
    ldomNode * el211 = el21->insertChildElement(el_strong);
    MYASSERT(!el21->isPersistent(), "mutable after insertChildElement");
    el211->persist();
    MYASSERT(el211->isPersistent(), "persistent before insertChildText");
    el211->insertChildText(lString16(L"bla bla bla"));
    el211->insertChildText(lString16(L"bla bla blaw"));
    MYASSERT(!el211->isPersistent(), "modifable after insertChildText");
    //el21->insertChildElement(el_strong);
    MYASSERT(el211->getChildCount()==2, "child count, in mutable");
    el211->persist();
    MYASSERT(el211->getChildCount()==2, "child count, in persistent");
    el211->modify();
    MYASSERT(el211->getChildCount()==2, "child count, in mutable again");
    CRTimerUtil infinite2;
    doc->persist(infinite2);

    ldomNode * f1 = root->findChildElement(path1);
    MYASSERT(f1->getNodeId()==el_p, "find 1");
    ldomNode * f2 = root->findChildElement(path2);
    MYASSERT(f2->getNodeId()==el_strong, "find 2");
    MYASSERT(f2 == el211, "find 2, ref");


    CRLog::info("* compacting");
    doc->compact();
    doc->dumpStatistics();
#endif
    
    delete doc;
    

    CRLog::info("Finished tinyDOM unit test");

    CRLog::info("==========================");

}

void runCHMUnitTest()
{
#if CHM_SUPPORT_ENABLED==1
#if BUILD_LITE!=1
    LVStreamRef stream = LVOpenFileStream("/home/lve/src/test/mysql.chm", LVOM_READ);
    MYASSERT ( !stream.isNull(), "container stream opened" );
    CRLog::trace("runCHMUnitTest() -- file stream opened ok");
    LVContainerRef dir = LVOpenCHMContainer( stream );
    MYASSERT ( !dir.isNull(), "container opened" );
    CRLog::trace("runCHMUnitTest() -- container opened ok");
    LVStreamRef s = dir->OpenStream(L"/index.html", LVOM_READ);
    MYASSERT ( !s.isNull(), "item opened" );
    CRLog::trace("runCHMUnitTest() -- index.html opened ok: size=%d", (int)s->GetSize());
    lvsize_t bytesRead = 0;
    char buf[1000];
    MYASSERT( s->SetPos(100)==100, "SetPos()" );
    MYASSERT( s->Read(buf, 1000, &bytesRead)==LVERR_OK, "Read()" );
    MYASSERT( bytesRead==1000, "Read() -- bytesRead" );
    buf[999] = 0;
    CRLog::trace("CHM/index.html Contents 1000: %s", buf);

    MYASSERT( s->SetPos(0)==0, "SetPos() 2" );
    MYASSERT( s->Read(buf, 1000, &bytesRead)==LVERR_OK, "Read() 2" );
    MYASSERT( bytesRead==1000, "Read() -- bytesRead 2" );
    buf[999] = 0;
    CRLog::trace("CHM/index.html Contents 0: %s", buf);
#endif
#endif
}

static void makeTestFile( const char * fname, int size )
{
    LVStreamRef s = LVOpenFileStream( fname, LVOM_WRITE );
    MYASSERT( !s.isNull(), "makeTestFile create" );
    int seed = 0;
    lUInt8 * buf = new lUInt8[size];
    for ( int i=0; i<size; i++ ) {
        buf[i] = (seed >> 9) & 255;
        seed = seed * 31 + 14323;
    }
    MYASSERT( s->Write(buf, size, NULL)==LVERR_OK, "makeTestFile write" );
    delete[] buf;
}

void runBlockWriteCacheTest()
{



    int sz = 2000000;
    const char * fn1 = "/tmp/tf1.dat";
    const char * fn2 = "/tmp/tf2.dat";
    //makeTestFile( fn1, sz );
    //makeTestFile( fn2, sz );

    CRLog::debug("BlockCache test started");

    LVStreamRef s1 = LVOpenFileStream( fn1, LVOM_APPEND );
    LVStreamRef s2 =  LVCreateBlockWriteStream( LVOpenFileStream( fn2, LVOM_APPEND ), 0x8000, 16);
    MYASSERT(! s1.isNull(), "s1");
    MYASSERT(! s2.isNull(), "s2");
    LVStreamRef ss = LVCreateCompareTestStream(s1, s2);
    lUInt8 buf[0x100000];
    for ( int i=0; i<sizeof(buf); i++ ) {
        buf[i] = (lUInt8)(rand() & 0xFF);
    }
    //memset( buf, 0xAD, 1000000 );
    ss->SetPos( 0 );
    ss->Write( buf, 150, NULL );
    ss->SetPos( 0 );
    ss->Write( buf, 150, NULL );
    ss->SetPos( 0 );
    ss->Write( buf, 150, NULL );


    ss->SetPos( 1000 );
    ss->Read( buf, 5000, NULL );
    ss->SetPos( 100000 );
    ss->Read( buf+10000, 150000, NULL );

    ss->SetPos( 1000 );
    ss->Write( buf, 15000, NULL );
    ss->SetPos( 1000 );
    ss->Read( buf+100000, 15000, NULL );
    ss->Read( buf, 1000000, NULL );


    ss->SetPos( 1000 );
    ss->Write( buf, 15000, NULL );
    ss->Write( buf, 15000, NULL );
    ss->Write( buf, 15000, NULL );
    ss->Write( buf, 15000, NULL );


    ss->SetPos( 100000 );
    ss->Write( buf+15000, 150000, NULL );
    ss->SetPos( 100000 );
    ss->Read( buf+25000, 200000, NULL );

    ss->SetPos( 100000 );
    ss->Read( buf+55000, 200000, NULL );

    ss->SetPos( 100000 );
    ss->Write( buf+1000, 250000, NULL );
    ss->SetPos( 150000 );
    ss->Read( buf, 50000, NULL );
    ss->SetPos( 1000000 );
    ss->Write( buf, 500000, NULL );
    for ( int i=0; i<10; i++ )
        ss->Write( buf, 5000, NULL );
    ss->Read( buf, 50000, NULL );

    ss->SetPos( 5000000 );
    ss->Write( buf, 500000, NULL );
    ss->SetPos( 4800000 );
    ss->Read( buf, 500000, NULL );

    for ( int i=0; i<20; i++ ) {
        int op = (rand() & 15) < 5;
        long offset = (rand()&0x7FFFF);
        long foffset = (rand()&0x3FFFFF);
        long size = (rand()&0x3FFFF);
        ss->SetPos(foffset);
        if ( op==0 ) {
            // read
            ss->Read(buf+offset, size, NULL);
        } else {
            ss->Write(buf+offset, size, NULL);
        }
    }

    CRLog::debug("BlockCache test finished");

}

void runTinyDomUnitTests()
{
    CRLog::info("runTinyDomUnitTests()");
    runBlockWriteCacheTest();

    runBasicTinyDomUnitTests();

    CRLog::info("==========================");
    testCacheFile();

    runFileCacheTest();
    CRLog::info("==========================");

}

#endif
