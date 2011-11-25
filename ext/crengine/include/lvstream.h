/** \file lvstream.h
    \brief stream classes interface

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.
    See LICENSE file for details.

 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * UNRAR library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.

*/


#ifndef __LVSTREAM_H_INCLUDED__
#define __LVSTREAM_H_INCLUDED__

#include "lvtypes.h"
#include "lvref.h"
#include "lvstring.h"
#include "lvarray.h"
#include "lvptrvec.h"

#if LVLONG_FILE_SUPPORT == 1
typedef   lUInt64   lvsize_t;    ///< file size type
typedef   lInt64    lvoffset_t;  ///< file offset type
typedef   lUInt64   lvpos_t;     ///< file position type
#else
typedef   lUInt32   lvsize_t;    ///< file size type
typedef   lInt32    lvoffset_t;  ///< file offset type
typedef   lUInt32   lvpos_t;     ///< file position type
#endif

#define LV_INVALID_SIZE ((lvsize_t)(-1))

/// Seek origins enum
enum lvseek_origin_t {
    LVSEEK_SET = 0,     ///< seek relatively to beginning of file
    LVSEEK_CUR = 1,     ///< seek relatively to current position
    LVSEEK_END = 2      ///< seek relatively to end of file
};

/// I/O errors enum
enum lverror_t {
    LVERR_OK = 0,       ///< no error
    LVERR_FAIL,         ///< failed (unknown error)
    LVERR_EOF,          ///< end of file reached
    LVERR_NOTFOUND,     ///< file not found
    LVERR_NOTIMPL,      ///< method is not implemented
};

/// File open modes enum
enum lvopen_mode_t {
    LVOM_ERROR=0,       ///< to indicate error state
    LVOM_CLOSED,        ///< to indicate closed state
    LVOM_READ,          ///< readonly mode, use for r/o mmap
    LVOM_WRITE,         ///< writeonly mode
    LVOM_APPEND,        ///< append (readwrite) mode, use for r/w mmap
    LVOM_READWRITE      ///< readwrite mode
};

#define LVOM_MASK 7
#define LVOM_FLAG_SYNC 0x10

class LVContainer;
class LVStream;

class LVStorageObject : public LVRefCounter
{
public:
    // construction/destruction
    //LVStorageObject() {  }
    virtual ~LVStorageObject() { }
    // storage object methods
    /// returns true for container (directory), false for stream (file)
    virtual bool IsContainer();
    /// returns stream/container name, may be NULL if unknown
    virtual const lChar16 * GetName();
    /// sets stream/container name, may be not implemented for some objects
    virtual void SetName(const lChar16 * name);
    /// returns parent container, if opened from container
    virtual LVContainer * GetParentContainer();
    /// returns object size (file size or directory entry count)
    virtual lverror_t GetSize( lvsize_t * pSize ) = 0;
    /// returns object size (file size or directory entry count)
    virtual lvsize_t GetSize( );
};

/// Read or write buffer for stream region
class LVStreamBuffer : public LVRefCounter
{
public:
    /// get pointer to read-only buffer, returns NULL if unavailable
    virtual const lUInt8 * getReadOnly() = 0;
    /// get pointer to read-write buffer, returns NULL if unavailable
    virtual lUInt8 * getReadWrite() = 0;
    /// get buffer size
    virtual lvsize_t getSize() = 0;
    /// flush on destroy
    virtual ~LVStreamBuffer() { close(); }
    /// detach from stream, write changes if necessary
    virtual bool close() { return true; }
};

typedef LVFastRef<LVStreamBuffer> LVStreamBufferRef;

/// Stream base class
class LVStream : public LVStorageObject
{
public:

    /// Get read buffer (optimal for mmap)
    virtual LVStreamBufferRef GetReadBuffer( lvpos_t pos, lvpos_t size );
    /// Get read/write buffer (optimal for mmap)
    virtual LVStreamBufferRef GetWriteBuffer( lvpos_t pos, lvpos_t size );

    /// Get stream open mode
    /** \return lvopen_mode_t open mode */
    virtual lvopen_mode_t GetMode() { return LVOM_READ; }

    /// Set stream mode, supported not by all streams
    /** \return LVERR_OK if change is ok */
    virtual lverror_t SetMode( lvopen_mode_t ) { return LVERR_NOTIMPL; }
    /// flushes unsaved data from buffers to file, with optional flush of OS buffers
    virtual lverror_t Flush( bool sync ) { return LVERR_OK; }

    /// Seek (change file pos)
    /**
        \param offset is file offset (bytes) relateve to origin
        \param origin is offset base
        \param pNewPos points to place to store new file position
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t Seek( lvoffset_t offset, lvseek_origin_t origin, lvpos_t * pNewPos ) = 0;

    /// Tell current file position
    /**
        \param pNewPos points to place to store file position
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t Tell( lvpos_t * pPos ) { return Seek(0, LVSEEK_CUR, pPos); }

    /// Set file position
    /**
        \param p is new position
        \return lverror_t status: LVERR_OK if success
    */
    //virtual lverror_t SetPos(lvpos_t p) { return Seek(p, LVSEEK_SET, NULL); }
    virtual lvpos_t   SetPos(lvpos_t p) { lvpos_t pos; return (Seek(p, LVSEEK_SET, &pos)==LVERR_OK)?pos:(lvpos_t)(~0); }

    /// Get file position
    /**
        \return lvpos_t file position
    */
    virtual lvpos_t   GetPos()
    {
        lvpos_t pos;
        if (Seek(0, LVSEEK_CUR, &pos)==LVERR_OK)
            return pos;
        else
            return (lvpos_t)(~0);
    }

    /// Get file size
    /**
        \return lvsize_t file size
    */
    virtual lvsize_t  GetSize()
    {
        lvpos_t pos = GetPos();
        lvsize_t sz = 0;
        Seek(0, LVSEEK_END, &sz);
        SetPos(pos);
        return sz;
    }

    virtual lverror_t GetSize( lvsize_t * pSize )
    {
        *pSize = LVStream::GetSize();
        return LVERR_OK;
    }

    /// Set file size
    /**
        \param size is new file size
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t SetSize( lvsize_t size ) = 0;

    /// Read
    /**
        \param buf is buffer to place bytes read from stream
        \param count is number of bytes to read from stream
        \param nBytesRead is place to store real number of bytes read from stream
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t Read( void * buf, lvsize_t count, lvsize_t * nBytesRead ) = 0;

    virtual bool Read( lUInt8 * buf )
	{
		lvsize_t nBytesRead;
		if ( Read( buf, sizeof(lUInt8), &nBytesRead )==LVERR_OK && nBytesRead==sizeof(lUInt8) )
			return true;
		return false;
	}

    virtual bool Read( lUInt16 * buf )
	{
		lvsize_t nBytesRead;
		if ( Read( buf, sizeof(lUInt16), &nBytesRead )==LVERR_OK && nBytesRead==sizeof(lUInt16) )
			return true;
		return false;
	}

    virtual bool Read( lUInt32 * buf )
	{
		lvsize_t nBytesRead;
		if ( Read( buf, sizeof(lUInt32), &nBytesRead )==LVERR_OK && nBytesRead==sizeof(lUInt32) )
			return true;
		return false;
	}

	virtual int ReadByte()
	{
		unsigned char buf[1];
		lvsize_t sz = 0;
		if ( Read( buf, 1, &sz ) == LVERR_OK && sz == 1 )
			return buf[0];
		return -1;
	}

    /// Write
    /**
        \param buf is data to write to stream
        \param count is number of bytes to write
        \param nBytesWritten is place to store real number of bytes written to stream
        \return lverror_t status: LVERR_OK if success
    */
    virtual lverror_t Write( const void * buf, lvsize_t count, lvsize_t * nBytesWritten ) = 0;

    /// Check whether end of file is reached
    /**
        \return true if end of file reached
    */
    virtual bool Eof() = 0;

    /// writes array
    lverror_t Write( LVArray<lUInt32> & array );

    /// calculate crc32 code for stream, if possible
    virtual lverror_t crc32( lUInt32 & dst );
    /// calculate crc32 code for stream, returns 0 for error or empty stream
    inline lUInt32 crc32() { lUInt32 res = 0; crc32( res ); return res; }

    /// Constructor
    LVStream() { }

    /// Destructor
    virtual ~LVStream() { }
};

/// Writes lString16 string to stream
inline LVStream & operator << (LVStream & stream, const lString16 & str)
{
   if (!str.empty())
      stream.Write( str.c_str(), sizeof(lChar16)*str.length(), NULL);
   return stream;
}

/// Writes lString8 string to stream
inline LVStream & operator << (LVStream & stream, const lString8 & str)
{
   if (!str.empty())
      stream.Write( str.c_str(), sizeof(lChar8)*str.length(), NULL);
   return stream;
}

/// Writes lChar16 string to stream
inline LVStream & operator << (LVStream & stream, const lChar16 * str)
{
   if (str)
      stream.Write( str, sizeof(lChar16)*lStr_len(str), NULL);
   return stream;
}

/// Writes lChar8 string to stream
inline LVStream & operator << (LVStream & stream, const lChar8 * str)
{
   if (str)
      stream.Write( str, sizeof(lChar8)*lStr_len(str), NULL);
   return stream;
}

/// Writes lUInt32 to stream
inline LVStream & operator << (LVStream & stream, lUInt32 d )
{
   stream.Write( &d, sizeof(d), NULL);
   return stream;
}

/// Writes lUInt16 to stream
inline LVStream & operator << (LVStream & stream, lUInt16 d )
{
   stream.Write( &d, sizeof(d), NULL);
   return stream;
}

/// Writes lUInt8 to stream
inline LVStream & operator << (LVStream & stream, lUInt8 d )
{
   stream.Write( &d, sizeof(d), NULL);
   return stream;
}

/// Writes value array to stream
template <typename T>
inline LVStream & operator << (LVStream & stream, LVArray<T> & array )
{
   stream.Write( array.ptr(), sizeof(T)*array.length(), NULL);
   return stream;
}

class LVNamedStream : public LVStream
{
protected:
    lString16 m_fname;
    lString16 m_filename;
    lString16 m_path;
    lvopen_mode_t          m_mode;
    lUInt32 _crc;
    bool _crcFailed;
public:
    LVNamedStream() : _crc(0), _crcFailed(false) { }
    /// returns stream/container name, may be NULL if unknown
    virtual const lChar16 * GetName();
    /// sets stream/container name, may be not implemented for some objects
    virtual void SetName(const lChar16 * name);
    /// returns open mode
    virtual lvopen_mode_t GetMode()
    {
        return (lvopen_mode_t)(m_mode & LVOM_MASK);
    }
    /// calculate crc32 code for stream, if possible
    virtual lverror_t crc32( lUInt32 & dst );
};


class LVStreamProxy : public LVStream
{
protected:
    LVStream * m_base_stream;
public:
    virtual const lChar16 * GetName()
            { return m_base_stream->GetName(); }
    virtual lvopen_mode_t GetMode()
            { return m_base_stream->GetMode(); }
    virtual lverror_t Seek( lvoffset_t offset, lvseek_origin_t origin, lvpos_t * pNewPos )
            { return m_base_stream->Seek(offset, origin, pNewPos); }
    virtual lverror_t Tell( lvpos_t * pPos )
            { return m_base_stream->Tell(pPos); }
    //virtual lverror_t   SetPos(lvpos_t p)
    virtual lvpos_t   SetPos(lvpos_t p)
            { return m_base_stream->SetPos(p); }
    virtual lvpos_t   GetPos()
            { return m_base_stream->GetPos(); }
    virtual lverror_t SetSize( lvsize_t size )
            { return m_base_stream->SetSize(size); }
    virtual lverror_t Read( void * buf, lvsize_t count, lvsize_t * nBytesRead )
            { return m_base_stream->Read(buf, count, nBytesRead); }
    virtual lverror_t Write( const void * buf, lvsize_t count, lvsize_t * nBytesWritten )
            { return m_base_stream->Write(buf, count, nBytesWritten); }
    virtual bool Eof()
            { return m_base_stream->Eof(); }
    LVStreamProxy( LVStream * stream ) : m_base_stream(stream) { }
    ~LVStreamProxy() { delete m_base_stream; }
};

class LVTextStream : public LVStreamProxy
{
public:
    virtual lvopen_mode_t GetMode();
    virtual lverror_t Seek( lvoffset_t offset, lvseek_origin_t origin, lvpos_t * pNewPos );
    virtual lverror_t Tell( lvpos_t * pPos );
    virtual lvpos_t   SetPos(lvpos_t p);
    virtual lvpos_t   GetPos();
    virtual lverror_t SetSize( lvsize_t size );
    virtual lverror_t Read( void * buf, lvsize_t count, lvsize_t * nBytesRead );
    virtual lverror_t Write( const void * buf, lvsize_t count, lvsize_t * nBytesWritten );
    virtual bool Eof();
    LVTextStream( LVStream * stream ) : LVStreamProxy(stream)
    { }
};

class LVContainerItemInfo
{
public:
    virtual lvsize_t        GetSize() const = 0;
    virtual const lChar16 * GetName() const = 0;
    virtual lUInt32         GetFlags() const = 0;
    virtual bool            IsContainer() const = 0;
    LVContainerItemInfo() {}
    virtual ~LVContainerItemInfo() {}
};

/// Stream reference
typedef LVFastRef<LVStream> LVStreamRef;

class LVContainer : public LVStorageObject
{
public:
    virtual LVContainer * GetParentContainer() = 0;
    //virtual const LVContainerItemInfo * GetObjectInfo(const wchar_t * pname);
    virtual const LVContainerItemInfo * GetObjectInfo(int index) = 0;
    virtual const LVContainerItemInfo * operator [] (int index) { return GetObjectInfo(index); }
    virtual int GetObjectCount() const = 0;
    virtual LVStreamRef OpenStream( const lChar16 * fname, lvopen_mode_t mode ) = 0;
    LVContainer() {}
    virtual ~LVContainer() { }
};

class LVCommonContainerItemInfo : public LVContainerItemInfo
{
    friend class LVDirectoryContainer;
    friend class LVArcContainer;
protected:
    lvsize_t     m_size;
    lString16    m_name;
    lUInt32      m_flags;
    bool         m_is_container;
    lUInt32      m_srcpos;
    lUInt32      m_srcsize;
    lUInt32      m_srcflags;
public:
    virtual lvsize_t        GetSize() const { return m_size; }
    virtual const lChar16 * GetName() const { return m_name.empty()?NULL:m_name.c_str(); }
    virtual lUInt32         GetFlags() const  { return m_flags; }
    virtual bool            IsContainer() const  { return m_is_container; }
    lUInt32 GetSrcPos() { return m_srcpos; }
    lUInt32 GetSrcSize() { return m_srcsize; }
    lUInt32 GetSrcFlags() { return m_srcflags; }
    void SetSrc( lUInt32 pos, lUInt32 size, lUInt32 flags )
    {
        m_srcpos = pos;
        m_srcsize = size;
        m_srcflags = flags;
    }
    void SetName( const lChar16 * name )
    {
        m_name = name;
    }
    void SetItemInfo( lString16 fname, lvsize_t size, lUInt32 flags, bool isContainer = false )
    {
        m_name = fname;
        m_size = size;
        m_flags = flags;
        m_is_container = isContainer;
    }
    LVCommonContainerItemInfo() : m_size(0), m_flags(0), m_is_container(false),
        m_srcpos(0), m_srcsize(0), m_srcflags(0)
    {
    }
    virtual ~LVCommonContainerItemInfo ()
    {
    }
};

class LVNamedContainer : public LVContainer
{
protected:
    lString16 m_fname;
    lString16 m_filename;
    lString16 m_path;
    lChar16 m_path_separator;
    LVPtrVector<LVCommonContainerItemInfo> m_list;
public:
    virtual bool IsContainer()
    {
        return true;
    }
    /// returns stream/container name, may be NULL if unknown
    virtual const lChar16 * GetName()
    {
        if (m_fname.empty())
            return NULL;
        return m_fname.c_str();
    }
    /// sets stream/container name, may be not implemented for some objects
    virtual void SetName(const lChar16 * name)
    {
        m_fname = name;
        m_filename.clear();
        m_path.clear();
        if (m_fname.empty())
            return;
        const lChar16 * fn = m_fname.c_str();

        const lChar16 * p = fn + m_fname.length() - 1;
        for ( ;p>fn; p--) {
            if (p[-1] == '/' || p[-1]=='\\')
            {
                m_path_separator = p[-1];
                break;
            }
        }
        int pos = p-fn;
        if (p>fn)
            m_path = m_fname.substr(0, pos);
        m_filename = m_fname.substr(pos, m_fname.length() - pos);
    }
    LVNamedContainer() : m_path_separator(
#ifdef _LINUX
        '/'
#else
        '\\'
#endif
    )
    {
    }
    virtual ~LVNamedContainer()
    {
    }
    void Add( LVCommonContainerItemInfo * item )
    {
        m_list.add( item );
    }
    void Clear()
    {
        m_list.clear();
    }
};

class LVArcContainerBase : public LVNamedContainer
{
protected:
    LVContainer * m_parent;
    LVStreamRef m_stream;
public:
    virtual LVStreamRef OpenStream( const wchar_t *, lvopen_mode_t )
    {
        return LVStreamRef();
    }
    virtual LVContainer * GetParentContainer()
    {
        return (LVContainer*)m_parent;
    }
    virtual const LVContainerItemInfo * GetObjectInfo(int index)
    {
        if (index>=0 && index<m_list.length())
            return m_list[index];
        return NULL;
    }
    virtual int GetObjectCount() const
    {
        return m_list.length();
    }
    virtual lverror_t GetSize( lvsize_t * pSize )
    {
        if (m_fname.empty())
            return LVERR_FAIL;
        *pSize = GetObjectCount();
        return LVERR_OK;
    }
    LVArcContainerBase( LVStreamRef stream ) : m_parent(NULL), m_stream(stream)
    {
    }
    virtual ~LVArcContainerBase()
    {
        SetName(NULL);
        Clear();
    }
    virtual int ReadContents() = 0;

};

class LVStreamFragment : public LVNamedStream
{
private:
    LVStreamRef m_stream;
    lvsize_t    m_start;
    lvsize_t    m_size;
    lvpos_t     m_pos;
public:
    LVStreamFragment( LVStreamRef stream, lvsize_t start, lvsize_t size )
        : m_stream(stream), m_start(start), m_size(size), m_pos(0)
    {
    }
    virtual bool Eof()
    {
        return m_pos >= m_size;
    }
    virtual lvsize_t  GetSize()
    {
        return m_size;
    }

    virtual lverror_t Seek(lvoffset_t pos, lvseek_origin_t origin, lvpos_t* newPos)
    {
        lverror_t res = m_stream->Seek( m_start + pos, origin, &m_pos );
        if (res == LVERR_OK)
            m_pos -= m_start;
        if (newPos)
        {
            *newPos =  m_pos;
        }
        return res;
    }
    virtual lverror_t Write(const void*, lvsize_t, lvsize_t*)
    {
        return LVERR_NOTIMPL;
    }
    virtual lverror_t Read(void* buf, lvsize_t size, lvsize_t* pBytesRead)
    {
        lvsize_t bytesRead = 0;
        lverror_t res = m_stream->Read( buf, size, &bytesRead );
        if (res == LVERR_OK)
            m_pos += bytesRead;
        if (pBytesRead)
            *pBytesRead = bytesRead;
        return res;
    }
    virtual lverror_t SetSize(lvsize_t)
    {
        return LVERR_NOTIMPL;
    }
};

/// Container reference
typedef LVFastRef<LVContainer> LVContainerRef;

/// Open file stream
/**
    \param pathname is file name to open (unicode)
    \param mode is mode file should be opened in
    \return reference to opened stream if success, NULL if error
*/
LVStreamRef LVOpenFileStream( const lChar16 * pathname, int mode );

/// Open file stream
/**
    \param pathname is file name to open (local codepage)
    \param mode is mode file should be opened in
    \return reference to opened stream if success, NULL if error
*/
LVStreamRef LVOpenFileStream( const lChar8 * pathname, int mode );

/// Open memory mapped file
/**
    \param pathname is file name to open (unicode)
    \param mode is mode file should be opened in (LVOM_READ or LVOM_APPEND only)
	\param minSize is minimum file size for R/W mode
    \return reference to opened stream if success, NULL if error
*/
LVStreamRef LVMapFileStream( const lChar16 * pathname, lvopen_mode_t mode, lvsize_t minSize );

/// Open memory mapped file
/**
    \param pathname is file name to open (unicode)
    \param mode is mode file should be opened in (LVOM_READ or LVOM_APPEND only)
	\param minSize is minimum file size for R/W mode
    \return reference to opened stream if success, NULL if error
*/
LVStreamRef LVMapFileStream( const lChar8 * pathname, lvopen_mode_t mode, lvsize_t minSize );


/// Open archieve from stream
/**
    \param stream is archieve file stream
    \return reference to opened archieve if success, NULL reference if error
*/
#if (USE_ZLIB==1)
LVContainerRef LVOpenArchieve( LVStreamRef stream );
#endif

/// Creates memory stream
/**
    \param buf is pointer to buffer, if NULL, empty read/write memory stream will be created
    \param bufSize is buffer size, in bytes
    \param createCopy if true, read/write copy of specified data is being created, otherwise non-managed readonly buffer is being used as is
    \param mode is open mode
    \return reference to opened stream if success, NULL reference if error
*/
LVStreamRef LVCreateMemoryStream( void * buf = NULL, int bufSize = 0, bool createCopy = false, lvopen_mode_t mode = LVOM_READ );
/// Creates memory stream as copy of another stream.
LVStreamRef LVCreateMemoryStream( LVStreamRef srcStream );
/// Creates memory stream as copy of file contents.
LVStreamRef LVCreateMemoryStream( lString16 filename );
/// Creates memory stream as copy of string contents
LVStreamRef LVCreateStringStream( lString8 data );
/// Creates memory stream as copy of string contents
LVStreamRef LVCreateStringStream( lString16 data );

/// creates cache buffers for stream, to write data by big blocks to optimize Flash drives writing performance
LVStreamRef LVCreateBlockWriteStream( LVStreamRef baseStream, int blockSize, int blockCount );

LVContainerRef LVOpenDirectory( const lChar16 * path, const wchar_t * mask = L"*.*" );

/// Create directory if not exist
bool LVCreateDirectory( lString16 path );
/// delete file, return true if file found and successfully deleted
bool LVDeleteFile( lString16 filename );

/// copies content of in stream to out stream
lvsize_t LVPumpStream( LVStreamRef out, LVStreamRef in );
/// copies content of in stream to out stream
lvsize_t LVPumpStream( LVStream * out, LVStream * in );

/// creates buffered stream object for stream
LVStreamRef LVCreateBufferedStream( LVStreamRef stream, int bufSize );
/// creates TCR decoder stream for stream
LVStreamRef LVCreateTCRDecoderStream( LVStreamRef stream );

/// returns path part of pathname (appended with / or \ delimiter)
lString16 LVExtractPath( lString16 pathName );
/// removes first path part from pathname and returns it
lString16 LVExtractFirstPathElement( lString16 & pathName );
/// removes last path part from pathname and returns it
lString16 LVExtractLastPathElement( lString16 & pathName );
/// returns filename part of pathname
lString16 LVExtractFilename( lString16 pathName );
/// returns filename part of pathname without extension
lString16 LVExtractFilenameWithoutExtension( lString16 pathName );
/// appends path delimiter character to end of path, if absent
void LVAppendPathDelimiter( lString16 & pathName );
/// replaces any found / or \\ separator with specified one
void LVReplacePathSeparator( lString16 & pathName, lChar16 separator );
/// removes path delimiter character from end of path, if exists
void LVRemovePathDelimiter( lString16 & pathName );
/// returns path delimiter character
lChar16 LVDetectPathDelimiter( lString16 pathName );
/// returns true if absolute path is specified
bool LVIsAbsolutePath( lString16 pathName );
/// returns full path to file identified by pathName, with base directory == basePath
lString16 LVMakeRelativeFilename( lString16 basePath, lString16 pathName );
// resolve relative links
lString16 LVCombinePaths( lString16 basePath, lString16 newPath );

/// tries to split full path name into archive name and file name inside archive using separator "@/" or "@\"
bool LVSplitArcName( lString16 fullPathName, lString16 & arcPathName, lString16 & arcItemPathName );

/// returns true if specified file exists
bool LVFileExists( lString16 pathName );
/// returns true if specified directory exists
bool LVDirectoryExists( lString16 pathName );

#endif // __LVSTREAM_H_INCLUDED__
