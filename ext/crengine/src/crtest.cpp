#include "../include/crtest.h"
#include "../include/lvtinydom.h"
#include "../include/chmfmt.h"

#ifdef _DEBUG

class LVCompareTestStream : public LVNamedStream
{
protected:
    LVStreamRef m_base_stream1;
    LVStreamRef m_base_stream2;
public:
    virtual const lChar16 * GetName()
            {
                static lChar16 buf[1024];
                lString16 res1 = m_base_stream1->GetName();
                lString16 res2 = m_base_stream2->GetName();
                MYASSERT( res1==res2, "getName, res");
                lStr_cpy( buf, res1.c_str());
                return buf;
            }
    virtual lvopen_mode_t GetMode()
            {
                lvopen_mode_t res1 = m_base_stream1->GetMode();
                lvopen_mode_t res2 = m_base_stream2->GetMode();
                MYASSERT( res1==res2, "getMode, res");
                return res1;
            }
    virtual lverror_t Seek( lvoffset_t offset, lvseek_origin_t origin, lvpos_t * pNewPos )
            {
                lvsize_t bw1 = 0;
                lvsize_t bw2 = 0;
                lverror_t res1 = m_base_stream1->Seek(offset, origin, &bw1);
                lverror_t res2 = m_base_stream2->Seek(offset, origin, &bw2);
                MYASSERT( res1==res2, "seek, res");
                if ( bw1!=bw2 ) {
                    CRLog::error("Result position after Seek(%d, %x) doesn't match: %x / %x   getpos1=%x, getpos2=%x",
                                 (int)origin, (int)offset,
                                 (int)bw1, (int)bw2,
                                 (int)m_base_stream1->GetPos(), (int)m_base_stream2->GetPos() );
                    res1 = m_base_stream1->Seek(offset, origin, &bw1);
                    res2 = m_base_stream2->Seek(offset, origin, &bw2);
                }
                MYASSERT( bw1==bw2, "seek, bw");
                MYASSERT( m_base_stream1->GetPos()==m_base_stream2->GetPos(), "seek, pos");
                MYASSERT( m_base_stream1->Eof()==m_base_stream2->Eof(), "seek, eof");
                if ( pNewPos )
                    *pNewPos = bw1;
                return res1;
            }
    virtual lverror_t Tell( lvpos_t * pPos )
            {
                lvsize_t bw1 = 0;
                lvsize_t bw2 = 0;
                lverror_t res1 = m_base_stream1->Tell(&bw1);
                lverror_t res2 = m_base_stream2->Tell(&bw2);
                MYASSERT( res1==res2, "tell, res");
                MYASSERT( bw1==bw2, "tell, bw");
                *pPos = bw1;
                return res1;
            }
    //virtual lverror_t   SetPos(lvpos_t p)
    virtual lvpos_t   SetPos(lvpos_t p)
            {
                lvpos_t res1 = m_base_stream1->SetPos(p);
                lvpos_t res2 = m_base_stream2->SetPos(p);
                MYASSERT( res1==res2, "setPos, res");
                MYASSERT( m_base_stream1->GetPos()==m_base_stream2->GetPos(), "setpos, pos");
                bool eof1 = m_base_stream1->Eof();
                bool eof2 = m_base_stream2->Eof();
                if ( eof1!=eof2 ) {
                    CRLog::error("EOF after SetPos(%x) don't match: %x / %x   getpos1=%x, getpos2=%x, size1=%x, size2=%x",
                                 (int)p,
                                 (int)eof1, (int)eof2,
                                 (int)m_base_stream1->GetPos(), (int)m_base_stream2->GetPos(),
                                 (int)m_base_stream1->GetSize(), (int)m_base_stream2->GetSize()
                                 );
                }
                MYASSERT( eof1==eof2, "setpos, eof");
                return res1;
            }
    virtual lvpos_t   GetPos()
            {
                lvpos_t res1 = m_base_stream1->GetPos();
                lvpos_t res2 = m_base_stream2->GetPos();
                MYASSERT( res1==res2, "getPos, res");
                return res1;
            }
    virtual lverror_t SetSize( lvsize_t size )
            {
                lverror_t res1 = m_base_stream1->SetSize(size);
                lverror_t res2 = m_base_stream2->SetSize(size);
                MYASSERT( res1==res2, "setSize, res");
                return res1;
            }
    virtual lverror_t Read( void * buf, lvsize_t count, lvsize_t * nBytesRead )
            {
                lvsize_t bw1 = 0;
                lvsize_t bw2 = 0;
                lUInt8 * buf1 = (lUInt8 *)buf;
                lUInt8 * buf2 = new lUInt8[count];
                memcpy( buf2, buf, count );
                lverror_t res1 = m_base_stream1->Read(buf, count, &bw1);
                lverror_t res2 = m_base_stream2->Read(buf2, count, &bw2);
                MYASSERT( res1==res2, "read, res");
                if ( bw1!=bw2 ) {
                    CRLog::error("BytesRead after Read(%d) don't match: %x / %x   getpos1=%x, getpos2=%x   getsize1=%x, getsize2=%x",
                                 (int)count,
                                 (int)bw1, (int)bw2,
                                 (int)m_base_stream1->GetPos(), (int)m_base_stream2->GetPos(),
                                 (int)m_base_stream1->GetSize(), (int)m_base_stream2->GetSize()
                                 );
                }
                MYASSERT( bw1==bw2, "read, bw");
                MYASSERT( m_base_stream1->GetPos()==m_base_stream2->GetPos(), "read, pos");
                bool eof1 = m_base_stream1->Eof();
                bool eof2 = m_base_stream2->Eof();
                if ( eof1!=eof2 ) {
                    CRLog::error("EOF after Read(%d) don't match: %x / %x   getpos1=%x, getpos2=%x, size1=%x, size2=%x",
                                 (int)count,
                                 (int)eof1, (int)eof2,
                                 (int)m_base_stream1->GetPos(), (int)m_base_stream2->GetPos(),
                                 (int)m_base_stream1->GetSize(), (int)m_base_stream2->GetSize()
                                 );
                }
                MYASSERT( eof1==eof2, "read, eof");
                for ( unsigned i=0; i<count; i++ ) {
                    if ( buf1[i]!=buf2[i] ) {
                        CRLog::error("read, different data by offset %x", i);
                        MYASSERT( 0, "read, equal data");
                    }
                }
                if ( nBytesRead )
                    *nBytesRead = bw1;
                delete[] buf2;
                return res1;
            }
    virtual lverror_t Write( const void * buf, lvsize_t count, lvsize_t * nBytesWritten )
            {
                lvsize_t bw1 = 0;
                lvsize_t bw2 = 0;
                lverror_t res1 = m_base_stream1->Write(buf, count, &bw1);
                lverror_t res2 = m_base_stream2->Write(buf, count, &bw2);
                MYASSERT( res1==res2, "write, res");
                MYASSERT( bw1==bw2, "write, bw");
                MYASSERT( m_base_stream1->GetPos()==m_base_stream2->GetPos(), "write, pos");
                MYASSERT( m_base_stream1->Eof()==m_base_stream2->Eof(), "write, eof");
                if ( nBytesWritten )
                    *nBytesWritten = bw1;
                return res1;
            }
    virtual bool Eof()
            {
                bool res1 = m_base_stream1->Eof();
                bool res2 = m_base_stream2->Eof();
                if ( res1!=res2 ) {
                    CRLog::trace("EOF doesn't match");
                }
                MYASSERT( res1==res2, "EOF" );
                return res1;
            }
    LVCompareTestStream( LVStreamRef stream1, LVStreamRef stream2 ) : m_base_stream1(stream1), m_base_stream2(stream2) { }
    ~LVCompareTestStream() { }
};

LVStreamRef LVCreateCompareTestStream( LVStreamRef stream1, LVStreamRef stream2 )
{
    return LVStreamRef( new LVCompareTestStream(stream1, stream2) );
}
#endif

// external tests declarations
void testTxtSelector();


void runCRUnitTests()
{
#ifdef _DEBUG
    //runCHMUnitTest();
    runTinyDomUnitTests();
    testTxtSelector();
#endif
}
