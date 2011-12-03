/*******************************************************

   CoolReader Engine

   lvxml.cpp:  XML parser implementation

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include "../include/lvxml.h"
#include "../include/crtxtenc.h"
#include "../include/fb2def.h"
#include "../include/lvdocview.h"


#define BUF_SIZE_INCREMENT 4096
#define MIN_BUF_DATA_SIZE 4096
#define CP_AUTODETECT_BUF_SIZE 0x10000


/// virtual destructor
LVFileFormatParser::~LVFileFormatParser()
{
}

LVFileParserBase::LVFileParserBase( LVStreamRef stream )
    : m_stream(stream)
    , m_buf(NULL)
    , m_buf_size(0)
    , m_stream_size(0)
    , m_buf_len(0)
    , m_buf_pos(0)
    , m_buf_fpos(0)
    , m_stopped(false)
    , m_progressCallback(NULL)
    , m_lastProgressTime((time_t)0)
    , m_progressLastPercent(0)
    , m_progressUpdateCounter(0)
    , m_firstPageTextCounter(-1)

{
    m_stream_size = stream.isNull()?0:stream->GetSize();
}

/// returns pointer to loading progress callback object
LVDocViewCallback * LVFileParserBase::getProgressCallback()
{
    return m_progressCallback;
}

// should be (2^N - 1)
#define PROGRESS_UPDATE_RATE_MASK 63
/// call to send progress update to callback, if timeout expired
void LVFileParserBase::updateProgress()
{
    //CRLog::trace("LVFileParserBase::updateProgress() is called");
    if ( m_progressCallback == NULL )
        return;
    //CRLog::trace("LVFileParserBase::updateProgress() is called - 2");
    /// first page is loaded from file an can be formatted for preview
    if ( m_firstPageTextCounter>=0 ) {
        m_firstPageTextCounter--;
        if ( m_firstPageTextCounter==0 ) {
            if ( getProgressPercent()<30 )
                m_progressCallback->OnLoadFileFirstPagesReady();
            m_firstPageTextCounter=-1;
        }
    }
    m_progressUpdateCounter = (m_progressUpdateCounter + 1) & PROGRESS_UPDATE_RATE_MASK;
    if ( m_progressUpdateCounter!=0 )
        return; // to speed up checks
    time_t t = (time_t)time((time_t)0);
    if ( m_lastProgressTime==0 ) {
        m_lastProgressTime = t;
        return;
    }
    if ( t == m_lastProgressTime )
        return;
    int p = getProgressPercent();
    if ( p!= m_progressLastPercent ) {
        m_progressCallback->OnLoadFileProgress( p );
        m_progressLastPercent = p;
        m_lastProgressTime = t;
    }
}

/// sets pointer to loading progress callback object
void LVFileParserBase::setProgressCallback( LVDocViewCallback * callback )
{
    //CRLog::debug("LVFileParserBase::setProgressCallback is called");
    m_progressCallback = callback;
}

/// override to return file reading position percent
int LVFileParserBase::getProgressPercent()
{
    if ( m_stream_size<=0 )
        return 0;
    return (int)((lInt64)100 * (m_buf_pos + m_buf_fpos) / m_stream_size);
}

lString16 LVFileParserBase::getFileName()
{
    if ( m_stream.isNull() )
        return lString16();
    lString16 name( m_stream->GetName() );
    int lastPathDelim = -1;
    for ( unsigned i=0; i<name.length(); i++ ) {
        if ( name[i]=='\\' || name[i]=='/' ) {
            lastPathDelim = i;
        }
    }
    name = name.substr( lastPathDelim+1, name.length()-lastPathDelim-1 );
    return name;
}

LVTextFileBase::LVTextFileBase( LVStreamRef stream )
    : LVFileParserBase(stream)
    , m_enc_type( ce_8bit_cp )
    , m_conv_table(NULL)
    , m_eof(false)
{
    clearCharBuffer();
}


/// stops parsing in the middle of file, to read header only
void LVFileParserBase::Stop()
{
    //CRLog::trace("LVTextFileBase::Stop() is called!");
    m_stopped = true;
}

/// destructor
LVFileParserBase::~LVFileParserBase()
{
    if (m_buf)
        free( m_buf );
}

/// destructor
LVTextFileBase::~LVTextFileBase()
{
    if (m_conv_table)
        delete[] m_conv_table;
}

static int charToHex( lUInt8 ch )
{
    if ( ch>='0' && ch<='9' )
        return ch-'0';
    if ( ch>='a' && ch<='f' )
        return ch-'a'+10;
    if ( ch>='A' && ch<='F' )
        return ch-'A'+10;
    return -1;
}


/// reads one character from buffer in RTF format
lChar16 LVTextFileBase::ReadRtfChar( int, const lChar16 * conv_table )
{
    lChar16 ch = m_buf[m_buf_pos++];
    lChar16 ch2 = m_buf[m_buf_pos];
    if ( ch=='\\' && ch2!='\'' ) {
    } else if (ch=='\\' ) {
        m_buf_pos++;
        int digit1 = charToHex( m_buf[0] );
        int digit2 = charToHex( m_buf[1] );
        m_buf_pos+=2;
        if ( digit1>=0 && digit2>=0 ) {
            ch = ( (lChar8)((digit1 << 4) | digit2) );
            if ( ch&0x80 )
                return conv_table[ch&0x7F];
            else
                return ch;
        } else {
            return '?';
        }
    } else {
        if ( ch>=' ' ) {
            if ( ch&0x80 )
                return conv_table[ch&0x7F];
            else
                return ch;
        }
    }
    return ' ';
}

#if 0
lChar16 LVTextFileBase::ReadChar()
{
    lUInt16 ch = m_buf[m_buf_pos++];
    switch ( m_enc_type ) {
    case ce_8bit_cp:
    case ce_utf8:
        if ( (ch & 0x80) == 0 )
            return ch;
        if (m_conv_table)
        {
            return m_conv_table[ch&0x7F];
        }
        else
        {
            // support only 11 and 16 bit UTF8 chars
            if ( (ch & 0xE0) == 0xC0 )
            {
                // 11 bits
                return ((lUInt16)(ch&0x1F)<<6)
                    | ((lUInt16)m_buf[m_buf_pos++]&0x3F);
            } else {
                // 16 bits
                ch = (ch&0x0F);
                lUInt16 ch2 = (m_buf[m_buf_pos++]&0x3F);
                lUInt16 ch3 = (m_buf[m_buf_pos++]&0x3F);
                return (ch<<12) | (ch2<<6) | ch3;
            }
        }
    case ce_utf16_be:
        {
            lUInt16 ch2 = m_buf[m_buf_pos++];
            return (ch << 8) | ch2;
        }
    case ce_utf16_le:
        {
            lUInt16 ch2 = m_buf[m_buf_pos++];
            return (ch2 << 8) | ch;
        }
    case ce_utf32_be:
        // support 16 bits only
        m_buf_pos++;
        {
            lUInt16 ch3 = m_buf[m_buf_pos++];
            lUInt16 ch4 = m_buf[m_buf_pos++];
            return (ch3 << 8) | ch4;
        }
    case ce_utf32_le:
        // support 16 bits only
        {
            lUInt16 ch2 = m_buf[m_buf_pos++];
            m_buf_pos+=2;
            return (ch << 8) | ch2;
        }
    default:
        return 0;
    }
}
#endif

void LVTextFileBase::checkEof()
{
    if ( m_buf_fpos+m_buf_len >= this->m_stream_size-4 )
        m_buf_pos = m_buf_len = m_stream_size - m_buf_fpos; //force eof
        //m_buf_pos = m_buf_len = m_stream_size - (m_buf_fpos+m_buf_len);
}


/// reads several characters from buffer
int LVTextFileBase::ReadChars( lChar16 * buf, int maxsize )
{
    if ( m_buf_pos>=m_buf_len )
        return 0;
    int count = 0;
    switch ( m_enc_type ) {
    case ce_8bit_cp:
    case ce_utf8:
        if ( m_conv_table!=NULL ) {
            for ( ; count<maxsize && m_buf_pos<m_buf_len; count++ ) {
                lUInt16 ch = m_buf[m_buf_pos++];
                buf[count] = ( (ch & 0x80) == 0 ) ? ch : m_conv_table[ch&0x7F];
            }
            return count;
        } else  {
            for ( ; count<maxsize && m_buf_pos<m_buf_len; count++ ) {
                lUInt16 ch = m_buf[m_buf_pos];
                // support only 11 and 16 bit UTF8 chars
                if ( (ch & 0x80) == 0 ) {
                    buf[count] = ch;
                    m_buf_pos++;
                } else if ( (ch & 0xE0) == 0xC0 ) {
                    // 11 bits
                    if ( m_buf_pos+1>=m_buf_len ) {
                        checkEof();
                        return count;
                    }
                    ch = (ch&0x1F);
#ifdef _DEBUG
//#define CHECK_UTF8_CODE 1
#endif
#if CHECK_UTF8_CODE==1
                    if ( (m_buf[m_buf_pos+1] & 0xC0) != 0x80 ) {
                        CRLog::error("Wrong utf8 character at position %08x", (int)(m_buf_fpos+m_buf_pos));
                    }
#endif
                    m_buf_pos++;
                    lUInt16 ch2 = m_buf[m_buf_pos++]&0x3F;
                    buf[count] = (ch<<6) | ch2;
                    //buf[count] = ((lUInt16)(ch&0x1F)<<6) | ((lUInt16)m_buf[m_buf_pos++]&0x3F);
                } else if ( (ch & 0xF0) == 0xE0 ) {
                    // 16 bits
                    if ( m_buf_pos+2>=m_buf_len ) {
                        checkEof();
                        return count;
                    }
                    ch = (ch&0x0F);
#if CHECK_UTF8_CODE==1
                    if ( (m_buf[m_buf_pos+1] & 0xC0) != 0x80 || (m_buf[m_buf_pos+2] & 0xC0) != 0x80 ) {
                        CRLog::error("Wrong utf8 character at position %08x", (int)(m_buf_fpos+m_buf_pos));
                    }
#endif
                    m_buf_pos++;
                    lUInt16 ch2 = m_buf[m_buf_pos++]&0x3F;
                    lUInt16 ch3 = m_buf[m_buf_pos++]&0x3F;
                    buf[count] = (ch<<12) | (ch2<<6) | ch3;
                } else {
                    // 20 bits
                    if ( m_buf_pos+3>=m_buf_len ) {
                        checkEof();
                        return count;
                    }
                    ch = (ch&0x07);
#if CHECK_UTF8_CODE==1
                    if ( (m_buf[m_buf_pos+1] & 0xC0) != 0x80 || (m_buf[m_buf_pos+2] & 0xC0) != 0x80  || (m_buf[m_buf_pos+3] & 0xC0) != 0x80 ) {
                        CRLog::error("Wrong utf8 character at position %08x", (int)(m_buf_fpos+m_buf_pos));
                    }
#endif
                    m_buf_pos++;
                    lUInt16 ch2 = m_buf[m_buf_pos++]&0x3F;
                    lUInt16 ch3 = m_buf[m_buf_pos++]&0x3F;
                    lUInt16 ch4 = m_buf[m_buf_pos++]&0x3F;
                    buf[count] = ((lChar16)ch<<18) | (ch2<12) | (ch3<<6) | ch4;
                }
            }
            return count;
        }
    case ce_utf16_be:
        {
            for ( ; count<maxsize; count++ ) {
                if ( m_buf_pos+1>=m_buf_len ) {
                    checkEof();
                    return count;
                }
                lUInt16 ch = m_buf[m_buf_pos++];
                lUInt16 ch2 = m_buf[m_buf_pos++];
                buf[count] = (ch << 8) | ch2;
            }
            return count;
        }
    case ce_utf16_le:
        {
            for ( ; count<maxsize; count++ ) {
                if ( m_buf_pos+1>=m_buf_len ) {
                    checkEof();
                    return count;
                }
                lUInt16 ch = m_buf[m_buf_pos++];
                lUInt16 ch2 = m_buf[m_buf_pos++];
                buf[count] = (ch2 << 8) | ch;
            }
            return count;
        }
    case ce_utf32_be:
        // support 24 bits only
        {
            for ( ; count<maxsize; count++ ) {
                if ( m_buf_pos+3>=m_buf_len ) {
                    checkEof();
                    return count;
                }
                m_buf_pos++; //lUInt16 ch = m_buf[m_buf_pos++];
                lUInt16 ch2 = m_buf[m_buf_pos++];
                lUInt16 ch3 = m_buf[m_buf_pos++];
                lUInt16 ch4 = m_buf[m_buf_pos++];
                buf[count] = (ch2 << 16) | (ch3 << 8) | ch4;
            }
            return count;
        }
    case ce_utf32_le:
        // support 24 bits only
        {
            for ( ; count<maxsize; count++ ) {
                if ( m_buf_pos+3>=m_buf_len ) {
                    checkEof();
                    return count;
                }
                lUInt16 ch = m_buf[m_buf_pos++];
                lUInt16 ch2 = m_buf[m_buf_pos++];
                lUInt16 ch3 = m_buf[m_buf_pos++];
                m_buf_pos++; //lUInt16 ch4 = m_buf[m_buf_pos++];
                buf[count] = (ch3 << 16) | (ch2 << 8) | ch;
            }
            return count;
        }
    default:
        return 0;
    }
}

/// tries to autodetect text encoding
bool LVTextFileBase::AutodetectEncoding( bool utfOnly )
{
    char enc_name[32];
    char lang_name[32];
    lvpos_t oldpos = m_stream->GetPos();
    unsigned sz = CP_AUTODETECT_BUF_SIZE;
    m_stream->SetPos( 0 );
    if ( sz>m_stream->GetSize() )
        sz = m_stream->GetSize();
    if ( sz < 16 )
        return false;
    unsigned char * buf = new unsigned char[ sz ];
    lvsize_t bytesRead = 0;
    if ( m_stream->Read( buf, sz, &bytesRead )!=LVERR_OK ) {
        delete[] buf;
        m_stream->SetPos( oldpos );
        return false;
    }

    int res = 0;
    if ( utfOnly )
        res = AutodetectCodePageUtf( buf, sz, enc_name, lang_name );
    else
        res = AutodetectCodePage( buf, sz, enc_name, lang_name );
    delete[] buf;
    m_stream->SetPos( oldpos );
    if ( res) {
        //CRLog::debug("Code page decoding results: encoding=%s, lang=%s", enc_name, lang_name);
        m_lang_name = lString16( lang_name );
        SetCharset( lString16( enc_name ).c_str() );
    }

    // restore state
    return res!=0  || utfOnly;
}

/// seek to specified stream position
bool LVFileParserBase::Seek( lvpos_t pos, int bytesToPrefetch )
{
    if ( pos >= m_buf_fpos && pos+bytesToPrefetch <= (m_buf_fpos+m_buf_len) ) {
        m_buf_pos = (pos - m_buf_fpos);
        return true;
    }
    if ( pos>=m_stream_size )
        return false;
    unsigned bytesToRead = (bytesToPrefetch > m_buf_size) ? bytesToPrefetch : m_buf_size;
    if ( bytesToRead < BUF_SIZE_INCREMENT )
        bytesToRead = BUF_SIZE_INCREMENT;
    if ( bytesToRead > (m_stream_size - pos) )
        bytesToRead = (m_stream_size - pos);
    if ( (unsigned)m_buf_size < bytesToRead ) {
        m_buf_size = bytesToRead;
        m_buf = cr_realloc( m_buf, m_buf_size );
    }
    m_buf_fpos = pos;
    m_buf_pos = 0;
    m_buf_len = m_buf_size;
    // TODO: add error handing
    if ( m_stream->SetPos( m_buf_fpos ) != m_buf_fpos ) {
        CRLog::error("cannot set stream position to %d", (int)m_buf_pos );
        return false;
    }
    lvsize_t bytesRead = 0;
    if ( m_stream->Read( m_buf, bytesToRead, &bytesRead ) != LVERR_OK ) {
        CRLog::error("error while reading %d bytes from stream", (int)bytesToRead);
        return false;
    }
    return true;
}

/// reads specified number of bytes, converts to characters and saves to buffer
int LVTextFileBase::ReadTextBytes( lvpos_t pos, int bytesToRead, lChar16 * buf, int buf_size, int flags)
{
    if ( !Seek( pos, bytesToRead ) ) {
        CRLog::error("LVTextFileBase::ReadTextBytes seek error! cannot set pos to %d to read %d bytes", (int)pos, (int)bytesToRead);
        return 0;
    }
    int chcount = 0;
    int max_pos = m_buf_pos + bytesToRead;
    if ( max_pos > m_buf_len )
        max_pos = m_buf_len;
    if ( (flags & TXTFLG_RTF)!=0 ) {
        char_encoding_type enc_type = ce_utf8;
        lChar16 * conv_table = NULL;
        if ( flags & TXTFLG_ENCODING_MASK ) {
        // set new encoding
            int enc_id = (flags & TXTFLG_ENCODING_MASK) >> TXTFLG_ENCODING_SHIFT;
            if ( enc_id >= ce_8bit_cp ) {
                conv_table = (lChar16 *)GetCharsetByte2UnicodeTableById( enc_id );
                enc_type = ce_8bit_cp;
            } else {
                conv_table = NULL;
                enc_type = (char_encoding_type)enc_id;
            }
        }
        while ( m_buf_pos<max_pos && chcount < buf_size ) {
            *buf++ = ReadRtfChar(enc_type, conv_table);
            chcount++;
        }
    } else {
        return ReadChars( buf, buf_size );
    }
    return chcount;
}

bool LVFileParserBase::FillBuffer( int bytesToRead )
{
    lvoffset_t bytesleft = (lvoffset_t) (m_stream_size - (m_buf_fpos+m_buf_len));
    if (bytesleft<=0)
        return true; //FIX
    if (bytesToRead > bytesleft)
        bytesToRead = (int)bytesleft;
    int space = m_buf_size - m_buf_len;
    if (space < bytesToRead)
    {
        if ( m_buf_pos>bytesToRead || m_buf_pos>((m_buf_len*3)>>2) )
        {
            // just move
            int sz = (int)(m_buf_len -  m_buf_pos);
            for (int i=0; i<sz; i++)
            {
                m_buf[i] = m_buf[i+m_buf_pos];
            }
            m_buf_len = sz;
            m_buf_fpos += m_buf_pos;
            m_buf_pos = 0;
            space = m_buf_size - m_buf_len;
        }
        if (space < bytesToRead)
        {
            m_buf_size = m_buf_size + (bytesToRead - space + BUF_SIZE_INCREMENT);
            m_buf = cr_realloc( m_buf, m_buf_size );
        }
    }
    lvsize_t n = 0;
    if ( m_stream->Read(m_buf+m_buf_len, bytesToRead, &n) != LVERR_OK )
        return false;
//    if ( CRLog::isTraceEnabled() ) {
//        const lUInt8 * s = m_buf + m_buf_len;
//        const lUInt8 * s2 = m_buf + m_buf_len + (int)n - 8;
//        CRLog::trace("fpos=%06x+%06x, sz=%04x, data: %02x %02x %02x %02x %02x %02x %02x %02x .. %02x %02x %02x %02x %02x %02x %02x %02x",
//                     m_buf_fpos, m_buf_len, (int) n,
//                     s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7],
//                     s2[0], s2[1], s2[2], s2[3], s2[4], s2[5], s2[6], s2[7]
//                     );
//    }
    m_buf_len += (int)n;
    return (n>0);
}

void LVFileParserBase::Reset()
{
    m_stream->SetPos(0);
    m_buf_fpos = 0;
    m_buf_pos = 0;
    m_buf_len = 0;
    m_stream_size = m_stream->GetSize();
}

void LVTextFileBase::Reset()
{
    LVFileParserBase::Reset();
    clearCharBuffer();
    // Remove Byte Order Mark from beginning of file
    if ( PeekCharFromBuffer()==0xFEFF )
        ReadCharFromBuffer();
}

void LVTextFileBase::SetCharset( const lChar16 * name )
{
    m_encoding_name = lString16( name );
    if ( m_encoding_name == L"utf-8" ) {
        m_enc_type = ce_utf8;
        SetCharsetTable( NULL );
    } else if ( m_encoding_name == L"utf-16" ) {
        m_enc_type = ce_utf16_le;
        SetCharsetTable( NULL );
    } else if ( m_encoding_name == L"utf-16le" ) {
        m_enc_type = ce_utf16_le;
        SetCharsetTable( NULL );
    } else if ( m_encoding_name == L"utf-16be" ) {
        m_enc_type = ce_utf16_be;
        SetCharsetTable( NULL );
    } else if ( m_encoding_name == L"utf-32" ) {
        m_enc_type = ce_utf32_le;
        SetCharsetTable( NULL );
    } else if ( m_encoding_name == L"utf-32le" ) {
        m_enc_type = ce_utf32_le;
        SetCharsetTable( NULL );
    } else if ( m_encoding_name == L"utf-32be" ) {
        m_enc_type = ce_utf32_be;
        SetCharsetTable( NULL );
    } else {
        m_enc_type = ce_8bit_cp;
        //CRLog::trace("charset: %s", LCSTR(lString16(name)));
        const lChar16 * table = GetCharsetByte2UnicodeTable( name );
        if ( table )
            SetCharsetTable( table );
    }
}

void LVTextFileBase::SetCharsetTable( const lChar16 * table )
{
    if (!table)
    {
        if (m_conv_table)
        {
            delete[] m_conv_table;
            m_conv_table = NULL;
        }
        return;
    }
    m_enc_type = ce_8bit_cp;
    if (!m_conv_table)
        m_conv_table = new lChar16[128];
    lStr_memcpy( m_conv_table, table, 128 );
}


static const lChar16 * heading_volume[] = {
    L"volume",
    L"vol",
    L"\x0442\x043e\x043c", // tom
    NULL
};

static const lChar16 * heading_part[] = {
    L"part",
    L"\x0447\x0430\x0441\x0442\x044c", // chast'
    NULL
};

static const lChar16 * heading_chapter[] = {
    L"chapter",
    L"\x0433\x043B\x0430\x0432\x0430", // glava
    NULL
};

static bool startsWithOneOf( const lString16 & s, const lChar16 * list[] )
{
    lString16 str = s;
    str.lowercase();
    const lChar16 * p = str.c_str();
    for ( int i=0; list[i]; i++ ) {
        const lChar16 * q = list[i];
        int j=0;
        for ( ; q[j]; j++ ) {
            if ( !p[j] ) {
                return (!q[j] || q[j]==' ');
            }
            if ( p[j] != q[j] )
                break;
        }
        if ( !q[j] )
            return true;
    }
    return false;
}

int DetectHeadingLevelByText( const lString16 & str )
{
    if ( str.empty() )
        return 0;
    if ( startsWithOneOf( str, heading_volume ) )
        return 1;
    if ( startsWithOneOf( str, heading_part ) )
        return 2;
    if ( startsWithOneOf( str, heading_chapter ) )
        return 3;
    lChar16 ch = str[0];
    if ( ch>='0' && ch<='9' ) {
        unsigned i;
        int point_count = 0;
        for ( i=1; i<str.length(); i++ ) {
            ch = str[i];
            if ( ch>='0' && ch<='9' )
                continue;
            if ( ch!='.' )
                return 0;
            point_count++;
        }
        return (str.length()<80) ? 5+point_count : 0;
    }
    if ( ch=='I' || ch=='V' || ch=='X' ) {
        // TODO: optimize
        static const char * romeNumbers[] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV", "XVI", "XVII", "XVIII", "XIX",
            "XX", "XXI", "XXII", "XXIII", "XXIV", "XXV", "XXVI", "XXVII", "XXVIII", "XXIX",
            "XXX", "XXXI", "XXXII", "XXXIII", "XXXIV", "XXXV", "XXXVI", "XXXVII", "XXXVIII", "XXXIX", NULL };
        int i=0;
        for ( i=0; romeNumbers[i]; i++ ) {
            if ( !lStr_cmp(str.c_str(), romeNumbers[i]) )
                return 4;
        }
    }
    return 0;
}

#define LINE_IS_HEADER 0x2000
#define LINE_HAS_EOLN 1

typedef enum {
    la_unknown,  // not detected
    la_empty,    // empty line
    la_left,     // left aligned
    la_indent,   // right aligned
    la_centered, // centered
    la_right,    // right aligned
    la_width     // justified width
} lineAlign_t;

class LVTextFileLine
{
public:
    //lvpos_t fpos;   // position of line in file
    //lvsize_t fsize;  // size of data in file
    lUInt32 flags;  // flags. 1=eoln
    lString16 text; // line text
    lUInt16 lpos;   // left non-space char position
    lUInt16 rpos;   // right non-space char posision + 1
    lineAlign_t align;
    bool empty() { return rpos==0; }
    bool isHeading() { return (flags & LINE_IS_HEADER)!=0; }
    LVTextFileLine( LVTextFileBase * file, int maxsize )
    : flags(0), lpos(0), rpos(0), align(la_unknown)
    {
        text = file->ReadLine( maxsize, flags );
        //CRLog::debug("  line read: %s", UnicodeToUtf8(text).c_str() );
        if ( !text.empty() ) {
            const lChar16 * s = text.c_str();
            for ( int p=0; *s; s++ ) {
                if ( *s == '\t' ) {
                    p = (p + 8)%8;
                } else {
                    if ( *s != ' ' ) {
                        if ( rpos==0 && p>0 ) {
                            //CRLog::debug("   lpos = %d", p);
                            lpos = p;
                        }
                        rpos = p + 1;
                    }
                    p++;
                }
            }
        }
    }
};

// returns char like '*' in "* * *"
lChar16 getSingleLineChar( const lString16 & s) {
    lChar16 nonSpace = 0;
    for ( const lChar16 * p = s.c_str(); *p; p++ ) {
        lChar16 ch = *p;
        if ( ch!=' ' && ch!='\t' && ch!='\r' && ch!='\n' ) {
            if ( nonSpace==0 )
                nonSpace = ch;
            else if ( nonSpace!=ch )
                return 0;
        }
    }
    return nonSpace;
}

#define MAX_HEADING_CHARS 48
#define MAX_PARA_LINES 30
#define MAX_BUF_LINES  200
#define MIN_MULTILINE_PARA_WIDTH 45

class LVTextLineQueue : public LVPtrVector<LVTextFileLine>
{
private:
    LVTextFileBase * file;
    int first_line_index;
    int maxLineSize;
    lString16 bookTitle;
    lString16 bookAuthors;
    lString16 seriesName;
    lString16 seriesNumber;
    int formatFlags;
    int min_left;
    int max_right;
    int avg_left;
    int avg_right;
    int avg_center;
    int paraCount;
    int linesToSkip;
    bool lastParaWasTitle;
    bool inSubSection;
    int max_left_stats_pos;
    int max_left_second_stats_pos;
    int max_right_stats_pos;

    enum {
        tftParaPerLine = 1,
        tftParaIdents  = 2,
        tftEmptyLineDelimPara = 4,
        tftCenteredHeaders = 8,
        tftEmptyLineDelimHeaders = 16,
        tftFormatted = 32, // text lines are wrapped and formatted
        tftJustified = 64, // right bound is justified
        tftDoubleEmptyLineBeforeHeaders = 128,
        tftPreFormatted = 256,
        tftPML = 512 // Palm Markup Language
    } formatFlags_t;
public:
    LVTextLineQueue( LVTextFileBase * f, int maxLineLen )
    : file(f), first_line_index(0), maxLineSize(maxLineLen), lastParaWasTitle(false), inSubSection(false)
    {
        min_left = -1;
        max_right = -1;
        avg_left = 0;
        avg_right = 0;
        avg_center = 0;
        paraCount = 0;
        linesToSkip = 0;
        formatFlags = tftPreFormatted;
    }
    // get index of first line of queue
    int  GetFirstLineIndex() { return first_line_index; }
    // get line count read from file. Use length() instead to get count of lines queued.
    int  GetLineCount() { return first_line_index + length(); }
    // get line by line file index
    LVTextFileLine * GetLine( int index )
    {
        return get(index - first_line_index);
    }
    // remove lines from head of queue
    void RemoveLines( int lineCount )
    {
        if ( lineCount>length() )
            lineCount = length();
        erase( 0, lineCount );
        first_line_index += lineCount;
    }
    // read lines and place to tail of queue
    bool ReadLines( int lineCount )
    {
        for ( int i=0; i<lineCount; i++ ) {
            if ( file->Eof() ) {
                if ( i==0 )
                    return false;
                break;
            }
            LVTextFileLine * line = new LVTextFileLine( file, maxLineSize );
            if ( min_left>=0 )
                line->align = getFormat( line );
            add( line );
        }
        return true;
    }
    inline static int absCompare( int v1, int v2 )
    {
        if ( v1<0 )
            v1 = -v1;
        if ( v2<0 )
            v2 = -v2;
        if ( v1>v2 )
            return 1;
        else if ( v1==v2 )
            return 0;
        else
            return -1;
    }
    lineAlign_t getFormat( LVTextFileLine * line )
    {
        if ( line->lpos>=line->rpos )
            return la_empty;
        int center_dist = (line->rpos + line->lpos) / 2 - avg_center;
        int right_dist = line->rpos - avg_right;
        int left_dist = line->lpos - max_left_stats_pos;
        if ( (formatFlags & tftJustified) || (formatFlags & tftFormatted) ) {
            if ( line->lpos==min_left && line->rpos==max_right )
                return la_width;
            if ( line->lpos==min_left )
                return la_left;
            if ( line->rpos==max_right )
                return la_right;
            if ( line->lpos==max_left_second_stats_pos )
                return la_indent;
            if ( line->lpos > max_left_second_stats_pos &&
                    absCompare( center_dist, left_dist )<0
                    && absCompare( center_dist, right_dist )<0 )
                return la_centered;
            if ( absCompare( right_dist, left_dist )<0 )
                return la_right;
            if ( line->lpos > min_left )
                return la_indent;
            return la_left;
        } else {
            if ( line->lpos == min_left )
                return la_left;
            else
                return la_indent;
        }
    }
    static bool isCentered( LVTextFileLine * line )
    {
        return line->align == la_centered;
    }
    /// checks text format options
    void detectFormatFlags()
    {
        //CRLog::debug("detectFormatFlags() enter");
        formatFlags = tftParaPerLine | tftEmptyLineDelimHeaders; // default format
        if ( length()<10 )
            return;
        formatFlags = 0;
        avg_center = 0;
        int empty_lines = 0;
        int ident_lines = 0;
        int center_lines = 0;
        min_left = -1;
        max_right = -1;
        avg_left = 0;
        avg_right = 0;
        int pmlTagCount = 0;
        int i;
#define MAX_PRE_STATS 1000
        int left_stats[MAX_PRE_STATS];
        int right_stats[MAX_PRE_STATS];
        for ( i=0; i<MAX_PRE_STATS; i++ )
            left_stats[i] = right_stats[i] = 0;
        for ( i=0; i<length(); i++ ) {
            LVTextFileLine * line = get(i);
            //CRLog::debug("   LINE: %d .. %d", line->lpos, line->rpos);
            if ( line->lpos == line->rpos ) {
                empty_lines++;
            } else {
                if ( line->lpos < MAX_PRE_STATS )
                    left_stats[line->lpos]++;
                if ( line->rpos < MAX_PRE_STATS )
                    right_stats[line->rpos]++;
                if ( min_left==-1 || line->lpos<min_left )
                    min_left = line->lpos;
                if ( max_right==-1 || line->rpos>max_right )
                    max_right = line->rpos;
                avg_left += line->lpos;
                avg_right += line->rpos;
                for (int j=line->lpos; j<line->rpos-1; j++ ) {
                    lChar16 ch = line->text[j];
                    lChar16 ch2 = line->text[j+1];
                    if ( ch=='\\' ) {
                        switch ( ch2 ) {
                        case 'p':
                        case 'x':
                        case 'X':
                        case 'C':
                        case 'c':
                        case 'r':
                        case 'u':
                        case 'o':
                        case 'v':
                        case 't':
                        case 'n':
                        case 's':
                        case 'b':
                        case 'l':
                        case 'a':
                        case 'U':
                        case 'm':
                        case 'q':
                        case 'Q':
                            pmlTagCount++;
                            break;
                        }
                    }
                }
            }
        }

        // pos stats
        int max_left_stats = 0;
        max_left_stats_pos = 0;
        int max_left_second_stats = 0;
        max_left_second_stats_pos = 0;
        int max_right_stats = 0;
        max_right_stats_pos = 0;
        for ( i=0; i<MAX_PRE_STATS; i++ ) {
            if ( left_stats[i] > max_left_stats ) {
                max_left_stats = left_stats[i];
                max_left_stats_pos = i;
            }
            if ( right_stats[i] > max_right_stats ) {
                max_right_stats = right_stats[i];
                max_right_stats_pos = i;
            }
        }
        for ( i=max_left_stats_pos + 1; i<MAX_PRE_STATS; i++ ) {
            if ( left_stats[i] > max_left_second_stats ) {
                max_left_second_stats = left_stats[i];
                max_left_second_stats_pos = i;
            }
        }

        if ( pmlTagCount>20 ) {
            formatFlags = tftPML; // Palm markup
            return;
        }



        int non_empty_lines = length() - empty_lines;
        if ( non_empty_lines < 10 )
            return;
        avg_left /= non_empty_lines;
        avg_right /= non_empty_lines;
        avg_center = (avg_left + avg_right) / 2;

        //int best_left_align_percent = max_left_stats * 100 / length();
        int best_right_align_percent = max_right_stats * 100 / length();
        //int best_left_second_align_percent = max_left_second_stats * 100 / length();


        int fw = max_right_stats_pos - max_left_stats_pos;
        for ( i=0; i<length(); i++ ) {
            LVTextFileLine * line = get(i);
            //CRLog::debug("    line(%d, %d)", line->lpos, line->rpos);
            int lw = line->rpos - line->lpos;
            if ( line->lpos > min_left+1 ) {
                int center_dist = (line->rpos + line->lpos) / 2 - avg_center;
                //int right_dist = line->rpos - avg_right;
                int left_dist = line->lpos - max_left_stats_pos;
                //if ( absCompare( center_dist, right_dist )<0 )
                if ( absCompare( center_dist, left_dist )<0 ) {
                    if ( line->lpos > min_left+fw/10 && line->lpos < max_right-fw/10 && lw < 9*fw/10 ) {
                        center_lines++;
                    }
                } else
                    ident_lines++;
            }
        }
        for ( i=0; i<length(); i++ ) {
            get(i)->align = getFormat( get(i) );
        }
        if ( avg_right >= 80 ) {
            if ( empty_lines>non_empty_lines && empty_lines<non_empty_lines*110/100 ) {
                formatFlags = tftParaPerLine | tftDoubleEmptyLineBeforeHeaders; // default format
                return;
            }
            if ( empty_lines>non_empty_lines*2/3 ) {
                formatFlags = tftEmptyLineDelimPara; // default format
                return;
            }
            //tftDoubleEmptyLineBeforeHeaders
            return;
        }
        formatFlags = 0;
        int ident_lines_percent = ident_lines * 100 / non_empty_lines;
        int center_lines_percent = center_lines * 100 / non_empty_lines;
        int empty_lines_percent = empty_lines * 100 / length();
        if ( empty_lines_percent > 5 )
            formatFlags |= tftEmptyLineDelimPara;
        if ( ident_lines_percent > 5 && ident_lines_percent<55 ) {
            formatFlags |= tftParaIdents;
            if ( empty_lines_percent<7 )
                formatFlags |= tftEmptyLineDelimHeaders;
        }
        if ( center_lines_percent > 1 )
            formatFlags |= tftCenteredHeaders;

        if ( max_right < 80 )
           formatFlags |= tftFormatted; // text lines are wrapped and formatted
        if ( max_right_stats_pos == max_right && best_right_align_percent > 30 )
           formatFlags |= tftJustified; // right bound is justified

        CRLog::debug("detectFormatFlags() min_left=%d, max_right=%d, ident=%d, empty=%d, flags=%d",
            min_left, max_right, ident_lines_percent, empty_lines_percent, formatFlags );

        if ( !formatFlags ) {
            formatFlags = tftParaPerLine | tftEmptyLineDelimHeaders; // default format
            return;
        }


    }

    bool testProjectGutenbergHeader()
    {
        int i = 0;
        for ( ; i<length() && get(i)->rpos==0; i++ )
            ;
        if ( i>=length() )
            return false;
        bookTitle.clear();
        bookAuthors.clear();
        lString16 firstLine = get(i)->text;
        lString16 pgPrefix = L"The Project Gutenberg Etext of ";
        if ( firstLine.length() < pgPrefix.length() )
            return false;
        if ( firstLine.substr(0, pgPrefix.length()) != pgPrefix )
            return false;
        firstLine = firstLine.substr( pgPrefix.length(), firstLine.length() - pgPrefix.length());
        int byPos = firstLine.pos(L", by ");
        if ( byPos<=0 )
            return false;
        bookTitle = firstLine.substr( 0, byPos );
        bookAuthors = firstLine.substr( byPos + 5, firstLine.length()-byPos-5 );
        for ( ; i<length() && i<500 && get(i)->text.pos(L"*END*") != 0; i++ )
            ;
        if ( i<length() && i<500 ) {
            for ( i++; i<length() && i<500 && get(i)->text.empty(); i++ )
                ;
            linesToSkip = i;
        }
        return true;
    }

    // Leo Tolstoy. War and Peace
    bool testAuthorDotTitleFormat()
    {
        int i = 0;
        for ( ; i<length() && get(i)->rpos==0; i++ )
            ;
        if ( i>=length() )
            return false;
        bookTitle.clear();
        bookAuthors.clear();
        lString16 firstLine = get(i)->text;
        firstLine.trim();
        int dotPos = firstLine.pos(L". ");
        if ( dotPos<=0 )
            return false;
        bookAuthors = firstLine.substr( 0, dotPos );
        bookTitle = firstLine.substr( dotPos + 2, firstLine.length() - dotPos - 2);
        if ( bookTitle.empty() || (lGetCharProps(bookTitle[bookTitle.length()]) & CH_PROP_PUNCT) )
            return false;
        return true;
    }

    /// check beginning of file for book title, author and series
    bool DetectBookDescription(LVXMLParserCallback * callback)
    {

        if ( !testProjectGutenbergHeader() && !testAuthorDotTitleFormat() ) {
            bookTitle = LVExtractFilenameWithoutExtension( file->getFileName() );
            bookAuthors.clear();
/*

            int necount = 0;
            lString16 s[3];
            unsigned i;
            for ( i=0; i<(unsigned)length() && necount<2; i++ ) {
                LVTextFileLine * item = get(i);
                if ( item->rpos>item->lpos ) {
                    lString16 str = item->text;
                    str.trimDoubleSpaces(false, false, true);
                    if ( !str.empty() ) {
                        s[necount] = str;
                        necount++;
                    }
                }
            }
            //update book description
            if ( i==0 ) {
                bookTitle = L"no name";
            } else {
                bookTitle = s[1];
            }
            bookAuthors = s[0];
*/
        }

        lString16Collection author_list;
        if ( !bookAuthors.empty() )
            author_list.parse( bookAuthors, ',', true );

        unsigned i;
        for ( i=0; i<author_list.length(); i++ ) {
            lString16Collection name_list;
            name_list.parse( author_list[i], ' ', true );
            if ( name_list.length()>0 ) {
                lString16 firstName = name_list[0];
                lString16 lastName;
                lString16 middleName;
                if ( name_list.length() == 2 ) {
                    lastName = name_list[1];
                } else if ( name_list.length()>2 ) {
                    middleName = name_list[1];
                    lastName = name_list[2];
                }
                callback->OnTagOpenNoAttr( NULL, L"author" );
                  callback->OnTagOpenNoAttr( NULL, L"first-name" );
                    if ( !firstName.empty() )
                        callback->OnText( firstName.c_str(), firstName.length(), TXTFLG_TRIM|TXTFLG_TRIM_REMOVE_EOL_HYPHENS );
                  callback->OnTagClose( NULL, L"first-name" );
                  callback->OnTagOpenNoAttr( NULL, L"middle-name" );
                    if ( !middleName.empty() )
                        callback->OnText( middleName.c_str(), middleName.length(), TXTFLG_TRIM|TXTFLG_TRIM_REMOVE_EOL_HYPHENS );
                  callback->OnTagClose( NULL, L"middle-name" );
                  callback->OnTagOpenNoAttr( NULL, L"last-name" );
                    if ( !lastName.empty() )
                        callback->OnText( lastName.c_str(), lastName.length(), TXTFLG_TRIM|TXTFLG_TRIM_REMOVE_EOL_HYPHENS );
                  callback->OnTagClose( NULL, L"last-name" );
                callback->OnTagClose( NULL, L"author" );
            }
        }
        callback->OnTagOpenNoAttr( NULL, L"book-title" );
            if ( !bookTitle.empty() )
                callback->OnText( bookTitle.c_str(), bookTitle.length(), 0 );
        callback->OnTagClose( NULL, L"book-title" );
        if ( !seriesName.empty() || !seriesNumber.empty() ) {
            callback->OnTagOpenNoAttr( NULL, L"sequence" );
            if ( !seriesName.empty() )
                callback->OnAttribute( NULL, L"name", seriesName.c_str() );
            if ( !seriesNumber.empty() )
                callback->OnAttribute( NULL, L"number", seriesNumber.c_str() );
            callback->OnTagClose( NULL, L"sequence" );
        }

        // remove description lines
        if ( linesToSkip>0 )
            RemoveLines( linesToSkip );
        return true;
    }
    /// add one paragraph
    void AddEmptyLine( LVXMLParserCallback * callback )
    {
        callback->OnTagOpenAndClose( NULL, L"empty-line" );
    }
    /// add one paragraph
    void AddPara( int startline, int endline, LVXMLParserCallback * callback )
    {
        // TODO: remove pos, sz tracking
        lString16 str;
        //lvpos_t pos = 0;
        //lvsize_t sz = 0;
        for ( int i=startline; i<=endline; i++ ) {
            LVTextFileLine * item = get(i);
            //if ( i==startline )
            //    pos = item->fpos;
            //sz = (item->fpos + item->fsize) - pos;
            str += item->text + L"\n";
        }
        bool singleLineFollowedByEmpty = false;
        bool singleLineFollowedByTwoEmpty = false;
        if ( startline==endline && endline<length()-1 ) {
            if ( !(formatFlags & tftParaIdents) || get(startline)->lpos>0 )
                if ( get(endline+1)->rpos==0 && (startline==0 || get(startline-1)->rpos==0) ) {
                    singleLineFollowedByEmpty = get(startline)->text.length()<MAX_HEADING_CHARS;
                    if ( (startline<=1 || get(startline-2)->rpos==0) )
                        singleLineFollowedByTwoEmpty = get(startline)->text.length()<MAX_HEADING_CHARS;
                }
        }
        str.trimDoubleSpaces(false, false, true);
        lChar16 singleChar = getSingleLineChar( str );
        if ( singleChar!=0 && singleChar>='A' )
            singleChar = 0;
        bool isHeader = singleChar!=0;
        if ( formatFlags & tftDoubleEmptyLineBeforeHeaders ) {
            isHeader = singleLineFollowedByTwoEmpty;
            if ( singleLineFollowedByEmpty && startline<3 && str.length()<MAX_HEADING_CHARS )
                isHeader = true;
            else if ( startline<2 && str.length()<MAX_HEADING_CHARS )
                isHeader = true;
            if ( str.length()==0 )
                return; // no empty lines
        } else {

            if ( ( startline==endline && str.length()<4) || (paraCount<2 && str.length()<50 && startline<length()-2 && (get(startline+1)->rpos==0||get(startline+2)->rpos==0) ) ) //endline<3 &&
                isHeader = true;
            if ( startline==endline && get(startline)->isHeading() )
                isHeader = true;
            if ( startline==endline && (formatFlags & tftCenteredHeaders) && isCentered( get(startline) ) )
                isHeader = true;
            int hlevel = DetectHeadingLevelByText( str );
            if ( hlevel>0 )
                isHeader = true;
            if ( singleLineFollowedByEmpty && !(formatFlags & tftEmptyLineDelimPara) )
                isHeader = true;
        }
        if ( str.length() > MAX_HEADING_CHARS )
            isHeader = false;
        if ( !str.empty() ) {
            const lChar16 * title_tag = L"title";
            if ( isHeader ) {
                if ( singleChar ) { //str.compare(L"* * *")==0 ) {
                    title_tag = L"subtitle";
                    lastParaWasTitle = false;
                } else {
                    if ( !lastParaWasTitle ) {
                        if ( inSubSection )
                            callback->OnTagClose( NULL, L"section" );
                        callback->OnTagOpenNoAttr( NULL, L"section" );
                        inSubSection = true;
                    }
                    lastParaWasTitle = true;
                }
                callback->OnTagOpenNoAttr( NULL, title_tag );
            } else
                    lastParaWasTitle = false;
            callback->OnTagOpenNoAttr( NULL, L"p" );
               callback->OnText( str.c_str(), str.length(), TXTFLG_TRIM | TXTFLG_TRIM_REMOVE_EOL_HYPHENS );
            callback->OnTagClose( NULL, L"p" );
            if ( isHeader ) {
                callback->OnTagClose( NULL, title_tag );
            } else {
            }
            paraCount++;
        } else {
            if ( !(formatFlags & tftEmptyLineDelimPara) || !isHeader ) {
                callback->OnTagOpenAndClose( NULL, L"empty-line" );
            }
        }
    }

    class PMLTextImport {
        LVXMLParserCallback * callback;
        bool insideInvisibleText;
        const lChar16 * cp1252;
        int align; // 0, 'c' or 'r'
        lString16 line;
        int chapterIndent;
        bool insideChapterTitle;
        lString16 chapterTitle;
        int sectionId;
        bool inSection;
        bool inParagraph;
        bool indented;
        bool inLink;
        lString16 styleTags;
    public:
        PMLTextImport( LVXMLParserCallback * cb )
        : callback(cb), insideInvisibleText(false), align(0)
        , chapterIndent(0)
        , insideChapterTitle(false)
        , sectionId(0)
        , inSection(false)
        , inParagraph(false)
        , indented(false)
        , inLink(false)
        {
            cp1252 = GetCharsetByte2UnicodeTable(L"windows-1252");
        }
        void addChar( lChar16 ch ) {
            if ( !insideInvisibleText )
                line << ch;
        }

        const lChar16 * getStyleTagName( lChar16 ch ) {
            switch ( ch ) {
            case 'b':
            case 'B':
                return L"b";
            case 'i':
                return L"i";
            case 'u':
                return L"u";
            case 's':
                return L"strikethrough";
            case 'a':
                return L"a";
            default:
                return NULL;
            }
        }

        int styleTagPos(lChar16 ch) {
            for ( unsigned i=0; i<styleTags.length(); i++ )
                if ( styleTags[i]==ch )
                    return i;
            return -1;
        }

        void closeStyleTag( lChar16 ch, bool updateStack ) {
            int pos = ch ? styleTagPos( ch ) : 0;
            if ( updateStack && pos<0 )
                return;
            //if ( updateStack )
            //if ( !line.empty() )
                postText();
            for ( int i=styleTags.length()-1; i>=pos; i-- ) {
                const lChar16 * tag = getStyleTagName(styleTags[i]);
                if ( updateStack )
                    styleTags.erase(styleTags.length()-1, 1);
                if ( tag ) {
                    callback->OnTagClose(L"", tag);
                }
            }
        }

        void openStyleTag( lChar16 ch, bool updateStack ) {
            int pos = styleTagPos( ch );
            if ( updateStack && pos>=0 )
                return;
            if ( updateStack )
            //if ( !line.empty() )
                postText();
            const lChar16 * tag = getStyleTagName(ch);
            if ( tag ) {
                callback->OnTagOpenNoAttr(L"", tag);
                if ( updateStack )
                    styleTags.append( 1,  ch );
            }
        }

        void openStyleTags() {
            for ( unsigned i=0; i<styleTags.length(); i++ )
                openStyleTag(styleTags[i], false);
        }

        void closeStyleTags() {
            for ( int i=styleTags.length()-1; i>=0; i-- )
                closeStyleTag(styleTags[i], false);
        }

        void onStyleTag(lChar16 ch ) {
            int pos = ch!=0 ? styleTagPos( ch ) : 0;
            if ( pos<0 ) {
                openStyleTag(ch, true);
            } else {
                closeStyleTag(ch, true);
            }

        }

        void onImage( lString16 url ) {
            //url = lString16("book_img/") + url;
            callback->OnTagOpen(L"", L"img");
            callback->OnAttribute(L"", L"src", url.c_str());
            callback->OnTagBody();
            callback->OnTagClose(L"", L"img");
        }

        void startParagraph() {
            if ( !inParagraph ) {
                callback->OnTagOpen(L"", L"p");
                lString16 style;
                if ( indented )
                    style<< L"left-margin: 15%; ";
                if ( align ) {
                    if ( align=='c' ) {
                        style << L"text-align: center; ";
                        if ( !indented )
                            style << L"text-indent: 0px; ";
                    } else if ( align=='r' )
                        style << L"text-align: right; ";
                }
                if ( !style.empty() )
                    callback->OnAttribute(L"", L"style", style.c_str() );
                callback->OnTagBody();
                openStyleTags();
                inParagraph = true;
            }
        }

        void postText() {
            startParagraph();

            if ( !line.empty() ) {
                callback->OnText(line.c_str(), line.length(), 0);
                line.clear();
            }
        }


        void startPage() {
            if ( inSection )
                return;
            sectionId++;
            callback->OnTagOpen(NULL, L"section");
            callback->OnAttribute(NULL, L"id", (lString16(L"_section") + lString16::itoa(sectionId)).c_str() );
            callback->OnTagBody();
            inSection = true;
            endOfParagraph();
        }
        void endPage() {
            if ( !inSection )
                return;
            indented = false;
            endOfParagraph();
            callback->OnTagClose(NULL, L"section");
            inSection = false;
        }
        void newPage() {
            endPage();
            startPage();
        }

        void endOfParagraph() {
//            if ( line.empty() )
//                return;
            // post text
            //startParagraph();
            if ( !line.empty() )
                postText();
            // clear current text
            line.clear();
            if ( inParagraph ) {
                //closeStyleTag(0);
                closeStyleTags();
                callback->OnTagClose(L"", L"p");
                inParagraph = false;
            }
        }

        void addSeparator( int width ) {
            endOfParagraph();
            callback->OnTagOpenAndClose(L"", L"hr");
        }

        void startOfChapterTitle( bool startNewPage, int level ) {
            endOfParagraph();
            if ( startNewPage )
                newPage();
            chapterTitle.clear();
            insideChapterTitle = true;
            chapterIndent = level;
            callback->OnTagOpenNoAttr(NULL, L"title");
        }

        void addChapterTitle( int level, lString16 title ) {
            // add title, invisible, for TOC only
        }

        void endOfChapterTitle() {
            chapterTitle.clear();
            if ( !insideChapterTitle )
                return;
            endOfParagraph();
            insideChapterTitle = false;
            callback->OnTagClose(NULL, L"title");
        }

        void addAnchor( lString16 ref ) {
            startParagraph();
            callback->OnTagOpen(NULL, L"a");
            callback->OnAttribute(NULL, L"name", ref.c_str());
            callback->OnTagBody();
            callback->OnTagClose(NULL, L"a");
        }

        void startLink( lString16 ref ) {
            if ( !inLink ) {
                postText();
                callback->OnTagOpen(NULL, L"a");
                callback->OnAttribute(NULL, L"href", ref.c_str());
                callback->OnTagBody();
                styleTags << L"a";
                inLink = true;
            }
        }

        void endLink() {
            if ( inLink ) {
                inLink = false;
                closeStyleTag('a', true);
                //callback->OnTagClose(NULL, L"a");
            }
        }

        lString16 readParam( const lChar16 * str, int & j ) {
            lString16 res;
            if ( str[j]!='=' || str[j+1]!='\"' )
                return res;
            for ( j=j+2; str[j] && str[j]!='\"'; j++ )
                res << str[j];
            return res;
        }

        void processLine( lString16 text ) {
            int len = text.length();
            const lChar16 * str = text.c_str();
            for ( int j=0; j<len; j++ ) {
                //bool isStartOfLine = (j==0);
                lChar16 ch = str[j];
                lChar16 ch2 = str[j+1];
                if ( ch=='\\' ) {
                    if ( ch2=='a' ) {
                        // \aXXX	Insert non-ASCII character whose Windows 1252 code is decimal XXX.
                        int n = decodeDecimal( str + j + 2, 3 );
                        bool use1252 = true;
                        if ( n>=128 && n<=255 && use1252 ) {
                            addChar( cp1252[n-128] );
                            j+=4;
                            continue;
                        } else if ( n>=1 && n<=255 ) {
                            addChar( n );
                            j+=4;
                            continue;
                        }
                    } else if ( ch2=='U' ) {
                        // \UXXXX	Insert non-ASCII character whose Unicode code is hexidecimal XXXX.
                        int n = decodeHex( str + j + 2, 4 );
                        if ( n>0 ) {
                            addChar( n );
                            j+=5;
                            continue;
                        }
                    } else if ( ch2=='\\' ) {
                        // insert '\'
                        addChar( ch2 );
                        j++;
                        continue;
                    } else if ( ch2=='-' ) {
                        // insert '\'
                        addChar( UNICODE_SOFT_HYPHEN_CODE );
                        j++;
                        continue;
                    } else if ( ch2=='T' ) {
                        // Indents the specified percentage of the screen width, 50% in this case.
                        // If the current drawing position is already past the specified screen location, this tag is ignored.
                        j+=2;
                        lString16 w = readParam( str, j );
                        // IGNORE
                        continue;
                    } else if ( ch2=='m' ) {
                        // Insert the named image.
                        j+=2;
                        lString16 image = readParam( str, j );
                        onImage( image );
                        continue;
                    } else if ( ch2=='Q' ) {
                        // \Q="linkanchor" - Specify a link anchor in the document.
                        j+=2;
                        lString16 anchor = readParam( str, j );
                        addAnchor(anchor);
                        continue;
                    } else if ( ch2=='q' ) {
                        // \q="#linkanchor"Some text\q	Reference a link anchor which is at another spot in the document.
                        // The string after the anchor specification and before the trailing \q is underlined
                        // or otherwise shown to be a link when viewing the document.
                        if ( !inLink ) {
                            j+=2;
                            lString16 ref = readParam( str, j );
                            startLink(ref);
                        } else {
                            j+=1;
                            endLink();
                        }
                        continue;
                    } else if ( ch2=='w' ) {
                        // Embed a horizontal rule of a given percentage width of the screen, in this case 50%.
                        // This tag causes a line break before and after it. The rule is centered. The percent sign is mandatory.
                        j+=2;
                        lString16 w = readParam( str, j );
                        addSeparator( 50 );
                        continue;
                    } else if ( ch2=='C' ) {
                        // \Cn="Chapter title"
                        // Insert "Chapter title" into the chapter listing, with level n (like \Xn).
                        // The text is not shown on the page and does not force a page break.
                        // This can sometimes be useful to insert a chapter mark at the beginning of an introduction to the chapter, for example.
                        if ( str[2] && str[3]=='=' && str[4]=='\"' ) {
                            int level = hexDigit(str[2]);
                            if ( level<0 || level>4 )
                                level = 0;
                            j+=5; // skip \Cn="
                            lString16 title;
                            for ( ;str[j] && str[j]!='\"'; j++ )
                                title << str[j];
                            addChapterTitle( level, title );
                            continue;
                        } else {
                            j++;
                            continue;
                        }
                    } else {
                        bool unknown = false;
                        switch( ch2 ) {
                        case 'v':
                            insideInvisibleText = !insideInvisibleText;
                            break;
                        case 'c':
                            //if ( isStartOfLine ) {
                                endOfParagraph();
                                align = (align==0) ? 'c' : 0;
                            //}
                            break;
                        case 'r':
                            //if ( isStartOfLine ) {
                                endOfParagraph();
                                align = (align==0) ? 'r' : 0;
                            //}
                            break;
                        case 't':
                            indented = !indented;
                            break;
                        case 'i':
                            onStyleTag('i');
                            break;
                        case 'u':
                            onStyleTag('u');
                            break;
                        case 'o':
                            onStyleTag('s');
                            break;
                        case 'b':
                            onStyleTag('b');
                            break;
                        case 'd':
                            break;
                        case 'B':
                            onStyleTag('B');
                            break;
                        case 'p': //New page
                            newPage();
                            break;
                        case 'n':
                            // normal font
                            break;
                        case 's':
                            // small font
                            break;
//                        case 'b':
//                            // bold font
//                            break;
                        case 'l':
                            // large font
                            break;
                        case 'x': //New chapter; also causes a new page break.
                                  //Enclose chapter title (and any style codes) with \x and \x
                        case 'X': //New chapter, indented n levels (n between 0 and 4 inclusive) in the Chapter dialog; doesn't cause a page break.
                                  //Enclose chapter title (and any style codes) with \Xn and \Xn
                            {
                                int level = 0;
                                if ( ch2=='X' ) {
                                    switch( str[j+2] ) {
                                    case '1':
                                        level = 1;
                                        break;
                                    case '2':
                                        level = 2;
                                        break;
                                    case '3':
                                        level = 3;
                                        break;
                                    case '4':
                                        level = 4;
                                        break;
                                    }
                                    j++;
                                }
                                if ( !insideChapterTitle ) {
                                    startOfChapterTitle( ch2=='x', level );
                                } else {
                                    endOfChapterTitle();
                                }
                                break;
                            }
                            break;
                        default:
                            unknown = true;
                            break;
                        }
                        if ( !unknown ) {
                            j++; // 2 chars processed
                            continue;
                        }
                    }
                }
                addChar( ch );
            }
            endOfParagraph();
        }
    };

    /// one line per paragraph
    bool DoPMLImport(LVXMLParserCallback * callback)
    {
        CRLog::debug("DoPMLImport()");
        RemoveLines( length() );
        file->Reset();
        file->SetCharset(L"windows-1252");
        ReadLines( 100 );
        int remainingLines = 0;
        PMLTextImport importer(callback);
        do {
            for ( int i=remainingLines; i<length(); i++ ) {
                LVTextFileLine * item = get(i);
                importer.processLine(item->text);
                file->updateProgress();
            }
            RemoveLines( length()-3 );
            remainingLines = 3;
        } while ( ReadLines( 100 ) );
        importer.endPage();
        return true;
    }

    /// one line per paragraph
    bool DoParaPerLineImport(LVXMLParserCallback * callback)
    {
        CRLog::debug("DoParaPerLineImport()");
        int remainingLines = 0;
        do {
            for ( int i=remainingLines; i<length(); i++ ) {
                if ( formatFlags & tftDoubleEmptyLineBeforeHeaders ) {
                    LVTextFileLine * item = get(i);
                    if ( !item->empty() )
                        AddPara( i, i, callback );
                } else {
                    AddPara( i, i, callback );
                }
                file->updateProgress();
            }
            RemoveLines( length()-3 );
            remainingLines = 3;
        } while ( ReadLines( 100 ) );
        if ( inSubSection )
            callback->OnTagClose( NULL, L"section" );
        return true;
    }

    /// delimited by first line ident
    bool DoIdentParaImport(LVXMLParserCallback * callback)
    {
        CRLog::debug("DoIdentParaImport()");
        int pos = 0;
        for ( ;; ) {
            if ( length()-pos <= MAX_PARA_LINES ) {
                if ( pos )
                    RemoveLines( pos );
                ReadLines( MAX_BUF_LINES );
                pos = 0;
            }
            if ( pos>=length() )
                break;
            int i=pos+1;
            bool emptyLineFlag = false;
            if ( pos>=length() || DetectHeadingLevelByText( get(pos)->text )==0 ) {
                for ( ; i<length() && i<pos+MAX_PARA_LINES; i++ ) {
                    LVTextFileLine * item = get(i);
                    if ( item->lpos>min_left ) {
                        // ident
                        break;
                    }
                    if ( item->lpos==item->rpos ) {
                        // empty line
                        i++;
                        emptyLineFlag = true;
                        break;
                    }
                }
            }
            AddPara( pos, i-1 - (emptyLineFlag?1:0), callback );
            file->updateProgress();
            pos = i;
        }
        if ( inSubSection )
            callback->OnTagClose( NULL, L"section" );
        return true;
    }
    /// delimited by empty lines
    bool DoEmptyLineParaImport(LVXMLParserCallback * callback)
    {
        CRLog::debug("DoEmptyLineParaImport()");
        int pos = 0;
        int shortLineCount = 0;
        int emptyLineCount = 0;
        for ( ;; ) {
            if ( length()-pos <= MAX_PARA_LINES ) {
                if ( pos )
                    RemoveLines( pos - 1 );
                ReadLines( MAX_BUF_LINES );
                pos = 1;
            }
            if ( pos>=length() )
                break;
            // skip starting empty lines
            while ( pos<length() ) {
                LVTextFileLine * item = get(pos);
                if ( item->lpos!=item->rpos )
                    break;
                pos++;
            }
            int i=pos;
            if ( pos>=length() || DetectHeadingLevelByText( get(pos)->text )==0 ) {
                for ( ; i<length() && i<pos+MAX_PARA_LINES; i++ ) {
                    LVTextFileLine * item = get(i);
                    if ( item->lpos==item->rpos ) {
                        // empty line
                        emptyLineCount++;
                        break;
                    }
                    if ( item->rpos - item->lpos < MIN_MULTILINE_PARA_WIDTH ) {
                        // next line is very short, possible paragraph start
                        shortLineCount++;
                        break;
                    }
                    shortLineCount = 0;
                    emptyLineCount = 0;
                }
            }
            if ( i==length() )
                i--;
            if ( i>=pos ) {
                AddPara( pos, i, callback );
                file->updateProgress();
                if ( emptyLineCount ) {
                    if ( shortLineCount > 1 )
                        AddEmptyLine( callback );
                    shortLineCount = 0;
                    emptyLineCount = 0;
                }
            }
            pos = i+1;
        }
        if ( inSubSection )
            callback->OnTagClose( NULL, L"section" );
        return true;
    }
    /// delimited by empty lines
    bool DoPreFormattedImport(LVXMLParserCallback * callback)
    {
        CRLog::debug("DoPreFormattedImport()");
        int remainingLines = 0;
        do {
            for ( int i=remainingLines; i<length(); i++ ) {
                LVTextFileLine * item = get(i);
                if ( item->rpos > item->lpos ) {
                    callback->OnTagOpenNoAttr( NULL, L"pre" );
                       callback->OnText( item->text.c_str(), item->text.length(), item->flags );
                       file->updateProgress();

                    callback->OnTagClose( NULL, L"pre" );
                } else {
                    callback->OnTagOpenAndClose( NULL, L"empty-line" );
                }
           }
            RemoveLines( length()-3 );
            remainingLines = 3;
        } while ( ReadLines( 100 ) );
        if ( inSubSection )
            callback->OnTagClose( NULL, L"section" );
        return true;
    }
    /// import document body
    bool DoTextImport(LVXMLParserCallback * callback)
    {
        if ( formatFlags & tftPML)
            return DoPMLImport( callback );
        else if ( formatFlags & tftPreFormatted )
            return DoPreFormattedImport( callback );
        else if ( formatFlags & tftParaIdents )
            return DoIdentParaImport( callback );
        else if ( formatFlags & tftEmptyLineDelimPara )
            return DoEmptyLineParaImport( callback );
        else
            return DoParaPerLineImport( callback );
    }
};

/// reads next text line, tells file position and size of line, sets EOL flag
lString16 LVTextFileBase::ReadLine( int maxLineSize, lUInt32 & flags )
{
    //fsize = 0;
    flags = 0;

    lString16 res;
    res.reserve( 80 );
    //FillBuffer( maxLineSize*3 );

    lChar16 ch = 0;
    while ( 1 ) {
        if ( m_eof ) {
            // EOF: treat as EOLN
            flags |= LINE_HAS_EOLN; // EOLN flag
            break;
        }
        ch = ReadCharFromBuffer();
        //if ( ch==0xFEFF && fpos==0 && res.empty() ) {
        //} else 
        if ( ch!='\r' && ch!='\n' ) {
            res.append( 1, ch );
            if ( ch==' ' || ch=='\t' ) {
                if ( res.length()>=(unsigned)maxLineSize )
                    break;
            }
        } else {
            // eoln
            if ( !m_eof ) {
                lChar16 ch2 = PeekCharFromBuffer();
                if ( ch2!=ch && (ch2=='\r' || ch2=='\n') ) {
                    ReadCharFromBuffer();
                }
            }
            flags |= LINE_HAS_EOLN; // EOLN flag
            break;
        }
    }

    if ( !res.empty() ) {
        int firstNs = 0;
        lChar16 ch = 0;
        for ( ;; firstNs++ ) {
            ch = res[firstNs];
            if ( !ch )
                break;
            if ( ch!=' ' && ch!='\t' )
                break;
        }
        if ( ch==0x14 ) {
            if ( res[res.length()-1] == 0x15 ) {
                // LIB.RU header flags
                res.erase( res.length()-1, 1 );
                res.erase( 0, firstNs+1 );
                flags |= LINE_IS_HEADER;
            }
        } else if ( ch=='-' || ch=='*' || ch=='=' ) {
            bool sameChars = true;
            for ( unsigned i=firstNs; i<res.length(); i++ ) {
                lChar16 ch2 = res[i];
                if ( ch2!=' ' && ch2!='\t' && ch2!=ch ) {
                    sameChars = false;
                    break;
                }
            }
            if ( sameChars ) {
                res = L"* * *"; // hline
                flags |= LINE_IS_HEADER;
            }
        }
    }


    res.pack();
    return res;
}

//=======================
// Text bookmark parser

/// constructor
LVTextBookmarkParser::LVTextBookmarkParser( LVStreamRef stream, LVXMLParserCallback * callback )
    : LVTextParser(stream, callback, false)
{
}

/// descructor
LVTextBookmarkParser::~LVTextBookmarkParser()
{
}

/// returns true if format is recognized by parser
bool LVTextBookmarkParser::CheckFormat()
{
    Reset();
    // encoding test
    m_lang_name = lString16("en");
    SetCharset( L"utf8" );

    #define TEXT_PARSER_DETECT_SIZE 16384
    Reset();
    lChar16 * chbuf = new lChar16[TEXT_PARSER_DETECT_SIZE];
    FillBuffer( TEXT_PARSER_DETECT_SIZE );
    int charsDecoded = ReadTextBytes( 0, m_buf_len, chbuf, TEXT_PARSER_DETECT_SIZE-1, 0 );
    bool res = false;
    lString16 pattern("# Cool Reader 3 - exported bookmarks\r\n# file name: ");
    if ( charsDecoded > (int)pattern.length() && chbuf[0]==0xFEFF) { // BOM
        res = true;
        for ( int i=0; i<(int)pattern.length(); i++ )
            if ( chbuf[i+1] != pattern[i] )
                res = false;
    }
    delete[] chbuf;
    Reset();
    return res;
}

static bool extractItem( lString16 & dst, const lString16 & src, const char * prefix )
{
    lString16 pref( prefix );
    if ( src.startsWith( pref ) ) {
        dst = src.substr( pref.length() );
        return true;
    }
    return false;
}

static void postParagraph( LVXMLParserCallback * callback, const char * prefix, lString16 text )
{
    lString16 title( prefix );
    if ( text.empty() )
        return;
    callback->OnTagOpen( NULL, L"p" );
    callback->OnAttribute(NULL, L"style", L"text-indent: 0em");
    callback->OnTagBody();
    if ( !title.empty() ) {
        callback->OnTagOpenNoAttr( NULL, L"strong" );
        callback->OnText( title.c_str(), title.length(), 0 );
        callback->OnTagClose( NULL, L"strong" );
    }
    callback->OnText( text.c_str(), text.length(), 0 );
    callback->OnTagClose( NULL, L"p" );
}

/// parses input stream
bool LVTextBookmarkParser::Parse()
{
    lString16 line;
    lUInt32 flags = 0;
    lString16 fname("Unknown");
    lString16 path;
    lString16 title("No Title");
    lString16 author;
    for ( ;; ) {
        line = ReadLine( 20000, flags );
        if ( line.empty() || m_eof )
            break;
        extractItem( fname, line, "# file name: " );
        extractItem( path,  line, "# file path: " );
        extractItem( title, line, "# book title: " );
        extractItem( author, line, "# author: " );
        //if ( line.startsWith( lString16() )
    }
    lString16 desc;
    desc << L"Bookmarks: ";
    if ( !author.empty() )
        desc << author << L"  ";
    if ( !title.empty() )
        desc << title << L"  ";
    else
        desc << fname << L"  ";
    //queue.
    // make fb2 document structure
    m_callback->OnTagOpen( NULL, L"?xml" );
    m_callback->OnAttribute( NULL, L"version", L"1.0" );
    m_callback->OnAttribute( NULL, L"encoding", GetEncodingName().c_str() );
    m_callback->OnEncoding( GetEncodingName().c_str(), GetCharsetTable( ) );
    m_callback->OnTagBody();
    m_callback->OnTagClose( NULL, L"?xml" );
    m_callback->OnTagOpenNoAttr( NULL, L"FictionBook" );
      // DESCRIPTION
      m_callback->OnTagOpenNoAttr( NULL, L"description" );
        m_callback->OnTagOpenNoAttr( NULL, L"title-info" );
          m_callback->OnTagOpenNoAttr( NULL, L"book-title" );
            m_callback->OnText( desc.c_str(), desc.length(), 0 );
          m_callback->OnTagClose( NULL, L"book-title" );
        m_callback->OnTagClose( NULL, L"title-info" );
      m_callback->OnTagClose( NULL, L"description" );
      // BODY
      m_callback->OnTagOpenNoAttr( NULL, L"body" );
          m_callback->OnTagOpenNoAttr( NULL, L"title" );
              postParagraph( m_callback, "", lString16("CoolReader Bookmarks file") );
          m_callback->OnTagClose( NULL, L"title" );
          postParagraph( m_callback, "file: ", fname );
          postParagraph( m_callback, "path: ", path );
          postParagraph( m_callback, "title: ", title );
          postParagraph( m_callback, "author: ", author );
          m_callback->OnTagOpenAndClose( NULL, L"empty-line" );
          m_callback->OnTagOpenNoAttr( NULL, L"section" );
          // process text
            for ( ;; ) {
                line = ReadLine( 20000, flags );
                if ( m_eof )
                    break;
                if ( line.empty() ) {
                  m_callback->OnTagOpenAndClose( NULL, L"empty-line" );
                } else {
                    lString16 prefix;
                    lString16 txt = line;
                    if ( txt.length()>3 && txt[1]==txt[0] && txt[2]==' ' ) {
                        if ( txt[0] < 'A' ) {
                            prefix = txt.substr(0, 3);
                            txt = txt.substr( 3 );
                        }
                        if ( prefix==L"## " ) {
                            prefix = txt;
                            txt = L" ";
                        }
                    }
                    postParagraph( m_callback, UnicodeToUtf8(prefix).c_str(), txt );
                }
            }
        m_callback->OnTagClose( NULL, L"section" );
      m_callback->OnTagClose( NULL, L"body" );
    m_callback->OnTagClose( NULL, L"FictionBook" );
    return true;
}


//==================================================
// Text file parser

/// constructor
LVTextParser::LVTextParser( LVStreamRef stream, LVXMLParserCallback * callback, bool isPreFormatted )
    : LVTextFileBase(stream)
    , m_callback(callback)
    , m_isPreFormatted( isPreFormatted )
{
    m_firstPageTextCounter = 300;
}

/// descructor
LVTextParser::~LVTextParser()
{
}


/// returns true if format is recognized by parser
bool LVTextParser::CheckFormat()
{
    Reset();
    // encoding test
    if ( !AutodetectEncoding() )
        return false;
    #define TEXT_PARSER_DETECT_SIZE 16384
    Reset();
    lChar16 * chbuf = new lChar16[TEXT_PARSER_DETECT_SIZE];
    FillBuffer( TEXT_PARSER_DETECT_SIZE );
    int charsDecoded = ReadTextBytes( 0, m_buf_len, chbuf, TEXT_PARSER_DETECT_SIZE-1, 0 );
    bool res = false;
    if ( charsDecoded > 16 ) {
        int illegal_char_count = 0;
        int crlf_count = 0;
        int space_count = 0;
        for ( int i=0; i<charsDecoded; i++ ) {
            if ( chbuf[i]<=32 ) {
                switch( chbuf[i] ) {
                case ' ':
                case '\t':
                    space_count++;
                    break;
                case 10:
                case 13:
                    crlf_count++;
                    break;
                case 12:
                //case 9:
                case 8:
                case 7:
                case 30:
                case 0x14:
                case 0x15:
                    break;
                default:
                    illegal_char_count++;
                }
            }
        }
        if ( illegal_char_count==0 && (space_count>=charsDecoded/16 || crlf_count>0) )
            res = true;
        if ( illegal_char_count>0 )
            CRLog::error("illegal characters detected: count=%d", illegal_char_count );
    }
    delete[] chbuf;
    Reset();
    return res;
}

/// parses input stream
bool LVTextParser::Parse()
{
    LVTextLineQueue queue( this, 2000 );
    queue.ReadLines( 2000 );
    if ( !m_isPreFormatted )
        queue.detectFormatFlags();
    // make fb2 document structure
    m_callback->OnTagOpen( NULL, L"?xml" );
    m_callback->OnAttribute( NULL, L"version", L"1.0" );
    m_callback->OnAttribute( NULL, L"encoding", GetEncodingName().c_str() );
    m_callback->OnEncoding( GetEncodingName().c_str(), GetCharsetTable( ) );
    m_callback->OnTagBody();
    m_callback->OnTagClose( NULL, L"?xml" );
    m_callback->OnTagOpenNoAttr( NULL, L"FictionBook" );
      // DESCRIPTION
      m_callback->OnTagOpenNoAttr( NULL, L"description" );
        m_callback->OnTagOpenNoAttr( NULL, L"title-info" );
          queue.DetectBookDescription( m_callback );
        m_callback->OnTagClose( NULL, L"title-info" );
      m_callback->OnTagClose( NULL, L"description" );
      // BODY
      m_callback->OnTagOpenNoAttr( NULL, L"body" );
        //m_callback->OnTagOpen( NULL, L"section" );
          // process text
          queue.DoTextImport( m_callback );
        //m_callback->OnTagClose( NULL, L"section" );
      m_callback->OnTagClose( NULL, L"body" );
    m_callback->OnTagClose( NULL, L"FictionBook" );
    return true;
}


/*******************************************************************************/
// LVXMLTextCache
/*******************************************************************************/

/// parses input stream
bool LVXMLTextCache::Parse()
{
    return true;
}

/// returns true if format is recognized by parser
bool LVXMLTextCache::CheckFormat()
{
    return true;
}

LVXMLTextCache::~LVXMLTextCache()
{
    while (m_head)
    {
        cache_item * ptr = m_head;
        m_head = m_head->next;
        delete ptr;
    }
}

void LVXMLTextCache::addItem( lString16 & str )
{
    cleanOldItems( str.length() );
    cache_item * ptr = new cache_item( str );
    ptr->next = m_head;
    m_head = ptr;
}

void LVXMLTextCache::cleanOldItems( lUInt32 newItemChars )
{
    lUInt32 sum_chars = newItemChars;
    cache_item * ptr = m_head, * prevptr = NULL;
    for ( lUInt32 n = 1; ptr; ptr = ptr->next, n++ )
    {
        sum_chars += ptr->text.length();
        if (sum_chars > m_max_charcount || n>=m_max_itemcount )
        {
            // remove tail
            for (cache_item * p = ptr; p; )
            {
                cache_item * tmp = p;
                p = p->next;
                delete tmp;
            }
            if (prevptr)
                prevptr->next = NULL;
            else
                m_head = NULL;
            return;
        }
        prevptr = ptr;
    }
}

lString16 LVXMLTextCache::getText( lUInt32 pos, lUInt32 size, lUInt32 flags )
{
    // TRY TO SEARCH IN CACHE
    cache_item * ptr = m_head, * prevptr = NULL;
    for ( ;ptr ;ptr = ptr->next )
    {
        if (ptr->pos == pos)
        {
            // move to top
            if (prevptr)
            {
                prevptr->next = ptr->next;
                ptr->next = m_head;
                m_head = ptr;
            }
            return ptr->text;
        }
    }
    // NO CACHE RECORD FOUND
    // read new pme
    lString16 text;
    text.reserve(size);
    text.append(size, ' ');
    lChar16 * buf = text.modify();
    unsigned chcount = (unsigned)ReadTextBytes( pos, size, buf, size, flags );
    //CRLog::debug("ReadTextBytes(%d,%d) done - %d chars read", (int)pos, (int)size, (int)chcount);
    text.limit( chcount );
    PreProcessXmlString( text, flags );
    if ( (flags & TXTFLG_TRIM) && (!(flags & TXTFLG_PRE) || (flags & TXTFLG_PRE_PARA_SPLITTING)) ) {
        text.trimDoubleSpaces(
            (flags & TXTFLG_TRIM_ALLOW_START_SPACE)?true:false,
            (flags & TXTFLG_TRIM_ALLOW_END_SPACE)?true:false,
            (flags & TXTFLG_TRIM_REMOVE_EOL_HYPHENS)?true:false );
    }
    // ADD TEXT TO CACHE
    addItem( text );
    m_head->pos = pos;
    m_head->size = size;
    m_head->flags = flags;
    return m_head->text;
}

/*******************************************************************************/
// XML parser
/*******************************************************************************/


/// states of XML parser
enum parser_state_t {
    ps_bof,
    ps_lt,
    ps_attr,     // waiting for attributes or end of tag
    ps_text,
};


void LVXMLParser::SetCharset( const lChar16 * name )
{
    LVTextFileBase::SetCharset( name );
    m_callback->OnEncoding( name, m_conv_table );
}

void LVXMLParser::Reset()
{
    //CRLog::trace("LVXMLParser::Reset()");
    LVTextFileBase::Reset();
    m_state = ps_bof;
}

LVXMLParser::LVXMLParser( LVStreamRef stream, LVXMLParserCallback * callback, bool allowHtml, bool fb2Only )
    : LVTextFileBase(stream)
    , m_callback(callback)
    , m_trimspaces(true)
    , m_state(0)
    , m_citags(false)
    , m_allowHtml(allowHtml)
    , m_fb2Only(fb2Only)

{
    m_firstPageTextCounter = 2000;
}

LVXMLParser::~LVXMLParser()
{
}

inline bool IsSpaceChar( lChar16 ch )
{
    return (ch == ' ')
        || (ch == '\t')
        || (ch == '\r')
        || (ch == '\n');
}

/// returns true if format is recognized by parser
bool LVXMLParser::CheckFormat()
{
    //CRLog::trace("LVXMLParser::CheckFormat()");
    #define XML_PARSER_DETECT_SIZE 8192
    Reset();
    AutodetectEncoding();
    Reset();
    lChar16 * chbuf = new lChar16[XML_PARSER_DETECT_SIZE];
    FillBuffer( XML_PARSER_DETECT_SIZE );
    int charsDecoded = ReadTextBytes( 0, m_buf_len, chbuf, XML_PARSER_DETECT_SIZE-1, 0 );
    chbuf[charsDecoded] = 0;
    bool res = false;
    if ( charsDecoded > 30 ) {
        lString16 s( chbuf, charsDecoded );
        bool flg = !m_fb2Only || s.pos(L"<FictionBook") >= 0;
        if ( flg && (( (s.pos(L"<?xml") >=0 || s.pos(L" xmlns=")>0 )&& s.pos(L"version=") >= 6) ||
             m_allowHtml && s.pos(L"<html xmlns=\"http://www.w3.org/1999/xhtml\"")>=0 )) {
            //&& s.pos(L"<FictionBook") >= 0
            res = true;
            int encpos=s.pos(L"encoding=\"");
            if ( encpos>=0 ) {
                lString16 encname = s.substr( encpos+10, 20 );
                int endpos = s.pos(L"\"");
                if ( endpos>0 ) {
                    encname.erase( endpos, encname.length() - endpos );
                    SetCharset( encname.c_str() );
                }
            } else {
            }
        }
        //else if ( s.pos(L"<html xmlns=\"http://www.w3.org/1999/xhtml\"") >= 0 )
        //    res = true;
    }
    delete[] chbuf;
    Reset();
    //CRLog::trace("LVXMLParser::CheckFormat() finished");
    return res;
}

bool LVXMLParser::Parse()
{
    //
    //CRLog::trace("LVXMLParser::Parse()");
    Reset();
//    bool dumpActive = false;
//    int txt_count = 0;
    bool inXmlTag = false;
    m_callback->OnStart(this);
    bool closeFlag = false;
    bool qFlag = false;
    bool bodyStarted = false;
    lString16 tagname;
    lString16 tagns;
    lString16 attrname;
    lString16 attrns;
    lString16 attrvalue;
    bool errorFlag = false;
    int flags = m_callback->getFlags();
    for (;!m_eof && !errorFlag;)
    {
        if ( m_stopped )
             break;
        // load next portion of data if necessary
        lChar16 ch = PeekCharFromBuffer();
        switch (m_state)
        {
        case ps_bof:
            {
                // skip file beginning until '<'
                for ( ; !m_eof && ch!='<'; ch = PeekNextCharFromBuffer() )
                    ;
                if (!m_eof)
                {
                    // m_buf[m_buf_pos] == '<'
                    m_state = ps_lt;
                    ReadCharFromBuffer();
                }
            }
            break;
        case ps_lt:
            {
                if ( !SkipSpaces() )
                    break;
                closeFlag = false;
                qFlag = false;
                if (ch=='/')
                {
                    ch = ReadCharFromBuffer();
                    closeFlag = true;
                }
                else if (ch=='?')
                {
                    // <?xml?>
                    ch = ReadCharFromBuffer();
                    qFlag = true;
                }
                else if (ch=='!')
                {
                    // comments etc...
                    if ( PeekCharFromBuffer(1)=='-' && PeekCharFromBuffer(2)=='-' ) {
                        // skip comments
                        ch = PeekNextCharFromBuffer( 2 );
                        while ( !m_eof && (ch!='-' || PeekCharFromBuffer(1)!='-'
                                || PeekCharFromBuffer(2)!='>') ) {
                            ch = PeekNextCharFromBuffer();
                        }
                        if ( ch=='-' && PeekCharFromBuffer(1)=='-'
                                && PeekCharFromBuffer(2)=='>' )
                            ch = PeekNextCharFromBuffer(2);
                        m_state = ps_text;
                        break;
                    }
                }
                if ( !ReadIdent(tagns, tagname) || PeekCharFromBuffer()=='=')
                {
                    // error!
                    if (SkipTillChar('>'))
                    {
                        m_state = ps_text;
                        ch = ReadCharFromBuffer();
                    }
                    break;
                }
                if ( m_citags ) {
                    tagns.lowercase();
                    tagname.lowercase();
                }
                if (closeFlag)
                {
//                    if ( tagname==L"body" ) {
//                        dumpActive = true;
//                    } else if ( tagname==L"section" ) {
//                        dumpActive = false;
//                    }
                    m_callback->OnTagClose(tagns.c_str(), tagname.c_str());
//                    if ( dumpActive )
//                        CRLog::trace("</%s>", LCSTR(tagname) );
                    if (SkipTillChar('>'))
                    {
                        m_state = ps_text;
                        ch = ReadCharFromBuffer();
                    }
                    break;
                }

                if (qFlag) {
                    tagname.insert(0, 1, '?');
                    inXmlTag = (tagname==L"?xml");
                } else {
                    inXmlTag = false;
                }
                m_callback->OnTagOpen(tagns.c_str(), tagname.c_str());
//                if ( dumpActive )
//                    CRLog::trace("<%s>", LCSTR(tagname) );
                if ( !bodyStarted && tagname==L"body" )
                    bodyStarted = true;

                m_state = ps_attr;
            }
            break;
        case ps_attr:
            {
                if (!SkipSpaces())
                    break;
                ch = PeekCharFromBuffer();
                lChar16 nch = PeekCharFromBuffer(1);
                if ( ch=='>' || (nch=='>' && (ch=='/' || ch=='?')) )
                {
                    m_callback->OnTagBody();
                    // end of tag
                    if ( ch!='>' )
                        m_callback->OnTagClose(tagns.c_str(), tagname.c_str());
                    if ( ch=='>' )
                        ch = PeekNextCharFromBuffer();
                    else
                        ch = PeekNextCharFromBuffer(1);
                    m_state = ps_text;
                    break;
                }
                if ( !ReadIdent(attrns, attrname) )
                {
                    // error: skip rest of tag
                    SkipTillChar('<');
                    ch = PeekNextCharFromBuffer(1);
                    m_callback->OnTagBody();
                    m_state = ps_lt;
                    break;
                }
                SkipSpaces();
                attrvalue.reset(16);
                ch = PeekCharFromBuffer();
                if ( ch=='=' )
                {
                    // read attribute value
                    //PeekNextCharFromBuffer();
                    ReadCharFromBuffer(); // skip '='
                    SkipSpaces();
                    lChar16 qChar = 0;
                    ch = PeekCharFromBuffer();
                    if (ch=='\"' || ch=='\'')
                    {
                        qChar = ReadCharFromBuffer();
                    }
                    for ( ;!m_eof; )
                    {
                        ch = PeekCharFromBuffer();
                        if (ch=='>')
                            break;
                        if (!qChar && IsSpaceChar(ch))
                            break;
                        if (qChar && ch==qChar)
                        {
                            ch = PeekNextCharFromBuffer();
                            break;
                        }
                        ch = ReadCharFromBuffer();
                        if (ch)
                            attrvalue += ch;
                        else
                            break;
                    }
                }
                if ( m_citags ) {
                    attrns.lowercase();
                    attrname.lowercase();
                }
                if ( (flags & TXTFLG_CONVERT_8BIT_ENTITY_ENCODING) && m_conv_table ) {
                    PreProcessXmlString( attrvalue, 0, m_conv_table );
                }
                m_callback->OnAttribute( attrns.c_str(), attrname.c_str(), attrvalue.c_str());
                if (inXmlTag && attrname==L"encoding")
                {
                    SetCharset( attrvalue.c_str() );
                }
            }
            break;
        case ps_text:
            {
//                if ( dumpActive ) {
//                    lString16 s;
//                    s << PeekCharFromBuffer(0) << PeekCharFromBuffer(1) << PeekCharFromBuffer(2) << PeekCharFromBuffer(3)
//                      << PeekCharFromBuffer(4) << PeekCharFromBuffer(5) << PeekCharFromBuffer(6) << PeekCharFromBuffer(7);
//                    CRLog::trace("text: %s...", LCSTR(s) );
//                    dumpActive = true;
//                }
//                txt_count++;
//                if ( txt_count<121 ) {
//                    if ( txt_count>118 ) {
//                        CRLog::trace("Text[%d]:", txt_count);
//                    }
//                }
                ReadText();
                if ( bodyStarted )
                    updateProgress();
                m_state = ps_lt;
            }
            break;
        default:
            {
            }
        }
    }
    //CRLog::trace("LVXMLParser::Parse() is finished, m_stopped=%s", m_stopped?"true":"false");
    m_callback->OnStop();
    return !errorFlag;
}

//#define TEXT_SPLIT_SIZE 8192
#define TEXT_SPLIT_SIZE 8192

typedef struct  {
    const wchar_t * name;
    wchar_t code;
} ent_def_t;

static const ent_def_t def_entity_table[] = {
{L"nbsp", 160},
{L"iexcl", 161},
{L"cent", 162},
{L"pound", 163},
{L"curren", 164},
{L"yen", 165},
{L"brvbar", 166},
{L"sect", 167},
{L"uml", 168},
{L"copy", 169},
{L"ordf", 170},
{L"laquo", 171},
{L"not", 172},
{L"shy", 173},
{L"reg", 174},
{L"macr", 175},
{L"deg", 176},
{L"plusmn", 177},
{L"sup2", 178},
{L"sup3", 179},
{L"acute", 180},
{L"micro", 181},
{L"para", 182},
{L"middot", 183},
{L"cedil", 184},
{L"sup1", 185},
{L"ordm", 186},
{L"raquo", 187},
{L"frac14", 188},
{L"frac12", 189},
{L"frac34", 190},
{L"iquest", 191},
{L"Agrave", 192},
{L"Aacute", 193},
{L"Acirc", 194},
{L"Atilde", 195},
{L"Auml", 196},
{L"Aring", 197},
{L"AElig", 198},
{L"Ccedil", 199},
{L"Egrave", 200},
{L"Eacute", 201},
{L"Ecirc", 202},
{L"Euml", 203},
{L"Igrave", 204},
{L"Iacute", 205},
{L"Icirc", 206},
{L"Iuml", 207},
{L"ETH", 208},
{L"Ntilde", 209},
{L"Ograve", 210},
{L"Oacute", 211},
{L"Ocirc", 212},
{L"Otilde", 213},
{L"Ouml", 214},
{L"times", 215},
{L"Oslash", 216},
{L"Ugrave", 217},
{L"Uacute", 218},
{L"Ucirc", 219},
{L"Uuml", 220},
{L"Yacute", 221},
{L"THORN", 222},
{L"szlig", 223},
{L"agrave", 224},
{L"aacute", 225},
{L"acirc", 226},
{L"atilde", 227},
{L"auml", 228},
{L"aring", 229},
{L"aelig", 230},
{L"ccedil", 231},
{L"egrave", 232},
{L"eacute", 233},
{L"ecirc", 234},
{L"euml", 235},
{L"igrave", 236},
{L"iacute", 237},
{L"icirc", 238},
{L"iuml", 239},
{L"eth", 240},
{L"ntilde", 241},
{L"ograve", 242},
{L"oacute", 243},
{L"ocirc", 244},
{L"otilde", 245},
{L"ouml", 246},
{L"divide", 247},
{L"oslash", 248},
{L"ugrave", 249},
{L"uacute", 250},
{L"ucirc", 251},
{L"uuml", 252},
{L"yacute", 253},
{L"thorn", 254},
{L"yuml", 255},
{L"quot", 34},
{L"amp", 38},
{L"lt", 60},
{L"gt", 62},
{L"apos", '\''},
{L"OElig", 338},
{L"oelig", 339},
{L"Scaron", 352},
{L"scaron", 353},
{L"Yuml", 376},
{L"circ", 710},
{L"tilde", 732},
{L"ensp", 8194},
{L"emsp", 8195},
{L"thinsp", 8201},
{L"zwnj", 8204},
{L"zwj", 8205},
{L"lrm", 8206},
{L"rlm", 8207},
{L"ndash", 8211},
{L"mdash", 8212},
{L"lsquo", 8216},
{L"rsquo", 8217},
{L"sbquo", 8218},
{L"ldquo", 8220},
{L"rdquo", 8221},
{L"bdquo", 8222},
{L"dagger", 8224},
{L"Dagger", 8225},
{L"permil", 8240},
{L"lsaquo", 8249},
{L"rsaquo", 8250},
{L"euro", 8364},
{L"fnof", 402},
{L"Alpha", 913},
{L"Beta", 914},
{L"Gamma", 915},
{L"Delta", 916},
{L"Epsilon", 917},
{L"Zeta", 918},
{L"Eta", 919},
{L"Theta", 920},
{L"Iota", 921},
{L"Kappa", 922},
{L"Lambda", 923},
{L"Mu", 924},
{L"Nu", 925},
{L"Xi", 926},
{L"Omicron", 927},
{L"Pi", 928},
{L"Rho", 929},
{L"Sigma", 931},
{L"Tau", 932},
{L"Upsilon", 933},
{L"Phi", 934},
{L"Chi", 935},
{L"Psi", 936},
{L"Omega", 937},
{L"alpha", 945},
{L"beta", 946},
{L"gamma", 947},
{L"delta", 948},
{L"epsilon", 949},
{L"zeta", 950},
{L"eta", 951},
{L"theta", 952},
{L"iota", 953},
{L"kappa", 954},
{L"lambda", 955},
{L"mu", 956},
{L"nu", 957},
{L"xi", 958},
{L"omicron", 959},
{L"pi", 960},
{L"rho", 961},
{L"sigmaf", 962},
{L"sigma", 963},
{L"tau", 964},
{L"upsilon", 965},
{L"phi", 966},
{L"chi", 967},
{L"psi", 968},
{L"omega", 969},
{L"thetasym", 977},
{L"upsih", 978},
{L"piv", 982},
{L"bull", 8226},
{L"hellip", 8230},
{L"prime", 8242},
{L"Prime", 8243},
{L"oline", 8254},
{L"frasl", 8260},
{L"weierp", 8472},
{L"image", 8465},
{L"real", 8476},
{L"trade", 8482},
{L"alefsym", 8501},
{L"larr", 8592},
{L"uarr", 8593},
{L"rarr", 8594},
{L"darr", 8595},
{L"harr", 8596},
{L"crarr", 8629},
{L"lArr", 8656},
{L"uArr", 8657},
{L"rArr", 8658},
{L"dArr", 8659},
{L"hArr", 8660},
{L"forall", 8704},
{L"part", 8706},
{L"exist", 8707},
{L"empty", 8709},
{L"nabla", 8711},
{L"isin", 8712},
{L"notin", 8713},
{L"ni", 8715},
{L"prod", 8719},
{L"sum", 8721},
{L"minus", 8722},
{L"lowast", 8727},
{L"radic", 8730},
{L"prop", 8733},
{L"infin", 8734},
{L"ang", 8736},
{L"and", 8743},
{L"or", 8744},
{L"cap", 8745},
{L"cup", 8746},
{L"int", 8747},
{L"there4", 8756},
{L"sim", 8764},
{L"cong", 8773},
{L"asymp", 8776},
{L"ne", 8800},
{L"equiv", 8801},
{L"le", 8804},
{L"ge", 8805},
{L"sub", 8834},
{L"sup", 8835},
{L"nsub", 8836},
{L"sube", 8838},
{L"supe", 8839},
{L"oplus", 8853},
{L"otimes", 8855},
{L"perp", 8869},
{L"sdot", 8901},
{L"lceil", 8968},
{L"rceil", 8969},
{L"lfloor", 8970},
{L"rfloor", 8971},
{L"lang", 9001},
{L"rang", 9002},
{L"loz", 9674},
{L"spades", 9824},
{L"clubs", 9827},
{L"hearts", 9829},
{L"diams", 9830},
{NULL, 0},
};

// returns new length
void PreProcessXmlString( lString16 & s, lUInt32 flags, const lChar16 * enc_table )
{
    lChar16 * str = s.modify();
    int len = s.length();
    int state = 0;
    lChar16 nch = 0;
    lChar16 lch = 0;
    lChar16 nsp = 0;
    bool pre = (flags & TXTFLG_PRE);
    bool pre_para_splitting = (flags & TXTFLG_PRE_PARA_SPLITTING)!=0;
    if ( pre_para_splitting )
        pre = false;
    //CRLog::trace("before: '%s' %s", LCSTR(s), pre ? "pre ":" ");
    int tabCount = 0;
    int j = 0;
    for (int i=0; i<len; ++i )
    {
        lChar16 ch = str[i];
        if ( pre && ch=='\t' )
            tabCount++;
        if ( !pre && (ch=='\r' || ch=='\n' || ch=='\t') )
            ch = ' ';
        if (ch=='\r')
        {
            if ((i==0 || lch!='\n') && (i==len-1 || str[i+1]!='\n'))
                str[j++] = '\n';
        }
        else if (ch=='\n')
        {
            str[j++] = '\n';
        }
        else if (ch=='&')
        {
            state = 1;
            nch = 0;
        }
        else if (state==0)
        {
            if (ch==' ')
            {
                if ( pre || !nsp )
                    str[j++] = ch;
                nsp++;
            }
            else
            {
                str[j++] = ch;
                nsp = 0;
            }
        }
        else
        {
            if (state == 2 && ch=='x')
                state = 22;
            else if (state == 22 && hexDigit(ch)>=0)
                nch = (nch << 4) | hexDigit(ch);
            else if (state == 2 && ch>='0' && ch<='9')
                nch = nch * 10 + (ch - '0');
            else if (ch=='#' && state==1)
                state = 2;
            else if (state==1 && ((ch>='a' && ch<='z') || (ch>='A' && ch<='Z')) ) {
                int k;
                lChar16 entname[16];
                for ( k=i; str[k] && str[k]!=';'  && str[k]!=' ' && k-i<16; k++ )
                    entname[k-i] = str[k];
                entname[k-i] = 0;
                int n;
                lChar16 code = 0;
                // TODO: optimize search
                if ( str[k]==';' || str[k]==' ' ) {
                    for ( n=0; def_entity_table[n].name; n++ ) {
                        if ( !lStr_cmp( def_entity_table[n].name, entname ) ) {
                            code = def_entity_table[n].code;
                            break;
                        }
                    }
                }
                if ( code ) {
                    i=k;
                    state = 0;
                    if ( enc_table && code<256 && code>=128 )
                        code = enc_table[code - 128];
                    str[j++] = code;
                    nsp = 0;
                } else {
                    // include & and rest of entity into output string
                    str[j++] = '&';
                    str[j++] = str[i];
                    state = 0;
                }

            } else if (ch == ';')
            {
                if (nch)
                    str[j++] = nch;
                state = 0;
                nsp = 0;
            }
            else
            {
                // error: return to normal mode
                state = 0;
            }
        }
        lch = ch;
    }


    // remove extra characters from end of line
    s.limit( j );

    if ( tabCount > 0 ) {
        // expand tabs
        lString16 buf;

        buf.reserve( j + tabCount * 8 );
        int x = 0;
        for ( int i=0; i<j; i++ ) {
            lChar16 ch = str[i];
            if ( ch=='\r' || ch=='\n' )
                x = 0;
            if ( ch=='\t' ) {
                int delta = 8 - (x & 7);
                x += delta;
                while ( delta-- )
                    buf << L' ';
            } else {
                buf << ch;
                x++;
            }
        }
        s = buf;
    }
    //CRLog::trace(" after: '%s'", LCSTR(s));
}

void LVTextFileBase::clearCharBuffer()
{
    m_read_buffer_len = m_read_buffer_pos = 0;
}

int LVTextFileBase::fillCharBuffer()
{
    int available = m_read_buffer_len - m_read_buffer_pos;
    if ( available > (XML_CHAR_BUFFER_SIZE>>3) )
        return available; // don't update if more than 1/8 of buffer filled
    if ( m_buf_len - m_buf_pos < MIN_BUF_DATA_SIZE )
        FillBuffer( MIN_BUF_DATA_SIZE*2 );
    if ( m_read_buffer_len > (XML_CHAR_BUFFER_SIZE - (XML_CHAR_BUFFER_SIZE>>3)) ) {
        memcpy( m_read_buffer, m_read_buffer+m_read_buffer_pos, available * sizeof(lChar16) );
        m_read_buffer_pos = 0;
        m_read_buffer_len = available;
    }
    int charsRead = ReadChars( m_read_buffer + m_read_buffer_len, XML_CHAR_BUFFER_SIZE - m_read_buffer_len );
    m_read_buffer_len += charsRead;
//#ifdef _DEBUG
//    CRLog::trace("buf: %s\n", UnicodeToUtf8(lString16(m_read_buffer, m_read_buffer_len)).c_str() );
//#endif
    //CRLog::trace("Buf:'%s'", LCSTR(lString16(m_read_buffer, m_read_buffer_len)) );
    return m_read_buffer_len - m_read_buffer_pos;
}

bool LVXMLParser::ReadText()
{
    // TODO: remove tracking of file pos
    //int text_start_pos = 0;
    //int ch_start_pos = 0;
    //int last_split_fpos = 0;
    int last_split_txtlen = 0;
    int tlen = 0;
    //text_start_pos = (int)(m_buf_fpos + m_buf_pos);
    m_txt_buf.reset(TEXT_SPLIT_SIZE+1);
    lUInt32 flags = m_callback->getFlags();
    bool pre_para_splitting = ( flags & TXTFLG_PRE_PARA_SPLITTING )!=0;
    bool last_eol = false;

    bool flgBreak = false;
    bool splitParas = false;
    while ( !flgBreak ) {
        int i=0;
        if ( m_read_buffer_pos + 1 >= m_read_buffer_len ) {
            if ( !fillCharBuffer() ) {
                m_eof = true;
                return false;
            }
        }
        for ( ; m_read_buffer_pos+i<m_read_buffer_len; i++ ) {
            lChar16 ch = m_read_buffer[m_read_buffer_pos + i];
            lChar16 nextch = m_read_buffer_pos + i + 1 < m_read_buffer_len ? m_read_buffer[m_read_buffer_pos + i + 1] : 0;
            flgBreak = ch=='<' || m_eof;
            if ( flgBreak && !tlen ) {
                m_read_buffer_pos++;
                return false;
            }
            splitParas = false;
            if (last_eol && pre_para_splitting && (ch==' ' || ch=='\t' || ch==160) && tlen>0 ) //!!!
                splitParas = true;
            if (!flgBreak && !splitParas)
            {
                tlen++;
            }
            if ( tlen > TEXT_SPLIT_SIZE || flgBreak || splitParas)
            {
                if ( last_split_txtlen==0 || flgBreak || splitParas )
                    last_split_txtlen = tlen;
                break;
            }
            else if (ch==' ' || (ch=='\r' && nextch!='\n')
                || (ch=='\n' && nextch!='\r') )
            {
                //last_split_fpos = (int)(m_buf_fpos + m_buf_pos);
                last_split_txtlen = tlen;
            }
            last_eol = (ch=='\r' || ch=='\n');
        }
        if ( i>0 ) {
            m_txt_buf.append( m_read_buffer + m_read_buffer_pos, i );
            m_read_buffer_pos += i;
        }
        if ( tlen > TEXT_SPLIT_SIZE || flgBreak || splitParas)
        {
            //=====================================================
            lString16 nextText = m_txt_buf.substr( last_split_txtlen );
            m_txt_buf.limit( last_split_txtlen );
            const lChar16 * enc_table = NULL;
            if ( flags & TXTFLG_CONVERT_8BIT_ENTITY_ENCODING )
                enc_table = this->m_conv_table;
            PreProcessXmlString( m_txt_buf, flags, enc_table );
            if ( (flags & TXTFLG_TRIM) && (!(flags & TXTFLG_PRE) || (flags & TXTFLG_PRE_PARA_SPLITTING)) ) {
                m_txt_buf.trimDoubleSpaces(
                    ((flags & TXTFLG_TRIM_ALLOW_START_SPACE) || pre_para_splitting)?true:false,
                    (flags & TXTFLG_TRIM_ALLOW_END_SPACE)?true:false,
                    (flags & TXTFLG_TRIM_REMOVE_EOL_HYPHENS)?true:false );
            }
            m_callback->OnText(m_txt_buf.c_str(), m_txt_buf.length(), flags );
            //=====================================================
            if (flgBreak)
            {
                // TODO:LVE???
                if ( PeekCharFromBuffer()=='<' )
                    m_read_buffer_pos++;
                //if ( m_read_buffer_pos < m_read_buffer_len )
                //    m_read_buffer_pos++;
                break;
            }
            m_txt_buf = nextText;
            tlen = m_txt_buf.length();
            //text_start_pos = last_split_fpos; //m_buf_fpos + m_buf_pos;
            //last_split_fpos = 0;
            last_split_txtlen = 0;
        }
    }


    //if (!Eof())
    //    m_buf_pos++;
    return (!m_eof);
}

bool LVXMLParser::SkipSpaces()
{
    for ( lUInt16 ch = PeekCharFromBuffer(); !m_eof; ch = PeekNextCharFromBuffer() ) {
        if ( !IsSpaceChar(ch) )
            break; // char found!
    }
    return (!m_eof);
}

bool LVXMLParser::SkipTillChar( lChar16 charToFind )
{
    for ( lUInt16 ch = PeekCharFromBuffer(); !m_eof; ch = PeekNextCharFromBuffer() ) {
        if ( ch == charToFind )
            return true; // char found!
    }
    return false; // EOF
}

inline bool isValidIdentChar( lChar16 ch )
{
    return ( (ch>='a' && ch<='z')
          || (ch>='A' && ch<='Z')
          || (ch>='0' && ch<='9')
          || (ch=='-')
          || (ch=='_')
          || (ch=='.')
          || (ch==':') );
}

inline bool isValidFirstIdentChar( lChar16 ch )
{
    return ( (ch>='a' && ch<='z')
          || (ch>='A' && ch<='Z')
           );
}

// read identifier from stream
bool LVXMLParser::ReadIdent( lString16 & ns, lString16 & name )
{
    // clear string buffer
    ns.reset(16);
    name.reset(16);
    // check first char
    lChar16 ch0 = PeekCharFromBuffer();
    if ( !isValidFirstIdentChar(ch0) )
        return false;

    name += ReadCharFromBuffer();

    for ( lUInt16 ch = PeekCharFromBuffer(); !m_eof; ch = PeekNextCharFromBuffer() ) {
        if ( !isValidIdentChar(ch) )
            break;
        if (ch == ':')
        {
            if ( ns.empty() )
                name.swap( ns ); // add namespace
            else
                break; // error
        }
        else
        {
            name += ch;
        }
    }
    lChar16 ch = PeekCharFromBuffer();
    return (!name.empty()) && (ch==' ' || ch=='/' || ch=='>' || ch=='?' || ch=='=' || ch==0);
}

void LVXMLParser::SetSpaceMode( bool flgTrimSpaces )
{
    m_trimspaces = flgTrimSpaces;
}

lString16 htmlCharset( lString16 htmlHeader )
{
    // META HTTP-EQUIV
    htmlHeader.lowercase();
    lString16 meta(L"meta http-equiv=\"content-type\"");
    int p = htmlHeader.pos( meta );
    if ( p<0 )
        return lString16();
    htmlHeader = htmlHeader.substr( p + meta.length() );
    p = htmlHeader.pos(L">");
    if ( p<0 )
        return lString16();
    htmlHeader = htmlHeader.substr( 0, p );
    CRLog::trace("http-equiv content-type: %s", UnicodeToUtf8(htmlHeader).c_str() );
    p = htmlHeader.pos(L"charset=");
    if ( p<0 )
        return lString16();
    htmlHeader = htmlHeader.substr( p + 8 ); // skip "charset="
    lString16 enc;
    for ( int i=0; i<(int)htmlHeader.length(); i++ ) {
        lChar16 ch = htmlHeader[i];
        if ( (ch>='a' && ch<='z') || (ch>='0' && ch<='9') || (ch=='-') || (ch=='_') )
            enc += ch;
        else
            break;
    }
    if ( enc==L"utf-16" )
        return lString16();
    return enc;
}

/// HTML parser
/// returns true if format is recognized by parser
bool LVHTMLParser::CheckFormat()
{
    Reset();
    // encoding test
    if ( !AutodetectEncoding(!this->m_encoding_name.empty()) )
        return false;
    lChar16 * chbuf = new lChar16[XML_PARSER_DETECT_SIZE];
    FillBuffer( XML_PARSER_DETECT_SIZE );
    int charsDecoded = ReadTextBytes( 0, m_buf_len, chbuf, XML_PARSER_DETECT_SIZE-1, 0 );
    chbuf[charsDecoded] = 0;
    bool res = false;
    if ( charsDecoded > 30 ) {
        lString16 s( chbuf, charsDecoded );
        s.lowercase();
        if ( s.pos(L"<html") >=0 && ( s.pos(L"<head") >= 0 || s.pos(L"<body") >=0 ) ) //&& s.pos(L"<FictionBook") >= 0
            res = true;
        lString16 name=m_stream->GetName();
        name.lowercase();
        bool html_ext = name.endsWith(lString16(".htm")) || name.endsWith(lString16(".html"))
                        || name.endsWith(lString16(".hhc"))
                        || name.endsWith(lString16(".xhtml"));
        if ( html_ext && (s.pos(L"<!--")>=0 || s.pos(L"UL")>=0
                           || s.pos(L"<p>")>=0 || s.pos(L"ul")>=0) )
            res = true;
        lString16 enc = htmlCharset( s );
        if ( !enc.empty() )
            SetCharset( enc.c_str() );
        //else if ( s.pos(L"<html xmlns=\"http://www.w3.org/1999/xhtml\"") >= 0 )
        //    res = true;
    }
    delete[] chbuf;
    Reset();
    //CRLog::trace("LVXMLParser::CheckFormat() finished");
    return res;
}

/// constructor
LVHTMLParser::LVHTMLParser( LVStreamRef stream, LVXMLParserCallback * callback )
: LVXMLParser( stream, callback )
{
    m_citags = true;
}

/// destructor
LVHTMLParser::~LVHTMLParser()
{
}

/// parses input stream
bool LVHTMLParser::Parse()
{
    bool res = LVXMLParser::Parse();
    return res;
}


/// read file contents to string
lString16 LVReadTextFile( lString16 filename )
{
	LVStreamRef stream = LVOpenFileStream( filename.c_str(), LVOM_READ );
	return LVReadTextFile( stream );
}

lString16 LVReadTextFile( LVStreamRef stream )
{
	if ( stream.isNull() )
		return lString16();
    lString16 buf;
    LVTextParser reader( stream, NULL, true );
    if ( !reader.AutodetectEncoding() )
        return buf;
    lUInt32 flags;
    while ( !reader.Eof() ) {
        lString16 line = reader.ReadLine( 4096, flags );
        if ( !buf.empty() )
            buf << L'\n';
        if ( !line.empty() ) {
            buf << line;
        }
    }
    return buf;
}


static const char * AC_P[]  = {"p", "p", "hr", NULL};
static const char * AC_COL[] = {"col", NULL};
static const char * AC_LI[] = {"li", "li", "p", NULL};
static const char * AC_UL[] = {"ul", "li", "p", NULL};
static const char * AC_OL[] = {"ol", "li", "p", NULL};
static const char * AC_DD[] = {"dd", "dd", "p", NULL};
static const char * AC_DL[] = {"dl", "dt", "p", NULL};
static const char * AC_DT[] = {"dt", "dt", "dd", "p", NULL};
static const char * AC_BR[] = {"br", NULL};
static const char * AC_HR[] = {"hr", NULL};
static const char * AC_PARAM[] = {"param", "param", NULL};
static const char * AC_IMG[]= {"img", NULL};
static const char * AC_TD[] = {"td", "td", "th", NULL};
static const char * AC_TH[] = {"th", "th", "td", NULL};
static const char * AC_TR[] = {"tr", "tr", "thead", "tfoot", "tbody", NULL};
static const char * AC_DIV[] = {"div", "p", NULL};
static const char * AC_TABLE[] = {"table", "p", NULL};
static const char * AC_THEAD[] = {"thead", "tr", "thead", "tfoot", "tbody", NULL};
static const char * AC_TFOOT[] = {"tfoot", "tr", "thead", "tfoot", "tbody", NULL};
static const char * AC_TBODY[] = {"tbody", "tr", "thead", "tfoot", "tbody", NULL};
static const char * AC_OPTION[] = {"option", "option", NULL};
static const char * AC_PRE[] = {"pre", "pre", NULL};
static const char * AC_INPUT[] = {"input", NULL};
const char * *
HTML_AUTOCLOSE_TABLE[] = {
    AC_INPUT,
    AC_OPTION,
    AC_PRE,
    AC_P,
    AC_LI,
    AC_UL,
    AC_OL,
    AC_TD,
    AC_TH,
    AC_DD,
    AC_DL,
    AC_DT,
    AC_TR,
    AC_COL,
    AC_BR,
    AC_HR,
    AC_PARAM,
    AC_IMG,
    AC_DIV,
    AC_THEAD,
    AC_TFOOT,
    AC_TBODY,
    AC_TABLE,
    NULL
};


