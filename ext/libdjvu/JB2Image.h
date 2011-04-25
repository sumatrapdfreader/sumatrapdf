//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifndef _JB2IMAGE_H
#define _JB2IMAGE_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

/** @name JB2Image.h

    Files #"JB2Image.h"# and #"JB2Image.cpp"# address the compression of
    bilevel images using the JB2 soft pattern matching scheme.  These files
    provide the complete decoder and the decoder back-end.  The JB2 scheme is
    optimized for images containing a large number of self-similar small
    components such as characters.  Typical text images can be compressed into
    files 3 to 5 times smaller than with G4/MMR and 2 to 4 times smaller than
    with JBIG1.

    {\bf JB2 and JBIG2} --- JB2 has strong similarities with the forthcoming
    JBIG2 standard developed by the "ISO/IEC JTC1 SC29 Working Group 1" which
    is responsible for both the JPEG and JBIG standards.  This is hardly
    surprising since JB2 was our own proposal for the JBIG2 standard
    and remained the only proposal for years.  The full JBIG2 standard however
    is significantly more complex and slighlty less efficient than JB2 because
    it addresses a broader range of applications.  Full JBIG2 compliance may
    be implemented in the future.

    {\bf JB2 Images} --- Class \Ref{JB2Image} is the central data structure
    implemented here.  A #JB2Image# is composed of an array of shapes
    and an array of blits.  Each shape contains a small bitmap representing an
    elementary blob of ink, such as a character or a segment of line art.
    Each blit instructs the decoder to render a particular shape at a
    specified position in the image.  Some compression is already achieved
    because several blits can refer to the same shape.  A shape can also
    contain a pointer to a parent shape.  Additional compression is achieved
    when both shapes are similar because each shape is encoded using the
    parent shape as a model.  A #"O"# shape for instance could be a parent for
    both a #"C"# shape and a #"Q"# shape.

    {\bf JB2 Dictionary} --- Class \Ref{JB2Dict} is a peculiar kind of
    JB2Image which only contains an array of shapes.  These shapes can be
    referenced from another JB2Dict/JB2Image.  This is arranged by setting the
    ``inherited dictionary'' of a JB2Dict/JB2Image using function
    \Ref{JB2Dict::set_inherited_dict}. Several JB2Images can use shapes from a
    same JB2Dict encoded separately.  This is how several pages of a same
    document can share information.
    
    {\bf Decoding JB2 data} --- The first step for decoding JB2 data consists of 
    creating an empty #JB2Image# object.  Function \Ref{JB2Image::decode} then
    reads the data and populates the #JB2Image# with the shapes and the blits.
    Function \Ref{JB2Image::get_bitmap} finally produces an anti-aliased image.

    {\bf Encoding JB2 data} --- The first step for decoding JB2 data also
    consists of creating an empty #JB2Image# object.  You must then use
    functions \Ref{JB2Image::add_shape} and \Ref{JB2Image::add_blit} to
    populate the #JB2Image# object.  Function \Ref{JB2Image::encode} finally
    produces the JB2 data.  Function #encode# sequentially encodes the blits
    and the necessary shapes.  The compression ratio depends on several
    factors:
    \begin{itemize}
    \item Blits should reuse shapes as often as possible.
    \item Blits should be sorted in reading order because this facilitates
          the prediction of the blit coordinates.
    \item Shapes should be sorted according to the order of first appearance
          in the sequence of blits because this facilitates the prediction of the
          shape indices.
    \item Shapes should be compared to all previous shapes in the shape array.
          The shape parent pointer should be set to a suitable parent shape if
          such a parent shape exists.  The parent shape should have almost the
          same size and the same pixels.
    \end{itemize}
    All this is quite easy to achieve in the case of an electronically
    produced document such as a DVI file or a PS file: we know what the
    characters are and where they are located.  If you only have a scanned
    image however you must first locate the characters (connected component
    analysis) and cut the remaining pieces of ink into smaller blobs.
    Ordering the blits and matching the shapes is then an essentially
    heuristic process.  Although the quality of the heuristics substantially
    effects the file size, misordering blits or mismatching shapes never
    effects the quality of the image.  The last refinement consists in
    smoothing the shapes in order to reduce the noise and maximize the
    similarities between shapes.
    
    {\bf JB2 extensions} --- Two extensions of the JB2
    encoding format have been introduced with DjVu files version 21. The first
    extension addresses the shared shape dictionaries. The second extension
    bounds the number of probability contexts used for coding numbers.
    Both extensions maintain backward compatibility with JB2 as 
    described in the ICFDD proposal. A more complete discussion
    can be found in section \Ref{JB2 extensions for version 21.}.

    {\bf References} 
    \begin{itemize}
    \item Paul G. Howard : {\em Text image compression using soft 
          pattern matching}, Computer Journal, volume 40:2/3, 1997.
    \item JBIG1 : \URL{http://www.jpeg.org/public/jbighomepage.htm}.
    \item JBIG2 draft : \URL{http://www.jpeg.org/public/jbigpt2.htm}.
    \item ICFDD Draft Proposed American National Standard, 1999-08-26.
    \end{itemize}

    @memo
    Coding bilevel images with JB2.
    @author
    Paul Howard <pgh@research.att.com> -- JB2 design\\
    L\'eon Bottou <leonb@research.att.com> -- this implementation 

// From: Leon Bottou, 1/31/2002
// Lizardtech has split the corresponding cpp file into a decoder and an encoder.
// Only superficial changes.  The meat is mine.

*/
//@{


#include "GString.h"
#include "ZPCodec.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class JB2Dict;
class JB2Image;
class GRect;
class GBitmap;
class ByteStream;

/** Blit data structure.  A #JB2Image# contains an array of #JB2Blit# data
    structures.  Each array entry instructs the decoder to render a particular
    shape at a particular location.  Members #left# and #bottom# specify the
    coordinates of the bottom left corner of the shape bitmap.  All
    coordinates are relative to the bottom left corner of the image.  Member
    #shapeno# is the subscript of the shape to be rendered.  */

class DJVUAPI JB2Blit {
public:
  /** Horizontal coordinate of the blit. */
  unsigned short left;
  /** Vertical coordinate of the blit. */
  unsigned short bottom;
  /** Index of the shape to blit. */
  unsigned int shapeno;
};


/** Shape data structure.  A #JB2Image# contains an array of #JB2Shape# data
    structures.  Each array entry represents an elementary blob of ink such as
    a character or a segment of line art.  Member #bits# points to a bilevel
    image representing the shape pixels.  Member #parent# is the subscript of
    the parent shape.  */

class DJVUAPI JB2Shape
{ 
public: 
  /** Subscript of the parent shape.  The parent shape must always be located
      before the current shape in the shape array.  A negative value indicates
      that this shape has no parent.  Any negative values smaller than #-1#
      further indicates that this shape does not look like a character.  This
      is used to enable a few internal optimizations.  This information is
      saved into the JB2 file, but the actual value of the #parent# variable
      is not. */
  int parent; 
  /** Bilevel image of the shape pixels.  This must be a pointer to a bilevel
      #GBitmap# image.  This pointer can also be null. The encoder will just
      silently discard all blits referring to a shape containing a null
      bitmap. */
  GP<GBitmap> bits;
  /** Private user data. This long word is provided as a convenience for users
      of the JB2Image data structures.  Neither the rendering functions nor
      the coding functions ever access this value. */
  long userdata;
};



/** JB2 Dictionary callback.
    The decoding function call this callback function when they discover that
    the current JB2Image or JB2Dict needs a pre-existing shape dictionary. 
    The callback function must return a pointer to the dictionary or NULL
    if none is found. */

typedef GP<JB2Dict> JB2DecoderCallback ( void* );


/** Dictionary of JB2 shapes. */

class DJVUAPI JB2Dict : public GPEnabled
{
protected:
  JB2Dict(void);
public:
  class JB2Codec;

  // CONSTRUCTION
  /** Default creator.  Constructs an empty #JB2Dict# object.  You can then
      call the decoding function #decode#.  You can also manually set the
      image size using #add_shape#. */
  static GP<JB2Dict> create(void);

  // INITIALIZATION
  /** Resets the #JB2Image# object.  This function reinitializes both the shape
     and the blit arrays.  All allocated memory is freed. */
  void init(void);

  // INHERITED
  /** Returns the inherited dictionary. */
  GP<JB2Dict> get_inherited_dict(void) const;
  /** Returns the number of inherited shapes. */
  int get_inherited_shape_count(void) const;
  /** Sets the inherited dictionary. */
  void set_inherited_dict(const GP<JB2Dict> &dict);

  // ACCESSING THE SHAPE LIBRARY
  /** Returns the total number of shapes.
      Shape indices range from #0# to #get_shape_count()-1#. */
  int get_shape_count(void) const;
  /** Returns a pointer to shape #shapeno#.
      The returned pointer directly points into the shape array.
      This pointer can be used for reading or writing the shape data. */
  JB2Shape &get_shape(const int shapeno);
  /** Returns a constant pointer to shape #shapeno#.
      The returned pointer directly points into the shape array.
      This pointer can only be used for reading the shape data. */
  const JB2Shape &get_shape(const int shapeno) const;
  /** Appends a shape to the shape array.  This function appends a copy of
      shape #shape# to the shape array and returns the subscript of the new
      shape.  The subscript of the parent shape #shape.parent# must 
      actually designate an already existing shape. */
  int  add_shape(const JB2Shape &shape);

  // MEMORY OPTIMIZATION
  /** Compresses all shape bitmaps.  This function reduces the memory required
      by the #JB2Image# by calling \Ref{GBitmap::compress} on all shapes
      bitmaps.  This function is best called after decoding a #JB2Image#,
      because function \Ref{get_bitmap} can directly use the compressed
      bitmaps.  */
  void compress(void);
  /** Returns the total memory used by the JB2Image.
      The returned value is expressed in bytes. */
  unsigned int get_memory_usage(void) const;

  // CODING
  /** Encodes the JB2Dict into ByteStream #bs#.  
      This function generates the JB2 data stream without any header.   */
  void encode(const GP<ByteStream> &gbs) const;
  /** Decodes JB2 data from ByteStream #bs#. This function decodes the image
      size and populates the shape and blit arrays.  The callback function
      #cb# is called when the decoder determines that the ByteStream data
      requires a shape dictionary which has not been set with
      \Ref{JB2Dict::set_inherited_dict}. The callback receives argument #arg#
      and must return a suitable dictionary which will be installed as the
      inherited dictionary.  The callback should return null if no such
      dictionary is found. */
  void decode(const GP<ByteStream> &gbs, JB2DecoderCallback *cb=0, void *arg=0);

  
public:
  /** Comment string coded by JB2 file. */
  GUTF8String comment;


private:
  friend class JB2Codec;
  int inherited_shapes;
  GP<JB2Dict> inherited_dict;
  GArray<JB2Shape> shapes;

  struct LibRect {
    int top,left,right,bottom;
    void compute_bounding_box(const GBitmap &cbm);
  };
  GTArray<LibRect> boxes;
  void get_bounding_box(int shapeno, LibRect &dest);
};

/** Main JB2 data structure.  Each #JB2Image# consists of an array of shapes
    and an array of blits.  These arrays can be populated by hand using
    functions \Ref{add_shape} and \Ref{add_blit}, or by decoding JB2 data
    using function \Ref{decode}.  You can then use function \Ref{get_bitmap}
    to render anti-aliased images, or use function \Ref{encode} to generate
    JB2 data. */

class DJVUAPI JB2Image : public JB2Dict
{
protected:
  JB2Image(void);
public:

  /** Creates an empty #JB2Image# object.  You can then
      call the decoding function #decode#.  You can also manually set the
      image size using #set_dimension# and populate the shape and blit arrays
      using #add_shape# and #add_blit#. */
  static GP<JB2Image> create(void) { return new JB2Image(); }

  // INITIALIZATION
  /** Resets the #JB2Image# object.  This function reinitializes both the shape
     and the blit arrays.  All allocated memory is freed. */
  void init(void);

  // DIMENSION
  /** Returns the width of the image.  
      This is the width value previously set with #set_dimension#. */
  int get_width(void) const;
  /** Returns the height of the image.  
      This is the height value previously set with #set_dimension#. */
  int get_height(void) const;
  /** Sets the size of the JB2Image.
      This function can be called at any time. 
      The corresponding #width# and the #height# are stored
      in the JB2 file. */
  void set_dimension(int width, int height);

  // RENDERING
  /** Renders an anti-aliased gray level image.  This function renders the
      JB2Image as a bilevel or gray level image.  Argument #subsample#
      specifies the desired subsampling ratio in range #1# to #15#.  The
      returned image uses #1+subsample^2# gray levels for representing
      anti-aliased edges.  Argument #align# specified the alignment of the
      rows of the returned images.  Setting #align# to #4#, for instance, will
      adjust the bitmap border in order to make sure that each row of the
      returned image starts on a word (four byte) boundary. */
  GP<GBitmap> get_bitmap(int subsample = 1, int align = 1) const;
  /** Renders an anti-aliased gray level sub-image.  This function renders a
      segment of the JB2Image as a bilevel or gray level image.  Conceptually,
      this function first renders the full JB2Image with subsampling ratio
      #subsample# and then extracts rectangle #rect# in the subsampled image.
      Both operations of course are efficiently performed simultaneously.
      Argument #align# specified the alignment of the rows of the returned
      images, as explained above.  Argument #dispy# should remain null. */
  GP<GBitmap> get_bitmap(const GRect &rect, int subsample=1, int align=1, int dispy=0) const;

  // ACCESSING THE BLIT LIBRARY
  /** Returns the total number of blits.
      Blit indices range from #0# to #get_blit_count(void)-1#. */
  int get_blit_count(void) const;
  /** Returns a pointer to blit #blitno#.
      The returned pointer directly points into the blit array.
      This pointer can be used for reading or writing the blit data. */
  JB2Blit *get_blit(int blitno);
  /** Returns a constant pointer to blit #blitno#.
      The returned pointer directly points into the shape array.
      This pointer can only be used for reading the shape data. */
  const JB2Blit *get_blit(int blitno) const;
  /** Appends a blit to the blit array.  This function appends a copy of blit
      #blit# to the blit array and returns the subscript of the new blit.  The
      shape subscript #blit.shapeno# must actually designate an already
      existing shape. */
  int  add_blit(const JB2Blit &blit);

  // MEMORY OPTIMIZATION
  /** Returns the total memory used by the JB2Image.
      The returned value is expressed in bytes. */
  unsigned int get_memory_usage(void) const;

  // CODING
  /** Encodes the JB2Image into ByteStream #bs#.  
      This function generates the JB2 data stream without any header. */
  void encode(const GP<ByteStream> &gbs) const;
  /** Decodes JB2 data from ByteStream #bs#. This function decodes the image
      size and populates the shape and blit arrays.  The callback function
      #cb# is called when the decoder determines that the ByteStream data
      requires a shape dictionary which has not been set with
      \Ref{JB2Dict::set_inherited_dict}. The callback receives argument #arg#
      and must return a suitable dictionary which will be installed as the
      inherited dictionary.  The callback should return null if no such
      dictionary is found. */
  void decode(const GP<ByteStream> &gbs, JB2DecoderCallback *cb=0, void *arg=0);
  
private:
  // Implementation
  int width;
  int height;
  GTArray<JB2Blit> blits;
public:
  /** Reproduces a old bug.  Setting this flag may be necessary for accurately
      decoding DjVu files with version smaller than #18#.  The default value
      is of couse #false#. */
  bool reproduce_old_bug;
};



// JB2DICT INLINE FUNCTIONS

inline int
JB2Dict::get_shape_count(void) const
{
  return inherited_shapes + shapes.size();
}

inline int
JB2Dict::get_inherited_shape_count(void) const
{
  return inherited_shapes;
}

inline GP<JB2Dict>
JB2Dict::get_inherited_dict(void) const
{
  return inherited_dict;
}

// JB2IMAGE INLINE FUNCTIONS

inline int
JB2Image::get_width(void) const
{
  return width;
}

inline int
JB2Image::get_height(void) const
{
  return height;
}


inline int
JB2Image::get_blit_count(void) const
{
  return blits.size();
}


inline JB2Blit *
JB2Image::get_blit(int blitno)
{
  return & blits[blitno];
}

inline const JB2Blit *
JB2Image::get_blit(int blitno) const
{
  return & blits[blitno];
}





/** @name JB2 extensions for version 21.

    Two extensions of the JB2 encoding format have been introduced
    with DjVu files version 21.  Both extensions maintain significant
    backward compatibility with previous version of the JB2 format.
    These extensions are described below by reference to the ICFDD
    proposal dated August 1999.  Both extension make use of the unused
    record type value #9# (cf. ICFDD page 24) which has been renamed
    #REQUIRED_DICT_OR_RESET#.

    {\bf Shared Shape Dictionaries} --- This extension provides
    support for sharing symbol definitions between the pages of a
    document.  To achieve this objective, the JB2 image data chunk
    must be able to address symbols defined elsewhere by a JB2
    dictionary data chunk shared by all the pages of a document.

    The arithmetically encoded JB2 image data logically consist of a
    sequence of records. The decoder processes these records in
    sequence and maintains a library of symbols which can be addressed
    by the following records.  The first record usually is a ``Start
    Of Image'' record describing the size of the image.

    Starting with version 21, a #REQUIRED_DICT_OR_RESET# (9) record
    type can appear {\em before} the #START_OF_DATA# (0) record.  The
    record type field is followed by a single number arithmetically
    encoded (cf. ICFDD page 26) using a sixteenth context (cf. ICFDD
    page 25).  This record appears when the JB2 data chunk requires
    symbols encoded in a separate JB2 dictionary data chunk.  The
    number (the {\bf dictionary size}) indicates how many symbols
    should have been defined by the JB2 dictionary data chunk.  The
    decoder should simply load these symbols in the symbol library and
    proceed as usual.  New symbols potentially defined by the
    subsequent JB2 image data records will therefore be numbered with
    integers greater or equal than the dictionary size.

    The JB2 dictionary data format is a pure subset of the JB2 image
    data format.  The #START_OF_DATA# (0) record always specifies an
    image width of zero and an image height of zero.  The only allowed
    record types are those defining library symbols only
    (#NEW_SYMBOL_LIBRARY_ONLY# (2) and #MATCHED_REFINE_LIBRARY_ONLY#
    (5) cf. ICFDD page 24) followed by a final #END_OF_DATA# (11)
    record.

    The JB2 dictionary data is usually located in an {\bf Djbz} chunk.
    Each page {\bf FORM:DJVU} may directly contain a {\bf Djbz} chunk,
    or may indirectly point to such a chunk using an {\bf INCL} chunk
    (cf. \Ref{Multipage DjVu documents.}).
    

    {\bf Numcoder Reset} --- This extension addresses a problem for
    hardware implementations.  The encoding of numbers (cf. ICFDD page
    26) potentially uses an unbounded number of binary coding
    contexts. These contexts are normally allocated when they are used
    for the first time (cf. ICFDD informative note, page 27).

    Starting with version 21, a #REQUIRED_DICT_OR_RESET# (9) record
    type can appear {\em after} the #START_OF_DATA# (0) record.  The
    decoder should proceed with the next record after {\em clearing
    all binary contexts used for coding numbers}.  This operation
    implies that all binary contexts previously allocated for coding
    numbers can be deallocated.
  
    Starting with version 21, the JB2 encoder should insert a
    #REQUIRED_DICT_OR_RESET# record type whenever the number of these
    allocated binary contexts exceeds #20000#.  Only very large
    documents ever reach such a large number of allocated binary
    contexts (e.g large maps).  Hardware implementation however can
    benefit greatly from a hard bound on the total number of binary
    coding contexts.  Old JB2 decoders will treat this record type as
    an #END_OF_DATA# record and cleanly stop decoding (cf. ICFDD page
    30, Image refinement data).


    {\bf References} ---
    \begin{itemize}
    \item ICFDD Draft Proposed American National Standard, 1999-08-26.
    \item DjVu Specification, \URL{http://www.lizardtech.com/djvu/sci/djvuspec}.
    \end{itemize}

    @memo Extensions to the JB2 format introduced in version 21.  */

//@}

////////////////////////////////////////
//// CLASS JB2CODEC:  DECLARATION
////////////////////////////////////////

// This class is accessed via the encode and decode
// functions of class JB2Image


//**** Class JB2Codec
// This class implements the base class for both the JB2 coder and decoder.
// The JB2Codec's Contains all contextual information for encoding/decoding
// a JB2Image.

class JB2Dict::JB2Codec
{
public:
  class Decode;
  class Encode;
  typedef unsigned int NumContext;
  virtual ~JB2Codec();
protected:
  // Constructors
  JB2Codec(const bool xencoding=false);
  // Forbidden assignment
  JB2Codec(const JB2Codec &ref);
  JB2Codec& operator=(const JB2Codec &ref);

  int CodeNum(int lo, int hi, NumContext *pctx,int v);
  void reset_numcoder(void);
  inline void code_eventual_lossless_refinement(void);
  void init_library(JB2Dict &jim);
  int add_library(const int shapeno, JB2Shape &jshp);
  void code_relative_location(JB2Blit *jblt, int rows, int columns);
  void code_bitmap_directly (GBitmap &bm);
  void code_bitmap_by_cross_coding (GBitmap &bm, GP<GBitmap> &cbm, const int libno);
  void code_record(int &rectype, const GP<JB2Dict> &jim, JB2Shape *jshp);
  void code_record(int &rectype, const GP<JB2Image> &jim, JB2Shape *jshp, JB2Blit *jblt);
  static void compute_bounding_box(GBitmap &cbm, LibRect &lrect);
  static int get_direct_context( unsigned char const * const up2,
    unsigned char const * const up1, unsigned char const * const up0,
    const int column);
  static int shift_direct_context ( const int context,
    const int next, unsigned char const * const up2,
    unsigned char const * const up1, unsigned char const * const up0,
    const int column);
  static int get_cross_context( unsigned char const * const up1,
    unsigned char const * const up0, unsigned char const * const xup1,
    unsigned char const * const xup0, unsigned char const * const xdn1,
    const int column );
  static int shift_cross_context( const int context,
    const int n, unsigned char const * const up1,
    unsigned char const * const up0, unsigned char const * const xup1,
    unsigned char const * const xup0, unsigned char const * const xdn1,
    const int column );

  virtual bool CodeBit(const bool bit, BitContext &ctx) = 0;
  virtual void code_comment(GUTF8String &comment) = 0;
  virtual void code_record_type(int &rectype) = 0;
  virtual int code_match_index(int &index, JB2Dict &jim)=0;
  virtual void code_inherited_shape_count(JB2Dict &jim)=0;
  virtual void code_image_size(JB2Dict &jim);
  virtual void code_image_size(JB2Image &jim);
  virtual void code_absolute_location(JB2Blit *jblt,  int rows, int columns)=0;
  virtual void code_absolute_mark_size(GBitmap &bm, int border=0) = 0;
  virtual void code_relative_mark_size(GBitmap &bm, int cw, int ch, int border=0) = 0;
  virtual void code_bitmap_directly(GBitmap &bm,const int dw, int dy,
    unsigned char *up2, unsigned char *up1, unsigned char *up0 )=0;
  virtual void code_bitmap_by_cross_coding (GBitmap &bm, GBitmap &cbm,
    const int xd2c, const int dw, int dy, int cy,
    unsigned char *up1, unsigned char *up0, unsigned char *xup1, 
    unsigned char *xup0, unsigned char *xdn1 )=0;
  // Code records
  virtual int get_diff(const int x_diff,NumContext &rel_loc) = 0;

private:
  bool encoding;

protected:
  // NumCoder
  int cur_ncell;
  BitContext *bitcells;
  GPBuffer<BitContext> gbitcells;
  NumContext *leftcell;
  GPBuffer<NumContext> gleftcell;
  NumContext *rightcell;
  GPBuffer<NumContext> grightcell;
  // Info
  bool refinementp;
  char gotstartrecordp;
  // Code comment
  NumContext dist_comment_byte;
  NumContext dist_comment_length;
  // Code values
  NumContext dist_record_type;
  NumContext dist_match_index;
  BitContext dist_refinement_flag;
  // Library
  GTArray<int> shape2lib;
  GTArray<int> lib2shape;
  GTArray<LibRect> libinfo;
  // Code pairs
  NumContext abs_loc_x;
  NumContext abs_loc_y;
  NumContext abs_size_x;
  NumContext abs_size_y;
  NumContext image_size_dist;
  NumContext inherited_shape_count_dist;
  BitContext offset_type_dist;
  NumContext rel_loc_x_current;
  NumContext rel_loc_x_last;
  NumContext rel_loc_y_current;
  NumContext rel_loc_y_last;
  NumContext rel_size_x;
  NumContext rel_size_y;
  int last_bottom;
  int last_left;
  int last_right;
  int last_row_bottom;
  int last_row_left;
  int image_columns;
  int image_rows;
  int short_list[3];
  int short_list_pos;
  inline void fill_short_list(const int v);
  int update_short_list(const int v);
  // Code bitmaps
  BitContext bitdist[1024];
  BitContext cbitdist[2048];
};

inline void
JB2Dict::JB2Codec::code_eventual_lossless_refinement(void)
{
  refinementp=CodeBit(refinementp, dist_refinement_flag);
}

inline void
JB2Dict::JB2Codec::fill_short_list(const int v)
{
  short_list[0] = short_list[1] = short_list[2] = v;
  short_list_pos = 0;
}

inline int
JB2Dict::JB2Codec::get_direct_context( unsigned char const * const up2,
                    unsigned char const * const up1,
                    unsigned char const * const up0,
                    const int column)
{
  return ( (up2[column - 1] << 9) |
              (up2[column    ] << 8) |
              (up2[column + 1] << 7) |
              (up1[column - 2] << 6) |
              (up1[column - 1] << 5) |
              (up1[column    ] << 4) |
              (up1[column + 1] << 3) |
              (up1[column + 2] << 2) |
              (up0[column - 2] << 1) |
              (up0[column - 1] << 0) );
}

inline int
JB2Dict::JB2Codec::shift_direct_context(const int context, const int next,
                     unsigned char const * const up2,
                     unsigned char const * const up1,
                     unsigned char const * const up0,
                     const int column)
{
  return ( ((context << 1) & 0x37a) |
              (up1[column + 2] << 2)   |
              (up2[column + 1] << 7)   |
              (next << 0)              );
}

inline int
JB2Dict::JB2Codec::get_cross_context( unsigned char const * const up1,
                   unsigned char const * const up0,
                   unsigned char const * const xup1,
                   unsigned char const * const xup0,
                   unsigned char const * const xdn1,
                   const int column )
{
  return ( ( up1[column - 1] << 10) |
              ( up1[column    ] <<  9) |
              ( up1[column + 1] <<  8) |
              ( up0[column - 1] <<  7) |
              (xup1[column    ] <<  6) |
              (xup0[column - 1] <<  5) |
              (xup0[column    ] <<  4) |
              (xup0[column + 1] <<  3) |
              (xdn1[column - 1] <<  2) |
              (xdn1[column    ] <<  1) |
              (xdn1[column + 1] <<  0) );
}

inline int
JB2Dict::JB2Codec::shift_cross_context( const int context, const int n,
                     unsigned char const * const up1,
                     unsigned char const * const up0,
                     unsigned char const * const xup1,
                     unsigned char const * const xup0,
                     unsigned char const * const xdn1,
                     const int column )
{
  return ( ((context<<1) & 0x636)  |
              ( up1[column + 1] << 8) |
              (xup1[column    ] << 6) |
              (xup0[column + 1] << 3) |
              (xdn1[column + 1] << 0) |
              (n << 7)             );
}

// ---------- THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif

