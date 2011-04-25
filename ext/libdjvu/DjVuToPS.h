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

#ifndef _DJVU_TO_PS_H_
#define _DJVU_TO_PS_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

/** @name DjVuToPS.h
    Files #"DjVuToPS.h"# and #"DjVuToPS.cpp"# implement code that can be
    used to convert a \Ref{DjVuImage} or \Ref{DjVuDocument} to PostScript
    format. The conversion is carried out by the \Ref{DjVuToPS} class.
    
    @memo PostScript file generator
    @author Andrei Erofeev <eaf@geocities.com> \\
            Florin Nicsa <Florin.Nicsa@insa-lyon.fr>
*/
//@{

#include "DjVuGlobal.h"
#include "GRect.h"
#include "DjVuDocument.h"
#include "DjVuText.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

/** DjVuImage to PostScript converter.
    Use this class to print \Ref{DjVuImage}s and \Ref{DjVuDocument}s.
    The behavior is customizable. See \Ref{DjVuToPS::Options} for the
    description of available options.*/
class DjVuToPS
{
public:
  class DecodePort;

  /** DjVuToPS options. Use this class to customize the way
      in which DjVu to PS conversion will be done. You can adjust
      the following things:
          \begin{description}
             \item[Format] ({\em EPS} or {\em PS}). Use the {\em EPS}
                format if you plan to embed the output image into another
                document. Print {\em PS} otherwise.
             \item[Language level] ({\em 1} or {\em 2}). Any PostScript
                printer or interpreter should understand PostScript Level 1
                files. Unfortunately we cannot efficiently compress and encode
                data when generating Level 1 files. PostScript Level 2 allows
                to employ an RLE compression and ASCII85 encoding scheme,
                which makes output files significantly smaller. Most of
                the printers and word processors nowadays support PostScript
                Level 2.
             \item[Orientation] ({\em PORTRAIT} or {\em LANDSCAPE})
             \item[Zoom factor] ({\em FIT_PAGE} or {\em ONE_TO_ONE}).
                {\em ONE_TO_ONE} mode is useful, if you want the output to
                be of the same size as the original image (before compression).
                This requires that the #dpi# setting inside the \Ref{DjVuImage}
                is correct. In most of the cases the {\em FIT_PAGE} zoom is
                would be your best choice.
             \item[Mode] ({\em COLOR}, {\em FORE}, {\em BACK}, or {\em BW})
                Specifies how the \Ref{DjVuImage}s will be rendered (all layers,
                foreground layer, background layer, and the mask respectively)
             \item[Color] ({\em TRUE} or {\em FALSE}). Choosing {\em FALSE}
                converts color images to gray scale.
             \item[Gamma] Printer color correction. 
                This parameter ranges from #0.3# to #5.0#. 
             \item[sRGB] ({\em TRUE} or {\em FALSE}).  Choosing {\em TRUE}
                enables accurate sRGB color calibration.  This option
                only works with language level 2.  When this is set,
                gamma correction is clamped to #2.2#.
             \item[Number of copies] Specifies how many copies should be
                printed. This does {\bf not} affect the size of the output file.
          \end{description}
  */
  class Options
  {
  public:
    /** Specifies the rendering mode */
    enum Mode { COLOR, FORE, BACK, BW };
    /** Selects the output format */
    enum Format { PS, EPS };
    /** Specifies the orientation of the output image */
    enum Orientation { PORTRAIT, LANDSCAPE, AUTO };
    /** Specifies the booklet mode */
    enum BookletMode { OFF, RECTO, VERSO, RECTOVERSO };
  private:
    Format format;
    int level;
    Orientation orientation;
    Mode mode;
    int zoom;
    bool color;
    bool calibrate;
    bool text;
    double gamma;
    int copies;
    bool frame;
    bool cropmarks;
    BookletMode bookletmode;
    int bookletmax;
    int bookletalign;
    int bookletfold;
    int bookletxfold;
  public:
    /** Sets output image format to #PS# or #EPS# */
    void set_format(Format format);
    /** Sets PostScript level (#1#, #2#, or #3#) */
    void set_level(int level);
    /** Sets orientation (#LANDSCAPE# or #PORTRAIT#) */
    void set_orientation(Orientation orientation);
    /** Sets \Ref{DjVuImage} rendering mode (#COLOR#, #BW#, #FORE#, #BACK#) */
    void set_mode(Mode mode);
    /** Sets zoom factor. Zoom 0 means fit page. */
    void set_zoom(int zoom);
    /** Affects automatic conversion to GreyScale mode. */
    void set_color(bool color);
    /** Sets gamma correction factor. Ranges from #0.3# to #5.0#. */    
    void set_gamma(double gamma);
    /** Sets sRGB color calibration flag. */
    void set_sRGB(bool calibrate);
    /** Specifies the number of copies to be printed. */
    void set_copies(int copies);
    /** Specifies if a gray frame around the image should be printed. */
    void set_frame(bool on);
    /** Specifies if crop marks should be printed. */
    void set_cropmarks(bool on);
    /** Specifies if a shadow text should be printed. */
    void set_text(bool on);
    /** Specifies the bookletmode */
    void set_bookletmode(BookletMode m);
    /** Specifies the maximal number of pages in a booklet */
    void set_bookletmax(int m);
    /** Specifies an offset (points) between 
        booklet recto(s) and verso(s). */
    void set_bookletalign(int m);
    /** Specifies the margin (points) required to fold 
        the booklet (#fold# in points) and the margin 
        increase required for each sheet (#xfold# in millipoints). */
    void set_bookletfold(int fold, int xfold=0);

    /** Returns output image format (#PS# or #EPS#) */
    Format get_format(void) const {
      return format; }
    /** Returns PostScript level (#1# or #2#) */
    int get_level(void) const {
      return level; }
    /** Returns output image orientation (#PORTRAIT# or #LANDSCAPE#) */
    Orientation get_orientation(void) const {
      return orientation; }
    /** Returns \Ref{DjVuImage} rendering mode 
        (#COLOR#, #FORE#, #BACK#, #BW#) */
    Mode get_mode(void) const {
      return mode; }
    /** Returns output zoom factor (#FIT_PAGE# or #ONE_TO_ONE#) */
    int get_zoom(void) const {
      return zoom; }
    /** Returns color printing flag. */
    bool get_color(void) const {
      return color; }
    /** Returns sRGB color calibration flag. */
    bool get_sRGB(void) const {
      return calibrate; }
    /** Returns printer gamma correction factor */
    double get_gamma(void) const {
      return ((calibrate) ? ((double)2.2) : gamma); }
    /** Returns the number of copies, which will be printed by printer
        This parameter does {\bf not} affect the size of output file */
    int get_copies(void) const {
      return copies; }
    /** Returns #TRUE# if there will be a gray frame */
    bool get_frame(void) const {
      return frame; }
    /** Returns #TRUE# if there will be a gray frame */
    bool get_cropmarks(void) const {
      return cropmarks; }
    /** Returns #TRUE# if there will be a shadow text printed */
    bool get_text(void) const {
      return text; }
    /** Returns the booklet mode */
    BookletMode get_bookletmode(void) const {
      return bookletmode; }
    /** Returns the booklet max size */
    int get_bookletmax(void) {
      return bookletmax; }
    /** Returns the booklet recto/verso offset */
    int get_bookletalign(void) {
      return bookletalign; }
    /** Returns the folding margin for sheet number #n#. */
    int get_bookletfold(int n=0) {
      return bookletfold + (n*bookletxfold+500)/1000; }
    /* Constructor */
    Options(void);
  };
  
  /** Describes current page processing stage. This is passed to
      the #info_cb()# callback. See \Ref{set_info_cb}() for details. */
  enum Stage { DECODING, PRINTING };
  
private:
  void (*refresh_cb)(void*);
  void  *refresh_cl_data;
  void (*prn_progress_cb)(double, void*);
  void  *prn_progress_cl_data;
  void (*dec_progress_cb)(double, void*);
  void  *dec_progress_cl_data;
  void (*info_cb)(int,int,int,Stage,void*);
  void  *info_cl_data;
  unsigned char ramp[256];
  GP<DecodePort> port;
protected:
  void store_doc_prolog(ByteStream&,int,int,GRect*);
  void store_doc_setup(ByteStream&);
  void store_doc_trailer(ByteStream&);
  void store_page_setup(ByteStream&,int,const GRect&,int align=0);
  void store_page_trailer(ByteStream&);
  void make_gamma_ramp(GP<DjVuImage>);
  void print_image_lev1(ByteStream&,GP<DjVuImage>,const GRect&);
  void print_image_lev2(ByteStream&,GP<DjVuImage>,const GRect&);
  void print_bg(ByteStream&,GP<DjVuImage>,const GRect&);
  void print_fg_3layer(ByteStream&,GP<DjVuImage>,const GRect&,unsigned char*);
  void print_fg_2layer(ByteStream&,GP<DjVuImage>,const GRect&,unsigned char*);
  void print_fg(ByteStream&,GP<DjVuImage>,const GRect&);
  void print_image(ByteStream&,GP<DjVuImage>,const GRect&,GP<DjVuTXT>);
  void parse_range(GP<DjVuDocument>,GUTF8String,GList<int>&);
  GP<DjVuImage> decode_page(GP<DjVuDocument>,int,int,int);
  void process_single_page(ByteStream&,GP<DjVuDocument>,int,int,int,int);
  void process_double_page(ByteStream&,GP<DjVuDocument>,void*,int,int);
  
public:
  /** Options affecting the print result. Please refer to
      \Ref{DjVuToPS::Options} for details. */
  Options options;
  
  /** @name Callbacks */
  //@{
  /** Refresh callback is a function, which will be called fairly
      often while the image (document) is being printed. It can
      be used to refresh a GUI, if necessary.
      
      @param refresh_cb Callback function to be called periodically
      @param refresh_cl_data Pointer passed to #refresh_cb()# */
   void set_refresh_cb(void (*refresh_cb)(void*), void *refresh_cl_data);
  /** Callback used to report the progress of printing.  The progress is a
      double number from 0 to 1.  If an existing \Ref{DjVuImage} is printed,
      this callback will be called at least twice: in the beginning and at the
      end of printing.  If a \Ref{DjVuDocument} is being printed, this
      callback will be used to report printing progress of every page. To
      learn the number of the page being printed you can use
      \Ref{set_info_cb}() function.  See \Ref{set_dec_progress_cb}() to find
      out how to learn the decoding progress.
      
      @param cb Callback function to be called
      @param data Pointer passed to #cb()#. */
  void set_prn_progress_cb(void (*cb)(double, void*), void *data);
  /** Callback used to report the progress of decoding.  The progress is a
      double number from 0 to 1.  This callback is only used when printing a
      \Ref{DjVuDocument} in a multithreaded environment. In all other cases it
      will not be called.  Whenever you \Ref{print}() a page range from a
      \Ref{DjVuDocument}, the #DjVuToPS# has to decode the mentioned pages
      before writing them to the output \Ref{ByteStream}. This callback can be
      helpful to find out the status of the decoding.  See
      \Ref{set_prn_progress_cb}() to find out how to learn the printing
      progress.  See \Ref{set_info_cb}() to learn how to find out the number
      of the page being processed, the total number of pages and the number of
      processed pages.
          
      @param cb Callback function to be called
      @param data Pointer passed to #cb()#. */
  void set_dec_progress_cb(void (*cb)(double, void*), void *data);
  /** Callback used to report the current printing stage of a
      \Ref{DjVuDocument}.  When printing a \Ref{DjVuDocument} ({\bf not} a
      \Ref{DjVuImage}), the #DjVuToPS# class will decode and output every page
      mentioned in the {\em page range}. Before decoding and outputing, it
      will call this #info_cb()# callback in order to let you know about what
      is going on. This can be quite useful in a GUI program to keep the user
      informed.  This function is not called when you print a \Ref{DjVuImage}.

      Description of the arguments passed to #info_cb#:
      \begin{description}
      \item[page_num] The number of the page being processed
      \item[page_cnt] Counts how many pages have already been processed.
      \item[tot_pages] Counts how many pages will be output enventually.
      \item[stage] Describes the current processing stage 
                   (#DECODING# or #PRINTING#).
      \end{description}
      @param cb Callback function to be called
      @param data Pointer, which will be passed to #cb()#. */
  void set_info_cb(void (*cb)(int,int,int,Stage,void*), void *data);
  //@}
  
  /** Prints the specified \Ref{DjVuImage} #dimg# into the
      \Ref{ByteStream} #str#. The function will first scale
      the image to fit the #img_rect#, then extract #prn_rect#
      from the obtained bitmap and will output it in the
      PostScript format. The function generates a legal PostScript
      (or Encapsulated PostScript) file taking care of all comments
      conforming to Document Structure Conventions v. 3.0.
      
      {\bf Warning:} The zoom factor specified in \Ref{Options} does
      not affect the amount of data stored into the PostScript file.
      It will be used by the PostScript code to additionally scale
      the image. We cannot pre-scale it here, because we do not know
      the future resolution of the printer. The #img_rect# and
      #prn_rect# alone define how much data will be sent to printer.
      
      Using #img_rect# one can upsample or downsample the image prior
      to sending it to the printer.
      
      @param str \Ref{ByteStream} where PostScript output will be sent
      @param dimg \Ref{DjVuImage} to print
      @param img_rect Rectangle to which the \Ref{DjVuImage} will be scaled.
             Note that this parameters defines the amount of data
             that will actually be sent to the printer. The PostScript
             code can futher resize the image according to the
             #zoom# parameter from the \Ref{Options} structure.
      @param prn_rect Part of img_rect to send to printer.
      @param override_dpi Optional parameter allowing you to override
             dpi setting that would otherwise be extracted from #dimg# */
   void print(ByteStream&, GP<DjVuImage> dimg,
              const GRect &prn_rect, const GRect &img_rect,
              int override_dpi=-1 );
  
  /** Outputs the specifies pages from the \Ref{DjVuDocument} into the
      \Ref{ByteStream} in PostScript format.  The function will generate a
      multipage PostScript document conforming to PS DSC 3.0 by storing into
      it every page mentioned in the #page_range#.

      If #page_range# is empty, all pages from the \Ref{DjVuDocument} #doc#
      will be printed.  The #page_range# is a set of ranges separated by
      commas. Every range has this form: {\em start_page}[-{\em
      end_page}]. {\em end_page} is optional and can be less than the {\em
      start_page}, in which case the pages will be printed in the reverse
      order.

      Examples:
      \begin{itemize}
      \item {\bf 1-10} - Will print pages 1 to 10.
      \item {\bf 10-1} - Will print pages 1 to 10 in reverse order.
      \item {\bf 1-10,12-20} - Will print pages 1 to 20 with page 11 skipped.
      \end{itemize} */
  void print(ByteStream&, GP<DjVuDocument> doc, GUTF8String page_range);
  void print(ByteStream&, GP<DjVuDocument> doc);
 
  
  /** Default constructor. Initializes the class. */
  DjVuToPS(void);
};


//****************************************************************************
//******************************** DjVuToPS **********************************
//****************************************************************************

//@}
// ------------

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
