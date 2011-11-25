/*******************************************************

   CoolReader Engine

   lvimg.cpp:  Image formats support

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License

   See LICENSE file for details

*******************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define XMD_H

#include "../include/lvimg.h"
#include "../include/lvtinydom.h"

#if (USE_LIBPNG==1)
#include <png.h>
#endif

#if (USE_LIBJPEG==1)

//#include "../../wxWidgets/src/jpeg/jinclude.h"
extern "C" {
#include <jpeglib.h>
}

#include <jerror.h>

#if !defined(HAVE_WXJPEG_BOOLEAN)
typedef boolean wxjpeg_boolean;
#endif

#endif


LVImageSource::~LVImageSource() {}


class LVNodeImageSource : public LVImageSource
{
protected:
    ldomNode *  _node;
    LVStreamRef _stream;
    int _width;
    int _height;
public:
    LVNodeImageSource( ldomNode * node, LVStreamRef stream )
        : _node(node), _stream(stream), _width(0), _height(0)
    {

    }

    ldomNode * GetSourceNode() { return _node; }
    virtual LVStream * GetSourceStream() { return _stream.get(); }
    virtual void   Compact() { }
    virtual int    GetWidth() { return _width; }
    virtual int    GetHeight() { return _height; }
    virtual bool   Decode( LVImageDecoderCallback * callback ) = 0;
    virtual ~LVNodeImageSource() {}
};

#if (USE_LIBJPEG==1)

METHODDEF(void)
cr_jpeg_error (j_common_ptr cinfo);


typedef struct {
    struct jpeg_source_mgr pub;   /* public fields */
    LVStream * stream;        /* source stream */
    JOCTET * buffer;      /* start of buffer */
    bool start_of_file;    /* have we gotten any data yet? */
} cr_jpeg_source_mgr;

#define INPUT_BUF_SIZE  4096    /* choose an efficiently fread'able size */

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */

METHODDEF(void)
cr_init_source (j_decompress_ptr cinfo)
{
    cr_jpeg_source_mgr * src = (cr_jpeg_source_mgr*) cinfo->src;

    /* We reset the empty-input-file flag for each image,
     * but we don't clear the input buffer.
     * This is correct behavior for reading a series of images from one source.
     */
    src->start_of_file = true;
}

/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * In typical applications, this should read fresh data into the buffer
 * (ignoring the current state of next_input_byte & bytes_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been reloaded.  It is not necessary to
 * fill the buffer entirely, only to obtain at least one more byte.
 *
 * There is no such thing as an EOF return.  If the end of the file has been
 * reached, the routine has a choice of ERREXIT() or inserting fake data into
 * the buffer.  In most cases, generating a warning message and inserting a
 * fake EOI marker is the best course of action --- this will allow the
 * decompressor to output however much of the image is there.  However,
 * the resulting error message is misleading if the real problem is an empty
 * input file, so we handle that case specially.
 *
 * In applications that need to be able to suspend compression due to input
 * not being available yet, a FALSE return indicates that no more data can be
 * obtained right now, but more may be forthcoming later.  In this situation,
 * the decompressor will return to its caller (with an indication of the
 * number of scanlines it has read, if any).  The application should resume
 * decompression after it has loaded more data into the input buffer.  Note
 * that there are substantial restrictions on the use of suspension --- see
 * the documentation.
 *
 * When suspending, the decompressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_input_byte & bytes_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point must be rescanned after resumption, so move it to
 * the front of the buffer rather than discarding it.
 */

METHODDEF(wxjpeg_boolean)
cr_fill_input_buffer (j_decompress_ptr cinfo)
{
    cr_jpeg_source_mgr * src = (cr_jpeg_source_mgr *) cinfo->src;
    lvsize_t bytesRead = 0;
    if ( src->stream->Read( src->buffer, INPUT_BUF_SIZE, &bytesRead ) != LVERR_OK )
        cr_jpeg_error((jpeg_common_struct*)cinfo);

    if (bytesRead <= 0) {
        if (src->start_of_file) /* Treat empty input file as fatal error */
            ERREXIT(cinfo, JERR_INPUT_EMPTY);
        WARNMS(cinfo, JWRN_JPEG_EOF);
        /* Insert a fake EOI marker */
        src->buffer[0] = (JOCTET) 0xFF;
        src->buffer[1] = (JOCTET) JPEG_EOI;
        bytesRead = 2;
    }

    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = (lUInt32)bytesRead;
    src->start_of_file = false;

    return TRUE;
}

/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 * Writers of suspendable-input applications must note that skip_input_data
 * is not granted the right to give a suspension return.  If the skip extends
 * beyond the data currently in the buffer, the buffer can be marked empty so
 * that the next read will cause a fill_input_buffer call that can suspend.
 * Arranging for additional bytes to be discarded before reloading the input
 * buffer is the application writer's problem.
 */

METHODDEF(void)
cr_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
    cr_jpeg_source_mgr * src = (cr_jpeg_source_mgr *) cinfo->src;

    /* Just a dumb implementation for now.  Could use fseek() except
     * it doesn't work on pipes.  Not clear that being smart is worth
     * any trouble anyway --- large skips are infrequent.
     */
    if (num_bytes > 0) {
        while (num_bytes > (long) src->pub.bytes_in_buffer) {
          num_bytes -= (long) src->pub.bytes_in_buffer;
          (void) cr_fill_input_buffer(cinfo);
          /* note we assume that fill_input_buffer will never return FALSE,
           * so suspension need not be handled.
           */
        }
        src->pub.next_input_byte += (size_t) num_bytes;
        src->pub.bytes_in_buffer -= (size_t) num_bytes;
    }
}

/*
 * An additional method that can be provided by data source modules is the
 * resync_to_restart method for error recovery in the presence of RST markers.
 * For the moment, this source module just uses the default resync method
 * provided by the JPEG library.  That method assumes that no backtracking
 * is possible.
 */
GLOBAL(wxjpeg_boolean)
cr_resync_to_restart (j_decompress_ptr, int)
{
    return FALSE;
}


/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

METHODDEF(void)
cr_term_source (j_decompress_ptr)
{
  /* no work necessary here */
}

GLOBAL(void)
cr_jpeg_src (j_decompress_ptr cinfo, LVStream * stream)
{
    cr_jpeg_source_mgr * src;

    /* The source object and input buffer are made permanent so that a series
     * of JPEG images can be read from the same file by calling jpeg_stdio_src
     * only before the first one.  (If we discarded the buffer at the end of
     * one image, we'd likely lose the start of the next one.)
     * This makes it unsafe to use this manager and a different source
     * manager serially with the same JPEG object.  Caveat programmer.
     */
    if (cinfo->src == NULL) { /* first time for this JPEG object? */
        src = (cr_jpeg_source_mgr *) new cr_jpeg_source_mgr;
        cinfo->src = (struct jpeg_source_mgr *) src;
        src->buffer = new JOCTET[INPUT_BUF_SIZE];
    }

    src = (cr_jpeg_source_mgr *) cinfo->src;
    src->pub.init_source = cr_init_source;
    src->pub.fill_input_buffer = cr_fill_input_buffer;
    src->pub.skip_input_data = cr_skip_input_data;
    src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
    src->pub.term_source = cr_term_source;
    src->stream = stream;
    src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
    src->pub.next_input_byte = NULL; /* until buffer loaded */
}

GLOBAL(void)
cr_jpeg_src_free (j_decompress_ptr cinfo)
{
    cr_jpeg_source_mgr * src = (cr_jpeg_source_mgr *) cinfo->src;
    if ( src && src->buffer )
    {
        delete[] src->buffer;
        src->buffer = NULL;
    }
    delete src;
}


/*
 * ERROR HANDLING:
 *
 * The JPEG library's standard error handler (jerror.c) is divided into
 * several "methods" which you can override individually.  This lets you
 * adjust the behavior without duplicating a lot of code, which you might
 * have to update with each future release.
 *
 * Our example here shows how to override the "error_exit" method so that
 * control is returned to the library's caller when a fatal error occurs,
 * rather than calling exit() as the standard error_exit method does.
 *
 * We use C's setjmp/longjmp facility to return control.  This means that the
 * routine which calls the JPEG library must first execute a setjmp() call to
 * establish the return point.  We want the replacement error_exit to do a
 * longjmp().  But we need to make the setjmp buffer accessible to the
 * error_exit routine.  To do this, we make a private extension of the
 * standard JPEG error handler object.  (If we were using C++, we'd say we
 * were making a subclass of the regular error handler.)
 *
 * Here's the extended error handler struct:
 */

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
cr_jpeg_error (j_common_ptr cinfo)
{
    //fprintf(stderr, "cr_jpeg_error() : fatal error while decoding JPEG image\n");

    //char buffer[JMSG_LENGTH_MAX];

    /* Create the message */
    //(*cinfo->err->format_message) (cinfo, buffer);

    //fprintf( stderr, "message: %s\n", buffer );

    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr) cinfo->err;

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    //(*cinfo->err->output_message) (cinfo);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}

#endif

#if (USE_LIBPNG==1)

class LVPngImageSource : public LVNodeImageSource
{
protected:
public:
    LVPngImageSource( ldomNode * node, LVStreamRef stream );
    virtual ~LVPngImageSource();
    virtual void   Compact();
    virtual bool   Decode( LVImageDecoderCallback * callback );
    static bool CheckPattern( const lUInt8 * buf, int len );
};


static void lvpng_error_func (png_structp png, png_const_charp)
{
    //fprintf(stderr, "png error: %s\n", msg)
    longjmp(png_jmpbuf(png), 1);
}

static void lvpng_warning_func (png_structp png, png_const_charp)
{
    //fprintf(stderr, "png warning: %s\n", msg)
    longjmp(png_jmpbuf(png), 1);
}

static void lvpng_read_func(png_structp png, png_bytep buf, png_size_t len)
{
    LVNodeImageSource * obj = (LVNodeImageSource *) png_get_io_ptr(png);
    LVStream * stream = obj->GetSourceStream();
    lvsize_t bytesRead = 0;
    if ( stream->Read( buf, len, &bytesRead )!=LVERR_OK || bytesRead!=(lvsize_t)len )
        longjmp(png_jmpbuf(png), 1);
}

#endif

/// dummy image source to show invalid image
class LVDummyImageSource : public LVImageSource
{
protected:
    ldomNode *  _node;
    int _width;
    int _height;
public:
    LVDummyImageSource( ldomNode * node, int width, int height )
        : _node(node), _width(width), _height(height)
    {

    }

    ldomNode * GetSourceNode() { return _node; }
    virtual LVStream * GetSourceStream() { return NULL; }
    virtual void   Compact() { }
    virtual int    GetWidth() { return _width; }
    virtual int    GetHeight() { return _height; }
    virtual bool   Decode( LVImageDecoderCallback * callback )
    {
        if ( callback )
        {
            callback->OnStartDecode(this);
            lUInt32 * row = new lUInt32[ _width ];
            for (int i=0; i<_height; i++)
            {
                if ( i==0 || i==_height-1 )
                {
                    for ( int x=0; x<_width; x++ )
                        row[ x ] = 0x000000;
                }
                else
                {
                    for ( int x=1; x<_width-1; x++ )
                        row[ x ] = 0xFFFFFF;
                    row[ 0 ] = 0x000000;
                    row[ _width-1 ] = 0x000000;
                }
                callback->OnLineDecoded(this, i, row);
            }
            delete[] row;
            callback->OnEndDecode(this, false);
        }
        return true;
    }
    virtual ~LVDummyImageSource() {}
};

/// dummy image source to show invalid image
class LVXPMImageSource : public LVImageSource
{
protected:
    char ** _rows;
    lUInt32 * _palette;
    lUInt8 _pchars[128];
    int _width;
    int _height;
    int _ncolors;
public:
    LVXPMImageSource( const char ** data )
        : _rows(NULL), _palette(NULL), _width(0), _height(0), _ncolors(0)
    {
        bool err = false;
        int charsperpixel;
        if ( sscanf( data[0], "%d %d %d %d", &_width, &_height, &_ncolors, &charsperpixel )!=4 ) {
            err = true;
        } else if ( _width>0 && _width<255 && _height>0 && _height<255 && _ncolors>=2 && _ncolors<255 && charsperpixel == 1 ) {
            _rows = new char * [_height];
            for ( int i=0; i<_height; i++ ) {
                _rows[i] = new char[_width];
                memcpy( _rows[i], data[i+1+_ncolors], _width );
            }

            _palette = new lUInt32[_ncolors];
            memset( _pchars, 0, 128 );
            for ( int cl=0; cl<_ncolors; cl++ ) {
                const char * src = data[1+cl];
                _pchars[((unsigned)(*src++)) & 127] = cl;
                if ( (*src++)!=' ' || (*src++)!='c' || (*src++)!=' ' ) {
                    err = true;
                    break;
                }
                if ( *src == '#' ) {
                    src++;
                    int c;
                    if ( sscanf( src, "%x", &c )!=1 ) {
                        err = true;
                        break;
                    }
                    _palette[cl] = (lUInt32)c;
                } else if ( !strcmp( src, "None" ) )
                    _palette[cl] = 0xFF000000;
                else if ( !strcmp( src, "Black" ) )
                    _palette[cl] = 0x000000;
                else if ( !strcmp( src, "White" ) )
                    _palette[cl] = 0xFFFFFF;
                else
                    _palette[cl] = 0x000000;
            }
        } else {
            err = true;
        }
        if ( err ) {
            _width = _height = 0;
        }
    }
    virtual ~LVXPMImageSource()
    {
        if ( _rows ) {
            for ( int i=0; i<_height; i++ ) {
                delete[]( _rows[i] );
            }
            delete[] _rows;
        }
        if ( _palette )
            delete[] _palette;
    }

    ldomNode * GetSourceNode() { return NULL; }
    virtual LVStream * GetSourceStream() { return NULL; }
    virtual void   Compact() { }
    virtual int    GetWidth() { return _width; }
    virtual int    GetHeight() { return _height; }
    virtual bool   Decode( LVImageDecoderCallback * callback )
    {
        if ( callback )
        {
            callback->OnStartDecode(this);
            lUInt32 * row = new lUInt32[ _width ];
            for (int i=0; i<_height; i++)
            {
                const char * src = _rows[i];
                for ( int x=0; x<_width; x++ ) {
                    row[x] = _palette[_pchars[(unsigned)src[x]]];
                }
                callback->OnLineDecoded(this, i, row);
            }
            delete[] row;
            callback->OnEndDecode(this, false);
        }
        return true;
    }
};

LVImageSourceRef LVCreateXPMImageSource( const char * data[] )
{
    LVImageSourceRef ref( new LVXPMImageSource( data ) );
    if ( ref->GetWidth()<1 )
        return LVImageSourceRef();
    return ref;
}


#if (USE_LIBJPEG==1)

class LVJpegImageSource : public LVNodeImageSource
{
protected:
public:
    LVJpegImageSource( ldomNode * node, LVStreamRef stream )
        : LVNodeImageSource(node, stream)
    {

    }
    virtual ~LVJpegImageSource() {}
    virtual void   Compact() { }
    virtual bool   Decode( LVImageDecoderCallback * callback )
    {
        struct jpeg_decompress_struct cinfo;
        /* Step 1: allocate and initialize JPEG decompression object */

	/* We use our private extension JPEG error handler.
	 * Note that this struct must live as long as the main JPEG parameter
	 * struct, to avoid dangling-pointer problems.
	 */
	struct my_error_mgr jerr;

        /* We set up the normal JPEG error routines, then override error_exit. */
        jpeg_error_mgr errmgr;
        cinfo.err = jpeg_std_error(&errmgr);
        errmgr.error_exit = cr_jpeg_error;

        lUInt8 * buffer = NULL;
        lUInt32 * row = NULL;

        if (setjmp(jerr.setjmp_buffer)) {
	    /* If we get here, the JPEG code has signaled an error.
	     * We need to clean up the JPEG object, close the input file, and return.
	     */
            if ( buffer )
                delete[] buffer;
            if ( row )
                delete[] row;
            cr_jpeg_src_free (&cinfo);
            jpeg_destroy_decompress(&cinfo);
            return false;
	}


            _stream->SetPos( 0 );
            /* Now we can initialize the JPEG decompression object. */
            jpeg_create_decompress(&cinfo);
            /* Step 2: specify data source (eg, a file) */
            cr_jpeg_src( &cinfo, _stream.get() );
            /* Step 3: read file parameters with jpeg_read_header() */

            //fprintf(stderr, "    try to call jpeg_read_header...\n");
            (void) jpeg_read_header(&cinfo, TRUE);
            /* We can ignore the return value from jpeg_read_header since
             *   (a) suspension is not possible with the stdio data source, and
             *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
             * See libjpeg.doc for more info.
             */
            _width = cinfo.image_width;
            _height = cinfo.image_height;
            //fprintf(stderr, "    jpeg_read_header() finished succesfully: image size = %d x %d\n", _width, _height);

            if ( callback )
            {
                callback->OnStartDecode(this);
                /* Step 4: set parameters for decompression */

                /* In this example, we don't need to change any of the defaults set by
                 * jpeg_read_header(), so we do nothing here.
                 */
                cinfo.out_color_space = JCS_RGB;

                /* Step 5: Start decompressor */

                (void) jpeg_start_decompress(&cinfo);
                /* We can ignore the return value since suspension is not possible
                 * with the stdio data source.
                 */
                buffer = new lUInt8 [ cinfo.output_width * cinfo.output_components ];
                row = new lUInt32 [ cinfo.output_width ];
                /* Step 6: while (scan lines remain to be read) */
                /*           jpeg_read_scanlines(...); */

                /* Here we use the library's state variable cinfo.output_scanline as the
                 * loop counter, so that we don't have to keep track ourselves.
                 */
                while (cinfo.output_scanline < cinfo.output_height) {
                    int y = cinfo.output_scanline;
                    /* jpeg_read_scanlines expects an array of pointers to scanlines.
                     * Here the array is only one element long, but you could ask for
                     * more than one scanline at a time if that's more convenient.
                     */
                    (void) jpeg_read_scanlines(&cinfo, &buffer, 1);
                    /* Assume put_scanline_someplace wants a pointer and sample count. */
                    lUInt8 * p = buffer;
                    for (int x=0; x<(int)cinfo.output_width; x++)
                    {
                        row[x] = (((lUInt32)p[0])<<16) | (((lUInt32)p[1])<<8) | (((lUInt32)p[2])<<0);
                        p += 3;
                    }
                    callback->OnLineDecoded( this, y, row );
                }
                callback->OnEndDecode(this, true);
            }

        if ( buffer )
            delete[] buffer;
        if ( row )
            delete[] row;
        cr_jpeg_src_free (&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return true;
    }
    static bool CheckPattern( const lUInt8 * buf, int )
    {
        //check for SOI marker at beginning of file
        return (buf[0]==0xFF && buf[1]==0xD8);
    }
};

#endif


#if (USE_LIBPNG==1)

LVPngImageSource::LVPngImageSource( ldomNode * node, LVStreamRef stream )
        : LVNodeImageSource(node, stream)
{
}
LVPngImageSource::~LVPngImageSource() {}
void LVPngImageSource::Compact() { }
bool LVPngImageSource::Decode( LVImageDecoderCallback * callback )
{
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    lUInt32 * row = NULL;
    _stream->SetPos( 0 );
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
        (png_voidp)this, lvpng_error_func, lvpng_warning_func);
    if ( !png_ptr )
        return false;

    if (setjmp( png_ptr->jmpbuf )) {
        _width = 0;
        _height = 0;
        if (png_ptr)
        {
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        }
        if ( row )
            delete row;
        if (callback)
            callback->OnEndDecode(this, true); // error!
        return false;
    }

    //
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        lvpng_error_func(png_ptr, "cannot create png info struct");
    png_set_read_fn(png_ptr,
        (voidp)this, lvpng_read_func);
    png_read_info( png_ptr, info_ptr );


    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height,
        &bit_depth, &color_type, &interlace_type,
        NULL, NULL);
    _width = width;
    _height = height;

    row = new lUInt32[ width ];

    if ( callback )
    {
        callback->OnStartDecode(this);

        //int png_transforms = PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_INVERT_ALPHA;
            //PNG_TRANSFORM_PACKING|
            //PNG_TRANSFORM_STRIP_16|
            //PNG_TRANSFORM_INVERT_ALPHA;

        // SET TRANSFORMS
        if (color_type & PNG_COLOR_MASK_PALETTE)
            png_set_palette_to_rgb(png_ptr);

        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
#if PNG_LIBPNG_VER_RELEASE==7
            png_set_gray_1_2_4_to_8(png_ptr);
#else
            png_set_expand_gray_1_2_4_to_8(png_ptr);
#endif

        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha(png_ptr);

        if (bit_depth == 16)
            png_set_strip_16(png_ptr);

        png_set_invert_alpha(png_ptr);

        if (bit_depth < 8)
            png_set_packing(png_ptr);

        //if (color_type == PNG_COLOR_TYPE_RGB)
            png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);

        //if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        //    png_set_swap_alpha(png_ptr);

        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
                png_set_gray_to_rgb(png_ptr);

        int number_passes = png_set_interlace_handling(png_ptr);
        //if (color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
        //    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)

        //if (color_type == PNG_COLOR_TYPE_RGB ||
        //    color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        png_set_bgr(png_ptr);

            for (int pass = 0; pass < number_passes; pass++)
        {
                for (lUInt32 y = 0; y < height; y++)
            {
                png_read_rows(png_ptr, (unsigned char **)&row, NULL, 1);
                callback->OnLineDecoded( this, y, row );
            }
        }

        png_read_end(png_ptr, info_ptr);

        callback->OnEndDecode(this, false);
    }
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    delete [] row;
    return true;
}

bool LVPngImageSource::CheckPattern( const lUInt8 * buf, int )
{
    return( !png_sig_cmp((unsigned char *)buf, (png_size_t)0, 4) );
}

#endif

// GIF support
#if (USE_GIF==1)

class LVGifImageSource;
class LVGifFrame;

class LVGifImageSource : public LVNodeImageSource
{
    friend class LVGifFrame;
protected:
    LVGifFrame ** m_frames;
    int m_frame_count;
    unsigned char m_version;
    unsigned char m_bpp;     //
    unsigned char m_flg_gtc; // GTC (gobal table of colors) flag
    unsigned char m_transparent_color; // index

    lUInt32 * m_global_color_table;
public:
    LVGifImageSource( ldomNode * node, LVStreamRef stream )
        : LVNodeImageSource(node, stream)
    {
        m_global_color_table = NULL;
        m_frames = NULL;
        m_frame_count = 0;
        Clear();
    }
public:
    static bool CheckPattern( const lUInt8 * buf, int )
    {
        if (buf[0]!='G' || buf[1]!='I' || buf[2]!='F')
            return false;
        // version: '87a' or '89a'
        if (buf[3]!='8' || buf[5]!='a')
            return false;
        if (buf[4]!='7' && buf[4]!='9')
            return false; // bad version
        return true;
    }
    virtual void   Compact()
    {
        // TODO: implement compacting
    }
    virtual bool Decode( LVImageDecoderCallback * callback );

    int DecodeFromBuffer(unsigned char *buf, int buf_size, LVImageDecoderCallback * callback);
    //int LoadFromFile( const char * fname );
    LVGifImageSource();
    virtual ~LVGifImageSource();
    void Clear();
    lUInt32 * GetColorTable() {
        if (m_flg_gtc)
            return m_global_color_table;
        else
            return NULL;
    };
};

class LVGifFrame
{
protected:
    int        m_cx;
    int        m_cy;
    int m_left;
    int m_top;
    unsigned char m_bpp;     // bits per pixel
    unsigned char m_flg_ltc; // GTC (gobal table of colors) flag
    unsigned char m_flg_interlaced; // interlace flag

    LVGifImageSource * m_pImage;
    lUInt32 *    m_local_color_table;

    unsigned char * m_buffer;
public:
    int DecodeFromBuffer( unsigned char * buf, int buf_size, int &bytes_read );
    LVGifFrame(LVGifImageSource * pImage);
    ~LVGifFrame();
    void Clear();
    lUInt32 * GetColorTable() {
        if (m_flg_ltc)
            return m_local_color_table;
        else
            return m_pImage->GetColorTable();
    };
    void Draw( LVImageDecoderCallback * callback )
    {
        int w = m_pImage->GetWidth();
        int h = m_pImage->GetHeight();
        if ( w<=0 || w>4096 || h<=0 || h>4096 )
            return; // wrong image width
        callback->OnStartDecode( m_pImage );
        lUInt32 * line = new lUInt32[w];
        int transp_color = m_pImage->m_transparent_color;
        lUInt32 * pColorTable = GetColorTable();
        for ( int i=0; i<h; i++ ) {
            for ( int j=0; j<w; j++ ) {
                line[j] = 0xFFFFFFFF; // transparent
            }
            if ( i >= m_top  && i < m_top+m_cy ) {
                unsigned char * p_line = m_buffer + (i-m_top)*m_cx;
                for ( int x=0; x<m_cx; x++ ) {
                    unsigned char b = p_line[x];
                    if (b!=transp_color) {
                        line[x + m_left] = pColorTable[b];
                    }
                }
            }
            callback->OnLineDecoded( m_pImage, i, line );
        }
        delete[] line;
        callback->OnEndDecode( m_pImage, false );
    }
};

LVGifImageSource::~LVGifImageSource()
{
    Clear();
}

inline lUInt32 lRGB(lUInt32 r, lUInt32 g, lUInt32 b )
{
    return (r<<16)|(g<<8)|b;
}


int LVGifImageSource::DecodeFromBuffer(unsigned char *buf, int buf_size, LVImageDecoderCallback * callback)
{
    // check GIF header (6 bytes)
    // 'GIF'
    if ( !CheckPattern( buf, buf_size ) )
        return 0;
    if (buf[0]!='G' || buf[1]!='I' || buf[2]!='F')
        return 0;
    // version: '87a' or '89a'
    if (buf[3]!='8' || buf[5]!='a')
        return 0;
    if (buf[4]=='7')
        m_version = 7;
    else if (buf[4]=='9')
        m_version = 9;
    else
        return 0; // bad version

    // read screen descriptor
    unsigned char * p = buf+6;

    _width = p[0] + (p[1]<<8);
    _height = p[2] + (p[3]<<8);
    m_bpp = (p[4]&7)+1;
    m_flg_gtc = (p[4]&0x80)?1:0;
    m_transparent_color = p[5];

    if ( !(_width>=1 && _height>=1 && _width<4096 && _height<4096 ) )
        return false;
    if ( !callback )
        return true;
    // next
    p+=7;


    // read global color table
    if (m_flg_gtc) {
        int m_color_count = 1<<m_bpp;

        if (m_color_count*3 + (p-buf) >= buf_size)
            return 0; // error

        m_global_color_table = new lUInt32[m_color_count];
        for (int i=0; i<m_color_count; i++) {
            //m_global_color_table[i] = RGB(p[i*3],p[i*3+1],p[i*3+2]);
            m_global_color_table[i] = lRGB(p[i*3+2],p[i*3+1],p[i*3+0]);
        }

        // next
        p+=(m_color_count * 3);
    }

    bool res = false;
    if (p - buf < buf_size ) {
        // search for delimiter char ','
        while (*p != ',' && p-buf<buf_size)
            p++;
        if (*p==',') {
            // found image descriptor!
            LVGifFrame * pFrame = new LVGifFrame(this);
            int cbRead = 0;
            if (pFrame->DecodeFromBuffer(p, buf_size-(p-buf), cbRead) ) {
                res = true;
                pFrame->Draw( callback );
            }
            delete pFrame;
        }
    }

    return res;
}

void LVGifImageSource::Clear()
{
    _width = 0;
    _height = 0;
    m_version = 0;
    m_bpp = 0;
    if (m_global_color_table) {
        delete[] m_global_color_table;
        m_global_color_table = NULL;
    }
    if (m_frame_count) {
        for (int i=0; i<m_frame_count; i++) {
            delete m_frames[i];
        }
        delete m_frames;//Looks like the delete[] operator should be used
        m_frames = NULL;
        m_frame_count = 0;
    }
}

#define LSWDECODER_MAX_TABLE_SIZE 4096
class CLZWDecoder
{
protected:

    // in_stream
    const unsigned char * p_in_stream;
    int          in_stream_size;
    int          in_bit_pos;

    // out_stream
    unsigned char * p_out_stream;
    int          out_stream_size;

    int  clearcode;
    int  eoicode;
    int  bits;
    int  lastadd;
    /* // old implementation
    unsigned char * * str_table;
    int             * str_size;
    */
    unsigned char str_table[LSWDECODER_MAX_TABLE_SIZE];
    unsigned char last_table[LSWDECODER_MAX_TABLE_SIZE];
    unsigned char rev_buf[LSWDECODER_MAX_TABLE_SIZE/2];
    short         str_nextchar[LSWDECODER_MAX_TABLE_SIZE];
    //int           str_size;
public:

    void SetInputStream (const unsigned char * p, int sz ) {
        p_in_stream = p;
        in_stream_size = sz;
        in_bit_pos = 0;
    };

    void SetOutputStream (unsigned char * p, int sz ) {
        p_out_stream = p;
        out_stream_size = sz;
    };

    int WriteOutChar( unsigned char b ) {
        if (--out_stream_size>=0) {
            *p_out_stream++ = b;
            return 1;
        } else {
            return 0;
        }

    };

    int WriteOutString( int code ) {
        int pos = 0;
        do {
            rev_buf[pos++] = str_table[code];
            code = str_nextchar[code];
        } while (code>=0);
        while (--pos>=0) {
            if (!WriteOutChar(rev_buf[pos]))
                return 0;
        }
        return 1;
    };

    void FillRestOfOutStream( unsigned char b ) {
        for (; out_stream_size>0; out_stream_size--) {
            *p_out_stream++ = b;
        }
    }

    int ReadInCode() {
        int code = (p_in_stream[0])+
            (p_in_stream[1]<<8)+
            (p_in_stream[2]<<16);
        code >>= in_bit_pos;
        code &= (1<<bits)-1;
        in_bit_pos += bits;
        if (in_bit_pos>8) {
            p_in_stream++;
            in_stream_size--;
            in_bit_pos -= 8;
            if (in_bit_pos>8) {
                p_in_stream++;
                in_stream_size--;
                in_bit_pos -= 8;
            }
        }
        if (in_stream_size<0)
            return -1;
        else
            return code;
    };

    int AddString( int OldCode, unsigned char NewChar ) {
        if (lastadd == LSWDECODER_MAX_TABLE_SIZE)
            return -1;
        if (lastadd == (1<<bits)-1) {
            // increase table size
            bits++;
            //ResizeTable(1<<bits);
        }

        str_table[lastadd] = NewChar;
        str_nextchar[lastadd] = OldCode;
        last_table[lastadd] = last_table[OldCode];


        lastadd++;
        return lastadd-1;
    };

    CLZWDecoder() {
        /* // ld implementation
        str_table = NULL;
        str_size = NULL;
        */
        lastadd=0;
    };

    void Clear() {
        /* // old implementation
        for (int i=0; i<lastadd; i++) {
            if (str_table[i])
                delete str_table[i];
        }
        */
        lastadd=0;
    };


    ~CLZWDecoder() {
        Clear();
    };

    void Init(int sizecode) {
        bits = sizecode + 1;
        // init table
        Clear();
        //ResizeTable(1<<bits);
        for (int i=(1<<sizecode)-1; i>=0; i--) {
            str_table[i] = i;
            last_table[i] = i;
            str_nextchar[i] = -1;
        }
        // init codes
        clearcode = (1<<sizecode);
        str_table[clearcode] = 0;
        str_nextchar[clearcode] = -1;
        eoicode = clearcode + 1;
        str_table[eoicode] = 0;
        str_nextchar[eoicode] = -1;
        //str_table[eoicode] = NULL;
        lastadd = eoicode + 1;
    };
    int  CodeExists(int code) {
        return (code<lastadd);
    };

    int  Decode( int init_code_size ) {

        int code, oldcode;

        Init( init_code_size );

        code = ReadInCode(); // == 256, ignore
        if (code<0 || code>lastadd)
            return 0;

        while (1) { // 3

            code = ReadInCode();
            if (code<0 || code>lastadd)
                return 0;

            if (!WriteOutString(code))
                return 0;

            while (1) { // 5

                oldcode = code;

                code = ReadInCode();
                if (code<0 || code>lastadd)
                    return 0;

                if (CodeExists(code)) {
                    if (code==eoicode)
                        return 1;
                    else if (code==clearcode)
                        break; // clear & goto 3

                    // write  code
                    if (!WriteOutString(code))
                        return 0;

                    // add  old + code[0]
                    if (AddString(oldcode, last_table[code])<0)
                        return 0; // table overflow


                } else {
                    // write  old + old[0]
                    if (!WriteOutString(oldcode))
                        return 0;
                    if (!WriteOutChar(last_table[oldcode]))
                        return 0;

                    // add  old + old[0]
                    if (AddString(oldcode, last_table[oldcode])<0)
                        return 0; // table overflow
                }
            }

            Init( init_code_size );
        }
    };
};

bool LVGifImageSource::Decode( LVImageDecoderCallback * callback )
{
    if ( _stream.isNull() )
        return false;
    lvsize_t sz = _stream->GetSize();
    if ( sz<32 || sz>0x80000 )
        return false; // wrong size
    lUInt8 * buf = new lUInt8[ sz ];
    lvsize_t bytesRead = 0;
    bool res = true;
    _stream->SetPos(0);
    if ( _stream->Read( buf, sz, &bytesRead )!=LVERR_OK || bytesRead!=sz )
        res = false;
    res = res && DecodeFromBuffer( buf, sz, callback );
    delete[] buf;
    return res;
}

int LVGifFrame::DecodeFromBuffer( unsigned char * buf, int buf_size, int &bytes_read )
{
    bytes_read = 0;
    unsigned char * p = buf;
    if (*p!=',' || buf_size<=10)
        return 0; // error: no delimiter
    p++;

    // read info
    m_left = p[0] + (p[1]<<8);
    m_top = p[2] + (p[3]<<8);
    m_cx = p[4] + (p[5]<<8);
    m_cy = p[6] + (p[7]<<8);

    if (m_cx<1 || m_cx>4096 ||
        m_cy<1 || m_cy>4096 ||
        m_left+m_cx>m_pImage->GetWidth() ||
        m_top+m_cy>m_pImage->GetHeight())
        return 0; // error: wrong size

    m_flg_ltc = (p[8]&0x80)?1:0;
    m_flg_interlaced = (p[8]&0x40)?1:0;
    m_bpp = (p[8]&0x7) + 1;

    if (m_bpp==1)
        m_bpp = m_pImage->m_bpp;
    else if (m_bpp!=m_pImage->m_bpp && !m_flg_ltc)
        return 0; // wrong color table

    // next
    p+=9;

    if (m_flg_ltc) {
        // read color table
        int m_color_count = 1<<m_bpp;

        if (m_color_count*3 + (p-buf) >= buf_size)
            return 0; // error

        m_local_color_table = new lUInt32[m_color_count];
        for (int i=0; i<m_color_count; i++) {
            m_local_color_table[i] = lRGB(p[i*3],p[i*3+1],p[i*3+2]);
        }
        // next
        p+=(m_color_count * 3);
    }

    // unpack image
    unsigned char * stream_buffer = NULL;
    int stream_buffer_size = 0;

    int size_code = *p++;

    // test raster stream size
    int i;
    int rest_buf_size = buf_size - (p-buf);
    for (i=0; i<rest_buf_size && p[i]; ) {
        // next block
        int block_size = p[i];
        stream_buffer_size += block_size;
        i+=block_size+1;
    }

    if (!stream_buffer_size || i>rest_buf_size)
        return 0; // error

    // set read bytes count
    bytes_read = (p-buf) + i;

    // create stream buffer
    stream_buffer = new unsigned char[stream_buffer_size+3];
    // copy data to stream buffer
    int sb_index = 0;
    for (i=0; p[i]; ) {
        // next block
        int block_size = p[i];
        for (int j=1; j<=block_size; j++) {
            stream_buffer[sb_index++] = p[i+j];
        }
        i+=block_size+1;
    }


    // create image buffer
    m_buffer = new unsigned char [m_cx*m_cy];

    // decode image to buffer
    CLZWDecoder decoder;
    decoder.SetInputStream( stream_buffer, stream_buffer_size );
    decoder.SetOutputStream( m_buffer, m_cx*m_cy );

    int res=0;

    if (decoder.Decode(size_code)) {
        // decoded Ok
        // fill rest with transparent color
        decoder.FillRestOfOutStream( m_pImage->m_transparent_color );
        res = 1;
    } else {
        // error
        delete[] m_buffer;
        m_buffer = NULL;
    }

    // cleanup
    delete[] stream_buffer;

    return res; // OK
}

LVGifFrame::LVGifFrame(LVGifImageSource * pImage)
{
    m_pImage = pImage;
    m_left = 0;
    m_top = 0;
    m_cx = 0;
    m_cy = 0;
    m_flg_ltc = 0; // GTC (gobal table of colors) flag
    m_local_color_table = NULL;
}

LVGifFrame::~LVGifFrame()
{
    Clear();
}

void LVGifFrame::Clear()
{
    if (m_buffer) {
        delete[] m_buffer;
        m_buffer = NULL;
    }
    if (m_local_color_table) {
        delete[] m_local_color_table;
        m_local_color_table = NULL;
    }
}

#endif
// ======= end of GIF support



LVImageDecoderCallback::~LVImageDecoderCallback()
{
}


/// dummy image object, to show invalid image
LVImageSourceRef LVCreateDummyImageSource( ldomNode * node, int width, int height )
{
    return LVImageSourceRef( new LVDummyImageSource( node, width, height ) );
}

LVImageSourceRef LVCreateStreamImageSource( ldomNode * node, LVStreamRef stream )
{
    LVImageSourceRef ref;
    if ( stream.isNull() )
        return ref;
    lUInt8 hdr[256];
    lvsize_t bytesRead = 0;
    if ( stream->Read( hdr, 256, &bytesRead )!=LVERR_OK )
        return ref;
    stream->SetPos( 0 );


    LVImageSource * img = NULL;
#if (USE_LIBPNG==1)
    if ( LVPngImageSource::CheckPattern( hdr, (lUInt32)bytesRead ) )
        img = new LVPngImageSource( node, stream );
    else
#endif
#if (USE_LIBJPEG==1)
    if ( LVJpegImageSource::CheckPattern( hdr, (lUInt32)bytesRead ) )
        img = new LVJpegImageSource( node, stream );
    else
#endif
#if (USE_GIF==1)
    if ( LVGifImageSource::CheckPattern( hdr, (lUInt32)bytesRead ) )
        img = new LVGifImageSource( node, stream );
    else
#endif
        img = new LVDummyImageSource( node, 50, 50 );
    if ( !img )
        return ref;
    ref = LVImageSourceRef( img );
    if ( !img->Decode( NULL ) )
    {
        return LVImageSourceRef();
    }
    return ref;
}

LVImageSourceRef LVCreateStreamImageSource( LVStreamRef stream )
{
    return LVCreateStreamImageSource( NULL, stream );
}

/// create image from node source
LVImageSourceRef LVCreateNodeImageSource( ldomNode * node )
{
    LVImageSourceRef ref;
    if (!node->isElement())
        return ref;
    LVStreamRef stream = node->createBase64Stream();
    if (stream.isNull())
        return ref;
//    if ( CRLog::isDebugEnabled() ) {
//        lUInt16 attr_id = node->getDocument()->getAttrNameIndex(L"id");
//        lString16 id = node->getAttributeValue(attr_id);
//        CRLog::debug("Opening node image id=%s", LCSTR(id));
//    }
    return LVCreateStreamImageSource( stream );
}

/// creates image source as memory copy of file contents
LVImageSourceRef LVCreateFileCopyImageSource( lString16 fname )
{
    return LVCreateStreamImageSource( LVCreateMemoryStream(fname) );
}

/// creates image source as memory copy of stream contents
LVImageSourceRef LVCreateStreamCopyImageSource( LVStreamRef stream )
{
    if ( stream.isNull() )
        return LVImageSourceRef();
    return LVCreateStreamImageSource( LVCreateMemoryStream(stream) );
}

class LVStretchImgSource : public LVImageSource, public LVImageDecoderCallback
{
protected:
	LVImageSourceRef _src;
	int _src_dx;
	int _src_dy;
	int _dst_dx;
	int _dst_dy;
    ImageTransform _hTransform;
    ImageTransform _vTransform;
	int _split_x;
	int _split_y;
	LVArray<lUInt32> _line;
	LVImageDecoderCallback * _callback;
public:
    LVStretchImgSource( LVImageSourceRef src, int newWidth, int newHeight, ImageTransform hTransform, ImageTransform vTransform, int splitX, int splitY )
		: _src( src )
		, _src_dx( src->GetWidth() )
		, _src_dy( src->GetHeight() )
		, _dst_dx( newWidth )
		, _dst_dy( newHeight )
        , _hTransform(hTransform)
        , _vTransform(vTransform)
		, _split_x( splitX )
		, _split_y( splitY )
	{
        if ( _hTransform == IMG_TRANSFORM_TILE )
            if ( _split_x>=_src_dx )
                _split_x %=_src_dx;
        if ( _vTransform == IMG_TRANSFORM_TILE )
            if ( _split_y>=_src_dy )
                _split_y %=_src_dy;
        if ( _split_x<0 || _split_x>=_src_dx )
			_split_x = _src_dx / 2;
		if ( _split_y<0 || _split_y>=_src_dy )
			_split_y = _src_dy / 2;
	}
    virtual void OnStartDecode( LVImageSource * )
	{
		_line.reserve( _dst_dx );
        _callback->OnStartDecode(this);
	}
    virtual bool OnLineDecoded( LVImageSource * obj, int y, lUInt32 * data );
    virtual void OnEndDecode( LVImageSource *, bool res)
	{
		_line.clear();
        _callback->OnEndDecode(this, res);
    }
	virtual ldomNode * GetSourceNode() { return NULL; }
	virtual LVStream * GetSourceStream() { return NULL; }
	virtual void   Compact() { }
	virtual int    GetWidth() { return _dst_dx; }
	virtual int    GetHeight() { return _dst_dy; }
    virtual bool   Decode( LVImageDecoderCallback * callback )
	{
		_callback = callback;
		return _src->Decode( this );
	}
    virtual ~LVStretchImgSource()
	{
	}
};

bool LVStretchImgSource::OnLineDecoded( LVImageSource * obj, int y, lUInt32 * data )
{
    bool res = false;

    switch ( _hTransform ) {
    case IMG_TRANSFORM_SPLIT:
        {
            int right_pixels = (_src_dx-_split_x-1);
            int first_right_pixel = _dst_dx - right_pixels;
            int right_offset = _src_dx - _dst_dx;
            //int bottom_pixels = (_src_dy-_split_y-1);
            //int first_bottom_pixel = _dst_dy - bottom_pixels;
            for ( int x=0; x<_dst_dx; x++ ) {
                if ( x<_split_x )
                    _line[x] = data[x];
                else if ( x < first_right_pixel )
                    _line[x] = data[_split_x];
                else
                    _line[x] = data[x + right_offset];
            }
        }
        break;
    case IMG_TRANSFORM_STRETCH:
        {
            for ( int x=0; x<_dst_dx; x++ )
                _line[x] = data[x * _src_dx / _dst_dx];
        }
        break;
    case IMG_TRANSFORM_NONE:
        {
            for ( int x=0; x<_dst_dx && x<_src_dx; x++ )
                _line[x] = data[x];
        }
        break;
    case IMG_TRANSFORM_TILE:
        {
            int offset = _src_dx - _split_x;
            for ( int x=0; x<_dst_dx; x++ )
                _line[x] = data[ (x + offset) % _src_dx];
        }
        break;
    }

    switch ( _vTransform ) {
    case IMG_TRANSFORM_SPLIT:
        {
            int middle_pixels = _dst_dy - _src_dy + 1;
            if ( y < _split_y ) {
                res = _callback->OnLineDecoded( obj, y, _line.get() );
            } else if ( y==_split_y ) {
                for ( int i=0; i < middle_pixels; i++ ) {
                    res = _callback->OnLineDecoded( obj, y+i, _line.get() );
                }
            } else {
                res = _callback->OnLineDecoded( obj, y + (_dst_dy - _src_dy), _line.get() );
            }
        }
        break;
    case IMG_TRANSFORM_STRETCH:
        {
            int y0 = y * _dst_dy / _src_dy;
            int y1 = (y+1) * _dst_dy / _src_dy;
            for ( int yy=y0; yy<y1; yy++ ) {
                res = _callback->OnLineDecoded( obj, yy, _line.get() );
            }
        }
        break;
    case IMG_TRANSFORM_NONE:
        {
            if ( y<_dst_dy )
                res = _callback->OnLineDecoded( obj, y, _line.get() );
        }
        break;
    case IMG_TRANSFORM_TILE:
        {
            int offset = _src_dy - _split_y;
            int y0 = (y + offset) % _src_dy;
            for ( int yy=y0; yy<_dst_dy; yy+=_src_dy ) {
                res = _callback->OnLineDecoded( obj, yy, _line.get() );
            }
        }
        break;
    }

    return res;
}

/// creates image which stretches source image by filling center with pixels at splitX, splitY
LVImageSourceRef LVCreateStretchFilledTransform( LVImageSourceRef src, int newWidth, int newHeight, ImageTransform hTransform, ImageTransform vTransform, int splitX, int splitY )
{
	if ( src.isNull() )
		return LVImageSourceRef();
    return LVImageSourceRef( new LVStretchImgSource( src, newWidth, newHeight, hTransform, vTransform, splitX, splitY ) );
}

/// creates image which fills area with tiled copy
LVImageSourceRef LVCreateTileTransform( LVImageSourceRef src, int newWidth, int newHeight, int offsetX, int offsetY )
{
    if ( src.isNull() )
        return LVImageSourceRef();
    return LVImageSourceRef( new LVStretchImgSource( src, newWidth, newHeight, IMG_TRANSFORM_TILE, IMG_TRANSFORM_TILE,
                                                     offsetX, offsetY ) );
}

class LVUnpackedImgSource : public LVImageSource, public LVImageDecoderCallback
{
protected:
    bool _isGray;
    int _bpp;
    lUInt8 * _grayImage;
    lUInt32 * _colorImage;
    lUInt16 * _colorImage16;
    int _dx;
    int _dy;
public:
    LVUnpackedImgSource( LVImageSourceRef src, int bpp )
        : _isGray(bpp<=8)
        , _bpp(bpp)
        , _grayImage(NULL)
        , _colorImage(NULL)
        , _colorImage16(NULL)
        , _dx( src->GetWidth() )
        , _dy( src->GetHeight() )
    {
        if ( bpp<=8  ) {
            _grayImage = (lUInt8*)malloc( _dx * _dy * sizeof(lUInt8) );
        } else if ( bpp==16 ) {
            _colorImage16 = (lUInt16*)malloc( _dx * _dy * sizeof(lUInt16) );
        } else {
            _colorImage = (lUInt32*)malloc( _dx * _dy * sizeof(lUInt32) );
        }
        src->Decode( this );
    }
    virtual void OnStartDecode( LVImageSource * )
    {
        //CRLog::trace( "LVUnpackedImgSource::OnStartDecode" );
    }
    // aaaaaaaarrrrrrrrggggggggbbbbbbbb -> yyyyyyaa
    inline lUInt8 grayPack( lUInt32 pixel )
    {
        lUInt8 gray = (lUInt8)(( (pixel & 255) + ((pixel>>16) & 255) + ((pixel>>7)&510) ) >> 2);
        lUInt8 alpha = (lUInt8)((pixel>>24) & 255);
        return (gray & 0xFC) | ((alpha >> 6) & 3);
    }
    // yyyyyyaa -> aaaaaaaarrrrrrrrggggggggbbbbbbbb
    inline lUInt32 grayUnpack( lUInt8 pixel )
    {
        lUInt32 gray = pixel & 0xFC;
        lUInt32 alpha = (pixel & 3) << 6;
        if ( alpha==0xC0 )
            alpha = 0xFF;
        return gray | (gray<<8) | (gray<<16) | (alpha<<24);
    }
    virtual bool OnLineDecoded( LVImageSource *, int y, lUInt32 * data )
    {
        if ( y<0 || y>=_dy )
            return false;
        if ( _isGray ) {
            lUInt8 * dst = _grayImage + _dx * y;
            for ( int x=0; x<_dx; x++ ) {
                dst[x] = grayPack( data[x] );
            }
        } else if ( _bpp==16 ) {
            lUInt16 * dst = _colorImage16 + _dx * y;
            for ( int x=0; x<_dx; x++ ) {
                dst[x] = rgb888to565( data[x] );
            }
        } else {
            lUInt32 * dst = _colorImage + _dx * y;
            memcpy( dst, data, sizeof(lUInt32) * _dx );
        }
        return true;
    }
    virtual void OnEndDecode( LVImageSource *, bool )
    {
        //CRLog::trace( "LVUnpackedImgSource::OnEndDecode" );
    }
    virtual ldomNode * GetSourceNode() { return NULL; }
    virtual LVStream * GetSourceStream() { return NULL; }
    virtual void   Compact() { }
    virtual int    GetWidth() { return _dx; }
    virtual int    GetHeight() { return _dy; }
    virtual bool   Decode( LVImageDecoderCallback * callback )
    {
        callback->OnStartDecode( this );
        bool res = false;
        if ( _isGray ) {
            // gray
            LVArray<lUInt32> line;
            line.reserve( _dx );
            for ( int y=0; y<_dy; y++ ) {
                lUInt8 * src = _grayImage + _dx * y;
                lUInt32 * dst = line.ptr();
                for ( int x=0; x<_dx; x++ )
                    dst[x] = grayUnpack( src[x] );
                callback->OnLineDecoded( this, y, dst );
            }
            line.clear();
        } else if ( _bpp==16 ) {
            // 16bit
            LVArray<lUInt32> line;
            line.reserve( _dx );
            for ( int y=0; y<_dy; y++ ) {
                lUInt16 * src = _colorImage16 + _dx * y;
                lUInt32 * dst = line.ptr();
                for ( int x=0; x<_dx; x++ )
                    dst[x] = rgb565to888( src[x] );
                callback->OnLineDecoded( this, y, dst );
            }
            line.clear();
        } else {
            // color
            for ( int y=0; y<_dy; y++ ) {
                res = callback->OnLineDecoded( this, y, _colorImage + _dx * y );
            }
        }
        callback->OnEndDecode( this, false );
        return true;
    }
    virtual ~LVUnpackedImgSource()
    {
        if ( _grayImage )
            free( _grayImage );
        if ( _colorImage )
            free( _colorImage );
        if ( _colorImage )
            free( _colorImage16 );
    }
};

class LVDrawBufImgSource : public LVImageSource
{
protected:
    LVColorDrawBuf * _buf;
    bool _own;
    int _dx;
    int _dy;
public:
    LVDrawBufImgSource( LVColorDrawBuf * buf, bool own )
        : _buf(buf)
        , _own(own)
        , _dx( buf->GetWidth() )
        , _dy( buf->GetHeight() )
    {
    }
    virtual ldomNode * GetSourceNode() { return NULL; }
    virtual LVStream * GetSourceStream() { return NULL; }
    virtual void   Compact() { }
    virtual int    GetWidth() { return _dx; }
    virtual int    GetHeight() { return _dy; }
    virtual bool   Decode( LVImageDecoderCallback * callback )
    {
        callback->OnStartDecode( this );
        bool res = false;
        if ( _buf->GetBitsPerPixel()==32 ) {
            // 32 bpp
            for ( int y=0; y<_dy; y++ ) {
                res = callback->OnLineDecoded( this, y, (lUInt32 *)_buf->GetScanLine(y) );
            }
        } else {
            // 16 bpp
            lUInt32 * row = new lUInt32[_dx];
            for ( int y=0; y<_dy; y++ ) {
                lUInt16 * src = (lUInt16 *)_buf->GetScanLine(y);
                for ( int x=0; x<_dx; x++ )
                    row[x] = rgb565to888(src[x]);
                res = callback->OnLineDecoded( this, y, row );
            }
            delete row;
        }
        callback->OnEndDecode( this, false );
        return true;
    }
    virtual ~LVDrawBufImgSource()
    {
        if ( _own )
            delete _buf;
    }
};

/// creates decoded memory copy of image, if it's unpacked size is less than maxSize
LVImageSourceRef LVCreateUnpackedImageSource( LVImageSourceRef srcImage, int maxSize, bool gray )
{
    if ( srcImage.isNull() )
        return srcImage;
    int dx = srcImage->GetWidth();
    int dy = srcImage->GetHeight();
    int sz = dx*dy * (gray?1:4);
    if ( sz>maxSize )
        return srcImage;
    CRLog::trace("Unpacking image %dx%d (%d)", dx, dy, sz);
    LVUnpackedImgSource * img = new LVUnpackedImgSource( srcImage, gray ? 8 : 32 );
    CRLog::trace("Unpacking done");
    return LVImageSourceRef( img );
}

/// creates decoded memory copy of image, if it's unpacked size is less than maxSize
LVImageSourceRef LVCreateUnpackedImageSource( LVImageSourceRef srcImage, int maxSize, int bpp )
{
    if ( srcImage.isNull() )
        return srcImage;
    int dx = srcImage->GetWidth();
    int dy = srcImage->GetHeight();
    int sz = dx*dy * (bpp>>3);
    if ( sz>maxSize )
        return srcImage;
    CRLog::trace("Unpacking image %dx%d (%d)", dx, dy, sz);
    LVUnpackedImgSource * img = new LVUnpackedImgSource( srcImage, bpp );
    CRLog::trace("Unpacking done");
    return LVImageSourceRef( img );
}

LVImageSourceRef LVCreateDrawBufImageSource( LVColorDrawBuf * buf, bool own )
{
    return LVImageSourceRef( new LVDrawBufImgSource( buf, own ) );
}


/// draws battery icon in specified rectangle of draw buffer; if font is specified, draws charge %
// first icon is for charging, the rest - indicate progress icon[1] is lowest level, icon[n-1] is full power
// if no icons provided, battery will be drawn
void LVDrawBatteryIcon( LVDrawBuf * drawbuf, const lvRect & batteryRc, int percent, bool charging, LVRefVec<LVImageSource> icons, LVFont * font )
{
    lvRect rc( batteryRc );
    bool drawText = (font != NULL);
    if ( icons.length()>1 ) {
        int iconIndex = 0;
        if ( !charging ) {
            if ( icons.length()>2 ) {
                int numTicks = icons.length() - 1;
                iconIndex = ((numTicks - 1) * percent + (100/numTicks/2) )/ 100 + 1;
                if ( iconIndex<1 )
                    iconIndex = 1;
                if ( iconIndex>icons.length()-1 )
                    iconIndex = icons.length()-1;
            } else {
                // empty battery icon, for % display
                iconIndex = 1;
            }
        }

        lvPoint sz( icons[0]->GetWidth(), icons[0]->GetHeight() );
        rc.left += (rc.width() - sz.x)/2;
        rc.top += (rc.height() - sz.y)/2;
        rc.right = rc.left + sz.x;
        rc.bottom = rc.top + sz.y;
        LVImageSourceRef icon = icons[iconIndex];
        drawbuf->Draw( icon, rc.left,
            rc.top,
            sz.x,
            sz.y, false );
        if ( charging )
            drawText = false;
        rc.left += 3;
    } else {
        // todo: draw w/o icons
    }

    if ( drawText ) {
        // rc is rectangle to draw text to
        lString16 txt;
        if ( charging )
            txt = L"+++";
        else
            txt = lString16::itoa(percent); // + L"%";
        int w = font->getTextWidth(txt.c_str(), txt.length());
        int h = font->getHeight();
        int x = (rc.left + rc.right - w)/2;
        int y = (rc.top + rc.bottom - h)/2+1;
        lUInt32 bgcolor = drawbuf->GetBackgroundColor();
        lUInt32 textcolor = drawbuf->GetTextColor();
        drawbuf->SetBackgroundColor( bgcolor );
        drawbuf->SetTextColor( textcolor );
        font->DrawTextString(drawbuf, x-1, y, txt.c_str(), txt.length(), '?', NULL);
        font->DrawTextString(drawbuf, x+1, y, txt.c_str(), txt.length(), '?', NULL);
//        font->DrawTextString(drawbuf, x-1, y+1, txt.c_str(), txt.length(), '?', NULL);
//        font->DrawTextString(drawbuf, x+1, y-1, txt.c_str(), txt.length(), '?', NULL);
        font->DrawTextString(drawbuf, x, y-1, txt.c_str(), txt.length(), '?', NULL);
        font->DrawTextString(drawbuf, x, y+1, txt.c_str(), txt.length(), '?', NULL);
//        font->DrawTextString(drawbuf, x+1, y+1, txt.c_str(), txt.length(), '?', NULL);
//        font->DrawTextString(drawbuf, x-1, y+1, txt.c_str(), txt.length(), '?', NULL);
        //drawbuf->SetBackgroundColor( textcolor );
        //drawbuf->SetTextColor( bgcolor );
        drawbuf->SetBackgroundColor( textcolor );
        drawbuf->SetTextColor( bgcolor );
        font->DrawTextString(drawbuf, x, y, txt.c_str(), txt.length(), '?', NULL);
    }
}
