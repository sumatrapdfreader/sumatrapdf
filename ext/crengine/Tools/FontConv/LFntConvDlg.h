// LFntConvDlg.h : header file
//

#if !defined(AFX_LFNTCONVDLG_H__A7B8E4AF_368D_4A87_A530_A3A94B59EF7B__INCLUDED_)
#define AFX_LFNTCONVDLG_H__A7B8E4AF_368D_4A87_A530_A3A94B59EF7B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CLFntConvDlg dialog

#include "lvfntgen.h"
#include "lvfonttest.h"
#include "crengine.h"

#define LCHARSET_LATIN       1
#define LCHARSET_GREEK       2
#define LCHARSET_CYRILLIC    4
#define LCHARSET_ARABIC      8
#define LCHARSET_HEBREW      16
#define LCHARSET_CHINEESE    32


typedef struct {
    lUInt8 value;
    lUInt8 count;
    lUInt8 nbits;
} hrle_table_t;

typedef struct {
    //====================
    int stats;
    //====================
    lUInt32 code;
    lUInt8  codelen;
    //====================
    lUInt8  value;
    lUInt8  count;
    //====================
    lUInt16 left;
    lUInt16 right;
    lUInt16 parent;
} hrle_stats_t;

typedef struct {
    lUInt32 code;
    lUInt8  codelen;
} hrle_encode_table_t;

// RLE/Huffmann encoder
class CRLEHuffTree
{
private:
    int m_max_run_len;
    int m_num_symbols;
    int m_max_code_bits;
    int m_decode_table_size;
    int m_decode_table_bits;
    int * m_stats; //[RHT_MAX_SYMBOL*RHT_MAX_RUN_LENGTH];
    hrle_encode_table_t * m_encode_table;
    hrle_decode_table_t * m_decode_table;
public:
    CRLEHuffTree( int numSymbols, int maxRunLen, int maxCodeBits )
        : m_stats(NULL), m_encode_table(NULL), m_decode_table(NULL)
    {
        Init(numSymbols, maxRunLen, maxCodeBits);
    }
    ~CRLEHuffTree()
    {
        delete m_stats;
        delete m_encode_table;
        if (m_decode_table)
            delete m_decode_table;
    }
    int getDecodeTableSize()
    {
        return sizeof(hrle_decode_info_t)+sizeof(hrle_decode_table_t)*(m_decode_table_size-1);
    }
    void getDecodeTable(hrle_decode_info_t * table)
    {
        table->bitcount = m_decode_table_bits;
        table->itemcount = m_decode_table_size;
        table->rightmask = (1<<m_decode_table_bits)-1;
        table->leftmask = ((1<<m_decode_table_bits)-1)<<(8-m_decode_table_bits);
        memcpy( table->table, m_decode_table, sizeof(hrle_decode_table_t)*m_decode_table_size );
    }
    void Init( int numSymbols, int maxRunLen, int maxCodeBits )
    {
        if (m_stats)
            delete m_stats;
        if (m_encode_table)
            delete m_encode_table;
        if (m_decode_table)
            delete m_decode_table;
        m_decode_table = NULL;
        m_num_symbols = numSymbols;
        m_max_run_len = maxRunLen;
        m_max_code_bits = maxCodeBits;
        m_stats = new int[numSymbols*maxRunLen];
        m_encode_table = new hrle_encode_table_t[numSymbols*maxRunLen];
        Clear();
    }
    void Clear()
    {
        memset( m_stats, 0, sizeof(int)*m_num_symbols*m_max_run_len );
        memset( m_encode_table, 0, sizeof(hrle_encode_table_t)*m_num_symbols*m_max_run_len );
    }
    void AddStat( int symbol, int count )
    {
        count--;
        if (count>=m_max_run_len)
        {
            //
            int n = (count + m_max_run_len - 1) / m_max_run_len;
            int dn = count % n;
            for (;n>0; n--)
            {
                m_stats[ symbol*m_max_run_len + (count/n + (dn>0)?1:0) ]++;
                if (dn)
                    dn--;
            }
        }
        else
        {
            m_stats[ symbol*m_max_run_len + count ]++;
        }
    }

    static int __cdecl compare_items( const void * p1, const void * p2 )
    {
        if ( ((hrle_stats_t*)p1)->stats < ((hrle_stats_t*)p2)->stats)
            return 1;
        if ( ((hrle_stats_t*)p1)->stats > ((hrle_stats_t*)p2)->stats)
            return -1;
        return 0;
    }

    // returns max num bits
    static int RecurseBits( hrle_stats_t * table, int index, int level, lUInt32 code )
    {
        int nb = 0;
        table[index].codelen = level;
        table[index].code = code;
        if (level>16)
        {
            nb = nb | 0;
        }
        if (table[index].count==0)
        {
            //if (table[index].left)
            {
                int n = RecurseBits( table, table[index].left, level+1, code<<1 );
                if (n>nb)
                    nb = n;
            }
            //if (table[index].right)
            {
                int n = RecurseBits( table, table[index].right, level+1, (code<<1) | 1 );
                if (n>nb)
                    nb = n;
            }
        }
        return nb;
    }

    void Encode( lUInt8 * & dst, int & bitpos, int value, int count )
    {
        int vindex = value*m_max_run_len;
        if ( count<m_max_run_len && m_encode_table[ vindex + count-1].codelen )
        {
            // put to destination
            lUInt32 code = m_encode_table[vindex + count-1].code;
            int codelen = m_encode_table[vindex + count-1].codelen;
            for (int i=codelen-1; i>=0; i--)
            {
                *dst |= (((code>>i) & 1) << (7-bitpos));
                if (++bitpos>7)
                {
                    dst++;
                    bitpos = 0;
                }
            }
        }
        else
        {
            // split into shorter parts
            int n = m_max_run_len;
            if (n>count)
                n = count;
            for ( ;count>0 && n>0; n-- )
            {
                if (m_encode_table[ vindex + n-1].codelen)
                {
                    for ( ;count>0 && n<=count; count-=n )
                    {
                        Encode( dst, bitpos, value, n );
                    }
                }
            }
        }
    }

    int MakeConfig( hrle_stats_t * srctable, hrle_stats_t * dsttable, int num_items )
    {
        memcpy(dsttable, srctable, sizeof(hrle_stats_t)*num_items);
        // move items of length=1 to main part
        for (int i=m_num_symbols*m_max_run_len-1; i>=num_items; i--)
        {
            if (srctable[i].count==1)
            {
                for (int j=num_items-1; j>=0; j--)
                {
                    if (dsttable[j].count>1)
                    {
                        dsttable[j] = srctable[i];
                        break;
                    }
                }
            }
        }
        // split long runs into shorter intervals, correct stats
        lUInt16 * run_index = new lUInt16[m_num_symbols*m_max_run_len];
        memset(run_index, -1, sizeof(lUInt16)*m_num_symbols*m_max_run_len);
        for (i=0; i<num_items; i++)
        {
            if (dsttable[i].count && dsttable[i].stats>0)
                run_index[dsttable[i].value*m_max_run_len + dsttable[i].count-1] = (lUInt16)i;
        }
        for (i=m_num_symbols*m_max_run_len-1; i>=num_items; i--)
        {
            if (srctable[i].count>1 && srctable[i].stats>0)
            {
                int len = srctable[i].count;
                lUInt8 val = srctable[i].value;
                while (len>0)
                {
                    for (int j=(len>=m_max_run_len)?m_max_run_len-1:len; j>=0; --j)
                    {
                        lUInt16 inx = run_index[val*m_max_run_len + j];
                        if ( inx!=(lUInt16)-1 )
                        {
                            dsttable[inx].stats+=srctable[i].stats;
                            len -= dsttable[inx].count;
                            break;
                        }
                    }
                }
            }
        }

        // make tree
        int head = num_items-1;
        int min_index, min_stat, min_index2, min_stat2;
        for (;;)
        {
            min_index = -1;
            for (i=0; i<=head; i++)
            {
                if (!dsttable[i].parent && (min_index==-1 || min_stat>dsttable[i].stats))
                {
                    min_index = i;
                    min_stat = dsttable[i].stats;
                }
            }
            min_index2 = -1;
            for (i=0; i<=head; i++)
            {
                if (!dsttable[i].parent && i!=min_index && (min_index2==-1 || min_stat2>dsttable[i].stats))
                {
                    min_index2 = i;
                    min_stat2 = dsttable[i].stats;
                }
            }
            if (min_index2>=0)
            {
                ++head;
                dsttable[head].left = min_index;
                dsttable[head].right = min_index2;
                dsttable[head].stats = min_stat+min_stat2;
                dsttable[min_index].parent = head;
                dsttable[min_index2].parent = head;
            } 
            else
            {
                break;
            }
        }
        if ( RecurseBits( dsttable, head, 0, 0 ) > m_max_code_bits )
            return -1; // error: too long code
        int total_len = 0;
        for (i=num_items-1; i>=0; i--)
        {
            total_len += dsttable[i].codelen * dsttable[i].stats;
        }
        delete run_index;
        return total_len;
    }

    void MakeTable()
    {
        hrle_stats_t * table = new hrle_stats_t[m_num_symbols*m_max_run_len];
        hrle_stats_t * dsttable = new hrle_stats_t[m_num_symbols*m_max_run_len*2+1];
        memset(table, 0, sizeof(hrle_stats_t)*m_num_symbols*m_max_run_len);
        int ncsize = 0;
        int rlesize = 0;
        for (int i=m_num_symbols*m_max_run_len-1; i>=0; i--)
        {
            table[i].value = i / m_max_run_len;
            table[i].count = i % m_max_run_len + 1;
            table[i].stats = m_stats[i];
            ncsize += m_stats[i]*2*table[i].count;
            rlesize += m_stats[i]*8;
        }
        qsort(table, m_num_symbols*m_max_run_len, sizeof(hrle_stats_t), compare_items);
        int min_size = -1;
        int best_num_items = -1;
        int num_items = 1 << m_max_code_bits;
        if (num_items>m_num_symbols*m_max_run_len)
            num_items = m_num_symbols*m_max_run_len;
        while ( num_items>0 && table[num_items-1].stats==0 )
            num_items--;
        for (; num_items>=m_num_symbols; num_items--)
        {
            memset(dsttable, 0, sizeof(hrle_stats_t)*(m_num_symbols*m_max_run_len*2+1));
            int sz = MakeConfig( table, dsttable, num_items );
            if ( sz>0 && (min_size==-1 || sz<min_size ) )
            {
                min_size = sz;
                best_num_items = num_items;
            }
        }
        if (min_size<=0)
            return; //ERROR
        memset(dsttable, 0, sizeof(hrle_stats_t)*(m_num_symbols*m_max_run_len*2+1));
        memset(m_encode_table, 0, sizeof(hrle_encode_table_t)*m_num_symbols*m_max_run_len);
            
        int sz = MakeConfig( table, dsttable, best_num_items );
        m_decode_table_bits = 1;
        for (i=0; i<best_num_items; i++)
        {
            hrle_encode_table_t * p = &m_encode_table[dsttable[i].value*m_max_run_len + dsttable[i].count-1];
            p->code = dsttable[i].code;
            p->codelen = dsttable[i].codelen;
            if (p->codelen>m_decode_table_bits)
                m_decode_table_bits = p->codelen;
        }
        m_decode_table_size = 1 << m_decode_table_bits;
        m_decode_table = new hrle_decode_table_t[m_decode_table_size];
        for (i=0; i<best_num_items; i++)
        {
            int inx = dsttable[i].code << (m_decode_table_bits - dsttable[i].codelen);
            int n = 1 << (m_decode_table_bits - dsttable[i].codelen);
            for (int j=0; j<n; j++)
            {
                m_decode_table[inx+j].value = dsttable[i].value;
                m_decode_table[inx+j].count = dsttable[i].count;
                m_decode_table[inx+j].code =  (lUInt8)dsttable[i].code;
                m_decode_table[inx+j].codelen = (lUInt8)dsttable[i].codelen;
            }
        }
        delete table;
        delete dsttable;
    }
};

#define RHT_MAX_RUN_LENGTH 64
#define RHT_MAX_SYMBOL     4
#define RHT_MAX_CODE_BITS  8

class CLFntGlyph
{
public:
   GLYPHMETRICS    m_metrics;
   unsigned char * m_glyph;
   int             m_glyph_size;
   DWORD           m_unknown_glyph_index;
   void Clear()
   {
      if (m_glyph) delete m_glyph;
      m_glyph = NULL;
   }
   static int GetGlyphIndex( HDC hdc, wchar_t code )
   {
      wchar_t s[2];
      wchar_t g[2];
      s[0] = code;
      s[1] = 0;
      g[0] = 0;
      GCP_RESULTSW gcp;
      gcp.lStructSize = sizeof(GCP_RESULTSW);
      gcp.lpOutString = NULL;
      gcp.lpOrder = NULL;
      gcp.lpDx = NULL;
      gcp.lpCaretPos = NULL;
      gcp.lpClass = NULL;
      gcp.lpGlyphs = g;
      gcp.nGlyphs = 2;
      gcp.nMaxFit = 2;

      DWORD res = GetCharacterPlacementW(
         hdc, s, 1,
         1000,
         &gcp,
         0
      );
      if (!res)
          return 0;
      return g[0];
   }
   bool Init( HDC hdc, wchar_t code )
   {
      Clear();


      if ( m_unknown_glyph_index == 0)
          m_unknown_glyph_index = GetGlyphIndex( hdc, 1 );

//      if ( !(
           //(code>=0x0020 && code<=0x007F)
//         ||(code>=0x0401 && code<=0x04E9) // cyrillic
//         ))
//         return false;

      DWORD res = GetGlyphIndex( hdc, code );
      if (res==0xFFFF)
         return false;
      if (res==0)
         return false;
      if (res == m_unknown_glyph_index && code!=32)
         return false;


      MAT2 identity = { {0,1}, {0,0}, {0,0}, {0,1} };
      res = GetGlyphOutlineW( hdc, code,
         GGO_METRICS,
         &m_metrics,
         0,
         NULL,
         &identity );
      if (res==GDI_ERROR)
         return false;
      m_glyph_size = GetGlyphOutlineW( hdc, code,
         GGO_GRAY8_BITMAP, //GGO_METRICS
         &m_metrics,
         0,
         NULL,
         &identity );
      if (m_glyph_size>0x10000)
         return false;
      if (m_glyph_size==0)
      {
         m_glyph = NULL;
      }
      else
      {
         m_glyph = new unsigned char[m_glyph_size];
      }
      res = GetGlyphOutlineW( hdc, code,
         GGO_GRAY8_BITMAP, //GGO_METRICS
         &m_metrics,
         m_glyph_size,
         m_glyph,
         &identity );
      return  (res!=GDI_ERROR);
   }
   CLFntGlyph() : m_glyph(NULL), m_unknown_glyph_index(0) { }
   ~CLFntGlyph() { Clear(); }
};

class CLVFntConvertor
{
public:
   LOGFONT      m_logfont;
   font_gen_buf m_buf;

   CFont   m_font;
   CBitmap m_drawbmp;
   CDC     m_drawdc;
   COLORREF * m_drawpixels;
   DWORD           m_charset_filter;
   CRLEHuffTree m_stats;
   lString8 m_copyright;

   unsigned char m_convtable[65];
	
   void SetCopyright( lString8 copyright )
   {
	   m_copyright = copyright;
   }
   void SetCharsetFilter( DWORD flags )
   {
       m_charset_filter = flags;
   }
   bool FilterCode( wchar_t code )
   {
      return ( code<=0x00BF )
         || ((code>=0x2000 && code<=0x20AC))
         || ((m_charset_filter & LCHARSET_LATIN)    && (code>=0x00C0) && (code<=0x02A8))
         || ((m_charset_filter & LCHARSET_CYRILLIC) && (code>=0x0401) && (code<=0x04E9))
         || ((m_charset_filter & LCHARSET_GREEK)    && (code>=0x0300) && (code<=0x03CE))
         || ((m_charset_filter & LCHARSET_CHINEESE) && (code>=0x4E00) && (code<=0xFFFF));
   }
   CLVFntConvertor(LOGFONT * lf)
       : m_stats( RHT_MAX_SYMBOL, RHT_MAX_RUN_LENGTH, RHT_MAX_CODE_BITS )
   {
      m_charset_filter = 0;
      memcpy( &m_logfont, lf, sizeof(LOGFONT) );
      m_logfont.lfQuality = ANTIALIASED_QUALITY;
      BITMAPINFO bmi;
      memset( &bmi, 0, sizeof(bmi) );
      bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
      bmi.bmiHeader.biWidth = 128;
      bmi.bmiHeader.biHeight = 128;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      bmi.bmiHeader.biSizeImage = 0;
      bmi.bmiHeader.biXPelsPerMeter = 1024;
      bmi.bmiHeader.biYPelsPerMeter = 1024;
      bmi.bmiHeader.biClrUsed = 0;
      bmi.bmiHeader.biClrImportant = 0;

      HBITMAP hbmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, (void**)(&m_drawpixels), NULL, 0 );
      m_drawbmp.Attach(hbmp);
      m_drawdc.CreateCompatibleDC(NULL);
      m_drawdc.SelectObject(&m_drawbmp);

      m_font.CreateFontIndirect(&m_logfont);
      m_drawdc.SelectObject(m_font);

      TEXTMETRIC metric;
      GetTextMetrics( m_drawdc.m_hDC, &metric );

      m_buf.init(metric.tmHeight, 0, 2, m_logfont.lfFaceName, m_copyright.c_str() );
	  css_font_family_t ff = css_ff_serif;
	  if ( (m_logfont.lfPitchAndFamily & 3) == FIXED_PITCH )
	  {
		  ff = css_ff_monospace;
	  }
	  else
	  {
		  switch (m_logfont.lfPitchAndFamily & 0xF0)
		  {
		  case FF_DECORATIVE:
			  ff = css_ff_fantasy;
			  break;
		  case FF_MODERN:
			  ff = css_ff_monospace;
			  break;
		  case FF_ROMAN:
			  ff = css_ff_serif;
			  break;
		  case FF_SCRIPT:
			  ff = css_ff_cursive;
			  break;
		  case FF_SWISS:
			  ff = css_ff_sans_serif;
			  break;
		  case FF_DONTCARE:
		  default:
			  ff = css_ff_sans_serif;
			  break;
		  }
	  }

	  
	  m_buf.hdr.fontFamily = (lUInt8)ff;
      m_buf.hdr.flgItalic = m_logfont.lfItalic?1:0;
      m_buf.hdr.flgBold =   m_logfont.lfWeight>400?1:0;

      m_buf.hdr.fontAvgWidth = (unsigned char)metric.tmAveCharWidth;
      m_buf.hdr.fontBaseline = (unsigned char)(metric.tmHeight - metric.tmDescent);

      for (int i=0; i<16; i++)
         m_convtable[i] = 0;
      for (; i<32; i++)
         m_convtable[i] = 1;
      for (; i<48; i++)
         m_convtable[i] = 2;
      for (; i<65; i++)
         m_convtable[i] = 3;
   }

    void AddGlyphStats( unsigned char * src, int src_dx, int src_dy )
    {
        lUInt16 sz = (src_dx*src_dy+3)>>2;
        int src_rowsize = (src_dx + 3)/4*4;
        int shift = 0;
        int last_digit = -1;
        int same_count = 0;
        for (int y=0; y<src_dy; y++)
        {
            //
            for (int x=0; x<src_dx; x++)
            {
                //int xx = x >> 2;
                //int dx = x & 3;
                int b = m_convtable[ src[x] ];
                if ( b == last_digit )
                {
                    same_count++;
                }
                else
                {
                    if (same_count)
                        m_stats.AddStat( last_digit, same_count );
                    last_digit = b;
                    same_count = 1;
                }
            }
            src += src_rowsize;
        }
        if (same_count)
            m_stats.AddStat( last_digit, same_count );
    }

    lUInt16 ConvertGlyphUnpacked( unsigned char * dst, unsigned char * src, int src_dx, int src_dy )
    {
        lUInt16 sz = (src_dx*src_dy+3)>>2;
        int src_rowsize = (src_dx + 3)/4*4;
        int shift = 0;
        for (int y=0; y<src_dy; y++)
        {
            //
            for (int x=0; x<src_dx; x++)
            {
                //int xx = x >> 2;
                //int dx = x & 3;
                unsigned char b = m_convtable[ src[x] ] << ((3 - shift)<<1);
                *dst |= b;
                if (++shift==4)
                {
                    shift = 0;
                    dst++;
                }
            }
            src += src_rowsize;
        }
        return sz;
    }

    lUInt16 ConvertGlyphPacked( lUInt8 * dst, unsigned char * src, int src_dx, int src_dy )
    {
        int src_rowsize = (src_dx + 3)/4*4;
        int shift = 0;
        int last_digit = -1;
        int same_count = 0;
        lUInt8 * p = dst;
        int bitpos=0;
        int nruns = 0;
        for (int y=0; y<src_dy; y++)
        {
            //
            for (int x=0; x<src_dx; x++)
            {
                //int xx = x >> 2;
                //int dx = x & 3;
                int b = m_convtable[ src[x] ];
                if ( b == last_digit )
                {
                    same_count++;
                }
                else
                {
                    if (same_count)
                    {
                        m_stats.Encode( p, bitpos, last_digit, same_count );
                        nruns++;
                    }
                    last_digit = b;
                    same_count = 1;
                }
            }
            src += src_rowsize;
        }
        if (same_count)
        {
            nruns++;
            m_stats.Encode( p, bitpos, last_digit, same_count );
        }
        if (bitpos)
            p++;
        return p-dst;
    }

   bool Convert(const char * fname)
   {
      CLFntGlyph glyph;
      int pxPerByte = 4;
      int chcount = 0;
      //FILE * f = fopen("fnt.log", "wt" );
      m_stats.Clear();
      
      // pass 1: make stats
      for (int i=32; i< 0xffff; i++)
      {
          //fprintf( f, "%04x   %d\n ", i, CLFntGlyph::GetGlyphIndex(m_drawdc.m_hDC, i) );
         if ( FilterCode( i ) && glyph.Init(m_drawdc.m_hDC, i) )
         {
             if (glyph.m_glyph_size>0)
                AddGlyphStats( glyph.m_glyph, glyph.m_metrics.gmBlackBoxX, glyph.m_metrics.gmBlackBoxY );
         }
      }
      char decode_table[1024];
      m_stats.MakeTable();
      m_stats.getDecodeTable( (hrle_decode_info_t *)decode_table );
      m_buf.setDecodeTable( (hrle_decode_info_t *)decode_table );
      
      for (i=32; i< 0xffff; i++)
      {
          //fprintf( f, "%04x   %d\n ", i, CLFntGlyph::GetGlyphIndex(m_drawdc.m_hDC, i) );
         if ( FilterCode( i ) && glyph.Init(m_drawdc.m_hDC, i) )
         {
            lvfont_glyph_t * dest = m_buf.addGlyph( i );
            dest->blackBoxX = glyph.m_glyph_size?glyph.m_metrics.gmBlackBoxX:0;
            dest->blackBoxY = glyph.m_glyph_size?glyph.m_metrics.gmBlackBoxY:0;
            dest->originX = (char)glyph.m_metrics.gmptGlyphOrigin.x;
            dest->originY = (char)glyph.m_metrics.gmptGlyphOrigin.y;
            dest->width =   (char)glyph.m_metrics.gmCellIncX;
            //dest->rowBytes = glyph.m_glyph_size?(dest->blackBoxX + (pxPerByte-1))/pxPerByte:0;
            //int bytes = dest->rowBytes * dest->blackBoxY;
            if (dest->blackBoxY && dest->blackBoxX)
            {
                lUInt16 sz = ConvertGlyphPacked( dest->glyph, glyph.m_glyph, dest->blackBoxX, dest->blackBoxY);
                dest->glyphSize = sz;
            }
            else
            {
                dest->glyphSize = 0;
            }
            //memset( dest->glyph, 0xAA, bytes );
            m_buf.commitGlyph();
            chcount++;
         }
      }
      //fclose(f);
      return m_buf.saveToFile( fname );
   }
};

class CLFntConvDlg : public CDialog
{
public:
   LOGFONT m_logfont;
   bool    m_logfont_selected;
   CFont   * m_pfont;
   CBitmap m_drawbmp;
   CDC     m_drawdc;
   COLORREF * m_drawpixels;
   LFormattedText * m_frmtext;
   bool    m_char_exists[0x10000];
   lvfont_handle m_bmpfont;
   LVFontRef m_fontref;
   
// Construction
public:
	/// returns number of files created
	int batchConvert( const char * fname );
    void FormatSampleText();
   bool DrawChar( int code, int & dx, int & dy );
	void InitFontList();
	CLFntConvDlg(CWnd* pParent = NULL);	// standard constructor
    virtual ~CLFntConvDlg() 
    { 
        if (m_bmpfont) lvfontClose(m_bmpfont); 
        if (m_pfont) delete m_pfont;
        if (m_frmtext) delete m_frmtext;
    }

// Dialog Data
	//{{AFX_DATA(CLFntConvDlg)
	enum { IDD = IDD_LFNTCONV_DIALOG };
	CEdit	m_edCopyright;
	CButton	m_cbAscii;
	CButton	m_cbLatin;
	CButton	m_cbCyrillic;
	CButton	m_cbChineeze;
	CStatic	m_lblFontName;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLFntConvDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CLFntConvDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBtnChooseFont();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LFNTCONVDLG_H__A7B8E4AF_368D_4A87_A530_A3A94B59EF7B__INCLUDED_)
