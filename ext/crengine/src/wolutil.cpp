/*******************************************************

   CoolReader Engine

   wolutil.cpp:  WOL file format support

   Based on code by SeNS   

*******************************************************/

#include <string.h>
#include "../include/wolutil.h"


#define N 4096
#define F 18
#define THRESHOLD 2
#define NIL N

//#define _DEBUG_LOG

#ifdef _DEBUG_LOG
DumpFile log( "wol.log" );
#endif

static lvByteOrderConv cnv;

/*
 * Based on LZSS.C by Haruhiko Okumura 4/6/1989
 */
class LZSSUtil {
    lUInt16 textsize;   /* text size counter */
    lUInt16 codesize;   /* code size counter */
    lUInt16 printcount; /* counter for reporting progress every 1K bytes */
    lUInt8  text_buf[N + F - 1];     /* ring buffer of size N,
              with extra F-1 bytes to facilitate string comparison */
    lUInt16 match_position, match_length;  /* of longest match.  These are
              set by the InsertNode() procedure. */
    lUInt16 lson[N + 1], rson[N + 257], dad[N + 1];  /* left & right children &
              parents -- These constitute binary search trees. */

    // private methods              
    void InsertNode(int r);
    void DeleteNode(int p);
    
public:
    /// init tables
    LZSSUtil();
    /// encode buffer
    bool Encode(
      const lUInt8 * in_buf, 
      int in_length,
      lUInt8 * out_buf, 
      int & out_length
    );
    /// decode buffer
    bool Decode(
      const lUInt8 * in_buf, 
      int in_length,
      lUInt8 * out_buf,
      int & out_length
    );
};



class InBuf {
    const lUInt8 * _buf;
    int _size;
    int _pos;
public:
    InBuf( const lUInt8 * buf, int size )
        : _buf(buf), _size(size), _pos(0) {}
    bool get( lUInt32 & ch )
    {
        if (_pos>=_size)
            return false;
        ch = _buf[ _pos++ ];
        return true;
    }
};

class OutBuf {
    lUInt8 * _buf;
    int _size;
    int _pos;
    bool _overflow;
public:
    OutBuf( lUInt8 * buf, int size )
        : _buf(buf), _size(size), _pos(0), _overflow(false) {}
    bool getOverflow() { return _overflow; }
    bool put( lUInt32 ch )
    {
        if (_pos>=_size) {
            _overflow = true;
            return false;
        }
        _buf[ _pos++ ] = (lUInt8)ch;
        return true;
    }
    int getPos() { return _pos; }
};

LZSSUtil::LZSSUtil()  
{
    /* initialize trees */
    int  i;
    /* For i = 0 to N - 1, rson[i] and lson[i] will be the right and
       left children of node i.  These nodes need not be initialized.
       Also, dad[i] is the parent of node i.  These are initialized to
       NIL (= N), which stands for 'not used.'
       For i = 0 to 255, rson[N + i + 1] is the root of the tree
       for strings that begin with character i.  These are initialized
       to NIL.  Note there are 256 trees. */
    for (i = N + 1; i <= N + 256; i++) 
        rson[i] = NIL;
    for (i = 0; i < N; i++) 
        dad[i] = NIL;
}

void LZSSUtil::InsertNode(int r)
    /* Inserts string of length F, text_buf[r..r+F-1], into one of the
       trees (text_buf[r]'th tree) and returns the longest-match position
       and length via the global variables match_position and match_length.
       If match_length = F, then removes the old node in favor of the new
       one, because the old one will be deleted sooner.
       Note r plays double role, as tree node and position in buffer. */
{
    int  i, p, cmp;
    unsigned char  *key;

    cmp = 1;  key = &text_buf[r];  p = N + 1 + key[0];
    rson[r] = lson[r] = NIL;  match_length = 0;
    for ( ; ; ) {
        if (cmp >= 0) {
              if (rson[p] != NIL) p = rson[p];
              else { rson[p] = r;  dad[r] = p; return; }
        } else {
            if (lson[p] != NIL)
                  p = lson[p];
            else { 
                lson[p] = r;  
                dad[r] = p; 
                return;
            }
        }
        for (i = 1; i < F; i++)
            if ((cmp = key[i] - text_buf[p + i]) != 0)
                break;
        if (i > match_length) {
            match_position = p;
            if ((match_length = i) >= F)
                break;
        }
    }
    dad[r] = dad[p];
    lson[r] = lson[p];
    rson[r] = rson[p];
    dad[lson[p]] = r;
    dad[rson[p]] = r;
    if (rson[dad[p]] == p)
        rson[dad[p]] = r;
    else
        lson[dad[p]] = r;
    dad[p] = NIL;  /* remove p */
}
    
void LZSSUtil::DeleteNode(int p)  /* deletes node p from tree */
{
    int  q;

    if (dad[p] == NIL) return;  /* not in tree */
    if (rson[p] == NIL) q = lson[p];
    else if (lson[p] == NIL) q = rson[p];
    else {
         q = lson[p];
         if (rson[q] != NIL) {
              do {  q = rson[q];  } while (rson[q] != NIL);
              rson[dad[q]] = lson[q];  dad[lson[q]] = dad[q];
              lson[q] = lson[p];  dad[lson[p]] = q;
         }
         rson[q] = rson[p];  dad[rson[p]] = q;
    }
    dad[q] = dad[p];
    if (rson[dad[p]] == p) rson[dad[p]] = q;  else lson[dad[p]] = q;
    dad[p] = NIL;
}

bool LZSSUtil::Encode(
  const lUInt8 * in_buf, 
  int in_length,
  lUInt8 * out_buf, 
  int & out_length
)
{
    InBuf in( in_buf, in_length );
    OutBuf out( out_buf, out_length );
    int  i, len, r, s, last_match_length, code_buf_ptr;
    lUInt32 c;
    lUInt8 code_buf[17], mask;
    code_buf[0] = 0;  /* code_buf[1..16] saves eight units of code, and
         code_buf[0] works as eight flags, "1" representing that the unit
         is an unencoded letter (1 byte), "0" a position-and-length pair
         (2 bytes).  Thus, eight units require at most 16 bytes of code. */
    code_buf_ptr = mask = 1;
    s = 0;  r = N - F;
    for (i = s; i < r; i++) 
        text_buf[i] = ' ';  /* Clear the buffer with
         any character that will appear often. */
    for (len = 0; len < F && in.get(c); len++)
         text_buf[r + len] = (lUInt8)c;  /* Read F bytes into the last F bytes of
              the buffer */
    if ((textsize = len) == 0)
        return false;  /* text of size zero */
    //for (i = 1; i <= F; i++) InsertNode(r - i);  
    /* Insert the F strings,
         each of which begins with one or more 'space' characters.  Note
         the order in which these strings are inserted.  This way,
         degenerate trees will be less likely to occur. */
    InsertNode(r);  /* Finally, insert the whole string just read.  The
         global variables match_length and match_position are set. */
    do {
         if (match_length > len) match_length = len;  /* match_length
              may be spuriously long near the end of text. */
         if (match_length <= THRESHOLD) {
              match_length = 1;  /* Not long enough match.  Send one byte. */
              code_buf[0] |= mask;  /* 'send one byte' flag */
              code_buf[code_buf_ptr++] = text_buf[r];  /* Send uncoded. */
         } else {
              code_buf[code_buf_ptr++] = (lUInt8) match_position;
              code_buf[code_buf_ptr++] = (lUInt8)
                   (((match_position >> 4) & 0xf0)
                | (match_length - (THRESHOLD + 1)));  /* Send position and
                        length pair. Note match_length > THRESHOLD. */
         }
         if ((mask <<= 1) == 0) {  /* Shift mask left one bit. */
              for (i = 0; i < code_buf_ptr; i++)  /* Send at most 8 units of */
                   out.put(code_buf[i]);     /* code together */
              codesize += code_buf_ptr;
              code_buf[0] = 0;  code_buf_ptr = mask = 1;
         }
         last_match_length = match_length;
         for (i = 0; i < last_match_length && in.get(c); i++) {
              DeleteNode(s);      /* Delete old strings and */
              text_buf[s] = (lUInt8)c;    /* read new bytes */
              if (s < F - 1) 
                    text_buf[s + N] = (lUInt8)c;  /* If the position is
                   near the end of buffer, extend the buffer to make
                   string comparison easier. */
              s = (s + 1) & (N - 1);  r = (r + 1) & (N - 1);
                   /* Since this is a ring buffer, increment the position
                      modulo N. */
              InsertNode(r); /* Register the string in text_buf[r..r+F-1] */
         }
         while (i++ < last_match_length) {  /* After the end of text, */
              DeleteNode(s);                     /* no need to read, but */
              s = (s + 1) & (N - 1);  r = (r + 1) & (N - 1);
              if (--len) InsertNode(r);          /* buffer may not be empty. */
         }
    } while (len > 0);  /* until length of string to be processed is zero */
    if (code_buf_ptr > 1) {       /* Send remaining code. */
         for (i = 0; i < code_buf_ptr; i++) 
           out.put(code_buf[i]);
         codesize += code_buf_ptr;
    }
    out_length=out.getPos();
    return true;
}

/* Just the reverse of Encode(). */

#if 0

bool LZSSUtil::Decode(
  const lUInt8 * in_buf, 
  int in_length,
  lUInt8 * out_buf, 
  int & out_length
)  
{
    InBitStream in( in_buf, in_length );
    OutBuf out( out_buf, out_length );
    lUInt32 i, k, r;
    lUInt32 c, ci, cj;
    unsigned int  flags;

    for (i = 0; i < N - F; i++)
        text_buf[i] = 0; //' '
    r = N - F;  flags = 0;
    for ( ; ; ) {
        if (!in.read(c, 1))
            break;
        if (c)
        {
            if (!in.read(c, 8))
                break;
            if (!out.put(c))
                break;
            text_buf[r++] = c;
            r &= (N - 1);
        }
        else
        {
            if (!in.read(cj, 4))
                break;
            cj += THRESHOLD;
            if (!in.read(ci, 12))
                break;
            for (k = 0; k <= cj; k++) {
                c = text_buf[(ci + k) & (N - 1)];
                if (!out.put(c))
                     break;
                text_buf[r++] = c;
                r &= (N - 1);
            }
        }
    }
    out_length=out.getPos();
    return !out.getOverflow();
}
#endif

bool LZSSUtil::Decode(
  const lUInt8 * in_buf, 
  int in_length,
  lUInt8 * out_buf, 
  int & out_length
)  
{
    InBuf in( in_buf, in_length );
    OutBuf out( out_buf, out_length );
    lUInt32 i, k, r;
    lUInt32 c, ci, cj;
    unsigned int  flags;

    for (i = 0; i < N - F; i++)
        text_buf[i] = 'a';
    r = N - F;  flags = 0;
    for ( ; ; ) {
        if (((flags >>= 1) & 256) == 0) {
            if (!in.get(c)) 
                break;
            flags = c | 0xff00;      /* uses higher byte cleverly */
        }                                  /* to count eight */
        if (flags & 1) {
            if (!in.get(c))
                break;
            if (!out.put(c))
                break;
            text_buf[r++] = (lUInt8)c;
            r &= (N - 1);
        } else {
            if (!in.get(ci))
                break;
            if (!in.get(cj))
                break;
            ci |= ((cj & 0xf0) << 4);
            cj = (cj & 0x0f) + THRESHOLD;
            for (k = 0; k <= cj; k++) {
                c = text_buf[(ci + k) & (N - 1)];
                if (!out.put(c))
                     break;
                text_buf[r++] = (lUInt8)c;
                r &= (N - 1);
            }
        }
    }
    out_length=out.getPos();
    return !out.getOverflow();
}




///////////////////////////////////////////////////////////////////////////
//
// WOLBase class
//
///////////////////////////////////////////////////////////////////////////




WOLBase::WOLBase( LVStream * stream )
: _stream(stream)
, _book_title_size(0)
, _cover_image_size(0)
, _page_data_size(0)
, _catalog_size(0)
, _wolf_start_pos(0)
, _subcatalog_level23_items(0)
, _subcatalog_offset(0)
, _catalog_level1_items(0)
, _catalog_subcatalog_size(0)
{
}

WOLReader::WOLReader( LVStream * stream )
: WOLBase(stream)
{
}

static void readMem( const lUInt8 * buf, int offset, lUInt16 & dest )
{
    dest = ((lUInt16)buf[offset+1]<<8) | buf[offset];
}

static void readMem( const lUInt8 * buf, int offset, lUInt32 & dest )
{
    dest = ((lUInt32)buf[offset+3]<<24) 
        | ((lUInt32)buf[offset+2]<<16) 
        | ((lUInt32)buf[offset+1]<<8) 
        | buf[offset];
}

bool WOLReader::readHeader()
{
    lUInt8 header[0x80];
    if (_stream->Read( header, 0x80, NULL )!=LVERR_OK)
        return false;;
    if (memcmp(header, "WolfEbook1.11", 13))
        return false;
    readMem(header, 0x17, _book_title_size);
    readMem(header, 0x19, _cover_image_size); // 0x19    
    readMem(header, 0x5F, _subcatalog_level23_items);  // 0x5F
    readMem(header, 0x61, _subcatalog_offset);  // 0x61
    readMem(header, 0x22, _catalog_level1_items);  // 0x1E
    readMem(header, 0x1E, _catalog_subcatalog_size);  // 0x1E
    readMem(header, 0x26, _page_data_size);   // 0x26
    readMem(header, 0x3C, _catalog_size);     // 0x3C
    _book_title = readString(0x80, _book_title_size);
    _stream->SetPos( 0x80 + _cover_image_size + _book_title_size );
    lString8 str = readTag();
    if (str!="wolf")
        return false;
    str = readTag();
    if (str!="catalog")
        return false;
    for (;;)
    {
        str = readTag();
        if (str.empty())
            return false;
        if (str=="/catalog")
            break;
        wolf_img_params params;
        //img bitcount=1 compact=1 width=794 height=1123 length=34020
        if (sscanf(str.c_str(), "img bitcount=%d compact=%d width=%d height=%d length=%d",
            &params.bitcount, &params.compact, 
            &params.width, &params.height, 
            &params.length)!=5)
            return false;
        params.offset = (lUInt32)_stream->GetPos();
        _stream->SetPos(params.offset+params.length);
        str = readTag();
        const char * s = str.c_str();
        lString8 tst(s);
        tst += s;
        if (str!="/img")
            return false;
        _images.add( params );
    }
    //_wolf_start_pos;
    return true;
}

LVGrayDrawBuf * WOLReader::getImage( int index )
{
    if (index<0 || index>=_images.length())
        return NULL;
    const wolf_img_params * img = &_images[index];
    LVArray<lUInt8> buf(img->length, 0);
    _stream->SetPos( img->offset );
    _stream->Read(buf.ptr(), img->length, NULL);
    int img_size = img->height*((img->width*img->bitcount+7)/8);
    int uncomp_len = img_size + 18;
    LVArray<lUInt8> uncomp(uncomp_len, 0);

    LZSSUtil unpacker;
    if (unpacker.Decode(buf.ptr(), buf.length(), uncomp.ptr(), uncomp_len))
    {
        LVStreamRef dump = LVOpenFileStream( "test.dat", LVOM_WRITE );
        if (!dump.isNull()) {
            dump->Write(uncomp.ptr(), uncomp_len, NULL);
        }

        // inverse 1-bit images
        if (img->bitcount==1)
        {
            for (int i=0; i<img_size; i++)
            {
               uncomp[i]= ~uncomp[i];
            }
        }
        
        LVGrayDrawBuf * image = new LVGrayDrawBuf(img->width, img->height, img->bitcount);
        memcpy(image->GetScanLine(0), uncomp.ptr(), img_size);
        return image;
    }

    //delete image;
    return NULL;
}

lString8 WOLReader::readString(int offset, int size)
{
    _stream->SetPos( offset );
    lString8 buf;
    buf.append(size, ' ');
    _stream->Read( buf.modify(), size, NULL);
    return buf;
}

lString8 WOLReader::readTag()
{
    lString8 buf;
    lChar8 ch = 0;
    for (;;)
    {
        if (_stream->Read(&ch, 1, NULL)!=LVERR_OK)
            return lString8();
        if (ch==' '||ch=='\r'||ch=='\n')
            continue;
        if (ch!='<')
            return lString8();
        break;
    }
    for (;;)
    {
        if (_stream->Read(&ch, 1, NULL)!=LVERR_OK)
            return lString8();
        if (ch==0 || buf.length()>100)
            return lString8();
        if (ch=='>')
            return buf;
        buf.append(1, ch);
    }
}

LVArray<lUInt8> * WOLReader::getBookCover()
{
    LVArray<lUInt8> * cover = new LVArray<lUInt8>(_cover_image_size, 0);
    _stream->SetPos( 0x80 + _book_title_size );
    _stream->Read( cover->ptr(), _cover_image_size, NULL);
    return cover;    
}

WOLWriter::WOLWriter( LVStream * stream )
: WOLBase(stream)
, _catalog_opened(false)
{
    lUInt8 header[0x80];
    memset(header, 0, sizeof(header));
    memcpy(header, "WolfEbook1.11", 13);
    header[0x11]=1;
    header[0x12]=2;
    header[0x22]=1;
    header[0x1D]=1;
    header[0x40]=1;
    stream->Write( header, 0x80, NULL );
}

void WOLWriter::startCatalog()
{
    if (!_catalog_opened)
    {
        _catalog_start = (lUInt32)_stream->GetPos();
        *_stream << "<catalog>";
        _catalog_opened=true;
    }
}

void WOLWriter::endCatalog()
{
    if (_catalog_opened) 
    {
        *_stream << "</catalog>";
        _catalog_opened=false;
    }
}

typedef struct
{
  lUInt32 PageOffs;    //[0x00] PageOffs: offset from beginning of section "WolfPages" to description of page (<img...>).
  lUInt32 NameOffs;    //[0x04] NameOffs: offset from beginning of file to beginnig of name (in "Names" area).
  lUInt16 NameSize;    //[0x08] NameSize: length (in chars) of name (in "Names" area).
  lUInt16 ChildsCount; //[0x0A] ChildsCount: count of subitems for this element from TOC.
  lUInt32 PrevPeerOffs;//[0x0C] PrevPeerOffs: offset from beginning of file to the description of previous peer element in SubCatalog table. 0, if current item is the first child of parent.
  lUInt32 NextPeerOffs;//[0x10] NextPeerOffs: offset from beginning of file to the description of next peer element in SubCatalog table. 0, if current item is the last child of parent.
  lUInt32 ChildOffs;   //[0x14] ChildOffs: offset from beginning of file to the description of first child in SubCatalog table. 0, if there is no subitems (in TOC) for current item.
  lUInt32 ParentOffs;  //[0x18] ParentOffs: offset from beginning of file to the description of parent element in SubCatalog table. 0, if element is on level 1.
  lUInt8  Level3Idx;   //[0x1C] Level3Idx: level 3 index of element; 0 if element is on level 1 or level 2 in TOC.
  lUInt8  Level2Idx;   //[0x1D] Level2Idx: level 2 index of element; 0 if element is on level 1 in TOC.
  lUInt8  Level1Idx;   //[0x1E] Level1Idx: level 1 index of element.
  lUInt8  AlignByte;   //[0x1F] AlignByte: constant = 0x00.
  lChar8  ItemName[48];//[0x20] ItemName: name of item in TOC.
} wol_toc_subcatalog_item;

void WOLWriter::writeToc()
{
    // enumerate ordered by level
    int n = 0;
    _subcatalog_level23_items = 0;
    _subcatalog_offset = 0;
    _catalog_level1_items = 0; // 0x22
    _catalog_subcatalog_size = 0; // 0x1E
    lUInt32 cat_start = (lUInt32) _stream->GetPos();
    int len = _tocItems.length();
    if ( len==0 ) {
        // fake catalog = book name
        *_stream << "<catalog><item>" << _book_name
            << "</item>";
        *_stream << cnv.lsf( getPageOffset(0) ) << "</catalog>";
        _catalog_level1_items = 1;
    } else {
        //============================================================
        // catalog
        *_stream << "<catalog>";
        for ( int ti=0; ti<len; ti++ ) {
            TocItemInfo * src = _tocItems[ti];
            if ( src->getLevel()==1 ) {
                *_stream << "<item>" << src->name
                    << "</item>" << cnv.lsf( getPageOffset(src->page) );
                _catalog_level1_items++;
            }
        }
        *_stream << "</catalog>";
    
        //============================================================
        // subcatalog
    
        // allocate TOC
        _subcatalog_offset = (lUInt32) _stream->GetPos();
        wol_toc_subcatalog_item * toc = new wol_toc_subcatalog_item[len];
        int catsize = sizeof(wol_toc_subcatalog_item) * len;
        memset( toc, 0, catsize );
        lString8 names;
        for ( int lvl=1; lvl<=3; lvl++ ) {
            for ( int i=0; i<_tocItems.length(); i++ ) {
                if ( _tocItems[i]->getLevel() == lvl ) {
                    _tocItems[i]->catindex = n++;
                    if ( lvl>1 )
                        _subcatalog_level23_items++;
                }
            }
        }
        int names_start = _subcatalog_offset + 12 + catsize;
        int i;
        for ( i=0; i<len; i++ ) {
            TocItemInfo * src = _tocItems[i];
            int n = src->catindex;
            wol_toc_subcatalog_item * item = &toc[n];
            item->Level1Idx = src->l1index;
            item->Level2Idx = src->l2index;
            item->Level3Idx = src->l3index;
            item->ChildOffs = cnv.lsf( src->firstChild ? src->firstChild->catindex * 80 + _subcatalog_offset + 12 : 0 );
            item->ParentOffs = cnv.lsf( src->parent ? src->parent->catindex * 80 + _subcatalog_offset + 12 : 0 );
            item->NextPeerOffs = cnv.lsf( src->nextSibling ? src->nextSibling->catindex * 80 + _subcatalog_offset + 12 : 0 );
            item->PrevPeerOffs = cnv.lsf( src->prevSibling ? src->prevSibling->catindex * 80 + _subcatalog_offset + 12 : 0 );
            lString8 name = src->name;
            item->NameOffs = cnv.lsf( (lUInt32)(names_start + names.length()) );
            name << ' ';
            item->NameSize = cnv.lsf( (lUInt16)(name.length()) );
            lStr_ncpy( item->ItemName, name.c_str(), 47 );
            item->PageOffs = cnv.lsf( getPageOffset(src->page) ); // ???
            names += name;
        }
#ifdef _DEBUG_LOG
        for ( i=0; i<len; i++ ) {
            wol_toc_subcatalog_item * item = &toc[i];
            lUInt32 currOffset = i * 80 + _subcatalog_offset + 12;
            fprintf( log, "%2d   %07d : (%d,%d,%d) \t parent=%07d  child=%07d  prev=%07d  next=%07d\t %s\n", 
                i, currOffset, item->Level1Idx, item->Level2Idx, item->Level3Idx, item->ParentOffs, item->ChildOffs, item->PrevPeerOffs, item->NextPeerOffs, item->ItemName );
        }
#endif
        *_stream << "<subcatalog>";
        _stream->Write( toc, sizeof(wol_toc_subcatalog_item) * len, NULL );
        *_stream << names;
        // SubcatalogSize = 26 + CountOfAllTOCItems * 80 + Length(NamesInAllTOCLevels)
        *_stream << "\x08</subcatalog>";
        delete[] ( toc );
    }
    // finalize
    lUInt32 cat_end = (lUInt32) _stream->GetPos();
    _catalog_subcatalog_size = cat_end - cat_start;
}

WOLWriter::~WOLWriter()
{
    writePageIndex();
    updateHeader();
}

void WOLWriter::updateHeader()
{
    _stream->SetPos(0x17);
    *_stream << cnv.lsf( _book_title_size );
    _stream->SetPos(0x19);
    *_stream << cnv.lsf( _cover_image_size );
    _stream->SetPos(0x26);
    *_stream << cnv.lsf( _page_data_size );
    _stream->SetPos(0x1e);
    *_stream << cnv.lsf( _catalog_subcatalog_size );
    _stream->SetPos(0x22);
    *_stream << cnv.lsf( _catalog_level1_items );
    _stream->SetPos(0x3c);
    *_stream << cnv.lsf( _catalog_size );
    _stream->SetPos(0x42);
    *_stream << cnv.lsf( (lUInt32)_page_starts.length() );
    _stream->SetPos(0x4b);
    *_stream << cnv.lsf( (lUInt32)_page_starts.length() );
    _stream->SetPos(0x5F);
    *_stream << cnv.lsf( _subcatalog_level23_items ); // 0x5F
    _stream->SetPos(0x61);
    *_stream << cnv.lsf( _subcatalog_offset ); // 0x61
}

void WOLWriter::addTitle(
          const lString8 & title,
          const lString8 & subject,
          const lString8 & author,
          const lString8 & adapter,
          const lString8 & translator,
          const lString8 & publisher,
          const lString8 & time_publish,
          const lString8 & introduction,
          const lString8 & isbn)
{
    //
    _book_name = title;
    lString8 buf;
    buf.reserve(128);
    buf << "<title>" << title << "\r\n"
        << "<subject>" << subject << "\r\n"
        << "<author>" << author << "\r\n"
        << "<adpter>" << adapter << "\r\n"
        << "<translator>" << translator << "\r\n"
        << "<publisher>" << publisher << "\r\n"
        << "<time_publish>" << time_publish << "\r\n"
        << "<introduction>" << introduction << "\r\n"
        << "<ISBN>" << isbn << "\r\n";
    _book_title_size = buf.length();
    *_stream << buf;
}

void WOLWriter::addCoverImage( const lUInt8 * buf, int size )
{
    static const lUInt8 cover_hdr[10] = {
    0xFF, 0xFF, 0x58, 0x02, 0x01, 0x00, 0x4B, 0x00, 0x20, 0x03
    };
    _stream->Write(cover_hdr, 10, NULL);
    _cover_image_size = size + 10;
    _stream->Write(buf, size, NULL);
    _wolf_start_pos = (lUInt32) _stream->GetPos();
    *_stream << "<wolf>\r\n";
}

void WOLWriter::addImage(
  int width, 
  int height, 
  const lUInt8 * bitmap, // [width*height/4]
  int num_bits
)
{
    int bmp_sz = (width * height * num_bits)>>3;
    startCatalog();
#if 0
    lUInt8 * inversed = NULL;
    if (num_bits==1)
    {
        inversed = new lUInt8 [bmp_sz];
        for (int i=0; i<bmp_sz; i++)
        {
           inversed[i]= ~bitmap[i];
        }
        bitmap = inversed;
    }
#endif
/*
#if 0
  compressed.reserve(bitmap.size()*9/8);
  for(int i=0; i<bitmap.size(); ) {
    compressed.push_back((unsigned char)0xff);
    for(int j=0; j<8 && i<bitmap.size(); j++) {
      compressed.push_back(bitmap[i++]);
    }
  }
  compressed.push_back((unsigned char)0x0); // extra last dummy char 
#else
  compressed.resize(bitmap.size()*2);
  int compressed_len;
#if 1
  Encode((const unsigned char*)&(bitmap[0]), bitmap.size(), 
    (unsigned char*)&(compressed[0]), &compressed_len);
#else
  EncodeLZSS(&(bitmap[0]), bitmap.size(), &(compressed[0]),
    compressed_len);
#endif
#endif
*/


    int compressed_len = bmp_sz * 9/8 + 18;
    lUInt8 * compressed = new lUInt8 [compressed_len];
    
    LZSSUtil packer;
    packer.Encode(bitmap, bmp_sz, compressed, compressed_len);

    compressed[ compressed_len++ ] = 0; // extra last dummy char
    
#if 0 //def _DEBUG_LOG
    LZSSUtil unpacker;
    lUInt8 * decomp = new lUInt8 [bmp_sz*2];
    int decomp_len = 0;
    unpacker.Decode(compressed, compressed_len-1, decomp, decomp_len);
    assert(compressed_len==decomp_len);
    for(int i=0; i<decomp_len; i++) {
        assert(bitmap[i]==decomp[i]);
    }
    delete[] decomp;
#endif

    _page_starts.add( (lUInt32)_stream->GetPos() );
    
    lString8 buf;
    buf.reserve(128);
    buf << "<img bitcount=" 
        << lString8::itoa(num_bits) 
        << " compact=1 width="
        << lString8::itoa(width)
        << " height="
        << lString8::itoa(height)
        << " length="
        << lString8::itoa((int)compressed_len)
        << ">";
    *_stream << buf; 

    //_page_starts.add( (lUInt32)_stream->GetPos() );

    _stream->Write( compressed, compressed_len, NULL );
    endPage();
    *_stream << lString8("</img>");
  
    // cleanup
    delete[] compressed;
    //if (inversed)
    //     delete inversed;
}

void WOLWriter::endPage()
{
    //m_page_starts.push_back(ftell(m_fp));
}

#define USE_001_FORMAT 0

void WOLWriter::addTocItem( int level1index, int level2index, int level3index, int pageNumber, lString8 title )
{
#ifdef _DEBUG_LOG
    fprintf(log, "addTocItem(lvl=%d,%d,%d, page=%d, text=%s\n", level1index, level2index, level3index, pageNumber, title.c_str());
#endif
    TocItemInfo * item = new TocItemInfo( _tocItems.length(), level1index, level2index, level3index, pageNumber, title );
    _tocItems.add( item );
    for ( int k=_tocItems.length()-2; k>=0; k-- ) {
        TocItemInfo * last = item;
        TocItemInfo * ki = _tocItems[k];
        if ( last->isPrevSibling( *ki ) ) {
            last->prevSibling = ki;
            ki->nextSibling = last;
        } else if ( last->isParent( *ki ) ) {
            last->parent = ki;
            if ( ki->firstChild==NULL )
                ki->firstChild = last;
            break;
        }
    }
}

void WOLWriter::writePageIndex()
{
    endCatalog();
    *_stream << "</wolf>";
    int pos0 = (int)_stream->GetPos();
    _page_data_size = pos0 - _wolf_start_pos;

    writeToc();

    //*_stream << "<catalog><item>" << _book_name
    //    << "</item>";
    //*_stream << (lUInt32)0x11 << "</catalog>";
    
    int pos1 = (int)_stream->GetPos();

    //_page_index_size = pos1-pos0; //0x1E
    
#if (USE_001_FORMAT==1)
    *_stream << "<pagetable ver=\"001\">";
#else
    *_stream << "<pagetable ver=\"021211 \">";
#endif
    
    int start_of_catalog_table = (int)_stream->GetPos();
    
    LVArray<lUInt32> pagegroup_1; 
    LVArray<lUInt32> pagegroup_2; 
    LVArray<lUInt32> pagegroup_delim; 

    pagegroup_delim.add( 0xFFFFFFFF );
    pagegroup_1.add( cnv.lsf( _catalog_start ) );
#if (USE_001_FORMAT!=1)
    pagegroup_1.add( cnv.lsf( _page_starts[0] ) );
#endif
    pagegroup_2.add( cnv.lsf( _catalog_start ) );
    for (int i=1; i<_page_starts.length(); i++)
    {
        pagegroup_1.add( cnv.lsf( _page_starts[i] ) );
#if (USE_001_FORMAT!=1)
        pagegroup_1.add( cnv.lsf( _page_starts[i] ) );
#endif
        pagegroup_2.add( cnv.lsf( _page_starts[i] ) );
    }
    int pagegroups_start = start_of_catalog_table + 13*4 + 12;
    lUInt32 p = pagegroups_start;
    LVArray<lUInt32> catalog; 
    catalog.add( cnv.lsf( p ) ); p += pagegroup_1.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_1.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_delim.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_1.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_1.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_delim.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_2.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_2.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_delim.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_1.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_1.length()*4;
    catalog.add( cnv.lsf( p ) ); p += pagegroup_delim.length()*4;
    catalog.add( cnv.lsf( p ) );
    *_stream << catalog << "</pagetable>";


    *_stream << pagegroup_1;
    *_stream << pagegroup_1;
    *_stream << pagegroup_delim;
    *_stream << pagegroup_1;
    *_stream << pagegroup_1;
    *_stream << pagegroup_delim;
    *_stream << pagegroup_2;
    *_stream << pagegroup_2;
    *_stream << pagegroup_delim;
    *_stream << pagegroup_1;
    *_stream << pagegroup_1;
    *_stream << pagegroup_delim;

    int pos2 = (lUInt32)_stream->GetPos();
    _catalog_size = pos2 - pos1;
}

void WOLWriter::addImage( LVGrayDrawBuf & image )
{
    addImage( image.GetWidth(), image.GetHeight(), 
        image.GetScanLine(0), image.GetBitsPerPixel() );
}

typedef struct {
  lUInt16 compression; // Compression: 0xFFFF for raw data (no compression); 0x0001 for LZSS compression.
  lUInt16 width;       // ImageWidth: width of image (in pixels)
  lUInt16 bpp;         // BitsPerPixel: 1 for monochrome images; 2 for 4-level gray images.
  lUInt16 bytesPerLine;// BytesPerLine: count of bytes in one row of image. (= ImageWidth*BitsPerPixel/8)
  lUInt16 height;      // ImageHeight: height of image (in pixels)
} CoverPageHeader_t;

void WOLWriter::addCoverImage( LVGrayDrawBuf & image )
{
    // convert 2bpp to 1bpp
#if 1


    CoverPageHeader_t hdr;
    hdr.compression = cnv.lsf( (lUInt16)1 );
    lUInt16 width = image.GetWidth();
    lUInt16 height = image.GetHeight();
    lUInt16 bpp = image.GetBitsPerPixel();
    lUInt16 bpl = (lUInt16)((width * bpp + 7)>>3);
    hdr.width = cnv.lsf( width );
    hdr.height = cnv.lsf( height );
    hdr.bpp = cnv.lsf( bpp );
    hdr.bytesPerLine = cnv.lsf( bpl );

    lUInt32 cover_start_pos = (lUInt32) _stream->GetPos();
    _stream->Write(&hdr, sizeof(hdr), NULL);

    int bmp_sz = bpl * height;
    
    lUInt8 * data = new lUInt8[ bmp_sz ];
    memcpy( data, image.GetScanLine(0), bmp_sz );
    if ( hdr.bpp == 2 )
    {
        for (int i=0; i<bmp_sz; i++)
            data[i] = ~data[i];
    }

    int compressed_len = bmp_sz * 9/8 + 18;
    lUInt8 * compressed = new lUInt8 [compressed_len];
    
    LZSSUtil packer;
    packer.Encode(data, bmp_sz, compressed, compressed_len);

    compressed[ compressed_len++ ] = 0; // extra last dummy char

    delete[] data;

    _stream->Write( compressed, compressed_len, NULL );
    
    _wolf_start_pos = (lUInt32) _stream->GetPos();
    _cover_image_size = _wolf_start_pos - cover_start_pos;
    *_stream << "<wolf>\r\n";
    
#else
    image.ConvertToBitmap(true);
    int sz = (image.GetWidth()+7)/8 * image.GetHeight();
    addCoverImage( image.GetScanLine(0), sz );
#endif
}

