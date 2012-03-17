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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

#include "DjVuToPS.h"
#include "IFFByteStream.h"
#include "BSByteStream.h"
#include "DjVuImage.h"
#include "DjVuText.h"
#include "DataPool.h"
#include "IW44Image.h"
#include "JB2Image.h"
#include "GBitmap.h"
#include "GPixmap.h"
#include "debug.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#ifdef UNIX
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#endif


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


static const size_t ps_string_size=15000;

// ***************************************************************************
// ****************************** Options ************************************
// ***************************************************************************

DjVuToPS::Options::
Options(void)
: format(PS), 
  level(2), 
  orientation(AUTO), 
  mode(COLOR), 
  zoom(0),
  color(true), 
  calibrate(true), 
  text(false),
  gamma((double)2.2), 
  copies(1), 
  frame(false),
  cropmarks(false),
  bookletmode(OFF),
  bookletmax(0),
  bookletalign(0),
  bookletfold(18),
  bookletxfold(200)
{}

void
DjVuToPS::Options::
set_format(Format xformat)
{
  if (xformat != EPS && xformat != PS)
    G_THROW(ERR_MSG("DjVuToPS.bad_format"));
  format=xformat;
}

void
DjVuToPS::Options::
set_level(int xlevel)
{
  if (xlevel<1 || xlevel>3)
    G_THROW(ERR_MSG("DjVuToPS.bad_level")
           + GUTF8String("\t") + GUTF8String(xlevel));
  level=xlevel;
}

void
DjVuToPS::Options::
set_orientation(Orientation xorientation)
{
  if (xorientation!=PORTRAIT && 
      xorientation!=LANDSCAPE &&
      xorientation!=AUTO )
    G_THROW(ERR_MSG("DjVuToPS.bad_orient"));
  orientation=xorientation;
}

void
DjVuToPS::Options::
set_mode(Mode xmode)
{
  if (xmode!=COLOR && xmode!=FORE && xmode!=BACK && xmode!=BW)
    G_THROW(ERR_MSG("DjVuToPS.bad_mode"));
  mode=xmode;
}

void
DjVuToPS::Options::
set_zoom(int xzoom)
{
  if (xzoom!=0 && !(xzoom>=5 && xzoom<=999))
    G_THROW(ERR_MSG("DjVuToPS.bad_zoom"));
  zoom=xzoom;
}

void
DjVuToPS::Options::
set_color(bool xcolor)
{
  color=xcolor;
}

void 
DjVuToPS::Options::
set_sRGB(bool xcalibrate)
{
  calibrate=xcalibrate;
}

void
DjVuToPS::Options::
set_gamma(double xgamma)
{
  if  (xgamma<(double)(0.3-0.0001) || xgamma>(double)(5.0+0.0001))
    G_THROW(ERR_MSG("DjVuToPS.bad_gamma"));
  gamma=xgamma;
}

void
DjVuToPS::Options::
set_copies(int xcopies)
{
  if (xcopies<=0)
    G_THROW(ERR_MSG("DjVuToPS.bad_number"));
  copies=xcopies;
}

void
DjVuToPS::Options::
set_frame(bool xframe)
{
  frame=xframe;
}

void
DjVuToPS::Options::
set_cropmarks(bool xmarks)
{
  cropmarks=xmarks;
}

void
DjVuToPS::Options::
set_text(bool xtext)
{
  text=xtext;
}

void 
DjVuToPS::Options::
set_bookletmode(BookletMode m)
{
  bookletmode = m;
}

void 
DjVuToPS::Options::
set_bookletmax(int m)
{
  bookletmax = 0;
  if (m > 0)
    bookletmax = (m+3)/4;
  bookletmax *= 4;
}

void 
DjVuToPS::Options::
set_bookletalign(int m)
{
  bookletalign = m;
}

void 
DjVuToPS::Options::
set_bookletfold(int fold, int xfold)
{
  if (fold >= 0)
    bookletfold = fold;
  if (xfold >= 0)
    bookletxfold = xfold;
}


// ***************************************************************************
// ******************************* DjVuToPS **********************************
// ***************************************************************************

static char bin2hex[256][2];

DjVuToPS::DjVuToPS(void)
{
  DEBUG_MSG("DjVuToPS::DjVuToPS(): initializing...\n");
  DEBUG_MAKE_INDENT(3);
  DEBUG_MSG("Initializing dig2hex[]\n");
  // Creating tables for bin=>text translation
  static const char * dig2hex="0123456789ABCDEF";
  int i;
  for(i=0;i<256;i++)
    {
      bin2hex[i][0]=dig2hex[i/16];
      bin2hex[i][1]=dig2hex[i%16];
    }
  refresh_cb=0;
  refresh_cl_data=0;
  prn_progress_cb=0;
  prn_progress_cl_data=0;
  dec_progress_cb=0;
  dec_progress_cl_data=0;
  info_cb=0;
  info_cl_data=0;
}

#ifdef __GNUC__
static void
write(ByteStream &str, const char *format, ...)
__attribute__((format (printf, 2, 3)));
#endif

static void
write(ByteStream &str, const char *format, ...)
{
  /* Will output the formated string to the specified \Ref{ByteStream}
     like #fprintf# would do it for a #FILE#. */
  va_list args;
  va_start(args, format);
  GUTF8String tmp;
  tmp.vformat(format, args);
  str.writall((const char *) tmp, tmp.length());
}

// ************************* DOCUMENT LEVEL *********************************

void
DjVuToPS::
store_doc_prolog(ByteStream &str, int pages, int dpi, GRect *grect)
{
  /* Will store the {\em document prolog}, which is basically a
     block of document-level comments in PS DSC 3.0 format.
     @param str Stream where PostScript data should be written
     @param pages Total number of pages
     @param dpi (EPS mode only) 
     @param grect (EPS mode only) */
  DEBUG_MSG("storing the document prolog\n");
  DEBUG_MAKE_INDENT(3);
  if (options.get_format()==Options::EPS)
    write(str,
          "%%!PS-Adobe-3.0 EPSF 3.0\n"
          "%%%%BoundingBox: 0 0 %d %d\n",
          (grect->width()*100+dpi-1)/dpi, 
          (grect->height()*100+dpi-1)/dpi );
  else
    write(str, "%%!PS-Adobe-3.0\n");
  write(str,
        "%%%%Title: DjVu PostScript document\n"
        "%%%%Copyright: Copyright (c) 1998-1999 AT&T\n"
        "%%%%Creator: DjVu (code by Andrei Erofeev)\n"
        "%%%%DocumentData: Clean7Bit\n");
  // Date
  time_t tm=time(0);
  write(str, "%%%%CreationDate: %s", ctime(&tm));
  // For
#ifdef UNIX
  passwd *pswd = getpwuid(getuid());
  if (pswd)
    {
      char *s = strchr(pswd->pw_gecos, ',');
      if (s) 
        *s = 0;
      s = 0;
      if (pswd->pw_gecos && strlen(pswd->pw_gecos))
        s = pswd->pw_gecos;
      else if (pswd->pw_name && strlen(pswd->pw_name))
        s = pswd->pw_name;
      if (s)
        write(str, "%%%%For: %s\n", s);
    }
#endif
  // Language
  write(str, "%%%%LanguageLevel: %d\n", options.get_level());
  if (options.get_level()<2 && options.get_color())
    write(str, "%%%%Extensions: CMYK\n");
  // Pages
  write(str, "%%%%Pages: %d\n",pages );
  write(str, "%%%%PageOrder: Ascend\n");
  // Orientation
  if (options.get_orientation() != Options::AUTO)
    write(str, "%%%%Orientation: %s\n", 
          options.get_orientation()==Options::PORTRAIT ?
          "Portrait" : "Landscape" );
  // Requirements
  if (options.get_format() == Options::PS)
    {
      write(str, "%%%%Requirements:");
      if (options.get_color())
        write(str, " color");
      if (options.get_copies()>1)
        write(str, " numcopies(%d)", options.get_copies());
      if (options.get_level()>=2)
        {
          if (options.get_copies()>1)
            write(str, " collate");
          if (options.get_bookletmode() == Options::RECTOVERSO)
            write(str, " duplex(tumble)");
        }
      write(str, "\n");
    }
  // End
  write(str,
        "%%%%EndComments\n"
        "%%%%EndProlog\n"
        "\n");
}

void
DjVuToPS::
store_doc_setup(ByteStream &str)
{
  /* Will store the {\em document setup}, which is a set of
     PostScript commands and functions used to inspect and prepare
     the PostScript interpreter environment before displaying images. */
  write(str, 
        "%%%%BeginSetup\n"
        "/doc-origstate save def\n");
  if (options.get_level()>=2)
    {
      if (options.get_format() == Options::PS)
        {
          if (options.get_copies()>1)
            write(str, 
                  "[{\n"
                  "%%%%BeginFeature: NumCopies %d\n"
                  "<< /NumCopies %d >> setpagedevice\n"
                  "%%%%EndFeature\n"
                  "} stopped cleartomark\n"
                  "[{\n"
                  "%%%%BeginFeature: Collate\n"
                  "<< /Collate true >> setpagedevice\n"
                  "%%%%EndFeature\n"
                  "} stopped cleartomark\n",
                  options.get_copies(),
                  options.get_copies() );
          if (options.get_bookletmode()==Options::RECTOVERSO)
            write(str, 
                  "[{\n"
                  "%%%%BeginFeature: Duplex DuplexTumble\n"
                  "<< /Duplex true /Tumble true >> setpagedevice\n"
                  "%%%%EndFeature\n"
                  "} stopped cleartomark\n");
        }
      if (options.get_color())
        write(str, 
              "%% -- procs for reading color image\n"
              "/readR () def\n"
              "/readG () def\n"
              "/readB () def\n"
              "/ReadData {\n"
              "   currentfile /ASCII85Decode filter dup\n"
              "   /RunLengthDecode filter\n"
              "   bufferR readstring pop /readR exch def\n"
              "   dup status { flushfile } { pop } ifelse\n"
              "   currentfile /ASCII85Decode filter dup\n"
              "   /RunLengthDecode filter\n"
              "   bufferG readstring pop /readG exch def\n"
              "   dup status { flushfile } { pop } ifelse\n"
              "   currentfile /ASCII85Decode filter dup\n"
              "   /RunLengthDecode filter\n"
              "   bufferB readstring pop /readB exch def\n"
              "   dup status { flushfile } { pop } ifelse\n"
              "} bind def\n"
              "/ReadR {\n"
              "   readR length 0 eq { ReadData } if\n"
              "   readR /readR () def\n"
              "} bind def\n"
              "/ReadG {\n"
              "   readG length 0 eq { ReadData } if\n"
              "   readG /readG () def\n"
              "} bind def\n"
              "/ReadB {\n"
              "   readB length 0 eq { ReadData } if\n"
              "   readB /readB () def\n"
              "} bind def\n");
      write(str,
            "%% -- procs for foreground layer\n"
            "/g {gsave 0 0 0 0 5 index 5 index setcachedevice\n"
            "    true [1 0 0 1 0 0] 5 4 roll imagemask grestore\n"
            "} bind def\n"
            "/gn {gsave 0 0 0 0 6 index 6 index setcachedevice\n"
            "  true [1 0 0 1 0 0] 3 2 roll 5 1 roll \n"
            "  { 1 sub 0 index 2 add 1 index  1 add roll\n"
            "  } imagemask grestore pop \n"
            "} bind def\n"
            "/c {setcolor rmoveto glyphshow} bind def\n"
            "/s {rmoveto glyphshow} bind def\n"
            "/S {rmoveto gsave show grestore} bind def\n" 
            "/F {(Helvetica) findfont exch scalefont setfont} bind def\n"
            "%% -- emulations\n"
            "systemdict /rectstroke known not {\n"
            "  /rectstroke  %% stack : x y width height \n"
            "  { newpath 4 2 roll moveto 1 index 0 rlineto\n"
            "    0 exch rlineto neg 0 rlineto closepath stroke\n"
            "  } bind def } if\n"
            "systemdict /rectclip known not {\n"
            "  /rectclip  %% stack : x y width height \n"
            "  { newpath 4 2 roll moveto 1 index 0 rlineto\n"
            "    0 exch rlineto neg 0 rlineto closepath clip\n"
            "  } bind def } if\n"
            "%% -- color space\n" );
      if (options.get_sRGB())
        write(str,
              "/DjVuColorSpace [ %s\n"
              "<< /DecodeLMN [ { dup 0.03928 le {\n"
              "       12.92321 div\n"
              "     } {\n"
              "       0.055 add 1.055 div 2.4 exp\n"
              "     } ifelse } bind dup dup ]\n"
              "   /MatrixLMN [\n"
              "      0.412457 0.212673 0.019334\n"
              "      0.357576 0.715152 0.119192\n"
              "      0.180437 0.072175 0.950301 ]\n"
              "   /WhitePoint [ 0.9505 1 1.0890 ] %% D65 \n"
              "   /BlackPoint[0 0 0] >> ] def\n",
              (options.get_color()) ? "/CIEBasedABC" : "/CIEBasedA" );
      else if (options.get_color())
        write(str,"/DjVuColorSpace /DeviceRGB def\n");
      else
        write(str,"/DjVuColorSpace /DeviceGray def\n");
    } 
  else 
    {
      // level<2
      if (options.get_format() == Options::PS)
        if (options.get_copies() > 1)
          write(str,"/#copies %d def\n", options.get_copies());
      if (options.get_color())
        write(str, 
              "%% -- buffers for reading image\n"
              "/buffer8 () def\n"
              "/buffer24 () def\n"
              "%% -- colorimage emulation\n"
              "systemdict /colorimage known {\n"
              "   /ColorProc {\n"
              "      currentfile buffer24 readhexstring pop\n"
              "   } bind def\n"
              "   /ColorImage {\n"
              "      colorimage\n"
              "   } bind def\n"
              "} {\n"
              "   /ColorProc {\n"
              "      currentfile buffer24 readhexstring pop\n"
              "      /data exch def /datalen data length def\n"
              "      /cnt 0 def\n"
              "      0 1 datalen 3 idiv 1 sub {\n"
              "         buffer8 exch\n"
              "                data cnt get 20 mul /cnt cnt 1 add def\n"
              "                data cnt get 32 mul /cnt cnt 1 add def\n"
              "                data cnt get 12 mul /cnt cnt 1 add def\n"
              "                add add 64 idiv put\n"
              "      } for\n"
              "      buffer8 0 datalen 3 idiv getinterval\n"
              "   } bind def\n"
              "   /ColorImage {\n"
              "      pop pop image\n"
              "   } bind def\n"
              "} ifelse\n");
    } // level<2
  write(str, "%%%%EndSetup\n\n");
}

void
DjVuToPS::
store_doc_trailer(ByteStream &str)
{
  /* Will store the {\em document trailer}, which is a clean-up code
     used to return the PostScript interpeter back to the state, in which
     it was before displaying this document. */
  write(str, 
        "%%%%Trailer\n"
        "doc-origstate restore\n"
        "%%%%EOF\n");
}

// ***********************************************************************
// ***************************** PAGE LEVEL ******************************
// ***********************************************************************

static unsigned char *
ASCII85_encode(unsigned char * dst, 
               const unsigned char * src_start,
               const unsigned char * src_end)
{
  /* Will read data between #src_start# and #src_end# pointers (excluding byte
     pointed by #src_end#), encode it using {\bf ASCII85} algorithm, and
     output the result into the destination buffer pointed by #dst#.  The
     function returns pointer to the first unused byte in the destination
     buffer. */
  int symbols=0;
  const unsigned char * ptr;
  for(ptr=src_start;ptr<src_end;ptr+=4)
    {
      unsigned int num=0;
      if (ptr+3<src_end)
        {
          num |= ptr[0] << 24; 
          num |= ptr[1] << 16; 
          num |= ptr[2] << 8; 
          num |= ptr[3];
        }
      else
        {
          num |= ptr[0] << 24; 
          if (ptr+1<src_end) 
            num |= ptr[1] << 16; 
          if (ptr+2<src_end) 
            num |= ptr[2] << 8; 
        }
      int a1, a2, a3, a4, a5;
      a5=num % 85; num/=85;
      a4=num % 85; num/=85;
      a3=num % 85; num/=85;
      a2=num % 85;
      a1=num / 85;
      *dst++ = a1+33;
      *dst++ = a2+33;
      if (ptr+1<src_end)
        *dst++ = a3+33;
      if (ptr+2<src_end)
        *dst++ = a4+33;
      if (ptr+3<src_end)
        *dst++ = a5+33;
      symbols += 5;
      if (symbols > 70 && ptr+4<src_end)
        { 
          *dst++='\n'; 
          symbols=0; 
        }
    }
  return dst;
}

static unsigned char *
RLE_encode(unsigned char * dst,
           const unsigned char * src_start,
           const unsigned char * src_end)
{
  /* Will read data between #src_start# and #src_end# pointers (excluding byte
     pointed by #src_end#), RLE encode it, and output the result into the
     destination buffer pointed by #dst#.  #counter# is used to count the
     number of output bytes.  The function returns pointer to the first unused
     byte in the destination buffer. */
  const unsigned char * ptr;
  for(ptr=src_start;ptr<src_end;ptr++)
    {
      if (ptr==src_end-1)
        {
          *dst++=0; *dst++=*ptr;
        } 
      else if (ptr[0]!=ptr[1])
        {
          // Guess how many non repeating bytes we have
          const unsigned char * ptr1;
          for(ptr1=ptr+1;ptr1<src_end-1;ptr1++)
            if (ptr1[0]==ptr1[1] || ptr1-ptr>=128) break;
          int pixels=ptr1-ptr;
          *dst++=pixels-1;
          for(int cnt=0;cnt<pixels;cnt++)
            *dst++=*ptr++;
          ptr--;
        } 
      else
        {
          // Get the number of repeating bytes
          const unsigned char * ptr1;
          for(ptr1=ptr+1;ptr1<src_end-1;ptr1++)
            if (ptr1[0]!=ptr1[1] || ptr1-ptr+1>=128) break;
          int pixels=ptr1-ptr+1;
          *dst++=257-pixels;
          *dst++=*ptr;
          ptr=ptr1;
        }
    }
  return dst;
}

#define GRAY(r,g,b) (((r)*20+(g)*32+(b)*12)/64)

void
DjVuToPS::
store_page_setup(ByteStream &str, 
                 int dpi, 
                 const GRect &grect, 
                 int align )
{
  /* Will store PostScript code necessary to prepare page for
     the coming \Ref{DjVuImage}. This is basically a scaling
     code plus initialization of some buffers. */
  if (options.get_format() == Options::EPS)
    write(str, 
          "/page-origstate save def\n"
          "%% -- coordinate system\n"
          "/image-dpi %d def\n"
          "/image-x 0 def\n"
          "/image-y 0 def\n"
          "/image-width  %d def\n"
          "/image-height %d def\n"
          "/coeff 100 image-dpi div def\n"
          "/a11 coeff def\n"
          "/a12 0 def\n"
          "/a13 0 def\n"
          "/a21 0 def\n"
          "/a22 coeff def\n"
          "/a23 0 def\n"
          "[a11 a21 a12 a22 a13 a23] concat\n"
          "gsave 0 0 image-width image-height rectclip\n"
          "%% -- begin printing\n",
          dpi, grect.width(), grect.height() );
  else
    {
      int margin = 0;
      const char *xauto = "false";
      const char *xportrait = "false";
      const char *xfit = "false";
      if (options.get_orientation()==Options::AUTO)
        xauto = "true";
      if (options.get_orientation()==Options::PORTRAIT)
        xportrait = "true";
      if (options.get_zoom()<=0)
        xfit = "true";
      if (options.get_cropmarks())
        margin = 36;
      else if (options.get_frame())
        margin = 6;
      write(str, 
            "/page-origstate save def\n"
            "%% -- coordinate system\n"
            "/auto-orient %s def\n"
            "/portrait %s def\n"
            "/fit-page %s def\n"
            "/zoom %d def\n"
            "/image-dpi %d def\n"
            "clippath pathbbox newpath\n"
            "2 index sub exch 3 index sub\n"
            "/page-width exch def\n"
            "/page-height exch def\n"
            "/page-y exch def\n"
            "/page-x exch def\n"
            "/image-x 0 def\n"
            "/image-y 0 def\n"
            "/image-width  %d def\n"
            "/image-height %d def\n"
            "/margin %d def\n"
            "/halign %d def\n"
            "/valign 0 def\n",
            xauto, xportrait, xfit, options.get_zoom(), 
            dpi, grect.width(), grect.height(),
            margin, align );
      write(str, 
            "%% -- position page\n"
            "auto-orient {\n"
            "  image-height image-width sub\n"
            "  page-height page-width sub\n"
            "  mul 0 ge /portrait exch def\n" 
            "} if\n"
            "fit-page {\n"
            "  /page-width page-width margin sub\n"
            "     halign 0 eq { margin sub } if def\n"
            "  /page-height page-height margin sub\n"
            "     valign 0 eq { margin sub } if def\n"
            "  /page-x page-x halign 0 ge { margin add } if def\n"
            "  /page-y page-y valign 0 ge { margin add } if def\n"
            "} if\n"
            "portrait {\n"
            "  fit-page {\n"
            "    image-height page-height div\n"
            "    image-width page-width div\n"
            "    gt {\n"
            "      page-height image-height div /coeff exch def\n"
            "    } {\n"
            "      page-width image-width div /coeff exch def\n"
            "    } ifelse\n"
            "  } {\n"
            "    /coeff 72 image-dpi div zoom mul 100 div def\n"
            "  } ifelse\n"
            "  /start-x page-x page-width image-width\n"
            "    coeff mul sub 2 div halign 1 add mul add def\n"
            "  /start-y page-y page-height image-height\n"
            "    coeff mul sub 2 div valign 1 add mul add def\n"
            "  /a11 coeff def\n"
            "  /a12 0 def\n"
            "  /a13 start-x def\n"
            "  /a21 0 def\n"
            "  /a22 coeff def\n"
            "  /a23 start-y def\n"
            "} { %% landscape\n"
            "  fit-page {\n"
            "    image-height page-width div\n"
            "    image-width page-height div\n"
            "    gt {\n"
            "      page-width image-height div /coeff exch def\n"
            "    } {\n"
            "      page-height image-width div /coeff exch def\n"
            "    } ifelse\n"
            "  } {\n"
            "    /coeff 72 image-dpi div zoom mul 100 div def\n"
            "  } ifelse\n"
            "  /start-x page-x page-width add page-width image-height\n"
            "    coeff mul sub 2 div valign 1 add mul sub def\n"
            "  /start-y page-y page-height image-width\n"
            "    coeff mul sub 2 div halign 1 add mul add def\n"
            "  /a11 0 def\n"
            "  /a12 coeff neg def\n"
            "  /a13 start-x image-y coeff neg mul sub def\n"
            "  /a21 coeff def\n"
            "  /a22 0 def\n"
            "  /a23 start-y image-x coeff mul add def \n"
            "} ifelse\n"
            "[a11 a21 a12 a22 a13 a23] concat\n"
            "gsave 0 0 image-width image-height rectclip\n"
            "%% -- begin print\n");
    }
}

void
DjVuToPS::
store_page_trailer(ByteStream &str)
{
  write(str, 
        "%% -- end print\n" 
        "grestore\n");
  if (options.get_frame())
    write(str, 
          "%% Drawing frame\n"
          "gsave 0.7 setgray 0.5 coeff div setlinewidth 0 0\n"
          "image-width image-height rectstroke\n"
          "grestore\n");
  if (options.get_cropmarks() &&
      options.get_format() != Options::EPS )
    write(str,
          "%% Drawing crop marks\n"
          "/cm { gsave translate rotate 1 coeff div dup scale\n"
          "      0 setgray 0.5 setlinewidth -36 0 moveto 0 0 lineto\n"
          "      0 -36 lineto stroke grestore } bind def\n"
          "0 0 0 cm 180 image-width image-height cm\n"
          "90 image-width 0 cm 270 0 image-height cm\n");
  write(str,
        "page-origstate restore\n");
}

static int
compute_red(int w, int h, int rw, int rh)
{
  for (int red=1; red<16; red++)
    if (((w+red-1)/red==rw) && ((h+red-1)/red==rh))
      return red;
  return 16;
}

static int
get_bg_red(GP<DjVuImage> dimg) 
{
  GP<GPixmap> pm = 0;
  // Access image size
  int width = dimg->get_width();
  int height = dimg->get_height();
  if (width<=0 || height<=0) return 0;
  // CASE1: Incremental BG IW44Image
  GP<IW44Image> bg44 = dimg->get_bg44();
  if (bg44)
    {
      int w = bg44->get_width();
      int h = bg44->get_height();
      // Avoid silly cases
      if (w==0 || h==0 || width==0 || height==0)
        return 0;
      return compute_red(width,height,w,h);
    }
  // CASE 2: Raw background pixmap
  GP<GPixmap>  bgpm = dimg->get_bgpm();
  if (bgpm)
    {
      int w = bgpm->columns();
      int h = bgpm->rows();
      // Avoid silly cases
      if (w==0 || h==0 || width==0 || height==0)
        return 0;
      return compute_red(width,height,w,h);
    }
  return 0;
}

static GP<GPixmap>
get_bg_pixmap(GP<DjVuImage> dimg, const GRect &rect)
{
  GP<GPixmap> pm = 0;
  // Access image size
  int width = dimg->get_width();
  int height = dimg->get_height();
  GP<DjVuInfo> info = dimg->get_info();
  if (width<=0 || height<=0 || !info) return 0;
  // CASE1: Incremental BG IW44Image
  GP<IW44Image> bg44 = dimg->get_bg44();
  if (bg44)
    {
      int w = bg44->get_width();
      int h = bg44->get_height();
      // Avoid silly cases
      if (w==0 || h==0 || width==0 || height==0)
        return 0;
      pm = bg44->get_pixmap(1,rect);
      return pm;
    }
  // CASE 2: Raw background pixmap
  GP<GPixmap>  bgpm = dimg->get_bgpm();
  if (bgpm)
    {
      int w = bgpm->columns();
      int h = bgpm->rows();
      // Avoid silly cases
      if (w==0 || h==0 || width==0 || height==0)
        return 0;
      pm->init(*bgpm, rect);
      return pm;
    }
  // FAILURE
  return 0;
}

void 
DjVuToPS::
make_gamma_ramp(GP<DjVuImage> dimg)
{
  double targetgamma = options.get_gamma();
  double whitepoint = (options.get_sRGB() ? 255 : 280);
  for (int i=0; i<256; i++)
    ramp[i] = i;
  if (! dimg->get_info()) 
    return;
  if (targetgamma < 0.1)
    return;
  double filegamma = dimg->get_info()->gamma;
  double correction = filegamma / targetgamma;
  if (correction<0.1 || correction>10)
    return;
  {
    for (int i=0; i<256; i++)
    {
      double x = (double)(i)/255.0;
      if (correction != 1.0) 
        x = pow(x, correction);        
      int j = (int) floor(whitepoint * x + 0.5);
      ramp[i] = (j>255) ? 255 : (j<0) ? 0 : j;
    }
  }
}

void
DjVuToPS::
print_fg_2layer(ByteStream &str, 
                GP<DjVuImage> dimg,
                const GRect &prn_rect, 
                unsigned char *blit_list)
{
  // Pure-jb2 or color-jb2 case.
  GPixel p;
  int currentx=0;
  int currenty=0;
  GP<DjVuPalette> pal = dimg->get_fgbc();
  GP<JB2Image> jb2 = dimg->get_fgjb();
  if (! jb2) return;
  int num_blits = jb2->get_blit_count();
  int current_blit;
  for(current_blit=0; current_blit<num_blits; current_blit++)
    {
      if (blit_list[current_blit])
        {
          JB2Blit *blit = jb2->get_blit(current_blit);
          if ((pal) && !(options.get_mode()==Options::BW))
            {
              pal->index_to_color(pal->colordata[current_blit], p);
              if (options.get_color())
                {
                  write(str,"/%d %d %d %f %f %f c\n",
                        blit->shapeno, 
                        blit->left-currentx, blit->bottom-currenty,
                        ramp[p.r]/255.0, ramp[p.g]/255.0, ramp[p.b]/255.0);
                } 
              else
                {
                  write(str,"/%d %d %d %f c\n",
                        blit->shapeno, 
                        blit->left-currentx, blit->bottom-currenty,
                        ramp[GRAY(p.r, p.g, p.b)]/255.0);
                }
            }
          else
            {
              write(str,"/%d %d %d s\n", 
                    blit->shapeno, 
                    blit->left-currentx, blit->bottom-currenty);
            }
          currentx = blit->left;
          currenty = blit->bottom;
        }
    }
}

void
DjVuToPS::
print_fg_3layer(ByteStream &str, 
                GP<DjVuImage> dimg,
                const GRect &cprn_rect, 
                unsigned char *blit_list )
{
  GRect prn_rect;
  GP<GPixmap> brush = dimg->get_fgpm();
  if (! brush) return;
  int br = brush->rows();
  int bc = brush->columns();
  int red = compute_red(dimg->get_width(),dimg->get_height(),bc,br);
  prn_rect.ymin = (cprn_rect.ymin)/red;
  prn_rect.xmin = (cprn_rect.xmin)/red;
  prn_rect.ymax = (cprn_rect.ymax+red-1)/red;
  prn_rect.xmax = (cprn_rect.xmax+red-1)/red;
  int color_nb = ((options.get_color()) ? 3 : 1);
  GP<JB2Image> jb2 = dimg->get_fgjb();
  if (! jb2) return;
  int pw = bc;
  int ph = 2;

  write(str,
        "/P {\n" 
        "  11 dict dup begin 4 1 roll\n"
        "    /PatternType 1 def\n"
        "    /PaintType 1 def\n"
        "    /TilingType 1 def\n"
        "    /H exch def\n"
        "    /W exch def\n"
        "    /Red %d def\n"
        "    /PatternString exch def\n"
        "    /XStep W Red mul def\n"
        "    /YStep H Red mul def\n"
        "    /BBox [0 0 XStep YStep] def\n"
        "    /PaintProc { begin\n"
        "       Red dup scale\n"
        "       << /ImageType 1 /Width W /Height H\n"
        "          /BitsPerComponent 8 /Interpolate false\n"
        "          /Decode [%s] /ImageMatrix [1 0 0 1 0 0]\n"
        "          /DataSource PatternString >> image\n"
        "       end } bind def\n"
        "     0 0 XStep YStep rectclip\n"
        "     end matrix makepattern\n"
        "  /Pattern setcolorspace setpattern\n"
        "  0 0 moveto\n"
        "} def\n", red, (color_nb == 1) ? "0 1" : "0 1 0 1 0 1" );

  unsigned char *s;
  GPBuffer<unsigned char> gs(s,pw*ph*color_nb);
  unsigned char *s_ascii_encoded;
  GPBuffer<unsigned char> gs_ascii_encoded(s_ascii_encoded,pw*ph*2*color_nb);
    {
      for (int y=prn_rect.ymin; y<prn_rect.ymax; y+=ph)
        for (int x=prn_rect.xmin; x<prn_rect.xmax; x+=pw)
          {
            int w = ((x+pw > prn_rect.xmax) ? prn_rect.xmax-x : pw);
            int h = ((y+ph > prn_rect.ymax) ? prn_rect.ymax-y : ph);
            int currentx = x * red;
            int currenty = y * red;
            // Find first intersecting blit
            int current_blit;
            int num_blits = jb2->get_blit_count();
            GRect rect1(currentx,currenty, w*red, h*red);
            for(current_blit=0; current_blit<num_blits; current_blit++)
              if (blit_list[current_blit])
                {
                  JB2Blit *blit = jb2->get_blit(current_blit);
                  GRect rect2(blit->left, blit->bottom,
                              jb2->get_shape(blit->shapeno).bits->columns(),
                              jb2->get_shape(blit->shapeno).bits->rows());
                  if (rect2.intersect(rect1,rect2)) 
                    break;
                }
            if (current_blit >= num_blits)
              continue;
            // Setup pattern
            write(str,"gsave %d %d translate\n", currentx, currenty);
            write(str,"<~");
            unsigned char *q = s;
            for(int current_row = y; current_row<y+h; current_row++)
              { 
                GPixel *row_pix = (*brush)[current_row];
                for(int current_col = x; current_col<x+w; current_col++)
                  { 
                    GPixel &p = row_pix[current_col];
                    if (color_nb>1)
                      {
                        *q++ = ramp[p.r];
                        *q++ = ramp[p.g];
                        *q++ = ramp[p.b];
                      }
                    else
                      {
                        *q++ = ramp[GRAY(p.r,p.g,p.b)];
                      }
                  }
              }
            unsigned char *stop_ascii = 
              ASCII85_encode(s_ascii_encoded,s,s+w*h*color_nb);
            *stop_ascii++='\0';
            write(str,"%s",s_ascii_encoded);
            write(str,"~> %d %d P\n", w, h);
            // Keep performing blits
            for(; current_blit<num_blits; current_blit++)
              if (blit_list[current_blit])
                {
                  JB2Blit *blit = jb2->get_blit(current_blit);
                  GRect rect2(blit->left, blit->bottom,
                              jb2->get_shape(blit->shapeno).bits->columns(),
                              jb2->get_shape(blit->shapeno).bits->rows()); 
                  if (rect2.intersect(rect1,rect2)) 
                    {   
                      write(str,"/%d %d %d s\n",
                            blit->shapeno, 
                            blit->left-currentx, blit->bottom-currenty);
                      currentx = blit->left;
                      currenty = blit->bottom;
                    }
                }
            write(str,"grestore\n");
          }
      // Cleanup
    }
}

void
DjVuToPS::
print_fg(ByteStream &str, 
         GP<DjVuImage> dimg,
         const GRect &prn_rect )
{
  GP<JB2Image> jb2=dimg->get_fgjb();
  if (! jb2) return;
  int num_blits = jb2->get_blit_count();
  int num_shapes = jb2->get_shape_count();
  unsigned char *dict_shapes = 0;
  unsigned char *blit_list = 0;
  GPBuffer<unsigned char> gdict_shapes(dict_shapes,num_shapes);
  GPBuffer<unsigned char> gblit_list(blit_list,num_blits);
  for(int i=0; i<num_shapes; i++)
  {
    dict_shapes[i]=0;
  }
  for(int current_blit=0; current_blit<num_blits; current_blit++)
  {
    JB2Blit *blit = jb2->get_blit(current_blit);
    JB2Shape *shape = & jb2->get_shape(blit->shapeno);
    blit_list[current_blit] = 0;
    if (! shape->bits) 
      continue;
    GRect rect2(blit->left, blit->bottom, 
      shape->bits->columns(), shape->bits->rows());
    if (rect2.intersect(rect2, prn_rect))
    {
      dict_shapes[blit->shapeno] = 1;
      blit_list[current_blit] = 1;
    }
  }
  write(str,
    "%% --- now doing the foreground\n"
    "gsave DjVuColorSpace setcolorspace\n" );
      // Define font
  write(str,
    "/$DjVuLocalFont 7 dict def\n"
    "$DjVuLocalFont begin\n"
    "/FontType 3 def \n"
    "/FontMatrix [1 0 0 1 0 0] def\n"
    "/FontBBox [0 0 1 .5] def\n"
    "/CharStrings %d dict def\n"
    "/Encoding 2 array def\n"
    "0 1 1 {Encoding exch /.notdef put} for \n"
    "CharStrings begin\n"
    "/.notdef {} def\n",
    num_shapes+1);
  for(int current_shape=0; current_shape<num_shapes; current_shape++)
  {
    if (dict_shapes[current_shape])
    {
      JB2Shape *shape = & jb2->get_shape(current_shape);
      GP<GBitmap> bitmap = shape->bits;
      int rows = bitmap->rows();
      int columns = bitmap->columns();
      int nbytes = (columns+7)/8*rows+1;
      int nrows = rows;
      int nstrings=0;
      if (nbytes>(int)ps_string_size)   //max string length
      {
        nrows=ps_string_size/((columns+7)/8);
        nbytes=(columns+7)/8*nrows+1;
      }
      unsigned char *s_start;
      GPBuffer<unsigned char> gs_start(s_start,nbytes);
      unsigned char *s_ascii;
      GPBuffer<unsigned char> gs_ascii(s_ascii,nbytes*2);
      write(str,"/%d {",current_shape);

      unsigned char *s = s_start;
      for(int current_row=0; current_row<rows; current_row++)
      {  
        unsigned char * row_bits = (*bitmap)[current_row];
        unsigned char acc = 0;
        unsigned char mask = 0;
        for(int current_col=0; current_col<columns; current_col++)
        {
          if (mask == 0)
            mask = 0x80;
          if (row_bits[current_col])
            acc |= mask;
          mask >>= 1;
          if (mask == 0)
          {
            *s=acc;
            s++;
            acc = mask = 0;
          }
        }
        if (mask != 0)
        {
          *s=acc;
          s++;
        }
        if (!((current_row+1)%nrows))
        {
          unsigned char *stop_ascii = ASCII85_encode(s_ascii,s_start,s); 
          *stop_ascii++='\0';
          write(str,"<~%s~> ",s_ascii);
          s=s_start;
          nstrings++;
        }
      }
      if (s!=s_start)
      {
        unsigned char *stop_ascii = ASCII85_encode(s_ascii,s_start,s);
        *stop_ascii++='\0';
        write(str,"<~%s~> ",s_ascii);
          nstrings++;
      }
      if (nstrings==1)
        write(str," %d %d g} def\n", columns, rows);                  
      else
        write(str," %d %d %d gn} def\n", columns, rows,nstrings);
    }
  }
  write(str, 
    "end\n"
    "/BuildGlyph {\n"
    "  exch /CharStrings get exch\n"
    "  2 copy known not\n"
    "  {pop /.notdef} if\n"
    "  get exec \n"
    "} bind def\n"
    "end\n"
    "/LocalDjVuFont $DjVuLocalFont definefont pop\n"
    "/LocalDjVuFont findfont setfont\n" );
  write(str,
    "-%d -%d translate\n"
    "0 0 moveto\n",
    prn_rect.xmin, prn_rect.ymin);
  // Print the foreground layer
  if (dimg->get_fgpm() && !(options.get_mode()==Options::BW)) 
    print_fg_3layer(str, dimg, prn_rect, blit_list);
  else
    print_fg_2layer(str, dimg, prn_rect, blit_list);        
  write(str, "/LocalDjVuFont undefinefont grestore\n");
}


void 
DjVuToPS::
print_bg(ByteStream &str, 
         GP<DjVuImage> dimg,
         const GRect &cprn_rect)
{
  GP<GPixmap> pm;
  GRect prn_rect;
  double print_done = 0;
  int red = 0;
  write(str, "%% --- now doing the background\n");
  if (! (red = get_bg_red(dimg)))
    return;
  write(str, 
        "gsave -%d -%d translate\n"
        "/bgred %d def bgred bgred scale\n",
        cprn_rect.xmin % red, 
        cprn_rect.ymin % red, 
        red);
  prn_rect.ymin = (cprn_rect.ymin)/red;
  prn_rect.ymax = (cprn_rect.ymax+red-1)/red;
  prn_rect.xmin = (cprn_rect.xmin)/red;
  prn_rect.xmax = (cprn_rect.xmax+red-1)/red;
  // Display image
  int band_bytes = 125000;
  int band_height = band_bytes/prn_rect.width();
  int buffer_size = band_height*prn_rect.width();
  int ps_chunk_height = 30960/prn_rect.width()+1;
  buffer_size = buffer_size*23/10;
  bool do_color = options.get_color();
  if ((!dimg->is_legal_photo() &&
       !dimg->is_legal_compound())
      || options.get_mode()==Options::BW)
    do_color = false;
  if (do_color) 
    buffer_size *= 3;
  if (do_color)
    write(str, 
          "/bufferR %d string def\n"
          "/bufferG %d string def\n"
          "/bufferB %d string def\n"
          "DjVuColorSpace setcolorspace\n"
          "<< /ImageType 1\n"
          "   /Width %d\n"
          "   /Height %d\n"
          "   /BitsPerComponent 8\n"
          "   /Decode [0 1 0 1 0 1]\n"
          "   /ImageMatrix [1 0 0 1 0 0]\n"
          "   /MultipleDataSources true\n"
          "   /DataSource [ { ReadR } { ReadG } { ReadB } ]\n"
          "   /Interpolate false >> image\n",
          ps_chunk_height*prn_rect.width(),
          ps_chunk_height*prn_rect.width(),
          ps_chunk_height*prn_rect.width(),
          prn_rect.width(), prn_rect.height());
  else
    write(str, 
          "DjVuColorSpace setcolorspace\n"
          "<< /ImageType 1\n"
          "   /Width %d\n"
          "   /Height %d\n"
          "   /BitsPerComponent 8\n"
          "   /Decode [0 1]\n"
          "   /ImageMatrix [1 0 0 1 0 0]\n"
          "   /DataSource currentfile /ASCII85Decode\n"
          "      filter /RunLengthDecode filter\n"
          "   /Interpolate false >> image\n",
          prn_rect.width(), prn_rect.height());
  
  unsigned char *buffer;
  GPBuffer<unsigned char> gbuffer(buffer,buffer_size);
  unsigned char *rle_in;
  GPBuffer<unsigned char> grle_in(rle_in,ps_chunk_height*prn_rect.width());
  unsigned char *rle_out;
  GPBuffer<unsigned char> grle_out(rle_out,2*ps_chunk_height*prn_rect.width());
  {
    // Start storing image in bands
    unsigned char * rle_out_end = rle_out;
    GRect grectBand = prn_rect;
    grectBand.ymax = grectBand.ymin;
    while(grectBand.ymax < prn_rect.ymax)
      {
        GP<GPixmap> pm = 0;
        // Compute next band
        grectBand.ymin=grectBand.ymax;
        grectBand.ymax=grectBand.ymin+band_bytes/grectBand.width();
        if (grectBand.ymax>prn_rect.ymax)
          grectBand.ymax=prn_rect.ymax;
        pm = get_bg_pixmap(dimg, grectBand);
        unsigned char *buf_ptr = buffer;
        if (pm)
          {
            if (do_color)
              {
                int y=0;
                while(y<grectBand.height())
                  {
                    int row, y1;
                    unsigned char *ptr, *ptr1;
                    // Doing R component of current chunk
                    for (row=0,ptr=rle_in,y1=y; 
                         row<ps_chunk_height && y1<grectBand.height(); 
                         row++,y1++)
                      {
                        GPixel *pix = (*pm)[y1];
                        for (int x=grectBand.width(); x>0; x--,pix++)
                          *ptr++ = ramp[pix->r];
                      }
                    ptr1 = RLE_encode(rle_out, rle_in, ptr); 
                    *ptr1++ = 0x80;
                    buf_ptr = ASCII85_encode(buf_ptr, rle_out, ptr1);
                    *buf_ptr++ = '~'; *buf_ptr++ = '>'; *buf_ptr++ = '\n';
                    // Doing G component of current chunk
                    for (row=0,ptr=rle_in,y1=y; 
                         row<ps_chunk_height && y1<grectBand.height(); 
                         row++,y1++)
                      {
                        GPixel *pix = (*pm)[y1];
                        for (int x=grectBand.width(); x>0; x--,pix++)
                          *ptr++ = ramp[pix->g];
                      }
                    ptr1 = RLE_encode(rle_out, rle_in, ptr); 
                    *ptr1++ = 0x80;
                    buf_ptr = ASCII85_encode(buf_ptr, rle_out, ptr1);
                    *buf_ptr++ = '~'; 
                    *buf_ptr++ = '>'; 
                    *buf_ptr++ = '\n';
                    // Doing B component of current chunk
                    for (row=0, ptr=rle_in, y1=y;
                         row<ps_chunk_height && y1<grectBand.height(); 
                         row++,y1++)
                      {
                        GPixel *pix = (*pm)[y1];
                        for (int x=grectBand.width(); x>0; x--,pix++)
                          *ptr++ = ramp[pix->b];
                      }
                    ptr1 = RLE_encode(rle_out, rle_in, ptr);
                    *ptr1++ = 0x80;
                    buf_ptr = ASCII85_encode(buf_ptr, rle_out, ptr1);
                    *buf_ptr++ = '~'; 
                    *buf_ptr++ = '>'; 
                    *buf_ptr++ = '\n';
                    y=y1;
                    if (refresh_cb) 
                      refresh_cb(refresh_cl_data);
                  } //while (y>=0)
              } 
            else
              {
                // Don't use color
                int y=0;
                while(y<grectBand.height())
                  {
                    unsigned char *ptr = rle_in;
                    for(int row=0; 
                        row<ps_chunk_height && y<grectBand.height(); 
                        row++,y++)
                      {
                        GPixel *pix = (*pm)[y];
                        for (int x=grectBand.width(); x>0; x--,pix++)
                          *ptr++ = ramp[GRAY(pix->r,pix->g,pix->b)];
                      }
                    rle_out_end = RLE_encode(rle_out_end, rle_in, ptr);
                    unsigned char *encode_to 
                      = rle_out+(rle_out_end-rle_out)/4*4;
                    int bytes_left = rle_out_end-encode_to;
                    buf_ptr = ASCII85_encode(buf_ptr, rle_out, encode_to);
                    *buf_ptr++ = '\n';
                    memcpy(rle_out, encode_to, bytes_left);
                    rle_out_end = rle_out+bytes_left;
                    if (refresh_cb) 
                      refresh_cb(refresh_cl_data);
                  }
              }
          } // if (pm)
        str.writall(buffer, buf_ptr-buffer);
        if (prn_progress_cb)
          {
            double done=(double)(grectBand.ymax 
                                 - prn_rect.ymin)/prn_rect.height();
            if ((int) (20*print_done)!=(int) (20*done))
              {
                print_done=done;
                prn_progress_cb(done, prn_progress_cl_data);
              }
          }
      } // while(grectBand.yax<grect.ymax)
    if (! do_color)
      {
        unsigned char * buf_ptr = buffer;
        *rle_out_end++ = 0x80;
        buf_ptr = ASCII85_encode(buf_ptr, rle_out, rle_out_end);
        *buf_ptr++='~'; 
        *buf_ptr++='>'; 
        *buf_ptr++='\n';
        str.writall(buffer, buf_ptr-buffer);
      }
  } 
  //restore the scaling
  write(str, "grestore\n");
}

void
DjVuToPS::
print_image_lev1(ByteStream &str, 
                 GP<DjVuImage> dimg,
                 const GRect &prn_rect)
{         
  double print_done=0;
  GRect all(0,0, dimg->get_width(),dimg->get_height());
  GP<GPixmap> pm;
  GP<GBitmap> bm;
  GRect test(0,0,1,1);
  if (options.get_mode() == Options::FORE)
    pm = dimg->get_fg_pixmap(test, all);
  else if (options.get_mode() == Options::BACK)
    pm = dimg->get_bg_pixmap(test, all);
  else if (options.get_mode() != Options::BW)
    pm = dimg->get_pixmap(test, all);
  if (! pm)
    bm = dimg->get_bitmap(test,all);
  if (! pm && ! bm)
    return;
  write(str,
        "%% --- now doing a level 1 image\n"
        "gsave\n");
  // Display image
  int band_bytes=125000;
  int band_height = band_bytes/prn_rect.width();
  int buffer_size = band_height*prn_rect.width();
  buffer_size = buffer_size*21/10;
  bool do_color = false;
  bool do_color_or_gray = false;
  if (pm && (options.get_mode() != Options::BW))
    do_color_or_gray = true;
  if (do_color_or_gray && options.get_color())
    do_color = true;
  if (do_color) 
    buffer_size *= 3;
  if (do_color)
    write(str, "/buffer24 %d string def\n", 3*prn_rect.width());
  if (do_color_or_gray)
    write(str, "/buffer8 %d string def\n", prn_rect.width());
  else
    write(str, "/buffer8 %d string def\n", (prn_rect.width()+7)/8);
  if (do_color)
    {
      write(str,
            "%d %d 8 [ 1 0 0 1 0 0 ]\n"
            "{ ColorProc } false 3 ColorImage\n",
            prn_rect.width(), prn_rect.height());
    } 
  else if (do_color_or_gray)
    {
      write(str,
            "%d %d 8 [ 1 0 0 1 0 0 ]\n"
            "{ currentfile buffer8 readhexstring pop } image\n",
            prn_rect.width(), prn_rect.height());
    } 
  else
    {
      write(str,
            "%d %d 1 [ 1 0 0 1 0 0 ]\n"
            "{ currentfile buffer8 readhexstring pop } image\n",
            prn_rect.width(), prn_rect.height());
    }
  unsigned char * buffer;
  GPBuffer<unsigned char> gbuffer(buffer,buffer_size);
    {
      // Start storing image in bands
      GRect grectBand = prn_rect;
      grectBand.ymax = grectBand.ymin;
      while(grectBand.ymax < prn_rect.ymax)
        {
          // Compute next band
          grectBand.ymin = grectBand.ymax;
          grectBand.ymax = grectBand.ymin+band_bytes/grectBand.width();
          if (grectBand.ymax > prn_rect.ymax)
            grectBand.ymax = prn_rect.ymax;
          GRect all(0,0, dimg->get_width(),dimg->get_height());
          pm = 0;
          bm = 0;
          if (do_color_or_gray)
            {
              if (options.get_mode() == Options::FORE)
                pm = dimg->get_fg_pixmap(grectBand, all);
              else if (options.get_mode() == Options::BACK)
                pm = dimg->get_bg_pixmap(grectBand, all);
              else
                pm = dimg->get_pixmap(grectBand, all);
            }
          else 
            {
              bm = dimg->get_bitmap(grectBand, all);
            }
          // Store next band
          unsigned char *buf_ptr = buffer;
          int symbols=0;
          for (int y=0; y<grectBand.height(); y++)
            {
              if (pm && do_color_or_gray)
                {
                  GPixel *pix = (*pm)[y];
                  for (int x=grectBand.width(); x>0; x--, pix++)
                    {
                      if (do_color)
                        {
                          char *data;
                          data = bin2hex[ramp[pix->r]];
                          *buf_ptr++ = data[0];
                          *buf_ptr++ = data[1];
                          data = bin2hex[ramp[pix->g]];
                          *buf_ptr++ = data[0];
                          *buf_ptr++ = data[1];
                          data = bin2hex[ramp[pix->b]];
                          *buf_ptr++ = data[0];
                          *buf_ptr++ = data[1];
                          symbols += 6;
                        }
                      else
                        {
                          char *data;
                          data = bin2hex[ramp[GRAY(pix->r,pix->g,pix->b)]];
                          *buf_ptr++ = data[0];
                          *buf_ptr++ = data[1];
                          symbols += 2;
                        }
                      if (symbols>70) 
                        { 
                          *buf_ptr++ = '\n'; 
                          symbols=0; 
                        }
                    }
                }
              else if (bm)
                {
                  unsigned char *pix = (*bm)[y];
                  unsigned char acc = 0;
                  unsigned char mask = 0;
                  char *data;
                  for (int x=grectBand.width(); x>0; x--, pix++)
                    {
                      if (mask == 0)
                        mask = 0x80;
                      if (! *pix)
                        acc |= mask;
                      mask >>= 1;
                      if (mask == 0)
                        {
                          data = bin2hex[acc];
                          acc = 0;
                          *buf_ptr++ = data[0];
                          *buf_ptr++ = data[1];
                          symbols += 2;
                          if (symbols>70) 
                            { 
                              *buf_ptr++ = '\n'; 
                              symbols = 0; 
                            }
                        }
                    }
                  if (mask != 0) 
                    {
                      data = bin2hex[acc];
                      *buf_ptr++ = data[0];
                      *buf_ptr++ = data[1];
                      symbols += 2;
                    }
                }
              if (refresh_cb) 
                refresh_cb(refresh_cl_data);
            }
          str.writall(buffer, buf_ptr-buffer);
          if (prn_progress_cb)
            {
              double done=(double) (grectBand.ymax 
                                    - prn_rect.ymin)/prn_rect.height();
              if ((int) (20*print_done)!=(int) (20*done))
                {
                  print_done=done;
                  prn_progress_cb(done, prn_progress_cl_data);
                }
            }
        }
      write(str, "\n");
    } 
  write(str, "grestore\n");
}

void
DjVuToPS::
print_image_lev2(ByteStream &str, 
                 GP<DjVuImage> dimg,
                 const GRect &prn_rect)
{         
  double print_done=0;
  GRect all(0,0, dimg->get_width(),dimg->get_height());
  GP<GPixmap> pm;
  GRect test(0,0,1,1);
  if (options.get_mode() == Options::FORE)
    pm = dimg->get_fg_pixmap(test, all);
  else if (options.get_mode() == Options::BACK)
    pm = dimg->get_bg_pixmap(test, all);
  else if (options.get_mode() != Options::BW)
    pm = dimg->get_pixmap(test, all);
  if (! pm)
    return;
  write(str,
        "%% --- now doing a level 2 image\n"
        "gsave\n");
  // Display image
  int band_bytes=125000;
  int band_height = band_bytes/prn_rect.width();
  int buffer_size = band_height*prn_rect.width();
  int ps_chunk_height = 30960/prn_rect.width()+1;
  buffer_size = buffer_size*21/10 + 32;
  bool do_color = options.get_color();
  if (do_color)
    {
      buffer_size *= 3;
      write(str, 
            "/bufferR %d string def\n"
            "/bufferG %d string def\n"
            "/bufferB %d string def\n"
            "DjVuColorSpace setcolorspace\n"
            "<< /ImageType 1\n"
            "   /Width %d\n"
            "   /Height %d\n"
            "   /BitsPerComponent 8\n"
            "   /Decode [0 1 0 1 0 1]\n"
            "   /ImageMatrix [1 0 0 1 0 0]\n"
            "   /MultipleDataSources true\n"
            "   /DataSource [ { ReadR } { ReadG } { ReadB } ]\n"
            "   /Interpolate false >> image\n",
            ps_chunk_height*prn_rect.width(),
            ps_chunk_height*prn_rect.width(),
            ps_chunk_height*prn_rect.width(),
            prn_rect.width(), prn_rect.height());
    } 
  else
    {
      write(str, 
            "DjVuColorSpace setcolorspace\n"
            "<< /ImageType 1\n"
            "   /Width %d\n"
            "   /Height %d\n"
            "   /BitsPerComponent 8\n"
            "   /Decode [0 1]\n"
            "   /ImageMatrix [1 0 0 1 0 0]\n"
            "   /DataSource currentfile /ASCII85Decode\n"
            "       filter /RunLengthDecode filter\n"
            "   /Interpolate false >> image\n",
            prn_rect.width(), prn_rect.height());
    } 
  unsigned char *buffer;
  GPBuffer<unsigned char> gbuffer(buffer,buffer_size);
  unsigned char *rle_in;
  GPBuffer<unsigned char> grle_in(rle_in,ps_chunk_height*prn_rect.width());
  unsigned char *rle_out;
  GPBuffer<unsigned char> grle_out(rle_out,2*ps_chunk_height*prn_rect.width());
    {
      // Start storing image in bands
      unsigned char * rle_out_end = rle_out;
      GRect grectBand = prn_rect;
      grectBand.ymax = grectBand.ymin;
      while(grectBand.ymax < prn_rect.ymax)
        {
          // Compute next band
          grectBand.ymin = grectBand.ymax;
          grectBand.ymax = grectBand.ymin+band_bytes/grectBand.width();
          if (grectBand.ymax > prn_rect.ymax)
            grectBand.ymax = prn_rect.ymax;
          GRect all(0,0, dimg->get_width(),dimg->get_height());
          pm = 0;
          if (options.get_mode() == Options::FORE)
            pm = dimg->get_fg_pixmap(grectBand, all);
          else if (options.get_mode() == Options::BACK)
            pm = dimg->get_bg_pixmap(grectBand, all);
          else
            pm = dimg->get_pixmap(grectBand, all);
          // Store next band
          unsigned char *buf_ptr = buffer;
          if (do_color && pm)
            {
              int y=0;
              while(y<grectBand.height())
                {
                  int row, y1;
                  unsigned char *ptr, *ptr1;
                  // Doing R component of current chunk
                  for (row=0,ptr=rle_in,y1=y; 
                       row<ps_chunk_height && y1<grectBand.height(); 
                       row++,y1++)
                    {
                      GPixel *pix = (*pm)[y1];
                      for (int x=grectBand.width(); x>0; x--,pix++)
                        *ptr++ = ramp[pix->r];
                    }
                  ptr1 = RLE_encode(rle_out, rle_in, ptr); 
                  *ptr1++ = 0x80;
                  buf_ptr = ASCII85_encode(buf_ptr, rle_out, ptr1);
                  *buf_ptr++ = '~'; *buf_ptr++ = '>'; *buf_ptr++ = '\n';
                  // Doing G component of current chunk
                  for (row=0,ptr=rle_in,y1=y; 
                       row<ps_chunk_height && y1<grectBand.height(); 
                       row++,y1++)
                    {
                      GPixel *pix = (*pm)[y1];
                      for (int x=grectBand.width(); x>0; x--,pix++)
                        *ptr++ = ramp[pix->g];
                    }
                  ptr1 = RLE_encode(rle_out, rle_in, ptr); 
                  *ptr1++ = 0x80;
                  buf_ptr = ASCII85_encode(buf_ptr, rle_out, ptr1);
                  *buf_ptr++ = '~'; 
                  *buf_ptr++ = '>'; 
                  *buf_ptr++ = '\n';
                  // Doing B component of current chunk
                  for (row=0, ptr=rle_in, y1=y;
                       row<ps_chunk_height && y1<grectBand.height(); 
                       row++,y1++)
                    {
                      GPixel *pix = (*pm)[y1];
                      for (int x=grectBand.width(); x>0; x--,pix++)
                        *ptr++ = ramp[pix->b];
                    }
                  ptr1 = RLE_encode(rle_out, rle_in, ptr);
                  *ptr1++ = 0x80;
                  buf_ptr = ASCII85_encode(buf_ptr, rle_out, ptr1);
                  *buf_ptr++ = '~'; 
                  *buf_ptr++ = '>'; 
                  *buf_ptr++ = '\n';
                  y=y1;
                  if (refresh_cb) 
                    refresh_cb(refresh_cl_data);
                } //while (y>=0)
            } 
          else if (pm)
            {
              // Don't use color
              int y=0;
              while(y<grectBand.height())
                {
                  unsigned char *ptr = rle_in;
                  for(int row=0;
                      row<ps_chunk_height && y<grectBand.height(); 
                      row++,y++)
                    {
                      GPixel *pix = (*pm)[y];
                      for (int x=grectBand.width(); x>0; x--,pix++)
                        *ptr++ = ramp[GRAY(pix->r,pix->g,pix->b)];
                    }
                  rle_out_end = RLE_encode(rle_out_end, rle_in, ptr);
                  unsigned char *encode_to = rle_out 
                    + (rle_out_end-rle_out)/4*4;
                  int bytes_left = rle_out_end-encode_to;
                  buf_ptr = ASCII85_encode(buf_ptr, rle_out, encode_to);
                  *buf_ptr++ = '\n';
                  memcpy(rle_out, encode_to, bytes_left);
                  rle_out_end = rle_out+bytes_left;
                  if (refresh_cb) 
                    refresh_cb(refresh_cl_data);
                }
              if (grectBand.ymax >= prn_rect.ymax)
                {
                  *rle_out_end++ = 0x80; // Add EOF marker
                  buf_ptr = ASCII85_encode(buf_ptr, rle_out, rle_out_end);
                  *buf_ptr++ = '~'; 
                  *buf_ptr++ = '>'; 
                  *buf_ptr++ = '\n';
                }
            }
          str.writall(buffer, buf_ptr-buffer);
          if (prn_progress_cb)
            {
              double done=(double) (grectBand.ymax
                                    - prn_rect.ymin)/prn_rect.height();
              if ((int) (20*print_done)!=(int) (20*done))
                {
                  print_done=done;
                  prn_progress_cb(done, prn_progress_cl_data);
                }
            }
        }
      write(str, "\n");
    } 
  write(str, "grestore\n");
}

static void 
get_anno_sub(IFFByteStream &iff, IFFByteStream &out)
{
  GUTF8String chkid;
  while (iff.get_chunk(chkid))
    {
      if (iff.composite())
        get_anno_sub(iff, out);
      else if (chkid == "ANTa" || chkid == "ANTz" ||
               chkid == "TXTa" || chkid == "TXTz"   )
        {
          out.put_chunk(chkid);
          out.copy(*iff.get_bytestream());
          out.close_chunk();
        }
      iff.close_chunk();
    }
}

static GP<ByteStream>
get_anno(GP<DjVuFile> f)
{
  if (! f->anno) 
    {
      GP<ByteStream> bs = f->get_init_data_pool()->get_stream();
      GP<ByteStream> anno = ByteStream::create();
      GP<IFFByteStream> in = IFFByteStream::create(bs);
      GP<IFFByteStream> out = IFFByteStream::create(anno);
      get_anno_sub(*in, *out);
      f->anno = anno;
    }
  f->anno->seek(0);
  return f->anno;
}

static GP<DjVuTXT>
get_text(GP<DjVuFile> file)
{ 
  GUTF8String chkid;
  GP<IFFByteStream> iff = IFFByteStream::create(get_anno(file));
  while (iff->get_chunk(chkid))
    {
      if (chkid == "TXTa") 
        {
          GP<DjVuTXT> txt = DjVuTXT::create();
          txt->decode(iff->get_bytestream());
          return txt;
        }
      else if (chkid == "TXTz") 
        {
          GP<DjVuTXT> txt = DjVuTXT::create();
          GP<ByteStream> bsiff = BSByteStream::create(iff->get_bytestream());
          txt->decode(bsiff);
          return txt;
        }
      iff->close_chunk();
    }
  return 0;
}

static void
print_ps_string(const char *data, int length, ByteStream &out)
{
  while (*data && length>0) 
    {
      int span = 0;
      while (span<length && data[span]>=0x20 && data[span]<0x7f 
             && data[span]!='(' && data[span]!=')' && data[span]!='\\' )
        span++;
      if (span > 0) 
        {
          out.write(data, span);
          data += span;
          length -= span;
        }
      else
        {
          char buffer[5];
          sprintf(buffer,"\\%03o", *(unsigned char*)data);
          out.write(buffer,4);
          data += 1;
          length -= 1;
        }
    }
}

static void
print_txt_sub(DjVuTXT &txt, DjVuTXT::Zone &zone, 
              ByteStream &out,int &lastx,int &lasty)
{
  // Get separator
  char separator = 0;
  switch(zone.ztype)
    {
    case DjVuTXT::COLUMN: 
      separator = DjVuTXT::end_of_column; break;
    case DjVuTXT::REGION: 
      separator = DjVuTXT::end_of_region; break;
    case DjVuTXT::PARAGRAPH: 
      separator = DjVuTXT::end_of_paragraph; break;
    case DjVuTXT::LINE: 
      separator = DjVuTXT::end_of_line; break;
    case DjVuTXT::WORD: 
      separator = ' '; break;
    default:
      separator = 0; break;
    }
  // Zone children
  if (zone.children.isempty()) 
    {
      const char *data = (const char*)txt.textUTF8 + zone.text_start;
      int length = zone.text_length;
      if (data[length-1] == separator)
        length -= 1;
      out.write("( ",2);
      print_ps_string(data,length,out);
      out.write(")",1);
      GUTF8String message;
      int tmpx= zone.rect.xmin-lastx;
      int tmpy= zone.rect.ymin-lasty;
      message.format(" %d %d S \n", tmpx, tmpy);
      lastx=zone.rect.xmin;
      lasty=zone.rect.ymin;
      out.write((const char*)message, message.length());
    }
  else
    {
      if (zone.ztype==DjVuTXT::LINE)
        {
          GUTF8String message;
          message.format("%d F\n",zone.rect.ymax-zone.rect.ymin);
          out.write((const char*)message,message.length());
        }
      for (GPosition pos=zone.children; pos; ++pos)
        print_txt_sub(txt, zone.children[pos], out,lastx,lasty);
    }
}

static void
print_txt(GP<DjVuTXT> txt, 
          ByteStream &out )
{
  if (txt)
    {
      int lastx=0;
      int lasty=0;
      GUTF8String message = 
        "%% -- now doing hidden text\n"
        "gsave -1 -1 0 0 clip 0 0 moveto\n";
      out.write((const char*)message,message.length());
      print_txt_sub(*txt, txt->page_zone, out,lastx,lasty);
      message = 
        "grestore \n";
      out.write((const char*)message,message.length());
    }
}

void
DjVuToPS::
print_image(ByteStream &str, 
            GP<DjVuImage> dimg,
            const GRect &prn_rect, 
            GP<DjVuTXT> txt)
{
  /* Just outputs the specified image. The function assumes, that
     all add-ons (like {\em document setup}, {\em page setup}) are
     already there. It will just output the image. Since
     output of this function will generate PostScript errors when
     used without output of auxiliary functions, it should be
     used carefully. */
  DEBUG_MSG("DjVuToPS::print_image(): Printing DjVuImage to a stream\n");
  DEBUG_MAKE_INDENT(3);
  if (!dimg)
    G_THROW(ERR_MSG("DjVuToPS.empty_image"));
  if (prn_rect.isempty())
    G_THROW(ERR_MSG("DjVuToPS.empty_rect"));
  if (prn_progress_cb)
    prn_progress_cb(0, prn_progress_cl_data);
  // Compute information for chosen display mode
  print_txt(txt, str);
  make_gamma_ramp(dimg);
  if (options.get_level() < 2)
    {
      print_image_lev1(str, dimg, prn_rect);
    }
  else if (options.get_level() < 3 && dimg->get_fgpm())
    {
      switch(options.get_mode())
        {
        case Options::COLOR:
        case Options::FORE:
          print_image_lev2(str, dimg, prn_rect);
          break;
        case Options::BW:
          print_fg(str, dimg, prn_rect);
          break;
        case Options::BACK:
          print_bg(str, dimg, prn_rect);
          break;
        }
    }
  else 
    {
      switch(options.get_mode())
        {
        case Options::COLOR:
          print_bg(str, dimg, prn_rect);
          print_fg(str, dimg, prn_rect);
          break;
        case Options::FORE:
        case Options::BW:
          print_fg(str, dimg, prn_rect);
          break;
        case Options::BACK:
          print_bg(str, dimg, prn_rect);
          break;
        }
    }
  if (prn_progress_cb)
    prn_progress_cb(1, prn_progress_cl_data);
}




// ***********************************************************************
// ******* PUBLIC FUNCTION FOR PRINTING A SINGLE PAGE ********************
// ***********************************************************************




void
DjVuToPS::
print(ByteStream &str, 
      GP<DjVuImage> dimg,
      const GRect &prn_rect_in, 
      const GRect &img_rect,
      int override_dpi)
{
  DEBUG_MSG("DjVuToPS::print(): Printing DjVu page to a stream\n");
  DEBUG_MAKE_INDENT(3);
  GRect prn_rect;
  prn_rect.intersect(prn_rect_in, img_rect);
  DEBUG_MSG("prn_rect=(" << prn_rect.xmin << ", " << prn_rect.ymin << ", " <<
            prn_rect.width() << ", " << prn_rect.height() << ")\n");
  DEBUG_MSG("img_rect=(" << img_rect.xmin << ", " << img_rect.ymin << ", " <<
            img_rect.width() << ", " << img_rect.height() << ")\n");
  if (!dimg)
    G_THROW(ERR_MSG("DjVuToPS.empty_image"));
  if (prn_rect.isempty())
    G_THROW(ERR_MSG("DjVuToPS.empty_rect"));
  if (img_rect.isempty())
    G_THROW(ERR_MSG("DjVuToPS.bad_scale"));
  GRectMapper mapper;
  mapper.set_input(img_rect);
  GRect full_rect(0, 0, dimg->get_width(), dimg->get_height());
  mapper.set_output(full_rect);
  mapper.map(prn_rect);
  int image_dpi =  dimg->get_dpi();
  if (override_dpi>0) 
    image_dpi = override_dpi;
  if (image_dpi <= 0) 
    image_dpi = 300;
  store_doc_prolog(str, 1, (int)(image_dpi), &prn_rect);
  store_doc_setup(str);
  write(str,"%%%%Page: 1 1\n");
  store_page_setup(str, (int)(image_dpi), prn_rect);
  print_image(str, dimg, prn_rect, 0);
  store_page_trailer(str);
  write(str,"showpage\n");
  store_doc_trailer(str);
}




// ***********************************************************************
// *************************** DOCUMENT LEVEL ****************************
// ***********************************************************************


void
DjVuToPS::
parse_range(GP<DjVuDocument> doc, 
             GUTF8String page_range, 
             GList<int> &pages_todo)
{
  int doc_pages = doc->get_pages_num();
  if (!page_range.length())
    page_range.format("1-%d", doc_pages);
  DEBUG_MSG("page_range='" << (const char *)page_range << "'\n");
  int spec = 0;
  int both = 1;
  int start_page = 1;
  int end_page = doc_pages;
  const char *q = (const char*)page_range;
  char *p = (char*)q;
  while (*p)
    {
      while (*p==' ')
        p += 1;
      if (! *p)
        break;
      if (*p>='0' && *p<='9') 
        {
          end_page = strtol(p, &p, 10);
          spec = 1;
        } 
      else if (*p=='$') 
        {
          spec = 1;
          end_page = doc_pages;
          p += 1;
        } 
      else if (both) 
        {
          end_page = 1;
        } 
      else 
        {
          end_page = doc_pages;
        }
      while (*p==' ')
        p += 1;
      if (both)
        {
          start_page = end_page;
          if (*p == '-') 
            {
              p += 1;
              both = 0;
              continue;
            }
        }
      both = 1;
      while (*p==' ')
        p += 1;
      if (*p && *p != ',')
        G_THROW(ERR_MSG("DjVuToPS.bad_range") 
                + GUTF8String("\t") + GUTF8String(p) );
      if (*p == ',')
        p += 1;
      if (! spec)
        G_THROW(ERR_MSG("DjVuToPS.bad_range") 
                + GUTF8String("\t") + page_range );
      spec = 0;
      if (end_page < 0)
        end_page = 0;
      if (start_page < 0)
        start_page = 0;
      if (end_page > doc_pages)
        end_page = doc_pages;
      if (start_page > doc_pages)
        start_page = doc_pages;
      if (start_page <= end_page)
        for(int page_num=start_page; page_num<=end_page; page_num++)
          pages_todo.append(page_num-1);
      else
        for(int page_num=start_page; page_num>=end_page; page_num--)
          pages_todo.append(page_num-1);
    }
}

class DjVuToPS::DecodePort : public DjVuPort
{
protected:
  DecodePort(void);
public:
  static GP<DecodePort> create(void);
  GEvent decode_event;
  bool decode_event_received;
  double decode_done;
  GURL decode_page_url;
  virtual void notify_file_flags_changed(const DjVuFile*,long,long);
  virtual void notify_decode_progress(const DjVuPort*,double);
};

DjVuToPS::DecodePort::
DecodePort(void)
  : decode_event_received(false),
    decode_done((double)0) 
{
}

GP<DjVuToPS::DecodePort> 
DjVuToPS::DecodePort::
create(void)
{
  return new DecodePort;
}

void 
DjVuToPS::DecodePort::
notify_file_flags_changed(const DjVuFile *source, 
                          long set_mask, long clr_mask)
{
  // WARNING! This function is called from another thread
  if (set_mask & (DjVuFile::DECODE_OK | 
                  DjVuFile::DECODE_FAILED | 
                  DjVuFile::DECODE_STOPPED ))
    {
      if (source->get_url() == decode_page_url)
        {
          decode_event_received=true;
          decode_event.set();
        }
    }
}

void 
DjVuToPS::DecodePort::
notify_decode_progress(const DjVuPort *source, double done)
{
  // WARNING! This function is called from another thread
  if (source->inherits("DjVuFile"))
    {
      DjVuFile * file=(DjVuFile *) source;
      if (file->get_url()==decode_page_url)
        if ((int) (decode_done*20)!=(int) (done*20))
          {
            decode_done=done;
            decode_event_received=true;
            decode_event.set();
          }
    }
}

void 
DjVuToPS::
set_refresh_cb(void (*_refresh_cb)(void*), void *_refresh_cl_data)
{
  refresh_cb = _refresh_cb;
  refresh_cl_data = _refresh_cl_data;
}

void 
DjVuToPS::
set_prn_progress_cb(void (*_prn_progress_cb)(double, void *),
                    void *_prn_progress_cl_data)
{
  prn_progress_cb=_prn_progress_cb;
  prn_progress_cl_data=_prn_progress_cl_data;
}

void 
DjVuToPS::
set_dec_progress_cb(void (*_dec_progress_cb)(double, void *),
                    void *_dec_progress_cl_data)
{
  dec_progress_cb=_dec_progress_cb;
  dec_progress_cl_data=_dec_progress_cl_data;
}

void 
DjVuToPS::
set_info_cb(void (*_info_cb)(int, int, int, Stage, void*),
            void *_info_cl_data)
{
  info_cb=_info_cb;
  info_cl_data=_info_cl_data;
}

GP<DjVuImage>
DjVuToPS::
decode_page(GP<DjVuDocument> doc, 
            int page_num, int cnt, int todo)
{
  DEBUG_MSG("processing page #" << page_num << "\n");
  if (! port)
    {
      port = DecodePort::create();
      DjVuPort::get_portcaster()->add_route((DjVuDocument*)doc, port);
    }
  port->decode_event_received = false;
  port->decode_done = 0;
  GP<DjVuFile> djvu_file;
  GP<DjVuImage> dimg;
  if (page_num >= 0 && page_num < doc->get_pages_num())
    djvu_file = doc->get_djvu_file(page_num);
  if (! djvu_file )
    return 0;
  if (djvu_file->is_decode_ok())
    return doc->get_page(page_num, false);
  // This is the best place to call info_cb(). Note, that
  // get_page() will start decoding if necessary, and will not
  // return until the decoding is over in a single threaded
  // environment. That's why we call get_djvu_file() first.
  if (info_cb)
    info_cb(page_num, cnt, todo, DECODING, info_cl_data);
  // Do NOT decode the page synchronously here!!!
  // The plugin will deadlock otherwise.
  dimg = doc->get_page(page_num, false);
  djvu_file = dimg->get_djvu_file();
  port->decode_page_url = djvu_file->get_url();
  if (djvu_file->is_decode_ok())
    return dimg;
  DEBUG_MSG("decoding\n");
  if (dec_progress_cb)
    dec_progress_cb(0, dec_progress_cl_data);
  while(! djvu_file->is_decode_ok())
    {
      while(!port->decode_event_received && 
            !djvu_file->is_decode_ok())
        {
          port->decode_event.wait(250);
          if (refresh_cb) 
            refresh_cb(refresh_cl_data);
        }
      port->decode_event_received = false;
      if (djvu_file->is_decode_failed() || 
          djvu_file->is_decode_stopped())
        G_THROW(ERR_MSG("DjVuToPS.no_image") 
                + GUTF8String("\t") 
                + GUTF8String(page_num));
      if (dec_progress_cb)
        dec_progress_cb(port->decode_done, dec_progress_cl_data);
    }
  if (dec_progress_cb)
    dec_progress_cb(1, dec_progress_cl_data);
  return dimg;
}

void
DjVuToPS::
process_single_page(ByteStream &str, 
                    GP<DjVuDocument> doc,
                    int page_num, int cnt, int todo,
                    int magic)
{
  GP<DjVuTXT> txt;
  GP<DjVuImage> dimg;
  dimg = decode_page(doc, page_num, cnt, todo);
  if (options.get_text())
    txt = get_text(dimg->get_djvu_file());
  if (info_cb)
    info_cb(page_num, cnt, todo, PRINTING, info_cl_data);
  if (!magic)
    write(str, "%%%%Page: %d %d\n", page_num+1, cnt+1);
  if (dimg)
    {
      int dpi = dimg->get_dpi();
      dpi = ((dpi <= 0) ? 300 : dpi);
      GRect img_rect(0, 0, dimg->get_width(), dimg->get_height());
      store_page_setup(str, dpi, img_rect, magic);
      print_image(str, dimg, img_rect,txt);
      store_page_trailer(str);
    }
  if (!magic)
    write(str,"showpage\n");
}


struct pdata {
  int page1, page2;
  int smax, spos;
  int offset;
};

void 
DjVuToPS::
process_double_page(ByteStream &str, 
                    GP<DjVuDocument> doc,
                    void *v, int cnt, int todo)
{
  const pdata *inf = (const pdata*)v;
  int off = abs(inf->offset);
  write(str,
        "%%%%Page: (%d,%d) %d\n"
        "gsave\n"
        "/fold-dict 8 dict dup 3 1 roll def begin\n"
        " clippath pathbbox newpath pop pop translate\n"
        " clippath pathbbox newpath 4 2 roll pop pop\n"
        " /ph exch def\n"
        " /pw exch def\n"
        " /w ph %d sub 2 div def\n"
        " /m1 %d def\n"
        " /m2 %d def\n"
        "end\n",
        inf->page1 + 1, inf->page2 + 1, cnt,
        2 * (off + options.get_bookletfold(inf->smax-1)),
        inf->offset + options.get_bookletfold(inf->spos),
        inf->offset - options.get_bookletfold(inf->spos));
  if (options.get_cropmarks())
    write(str,
          "%% -- folding marks\n"
          "fold-dict begin\n"
          " 0 setgray 0.5 setlinewidth\n"
          " ph m1 m2 add add 2 div dup\n"
          " 0 exch moveto 36 0 rlineto stroke\n"
          " pw exch moveto -36 0 rlineto stroke\n"
          "end\n");
  write(str,
        "%% -- first page\n"
        "gsave fold-dict begin\n"
        " 0 ph 2 div w add m1 add translate 270 rotate\n"
        " 0 0 w pw rectclip end\n");
  if (inf->page1 >= 0)
    process_single_page(str, doc, inf->page1, cnt*2, todo*2, +1);
  write(str,
        "grestore\n"
        "%% -- second page\n"
        "gsave fold-dict begin\n"
        " 0 ph 2 div m2 add translate 270 rotate\n"
        " 0 0 w pw rectclip end\n");
  if (inf->page2 >= 0)
    process_single_page(str, doc, inf->page2, cnt*2+1, todo*2, -1);
  write(str,
        "grestore\n"
        "grestore\n"
        "showpage\n");
}

static void
booklet_order(GList<int>& pages, int smax)
{
  // -- make a multiple of four
  while (pages.size() & 0x3)
    pages.append(-1);
  // -- copy to array
  int i = 0;
  int n = pages.size();
  GTArray<int> p(0,n-1);
  for (GPosition pos=pages; pos; ++pos)
    p[i++] = pages[pos];
  // -- rebuild
  pages.empty();
  for (i=0; i<n; i+=smax)
    {
      int lo = i;
      int hi = i+smax-1;
      if (hi >= n)
        hi = n-1;
      while (lo < hi)
        {
          pages.append(p[hi--]);
          pages.append(p[lo++]);
          pages.append(p[lo++]);
          pages.append(p[hi--]);
        }
    }
}


// ***********************************************************************
// ******* PUBLIC FUNCTIONS FOR PRINTING MULTIPLE PAGES ******************
// ***********************************************************************



void
DjVuToPS::
print(ByteStream &str, 
      GP<DjVuDocument> doc, 
      GUTF8String page_range)
{
  DEBUG_MSG("DjVuToPS::print(): Printing DjVu document\n");
  DEBUG_MAKE_INDENT(3);
  // Get page range
  GList<int> pages_todo;
  parse_range(doc, page_range, pages_todo);
  int todo = pages_todo.size();
  if (options.get_format()==Options::EPS)
    {
      /* Encapsulated Postscript mode */
      if (todo != 1)
        G_THROW(ERR_MSG("DjVuToPS.only_one_page"));
      GPosition pos = pages_todo;
      int page_num = pages_todo[pos];
      GP<DjVuImage> dimg = decode_page(doc,page_num,0,todo);
      if (! dimg)
        G_THROW(ERR_MSG("DjVuToPS.no_image") + GUTF8String("\t1"));
      GRect bbox(0, 0, dimg->get_width(), dimg->get_height());
      store_doc_prolog(str, 1, dimg->get_dpi(), &bbox);
      store_doc_setup(str);
      process_single_page(str, doc, page_num, 0, todo, 0);
    }
  else if (options.get_bookletmode()==Options::OFF)
    {
      /* Normal mode */
      int cnt = 0;
      store_doc_prolog(str, todo, 0, 0);
      store_doc_setup(str);
      for(GPosition pos = pages_todo; pos; ++pos)
        process_single_page(str,doc,pages_todo[pos],cnt++,todo,0);
      store_doc_trailer(str);
    }
  else
    {
      /* Booklet mode */
      int sheets_left = (todo+3)/4;
      int sides_todo = sheets_left;
      if (options.get_bookletmode() == Options::RECTOVERSO)
        sides_todo *= 2;
      int sheets_max = (options.get_bookletmax()+3)/4;
      if (! sheets_max)
        sheets_max = sheets_left;
      // -- reorder pages
      booklet_order(pages_todo, sheets_max*4);
      // -- print
      int sides = 0;
      int sheetpos = sheets_max;
      store_doc_prolog(str, sides_todo, 0, 0);
      store_doc_setup(str);
      for (GPosition p=pages_todo; p; ++p)
        {
          struct pdata inf;
          inf.page1 = pages_todo[p]; 
          inf.page2 = pages_todo[++p]; 
          inf.smax = sheets_max;
          inf.spos = --sheetpos;
          inf.offset = options.get_bookletalign();
          if (options.get_bookletmode() != Options::VERSO)
            process_double_page(str,doc,(void*)&inf,sides++,sides_todo);
          inf.page1 = pages_todo[++p]; 
          inf.page2 = pages_todo[++p]; 
          inf.offset = -inf.offset;
          if (options.get_bookletmode() != Options::RECTO)
            process_double_page(str,doc,(void*)&inf,sides++,sides_todo);
          sheets_left -= 1;
          if (sheetpos <= 0)
            sheetpos = ((sheets_max<sheets_left) ? sheets_max : sheets_left);
        }
      store_doc_trailer(str);
    }
}


void
DjVuToPS::
print(ByteStream &str, GP<DjVuDocument> doc)
{
  GUTF8String dummy;
  print(str,doc,dummy);
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

