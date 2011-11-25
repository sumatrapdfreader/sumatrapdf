/** \file lvimg.h
    \brief Image formats support

    CoolReader Engine C-compatible API

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifndef __LVIMG_H_INCLUDED__
#define __LVIMG_H_INCLUDED__


#include "lvref.h"
#include "lvstream.h"

class LVImageSource;
class ldomNode;
class LVColorDrawBuf;

/// image decoding callback interface
class LVImageDecoderCallback
{
public:
    virtual ~LVImageDecoderCallback();
    virtual void OnStartDecode( LVImageSource * obj ) = 0;
    virtual bool OnLineDecoded( LVImageSource * obj, int y, lUInt32 * data ) = 0;
    virtual void OnEndDecode( LVImageSource * obj, bool errors ) = 0;
};

class LVImageSource
{
public:
    virtual ldomNode * GetSourceNode() = 0;
    virtual LVStream * GetSourceStream() = 0;
    virtual void   Compact() = 0;
    virtual int    GetWidth() = 0;
    virtual int    GetHeight() = 0;
    virtual bool   Decode( LVImageDecoderCallback * callback ) = 0;
    virtual ~LVImageSource();
};

typedef LVRef< LVImageSource > LVImageSourceRef;

/// type of image transform
enum ImageTransform {
    IMG_TRANSFORM_NONE,    // just draw w/o any resizing/tiling
    IMG_TRANSFORM_SPLIT,   // split at specified pixel, fill extra middle space with value of this pixel
    IMG_TRANSFORM_STRETCH, // stretch image proportionally to fill whole area
    IMG_TRANSFORM_TILE,    // tile image
};

/// creates image which stretches source image by filling center with pixels at splitX, splitY
LVImageSourceRef LVCreateStretchFilledTransform( LVImageSourceRef src, int newWidth, int newHeight, ImageTransform hTransform=IMG_TRANSFORM_SPLIT, ImageTransform vTransform=IMG_TRANSFORM_SPLIT, int splitX=-1, int splitY=-1 );
/// creates image which fills area with tiled copy
LVImageSourceRef LVCreateTileTransform( LVImageSourceRef src, int newWidth, int newHeight, int offsetX, int offsetY );
/// creates XPM image
LVImageSourceRef LVCreateXPMImageSource( const char * data[] );
LVImageSourceRef LVCreateNodeImageSource( ldomNode * node );
LVImageSourceRef LVCreateDummyImageSource( ldomNode * node, int width, int height );
/// creates image source from stream
LVImageSourceRef LVCreateStreamImageSource( LVStreamRef stream );
/// creates image source as memory copy of file contents
LVImageSourceRef LVCreateFileCopyImageSource( lString16 fname );
/// creates image source as memory copy of stream contents
LVImageSourceRef LVCreateStreamCopyImageSource( LVStreamRef stream );
/// creates decoded memory copy of image, if it's unpacked size is less than maxSize (8 bit gray or 32 bit color)
LVImageSourceRef LVCreateUnpackedImageSource( LVImageSourceRef srcImage, int maxSize = MAX_SKIN_IMAGE_CACHE_ITEM_UNPACKED_SIZE, bool gray=false );
/// creates decoded memory copy of image, if it's unpacked size is less than maxSize; bpp: 8,16,32 supported
LVImageSourceRef LVCreateUnpackedImageSource( LVImageSourceRef srcImage, int maxSize, int bpp );
/// creates image source based on draw buffer
LVImageSourceRef LVCreateDrawBufImageSource( LVColorDrawBuf * buf, bool own );


class LVFont;
class LVDrawBuf;

/// draws battery icon in specified rectangle of draw buffer; if font is specified, draws charge %
// first icon is for charging, the rest - indicate progress icon[1] is lowest level, icon[n-1] is full power
// if no icons provided, battery will be drawn
void LVDrawBatteryIcon( LVDrawBuf * drawbuf, const lvRect & batteryRc, int percent, bool charging, LVRefVec<LVImageSource> icons, LVFont * font );


#define IMAGE_SOURCE_FROM_BYTES( imgvar , bufvar ) \
    extern unsigned char bufvar []; \
    extern int bufvar ## _size ; \
    LVImageSourceRef imgvar = LVCreateStreamImageSource( \
        LVCreateMemoryStream( bufvar , bufvar ## _size ) )


#endif
