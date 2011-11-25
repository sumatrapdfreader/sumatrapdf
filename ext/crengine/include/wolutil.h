/** \file wolutil.h
    \brief Wolf file support

    Based on WOL encoder source by SeNS

*/

#ifndef __WOLUTIL_H_INCLUDED__
#define __WOLUTIL_H_INCLUDED__

#include "../include/crengine.h"



class WOLBase {
protected:
    LVStream * _stream;
    lString8 _book_name; 
    lUInt16 _book_title_size;  // 0x17
    lUInt32 _cover_image_size; // 0x19    
    //lUInt32 _page_index_size;  // 0x1E
    lUInt32 _page_data_size;   // 0x26
    lUInt32 _catalog_size;     // 0x3C
    lUInt32 _catalog_start;     // 0x3C
    lUInt32 _wolf_start_pos;
    lUInt16 _subcatalog_level23_items; // 0x5F
    lUInt32 _subcatalog_offset; // 0x61
    lUInt32 _catalog_level1_items; // 0x22
    lUInt32 _catalog_subcatalog_size; // 0x1E
    LVArray<lUInt32> _page_starts;
    lUInt32 getPageOffset( int index )
    {
        return (_page_starts[index] - _wolf_start_pos);
    }
public:
    WOLBase( LVStream * stream );
    ~WOLBase() {  }
};

/// Wolf file writer
class WOLWriter : public WOLBase {
    bool _catalog_opened;
    // private methods
    void updateHeader();
    void endPage();
    void startCatalog();
    void endCatalog();
    void writePageIndex();

    class TocItemInfo {
    public:
        int index;
        int l1index;
        int l2index;
        int l3index;
        int page;
        lString8 name;
        int catindex;
        TocItemInfo * parent;
        TocItemInfo * firstChild;
        TocItemInfo * nextSibling;
        TocItemInfo * prevSibling;
        int getLevel()
        {
            if ( l3index ==0 && l2index == 0 )
                return 1;
            else if ( l3index==0 )
                return 2;
            else
                return 1;
        }
        bool isPrevSibling(TocItemInfo & v ) {
            if ( l1index==v.l1index ) {
                if ( l2index!=0 && l2index==v.l2index ) {
                    return (l3index!=0 && l3index==v.l3index+1);
                } else {
                    return (l2index!=0 && l2index==v.l2index+1 && l3index==0 && v.l3index==0);
                }
            } else {
                return (l1index==v.l1index+1 && l2index==0 && v.l2index==0);
            }
        }
        bool isParent( TocItemInfo & v ) {
            if ( v.l1index==l1index ) {
                if ( l2index!=0 && v.l2index==0 )
                    return true;
                if ( l2index!=0 && l2index==v.l2index ) {
                    if ( l3index!=0 && v.l3index==0 )
                        return true;
                }
                return true;
            }
            /*
            if ( v.l1index==l1index && l2index==0 )
                return true;
            if ( v.l1index==l1index ) {
                if (v.l2index==0 && l2index!=0)
                    return true;
                if ( v.l2index==l2index ) {
                    if (v.l3index==0 && l3index!=0 )
                        return true;
                }
            }
            */
            return false;
        }
        TocItemInfo( int idx, int l1, int l2, int l3, int p, lString8 n )
            : index(idx), l1index(l1), l2index(l2), l3index(l3), page(p), name(n), catindex(0),
            parent(NULL), firstChild(NULL), nextSibling(NULL), prevSibling(NULL)
        {

        }
    };

    void writeToc();

    LVPtrVector<TocItemInfo> _tocItems;

public:
    WOLWriter( LVStream * stream );
    ~WOLWriter();
    void addTocItem( int level1index, int level2index, int level3index, int pageNumber, lString8 title );
    void addTitle(
          const lString8 & title,
          const lString8 & subject,
          const lString8 & author,
          const lString8 & adapter,
          const lString8 & translator,
          const lString8 & publisher,
          const lString8 & time_publish,
          const lString8 & introduction,
          const lString8 & isbn);
      void addCoverImage( const lUInt8 * buf, int size );
    void addImage(
      int width, 
      int height, 
      const lUInt8 * bitmap, // [width*height/4]
      int num_bits
    );
    void addCoverImage( LVGrayDrawBuf & image );
    void addImage( LVGrayDrawBuf & image );
};

typedef struct {
    int bitcount;
    int compact;
    int width;
    int height;
    int length;
    lUInt32 offset;
} wolf_img_params;

/// Wolf file reader
class WOLReader : public WOLBase {
private:
    lString8 _book_title;
    LVArray<wolf_img_params> _images;
    lString8 readTag();
    lString8 readString(int offset, int size);
public:
    int getImageCount() { return _images.length(); }
    const wolf_img_params * getImageInfo( int index ) { return &_images[index]; }
    LVArray<lUInt8> * getBookCover();
    LVGrayDrawBuf * getImage( int index );
    WOLReader( LVStream * stream );
    bool readHeader();
    lString8 getBookTitle() { return _book_title; }
};


#endif
