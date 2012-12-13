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

#ifndef _GSTRING_H_
#define _GSTRING_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

/** @name GString.h

    Files #"GString.h"# and #"GString.cpp"# implement a general
    purpose string class \Ref{GBaseString}, with dirived types
    \Ref{GUTF8String} and \Ref{GNativeString} for UTF8 MBS encoding
    and the current Native MBS encoding respectively.  This
    implementation relies on smart pointers (see
    \Ref{GSmartPointer.h}).

    {\bf Historical Comments} --- At some point during the DjVu
    research era, it became clear that C++ compilers rarely provided
    portable libraries. We then decided to avoid fancy classes (like
    #iostream# or #string#) and to rely only on the good old C
    library.  A good string class however is very useful.  We had
    already randomly picked letter 'G' to prefix class names and we
    logically derived the new class name.  Native English speakers
    kept laughing in hiding.  This is ironic because we completely
    forgot this letter 'G' when creating more challenging things
    like the ZP Coder or the IW44 wavelets.  

    {\bf Later Changes} 
    When converting to I18N, we (Lizardtech) decided that two string classes
    where needing, replacing the original GString with \Ref{GUTF8String} and
    \Ref{GNativeString}.

    @memo
    General purpose string class.
    @author
    L\'eon Bottou <leonb@research.att.com> -- initial implementation.\\

// From: Leon Bottou, 1/31/2002
// This file has very little to do with my initial implementation.
// It has been practically rewritten by Lizardtech for i18n changes.
// My original implementation was very small in comparison
// <http://prdownloads.sourceforge.net/djvu/DjVu2_2b-src.tgz>.
// In my opinion, the duplication of the string classes is a failed
// attempt to use the type system to enforce coding policies.
// This could be fixed.  But there are better things to do in djvulibre.
    
*/
//@{


#include "DjVuGlobal.h"
#include "GContainer.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef WIN32
# include <windows.h>
# define HAS_WCHAR 1
# define HAS_WCTYPE 1
# define HAS_MBSTATE 1
#endif

#if HAS_WCHAR
# if !defined(AUTOCONF) || HAVE_WCHAR_H
#  include <wchar.h>
# endif
#endif

#if HAVE_STDINT_H
# include <stdint.h>
#elif HAVE_INTTYPES_H
# include <inttypes.h>
#else
# ifdef WIN32
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
# else
# pragma message("Please verify defs for uint32_t and uint16_t")
typedef unsigned int   uint32_t // verify
typedef unsigned short uint16_t // verify
# endif
#endif

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

#if !HAS_MBSTATE
# ifndef HAVE_MBSTATE_T
typedef int mbstate_t;
# endif
#endif

class GBaseString;
class GUTF8String;
class GNativeString;

// Internal string representation.
class DJVUAPI GStringRep : public GPEnabled
{
public:
  enum EncodeType { XUCS4, XUCS4BE, XUCS4LE, XUCS4_2143, XUCS4_3412,
    XUTF16, XUTF16BE, XUTF16LE, XUTF8, XEBCDIC, XOTHER } ;

  enum EscapeMode { UNKNOWN_ESCAPED=0,  IS_ESCAPED=1, NOT_ESCAPED=2 };

  class UTF8;
  friend class UTF8;
  class Unicode;
  friend class Unicode;

  class ChangeLocale;
#if HAS_WCHAR
  class Native;
  friend class Native;
#endif // HAS_WCHAR
  friend class GBaseString;
  friend class GUTF8String;
  friend class GNativeString;
  friend DJVUAPI unsigned int hash(const GBaseString &ref);

public:
  // default constructor
  GStringRep(void);
  // virtual destructor
  virtual ~GStringRep();

    // Other virtual methods.
      // Create an empty string.
  virtual GP<GStringRep> blank(const unsigned int sz) const = 0;
      // Create a duplicate at the given size.
  GP<GStringRep>  getbuf(int n) const;
      // Change the value of one of the bytes.
  GP<GStringRep> setat(int n, char ch) const;
      // Append a string.
  virtual GP<GStringRep> append(const GP<GStringRep> &s2) const = 0;
      // Test if isUTF8.
  virtual bool isUTF8(void) const { return false; }
      // Test if Native.
  virtual bool isNative(void) const { return false; }
      // Convert to Native.
  virtual GP<GStringRep> toNative(
    const EscapeMode escape=UNKNOWN_ESCAPED ) const = 0;
      // Convert to UTF8.
  virtual GP<GStringRep> toUTF8(const bool nothrow=false) const = 0;
      // Convert to same as current class.
  virtual GP<GStringRep> toThis(
    const GP<GStringRep> &rep,const GP<GStringRep> &locale=0) const = 0;
      // Compare with #s2#.
  virtual int cmp(const GP<GStringRep> &s2,const int len=(-1)) const = 0;

  // Convert strings to numbers.
  virtual int toInt(void) const = 0;
  virtual long int toLong(
    const int pos, int &endpos, const int base=10) const = 0;
  virtual unsigned long toULong(
    const int pos, int &endpos, const int base=10) const = 0;
  virtual double toDouble(const int pos, int &endpos) const = 0;

  // return the position of the next character
  int nextChar( const int from=0 ) const;

  // return next non space position
  int nextNonSpace( const int from=0, const int len=(-1) ) const;

  // return next white space position
  int nextSpace( const int from=0, const int len=(-1) ) const;

  // return the position after the last non-whitespace character.
  int firstEndSpace( int from=0, const int len=(-1) ) const;

    // Create an empty string.
  template <class TYPE> static GP<GStringRep> create(
    const unsigned int sz,TYPE *);
    // Creates with a strdup string.
  GP<GStringRep> strdup(const char *s) const;

    // Creates by appending to the current string
  GP<GStringRep> append(const char *s2) const;

    // Creates with a concat operation.
  GP<GStringRep> concat(const GP<GStringRep> &s1,const GP<GStringRep> &s2) const;
  GP<GStringRep> concat(const char *s1,const GP<GStringRep> &s2) const;
  GP<GStringRep> concat(const GP<GStringRep> &s1,const char *s2) const;
  GP<GStringRep> concat(const char *s1,const char *s2) const;

   /* Creates with a strdup and substr.  Negative values have strlen(s)+1
      added to them.
   */
  GP<GStringRep> substr(
    const char *s,const int start,const int length=(-1)) const;

  GP<GStringRep> substr(
    const uint16_t *s,const int start,const int length=(-1)) const;

  GP<GStringRep> substr(
    const uint32_t *s,const int start,const int length=(-1)) const;

  /** Initializes a string with a formatted string (as in #vprintf#).  The
      string is re-initialized with the characters generated according to the
      specified format #fmt# and using the optional arguments.  See the ANSI-C
      function #vprintf()# for more information. The current implementation
      will cause a segmentation violation if the resulting string is longer
      than 32768 characters. */
  GP<GStringRep> vformat(va_list args) const;
  // -- SEARCHING

  static GP<GStringRep> UTF8ToNative( const char *s,
    const EscapeMode escape=UNKNOWN_ESCAPED );
  static GP<GStringRep> NativeToUTF8( const char *s );

  // Creates an uppercase version of the current string.
  GP<GStringRep> upcase(void) const;
  // Creates a lowercase version of the current string.
  GP<GStringRep> downcase(void) const;

  /** Returns the next UCS4 character, and updates the pointer s. */
  static uint32_t UTF8toUCS4(
    unsigned char const *&s, void const * const endptr );

  /** Returns the number of bytes in next UCS4 character,
      and sets #w# to the next UCS4 chacter.  */
  static int UTF8toUCS4(
    uint32_t &w, unsigned char const s[], void const * const endptr )
  { unsigned char const *r=s;w=UTF8toUCS4(r,endptr);return (int)((size_t)r-(size_t)s); }

  /** Returns the next UCS4 word from the UTF16 string. */
  static int UTF16toUCS4(
     uint32_t &w, uint16_t const * const s,void const * const eptr);

  static int UCS4toUTF16(
    uint32_t w, uint16_t &w1, uint16_t &w2);

  int cmp(const char *s2, const int len=(-1)) const;
  static int cmp(
    const GP<GStringRep> &s1, const GP<GStringRep> &s2, const int len=(-1)) ;
  static int cmp(
    const GP<GStringRep> &s1, const char *s2, const int len=(-1));
  static int cmp(
    const char *s1, const GP<GStringRep> &s2, const int len=(-1));
  static int cmp(
    const char *s1, const char *s2, const int len=(-1));

  // Lookup the next character, and return the position of the next character.
  int getUCS4(uint32_t &w, const int from) const;

  virtual unsigned char *UCS4toString(
    const uint32_t w, unsigned char *ptr, mbstate_t *ps=0) const = 0;

  static unsigned char *UCS4toUTF8(
    const uint32_t w,unsigned char *ptr);

  static unsigned char *UCS4toNative(
    const uint32_t w,unsigned char *ptr, mbstate_t *ps);

  int search(char c, int from=0) const;

  int search(char const *str, int from=0) const;

  int rsearch(char c, int from=0) const;

  int rsearch(char const *str, int from=0) const;

  int contains(char const accept[], int from=0) const;

  int rcontains(char const accept[], int from=0) const;

protected:
  // Return the next character and increment the source pointer.
  virtual uint32_t getValidUCS4(const char *&source) const = 0;

  GP<GStringRep> tocase(
    bool (*xiswcase)(const unsigned long wc),
    unsigned long (*xtowcase)(const unsigned long wc)) const;

  // Tests if the specified character passes the xiswtest.  If so, the
  // return pointer is incremented to the next character, otherwise the
  // specified #ptr# is returned.
  const char * isCharType( bool (*xiswtest)(const unsigned long wc), const char *ptr,
    const bool reverse=false) const;

  // Find the next character position that passes the isCharType test.
  int nextCharType(
    bool (*xiswtest)(const unsigned long wc),const int from,const int len,
    const bool reverse=false) const;

  static bool giswspace(const unsigned long w);
  static bool giswupper(const unsigned long w);
  static bool giswlower(const unsigned long w);
  static unsigned long gtowupper(const unsigned long w);
  static unsigned long gtowlower(const unsigned long w);

  virtual void set_remainder( void const * const buf, const unsigned int size,
    const EncodeType encodetype);
  virtual void set_remainder( void const * const buf, const unsigned int size,
    const GP<GStringRep> &encoding );
  virtual void set_remainder ( const GP<Unicode> &remainder );

  virtual GP<Unicode> get_remainder( void ) const;

public:
  /* Returns a copy of this string with characters used in XML with
      '<'  to "&lt;", '>'  to "&gt;",  '&' to "&amp;" '\'' to
      "&apos;", and  '\"' to  "&quot;".   Characters 0x01 through
      0x1f are also escaped. */
  GP<GStringRep> toEscaped( const bool tosevenbit ) const;

  // Tests if a string is legally encoded in the current character set.
  virtual bool is_valid(void) const = 0;
#if HAS_WCHAR
  virtual int ncopy(wchar_t * const buf, const int buflen) const = 0;
#endif
protected:

// Actual string data.
  int  size;
  char *data;
};

class DJVUAPI GStringRep::UTF8 : public GStringRep
{
public:
  // default constructor
  UTF8(void);
  // virtual destructor
  virtual ~UTF8();

    // Other virtual methods.
  virtual GP<GStringRep> blank(const unsigned int sz = 0) const;
  virtual GP<GStringRep> append(const GP<GStringRep> &s2) const;
      // Test if Native.
  virtual bool isUTF8(void) const;
      // Convert to Native.
  virtual GP<GStringRep> toNative(
    const EscapeMode escape=UNKNOWN_ESCAPED) const;
      // Convert to UTF8.
  virtual GP<GStringRep> toUTF8(const bool nothrow=false) const;
      // Convert to same as current class.
  virtual GP<GStringRep> toThis(
    const GP<GStringRep> &rep,const GP<GStringRep> &) const;
      // Compare with #s2#.
  virtual int cmp(const GP<GStringRep> &s2,const int len=(-1)) const;

  static GP<GStringRep> create(const unsigned int sz = 0);

  // Convert strings to numbers.
  virtual int toInt(void) const;
  virtual long int toLong(
    const int pos, int &endpos, const int base=10) const;
  virtual unsigned long toULong(
    const int pos, int &endpos, const int base=10) const;
  virtual double toDouble(
    const int pos, int &endpos) const;

    // Create a strdup string.
  static GP<GStringRep> create(const char *s);

   // Creates with a concat operation.
  static GP<GStringRep> create(
    const GP<GStringRep> &s1,const GP<GStringRep> &s2);
  static GP<GStringRep> create( const GP<GStringRep> &s1,const char *s2);
  static GP<GStringRep> create( const char *s1, const GP<GStringRep> &s2);
  static GP<GStringRep> create( const char *s1,const char *s2);

    // Create with a strdup and substr operation.
  static GP<GStringRep> create(
    const char *s,const int start,const int length=(-1));

  static GP<GStringRep> create(
    const uint16_t *s,const int start,const int length=(-1));

  static GP<GStringRep> create(
    const uint32_t *s,const int start,const int length=(-1));

  static GP<GStringRep> create_format(const char fmt[],...);
  static GP<GStringRep> create(const char fmt[],va_list& args);

  virtual unsigned char *UCS4toString(
    const uint32_t w,unsigned char *ptr, mbstate_t *ps=0) const;

  // Tests if a string is legally encoded in the current character set.
  virtual bool is_valid(void) const;
#if HAS_WCHAR
  virtual int ncopy(wchar_t * const buf, const int buflen) const;
#endif
  friend class GBaseString;

protected:
  // Return the next character and increment the source pointer.
  virtual uint32_t getValidUCS4(const char *&source) const;
};


/** General purpose character string.
    Each dirivied instance of class #GBaseString# represents a
    character string.  Overloaded operators provide a value semantic
    to #GBaseString# objects.  Conversion operators and constructors
    transparently convert between #GBaseString# objects and
    #const char*# pointers.  The #GBaseString# class has no public
    constructors, since a dirived type should always be used
    to specify the desired multibyte character encoding.

    Functions taking strings as arguments should declare their
    arguments as "#const char*#".  Such functions will work equally
    well with dirived #GBaseString# objects since there is a fast
    conversion operator from the dirivied #GBaseString# objects
    to "#const char*#".  Functions returning strings should return
    #GUTF8String# or #GNativeString# objects because the class will
    automatically manage the necessary memory.

    Characters in the string can be identified by their position.  The
    first character of a string is numbered zero. Negative positions
    represent characters relative to the end of the string (i.e.
    position #-1# accesses the last character of the string,
    position #-2# represents the second last character, etc.)  */

class DJVUAPI GBaseString : protected GP<GStringRep>
{
public:
  enum EscapeMode {
    UNKNOWN_ESCAPED=GStringRep::UNKNOWN_ESCAPED,
    IS_ESCAPED=GStringRep::IS_ESCAPED,
    NOT_ESCAPED=GStringRep::NOT_ESCAPED };

  friend class GUTF8String;
  friend class GNativeString;
protected:
  // Sets the gstr pointer;
  inline void init(void);

  ~GBaseString();
  inline GBaseString &init(const GP<GStringRep> &rep);

  // -- CONSTRUCTORS
  /** Null constructor. Constructs an empty string. */
  GBaseString( void );

public:
  // -- ACCESS
  /** Converts a string into a constant null terminated character
      array.  This conversion operator is very efficient because
      it simply returns a pointer to the internal string data. The
      returned pointer remains valid as long as the string is
      unmodified. */
  operator const char* ( void ) const  ;
  /// Returns the string length.
  unsigned int length( void ) const;
  /** Returns true if and only if the string contains zero characters.
      This operator is useful for conditional expression in control
      structures.
      \begin{verbatim}
         if (! str) { ... }
         while (!! str) { ... }  -- Note the double operator!
      \end{verbatim}
      Class #GBaseString# does not to support syntax
      "#if# #(str)# #{}#" because the required conversion operator
      introduces dangerous ambiguities with certain compilers. */
  bool operator! ( void ) const;

  // -- INDEXING
  /** Returns the character at position #n#. An exception
      \Ref{GException} is thrown if number #n# is not in range #-len#
      to #len-1#, where #len# is the length of the string.  The first
      character of a string is numbered zero.  Negative positions
      represent characters relative to the end of the string. */
  char operator[] (int n) const;
  /// Returns #TRUE# if the string contains an integer number.
  bool is_int(void) const;
  /// Returns #TRUE# if the string contains a float number.
  bool is_float(void) const;

  /** Converts strings between native & UTF8 **/
  GNativeString getUTF82Native( EscapeMode escape=UNKNOWN_ESCAPED ) const;
  GUTF8String getNative2UTF8( void ) const;

  // -- ALTERING
  /// Reinitializes a string with the null string.
  void empty( void );
  // -- SEARCHING
  /** Searches character #c# in the string, starting at position
      #from# and scanning forward until reaching the end of the
      string.  This function returns the position of the matching
      character.  It returns #-1# if character #c# cannot be found. */
  int search(char c, int from=0) const;

  /** Searches sub-string #str# in the string, starting at position
      #from# and scanning forward until reaching the end of the
      string.  This function returns the position of the first
      matching character of the sub-string.  It returns #-1# if
      string #str# cannot be found. */
  int search(const char *str, int from=0) const;

  /** Searches character #c# in the string, starting at position
      #from# and scanning backwards until reaching the beginning of
      the string.  This function returns the position of the matching
      character.  It returns #-1# if character #c# cannot be found. */
  int rsearch(char c, const int from=0) const;
  /** Searches sub-string #str# in the string, starting at position
      #from# and scanning backwards until reaching the beginning of
      the string.  This function returns the position of the first
      matching character of the sub-string. It returns #-1# if
      string #str# cannot be found. */
  int rsearch(const char *str, const int from=0) const;
  /** Searches for any of the specified characters in the accept
      string.  It returns #-1# if the none of the characters and
      be found, otherwise the position of the first match. */
  int contains(const char accept[], const int from=0) const;
  /** Searches for any of the specified characters in the accept
      string.  It returns #-1# if the none of the characters and be
      found, otherwise the position of the last match. */
  int rcontains(const char accept[], const int from=0) const;

  /** Concatenates strings. Returns a string composed by concatenating
      the characters of strings #s1# and #s2#. */
  GUTF8String operator+(const GUTF8String &s2) const;
  GNativeString operator+(const GNativeString &s2) const;

  /** Returns an integer.  Implements i18n atoi.  */
  int toInt(void) const;

  /** Returns a long intenger.  Implments i18n strtol.  */
  long toLong(const int pos, int &endpos, const int base=10) const;

  /** Returns a unsigned long integer.  Implements i18n strtoul. */
  unsigned long toULong(
    const int pos, int &endpos, const int base=10) const;

  /** Returns a double.  Implements the i18n strtod.  */
  double toDouble(
    const int pos, int &endpos ) const;

  /** Returns a long intenger.  Implments i18n strtol.  */
  static long toLong(
    const GUTF8String& src, const int pos, int &endpos, const int base=10);

  static unsigned long toULong(
    const GUTF8String& src, const int pos, int &endpos, const int base=10);

  static double toDouble(
    const GUTF8String& src, const int pos, int &endpos);

  /** Returns a long intenger.  Implments i18n strtol.  */
  static long toLong(
    const GNativeString& src, const int pos, int &endpos, const int base=10);

  static unsigned long toULong(
    const GNativeString& src, const int pos, int &endpos, const int base=10);

  static double toDouble(
    const GNativeString& src, const int pos, int &endpos);

  // -- HASHING

  // -- COMPARISONS
    /** Returns an #int#.  Compares string with #s2# and returns
        sorting order. */
  int cmp(const GBaseString &s2, const int len=(-1)) const;
    /** Returns an #int#.  Compares string with #s2# and returns
        sorting order. */
  int cmp(const char *s2, const int len=(-1)) const;
    /** Returns an #int#.  Compares string with #s2# and returns
        sorting order. */
  int cmp(const char s2) const;
    /** Returns an #int#.  Compares #s2# with #s2# and returns
        sorting order. */
  static int cmp(const char *s1, const char *s2, const int len=(-1));
  /** Returns a boolean. The Standard C strncmp takes two string and
      compares the first N characters.  static bool GBaseString::ncmp
      will compare #s1# with #s2# with the #len# characters starting
      from the beginning of the string. */
  /** String comparison. Returns true if and only if character
      strings #s1# and #s2# are equal (as with #strcmp#.)
    */
  bool operator==(const GBaseString &s2) const;
  bool operator==(const char *s2) const;
  friend bool operator==(const char    *s1, const GBaseString &s2);

  /** String comparison. Returns true if and only if character
      strings #s1# and #s2# are not equal (as with #strcmp#.)
    */
  bool operator!=(const GBaseString &s2) const;
  bool operator!=(const char *s2) const;
  friend bool operator!=(const char *s1, const GBaseString &s2);

  /** String comparison. Returns true if and only if character
      strings #s1# is lexicographically greater than or equal to
      string #s2# (as with #strcmp#.) */
  bool operator>=(const GBaseString &s2) const;
  bool operator>=(const char *s2) const;
  bool operator>=(const char s2) const;
  friend bool operator>=(const char    *s1, const GBaseString &s2);
  friend bool operator>=(const char s1, const GBaseString &s2);

  /** String comparison. Returns true if and only if character
      strings #s1# is lexicographically less than string #s2#
      (as with #strcmp#.)
   */
  bool operator<(const GBaseString &s2) const;
  bool operator<(const char *s2) const;
  bool operator<(const char s2) const;
  friend bool operator<(const char *s1, const GBaseString &s2);
  friend bool operator<(const char s1, const GBaseString &s2);

  /** String comparison. Returns true if and only if character
      strings #s1# is lexicographically greater than string #s2#
      (as with #strcmp#.)
   */
  bool operator> (const GBaseString &s2) const;
  bool operator> (const char *s2) const;
  bool operator> (const char s2) const;
  friend bool operator> (const char    *s1, const GBaseString &s2);
  friend bool operator> (const char s1, const GBaseString &s2);

  /** String comparison. Returns true if and only if character
      strings #s1# is lexicographically less than or equal to string
      #s2# (as with #strcmp#.)
   */
  bool operator<=(const GBaseString &s2) const;
  bool operator<=(const char *s2) const;
  bool operator<=(const char s2) const;
  friend bool operator<=(const char    *s1, const GBaseString &s2);
  friend bool operator<=(const char    s1, const GBaseString &s2);

   /** Returns an integer.  Implements a functional i18n atoi. Note
       that if you pass a GBaseString that is not in Native format
       the results may be disparaging. */

  /** Returns a hash code for the string.  This hashing function
      helps when creating associative maps with string keys (see
      \Ref{GMap}).  This hash code may be reduced to an arbitrary
      range by computing its remainder modulo the upper bound of
      the range. */
  friend DJVUAPI unsigned int hash(const GBaseString &ref);
  // -- HELPERS
  friend class GStringRep;

  /// Returns next non space position.
  int nextNonSpace( const int from=0, const int len=(-1) ) const;

  /// Returns next character position.
  int nextChar( const int from=0 ) const;

  /// Returns next non space position.
  int nextSpace( const int from=0, const int len=(-1) ) const;

  /// return the position after the last non-whitespace character.
  int firstEndSpace( const int from=0,const int len=(-1) ) const;

  /// Tests if the string is legally encoded in the current codepage.
  bool is_valid(void) const;

  /// copy to a wchar_t buffer
#if HAS_WCHAR
  int ncopy(wchar_t * const buf, const int buflen) const;
#endif
protected:
  const char *gstr;
  static void throw_illegal_subscript() no_return;
  static const char *nullstr;
public:
  GNativeString UTF8ToNative(
    const bool currentlocale=false,
    const EscapeMode escape=UNKNOWN_ESCAPED) const;
  GUTF8String NativeToUTF8(void) const;
protected:
  inline int CheckSubscript(int n) const;
};

/** General purpose character string.
    Each instance of class #GUTF8String# represents a character
    string.  Overloaded operators provide a value semantic to
    #GUTF8String# objects.  Conversion operators and constructors
    transparently convert between #GUTF8String# objects and
    #const char*# pointers.

    Functions taking strings as arguments should declare their
    arguments as "#const char*#".  Such functions will work equally
    well with #GUTF8String# objects since there is a fast conversion
    operator from #GUTF8String# to "#const char*#".  Functions
    returning strings should return #GUTF8String# or #GNativeString#
    objects because the class will automatically manage the necessary
    memory.

    Characters in the string can be identified by their position.  The
    first character of a string is numbered zero. Negative positions
    represent characters relative to the end of the string (i.e.
    position #-1# accesses the last character of the string,
    position #-2# represents the second last character, etc.)  */

class DJVUAPI GUTF8String : public GBaseString
{
public:
  ~GUTF8String();
  inline void init(void);

  inline GUTF8String &init(const GP<GStringRep> &rep);

  // -- CONSTRUCTORS
  /** Null constructor. Constructs an empty string. */
  GUTF8String(void);
  /// Constructs a string from a character.
  GUTF8String(const char dat);
  /// Constructs a string from a null terminated character array.
  GUTF8String(const char *str);
  /// Constructs a string from a null terminated character array.
  GUTF8String(const unsigned char *str);
  GUTF8String(const uint16_t *dat);
  GUTF8String(const uint32_t *dat);
  /** Constructs a string from a character array.  Elements of the
      character array #dat# are added into the string until the
      string length reaches #len# or until encountering a null
      character (whichever comes first). */
  GUTF8String(const char *dat, unsigned int len);
  GUTF8String(const uint16_t *dat, unsigned int len);
  GUTF8String(const uint32_t *dat, unsigned int len);

  /// Construct from base class.
  GUTF8String(const GP<GStringRep> &str);
  GUTF8String(const GBaseString &str);
  GUTF8String(const GUTF8String &str);
  GUTF8String(const GNativeString &str);
  /** Constructs a string from a character array.  Elements of the
      character array #dat# are added into the string until the
      string length reaches #len# or until encountering a null
      character (whichever comes first). */
  GUTF8String(const GBaseString &gs, int from, int len);

  /** Copy a null terminated character array. Resets this string
      with the character string contained in the null terminated
      character array #str#. */
  GUTF8String& operator= (const char str);
  GUTF8String& operator= (const char *str);
  inline GUTF8String& operator= (const GP<GStringRep> &str);
  inline GUTF8String& operator= (const GBaseString &str);
  inline GUTF8String& operator= (const GUTF8String &str);
  inline GUTF8String& operator= (const GNativeString &str);

  /** Constructs a string with a formatted string (as in #vprintf#).
      The string is re-initialized with the characters generated
      according to the specified format #fmt# and using the optional
      arguments.  See the ANSI-C function #vprintf()# for more
      information. The current implementation will cause a
      segmentation violation if the resulting string is longer
      than 32768 characters. */
  GUTF8String(const GUTF8String &fmt, va_list &args);

  /// Constructs a string from a character.
  /** Constructs a string with a human-readable representation of
      integer #number#.  The format is similar to format #"%d"# in
      function #printf#. */
  GUTF8String(const int number);

  /** Constructs a string with a human-readable representation of
      floating point number #number#. The format is similar to
      format #"%f"# in function #printf#.  */
  GUTF8String(const double number);


  /** Initializes a string with a formatted string (as in #printf#).
      The string is re-initialized with the characters generated
      according to the specified format #fmt# and using the optional
      arguments.  See the ANSI-C function #printf()# for more
      information. The current implementation will cause a
      segmentation violation if the resulting string is longer
      than 32768 characters. */
  GUTF8String &format(const char *fmt, ... );
  /** Initializes a string with a formatted string (as in #vprintf#).
      The string is re-initialized with the characters generated
      according to the specified format #fmt# and using the optional
      arguments.  See the ANSI-C function #vprintf()# for more
      information. The current implementation will cause a
      segmentation violation if the resulting string is longer
      than 32768 characters. */
  GUTF8String &vformat(const GUTF8String &fmt, va_list &args);

  /** Returns a copy of this string with characters used in XML with
      '<'  to "&lt;", '>'  to "&gt;",  '&' to "&amp;" '\'' to
      "&apos;", and  '\"' to  "&quot;".   Characters 0x01 through
      0x1f are also escaped. */
  GUTF8String toEscaped( const bool tosevenbit=false ) const;

  /** Converts strings containing HTML/XML escaped characters into
      their unescaped forms. Numeric representations of characters
      (e.g., "&#38;" or "&#x26;" for "*") are the only forms
      converted by this function. */
  GUTF8String fromEscaped( void ) const;

  /** Converts strings containing HTML/XML escaped characters
      (e.g., "&lt;" for "<") into their unescaped forms. The
      conversion is partially defined by the ConvMap argument which
      specifies the conversion strings to be recognized. Numeric
      representations of characters (e.g., "&#38;" or "&#x26;"
      for "*") are always converted. */
  GUTF8String fromEscaped(
    const GMap<GUTF8String,GUTF8String> ConvMap ) const;


  // -- CONCATENATION
  /// Appends character #ch# to the string.
  GUTF8String& operator+= (char ch);

  /// Appends the null terminated character array #str# to the string.
  GUTF8String& operator+= (const char *str);
  /// Appends the specified GBaseString to the string.
  GUTF8String& operator+= (const GBaseString &str);

  /** Returns a sub-string.  The sub-string is composed by copying
      #len# characters starting at position #from# in this string.
      The length of the resulting string may be smaller than #len#
      if the specified range is too large. */
  GUTF8String substr(int from, int len/*=(-1)*/) const;

  /** Returns an upper case copy of this string.  The returned string
      contains a copy of the current string with all letters turned
      into upper case letters. */
  GUTF8String upcase( void ) const;
  /** Returns an lower case copy of this string.  The returned string
      contains a copy of the current string with all letters turned
      into lower case letters. */
  GUTF8String downcase( void ) const;

  /** Concatenates strings. Returns a string composed by concatenating
      the characters of strings #s1# and #s2#.
  */
  GUTF8String operator+(const GBaseString &s2) const;
  GUTF8String operator+(const GUTF8String &s2) const;
  GUTF8String operator+(const GNativeString &s2) const;
  GUTF8String operator+(const char *s2) const;
  friend DJVUAPI GUTF8String operator+(const char *s1, const GUTF8String &s2);

  /** Provides a direct access to the string buffer.  Returns a
      pointer for directly accessing the string buffer.  This pointer
      valid remains valid as long as the string is not modified by
      other means.  Positive values for argument #n# represent the
      length of the returned buffer.  The returned string buffer will
      be large enough to hold at least #n# characters plus a null
      character.  If #n# is positive but smaller than the string
      length, the string will be truncated to #n# characters. */
  char *getbuf(int n = -1);
  /** Set the character at position #n# to value #ch#.  An exception
      \Ref{GException} is thrown if number #n# is not in range #-len#
      to #len#, where #len# is the length of the string.  If character
      #ch# is zero, the string is truncated at position #n#.  The
      first character of a string is numbered zero. Negative
      positions represent characters relative to the end of the
      string. If position #n# is equal to the length of the string,
      this function appends character #ch# to the end of the string. */
  void setat(const int n, const char ch);
public:
  typedef enum GStringRep::EncodeType EncodeType;
  static GUTF8String create(void const * const buf,
    const unsigned int size,
    const EncodeType encodetype, const GUTF8String &encoding);
  static GUTF8String create( void const * const buf,
    unsigned int size, const EncodeType encodetype );
  static GUTF8String create( void const * const buf,
    const unsigned int size, const GUTF8String &encoding );
  static GUTF8String create( void const * const buf,
    const unsigned int size, const GP<GStringRep::Unicode> &remainder);
  GP<GStringRep::Unicode> get_remainder(void) const;
  static GUTF8String create( const char *buf, const unsigned int bufsize );
  static GUTF8String create( const uint16_t *buf, const unsigned int bufsize );
  static GUTF8String create( const uint32_t *buf, const unsigned int bufsize );
};


#if !HAS_WCHAR
#define GBaseString GUTF8String
#endif

/** General purpose character string.
    Each instance of class #GNativeString# represents a character
    string.  Overloaded operators provide a value semantic to
    #GNativeString# objects.  Conversion operators and constructors
    transparently convert between #GNativeString# objects and
    #const char*# pointers.

    Functions taking strings as arguments should declare their
    arguments as "#const char*#".  Such functions will work equally
    well with #GNativeString# objects since there is a fast conversion
    operator from #GNativeString# to "#const char*#".  Functions
    returning strings should return #GUTF8String# or #GNativeString#
    objects because the class will automatically manage the necessary
    memory.

    Characters in the string can be identified by their position.  The
    first character of a string is numbered zero. Negative positions
    represent characters relative to the end of the string (i.e.
    position #-1# accesses the last character of the string,
    position #-2# represents the second last character, etc.)  */

class DJVUAPI GNativeString : public GBaseString
{
public:
  ~GNativeString();
  // -- CONSTRUCTORS
  /** Null constructor. Constructs an empty string. */
  GNativeString(void);
  /// Constructs a string from a character.
  GNativeString(const char dat);
  /// Constructs a string from a null terminated character array.
  GNativeString(const char *str);
  /// Constructs a string from a null terminated character array.
  GNativeString(const unsigned char *str);
  GNativeString(const uint16_t *str);
  GNativeString(const uint32_t *str);
  /** Constructs a string from a character array.  Elements of the
      character array #dat# are added into the string until the
      string length reaches #len# or until encountering a null
      character (whichever comes first). */
  GNativeString(const char *dat, unsigned int len);
  GNativeString(const uint16_t *dat, unsigned int len);
  GNativeString(const uint32_t *dat, unsigned int len);
  /// Construct from base class.
  GNativeString(const GP<GStringRep> &str);
  GNativeString(const GBaseString &str);
#if HAS_WCHAR
  GNativeString(const GUTF8String &str);
#endif
  GNativeString(const GNativeString &str);
  /** Constructs a string from a character array.  Elements of the
      character array #dat# are added into the string until the
      string length reaches #len# or until encountering a null
      character (whichever comes first). */
  GNativeString(const GBaseString &gs, int from, int len);

  /** Constructs a string with a formatted string (as in #vprintf#).
      The string is re-initialized with the characters generated
      according to the specified format #fmt# and using the optional
      arguments.  See the ANSI-C function #vprintf()# for more
      information. The current implementation will cause a
      segmentation violation if the resulting string is longer than
      32768 characters. */
  GNativeString(const GNativeString &fmt, va_list &args);

  /** Constructs a string with a human-readable representation of
      integer #number#.  The format is similar to format #"%d"# in
      function #printf#. */
  GNativeString(const int number);

  /** Constructs a string with a human-readable representation of
      floating point number #number#. The format is similar to
      format #"%f"# in function #printf#.  */
  GNativeString(const double number);

#if !HAS_WCHAR
#undef GBaseString
#else
  /// Initialize this string class
  void init(void);

  /// Initialize this string class
  GNativeString &init(const GP<GStringRep> &rep);

  /** Copy a null terminated character array. Resets this string with
      the character string contained in the null terminated character
      array #str#. */
  GNativeString& operator= (const char str);
  GNativeString& operator= (const char *str);
  inline GNativeString& operator= (const GP<GStringRep> &str);
  inline GNativeString& operator= (const GBaseString &str);
  inline GNativeString& operator= (const GUTF8String &str);
  inline GNativeString& operator= (const GNativeString &str);
  // -- CONCATENATION
  /// Appends character #ch# to the string.
  GNativeString& operator+= (char ch);
  /// Appends the null terminated character array #str# to the string.
  GNativeString& operator+= (const char *str);
  /// Appends the specified GBaseString to the string.
  GNativeString& operator+= (const GBaseString &str);

  /** Returns a sub-string.  The sub-string is composed by copying
      #len# characters starting at position #from# in this string.
      The length of the resulting string may be smaller than #len#
      if the specified range is too large. */
  GNativeString substr(int from, int len/*=(-1)*/) const;

  /** Returns an upper case copy of this string.  The returned
      string contains a copy of the current string with all letters
      turned into upper case letters. */
  GNativeString upcase( void ) const;
  /** Returns an lower case copy of this string.  The returned
      string contains a copy of the current string with all letters
      turned into lower case letters. */
  GNativeString downcase( void ) const;


  GNativeString operator+(const GBaseString &s2) const;
  GNativeString operator+(const GNativeString &s2) const;
  GUTF8String operator+(const GUTF8String &s2) const;
  GNativeString operator+(const char *s2) const;
  friend DJVUAPI GNativeString operator+(const char *s1, const GNativeString &s2);

  /** Initializes a string with a formatted string (as in #printf#).
      The string is re-initialized with the characters generated
      according to the specified format #fmt# and using the optional
      arguments.  See the ANSI-C function #printf()# for more
      information. The current implementation will cause a
      segmentation violation if the resulting string is longer than
      32768 characters. */
  GNativeString &format(const char *fmt, ... );
  /** Initializes a string with a formatted string (as in #vprintf#).
      The string is re-initialized with the characters generated
      according to the specified format #fmt# and using the optional
      arguments.  See the ANSI-C function #vprintf()# for more
      information. The current implementation will cause a
      segmentation violation if the resulting string is longer than
      32768 characters. */
  GNativeString &vformat(const GNativeString &fmt, va_list &args);

  /** Returns a copy of this string with characters used in XML with
      '<'  to "&lt;", '>'  to "&gt;",  '&' to "&amp;" '\'' to
      "&apos;", and  '\"' to  "&quot;".   Characters 0x01 through
      0x1f are also escaped. */
  GNativeString toEscaped( const bool tosevenbit=false ) const;


  /** Provides a direct access to the string buffer.  Returns a
      pointer for directly accessing the string buffer.  This
      pointer valid remains valid as long as the string is not
      modified by other means.  Positive values for argument #n#
      represent the length of the returned buffer.  The returned
      string buffer will be large enough to hold at least #n#
      characters plus a null character.  If #n# is positive but
      smaller than the string length, the string will be truncated
      to #n# characters. */
  char *getbuf(int n = -1);
  /** Set the character at position #n# to value #ch#.  An exception
      \Ref{GException} is thrown if number #n# is not in range #-len#
      to #len#, where #len# is the length of the string.  If
      character #ch# is zero, the string is truncated at position
      #n#.  The first character of a string is numbered zero.
      Negative positions represent characters relative to the end of
      the string. If position #n# is equal to the length of the
      string, this function appends character #ch# to the end of the
      string. */
  void setat(const int n, const char ch);

  static GNativeString create( const char *buf, const unsigned int bufsize );
  static GNativeString create( const uint16_t *buf, const unsigned int bufsize );
  static GNativeString create( const uint32_t *buf, const unsigned int bufsize );
#endif // WinCE
};

//@}

inline
GBaseString::operator const char* ( void ) const
{
  return ptr?(*this)->data:nullstr;
}

inline unsigned int
GBaseString::length( void ) const
{
  return ptr ? (*this)->size : 0;
}

inline bool
GBaseString::operator! ( void ) const
{
  return !ptr;
}

inline GUTF8String
GUTF8String::upcase( void ) const
{
  if (ptr) return (*this)->upcase();
  return *this;
}

inline GUTF8String
GUTF8String::downcase( void ) const
{
  if (ptr) return (*this)->downcase();
  return *this;
}

inline void
GUTF8String::init(void)
{ GBaseString::init(); }

inline GUTF8String &
GUTF8String::init(const GP<GStringRep> &rep)
{ GP<GStringRep>::operator=(rep?rep->toUTF8(true):rep); init(); return *this; }

inline GUTF8String &
GUTF8String::vformat(const GUTF8String &fmt, va_list &args)
{ return (*this = (fmt.ptr?GUTF8String(fmt,args):fmt)); }

inline GUTF8String
GUTF8String::toEscaped( const bool tosevenbit ) const
{ return ptr?GUTF8String((*this)->toEscaped(tosevenbit)):(*this); }

inline GP<GStringRep::Unicode> 
GUTF8String::get_remainder(void) const
{
  GP<GStringRep::Unicode> retval;
  if(ptr)
    retval=((*this)->get_remainder());
  return retval;
}

inline
GUTF8String::GUTF8String(const GNativeString &str)
{ init(str.length()?(str->toUTF8(true)):(GP<GStringRep>)str); }

inline
GUTF8String::GUTF8String(const GP<GStringRep> &str)
{ init(str?(str->toUTF8(true)):str); }

inline
GUTF8String::GUTF8String(const GBaseString &str)
{ init(str.length()?(str->toUTF8(true)):(GP<GStringRep>)str); }

inline void
GBaseString::init(void)
{
  gstr=ptr?((*this)->data):nullstr;
}
/** Returns an integer.  Implements i18n atoi.  */
inline int
GBaseString::toInt(void) const
{ return ptr?(*this)->toInt():0; }

/** Returns a long intenger.  Implments i18n strtol.  */
inline long
GBaseString::toLong(const int pos, int &endpos, const int base) const
{
  long int retval=0;
  if(ptr)
  {
    retval=(*this)->toLong(pos, endpos, base);
  }else
  {
    endpos=(-1);
  }
  return retval;
}

inline long
GBaseString::toLong(
  const GUTF8String& src, const int pos, int &endpos, const int base)
{
  return src.toLong(pos,endpos,base);
}

inline long
GBaseString::toLong(
  const GNativeString& src, const int pos, int &endpos, const int base)
{
  return src.toLong(pos,endpos,base);
}

/** Returns a unsigned long integer.  Implements i18n strtoul. */
inline unsigned long
GBaseString::toULong(const int pos, int &endpos, const int base) const
{
  unsigned long retval=0;
  if(ptr)
  {
    retval=(*this)->toULong(pos, endpos, base);
  }else
  {
    endpos=(-1);
  }
  return retval;
}

inline unsigned long
GBaseString::toULong(
  const GUTF8String& src, const int pos, int &endpos, const int base)
{
  return src.toULong(pos,endpos,base);
}

inline unsigned long
GBaseString::toULong(
  const GNativeString& src, const int pos, int &endpos, const int base)
{
  return src.toULong(pos,endpos,base);
}

/** Returns a double.  Implements the i18n strtod.  */
inline double
GBaseString::toDouble(
  const int pos, int &endpos ) const
{
  double retval=(double)0;
  if(ptr)
  {
    retval=(*this)->toDouble(pos, endpos);
  }else
  {
    endpos=(-1);
  }
  return retval;
}

inline double
GBaseString::toDouble(
  const GUTF8String& src, const int pos, int &endpos)
{
  return src.toDouble(pos,endpos);
}

inline double
GBaseString::toDouble(
  const GNativeString& src, const int pos, int &endpos)
{
  return src.toDouble(pos,endpos);
}

inline GBaseString &
GBaseString::init(const GP<GStringRep> &rep)
{ GP<GStringRep>::operator=(rep); init(); return *this;}

inline char
GBaseString::operator[] (int n) const
{ return ((n||ptr)?((*this)->data[CheckSubscript(n)]):0); }

inline int
GBaseString::search(char c, int from) const
{ return ptr?((*this)->search(c,from)):(-1); }

inline int
GBaseString::search(const char *str, int from) const
{ return ptr?((*this)->search(str,from)):(-1); }

inline int
GBaseString::rsearch(char c, const int from) const
{ return ptr?((*this)->rsearch(c,from)):(-1); }

inline int
GBaseString::rsearch(const char *str, const int from) const
{ return ptr?((*this)->rsearch(str,from)):(-1); }

inline int
GBaseString::contains(const char accept[], const int from) const
{ return ptr?((*this)->contains(accept,from)):(-1); }

inline int
GBaseString::rcontains(const char accept[], const int from) const
{ return ptr?((*this)->rcontains(accept,from)):(-1); }

inline int
GBaseString::cmp(const GBaseString &s2, const int len) const
{ return GStringRep::cmp(*this,s2,len); }

inline int
GBaseString::cmp(const char *s2, const int len) const
{ return GStringRep::cmp(*this,s2,len); }

inline int
GBaseString::cmp(const char s2) const
{ return GStringRep::cmp(*this,&s2,1); }

inline int
GBaseString::cmp(const char *s1, const char *s2, const int len)
{ return GStringRep::cmp(s1,s2,len); }

inline bool
GBaseString::operator==(const GBaseString &s2) const
{ return !cmp(s2); }

inline bool
GBaseString::operator==(const char *s2) const
{ return !cmp(s2); }

inline bool
GBaseString::operator!=(const GBaseString &s2) const
{ return !!cmp(s2); }

inline bool
GBaseString::operator!=(const char *s2) const
{ return !!cmp(s2); }

inline bool
GBaseString::operator>=(const GBaseString &s2) const
{ return (cmp(s2)>=0); }

inline bool
GBaseString::operator>=(const char *s2) const
{ return (cmp(s2)>=0); }

inline bool
GBaseString::operator>=(const char s2) const
{ return (cmp(s2)>=0); }

inline bool
GBaseString::operator<(const GBaseString &s2) const
{ return (cmp(s2)<0); }

inline bool
GBaseString::operator<(const char *s2) const
{ return (cmp(s2)<0); }

inline bool
GBaseString::operator<(const char s2) const
{ return (cmp(s2)<0); }

inline bool
GBaseString::operator> (const GBaseString &s2) const
{ return (cmp(s2)>0); }

inline bool
GBaseString::operator> (const char *s2) const
{ return (cmp(s2)>0); }

inline bool
GBaseString::operator> (const char s2) const
{ return (cmp(s2)>0); }

inline bool
GBaseString::operator<=(const GBaseString &s2) const
{ return (cmp(s2)<=0); }

inline bool
GBaseString::operator<=(const char *s2) const
{ return (cmp(s2)<=0); }

inline bool
GBaseString::operator<=(const char s2) const
{ return (cmp(s2)<=0); }

inline int
GBaseString::nextNonSpace( const int from, const int len ) const
{ return ptr?(*this)->nextNonSpace(from,len):0; }

inline int
GBaseString::nextChar( const int from ) const
{ return ptr?(*this)->nextChar(from):0; }

inline int
GBaseString::nextSpace( const int from, const int len ) const
{ return ptr?(*this)->nextSpace(from,len):0; }

inline int
GBaseString::firstEndSpace( const int from,const int len ) const
{ return ptr?(*this)->firstEndSpace(from,len):0; }

inline bool
GBaseString::is_valid(void) const
{ return ptr?((*this)->is_valid()):true; }

#if HAS_WCHAR
inline int
GBaseString::ncopy(wchar_t * const buf, const int buflen) const
{if(buf&&buflen)buf[0]=0;return ptr?((*this)->ncopy(buf,buflen)):0;}
#endif

inline int
GBaseString::CheckSubscript(int n) const
{
  if(n)
  {
    if (n<0 && ptr)
      n += (*this)->size;
    if (n<0 || !ptr || n > (int)(*this)->size)
      throw_illegal_subscript();
  }
  return n;
}

inline GBaseString::GBaseString(void) { init(); }

inline GUTF8String::GUTF8String(void) { }

inline GUTF8String::GUTF8String(const GUTF8String &str)
{ init(str); }

inline GUTF8String& GUTF8String::operator= (const GP<GStringRep> &str)
{ return init(str); }

inline GUTF8String& GUTF8String::operator= (const GBaseString &str)
{ return init(str); }

inline GUTF8String& GUTF8String::operator= (const GUTF8String &str)
{ return init(str); }

inline GUTF8String& GUTF8String::operator= (const GNativeString &str)
{ return init(str); }

inline GUTF8String
GUTF8String::create( const char *buf, const unsigned int bufsize )
{
#if HAS_WCHAR
  return GNativeString(buf,bufsize);
#else
  return GUTF8String(buf,bufsize);
#endif
}

inline GUTF8String
GUTF8String::create( const uint16_t *buf, const unsigned int bufsize )
{
  return GUTF8String(buf,bufsize);
}

inline GUTF8String
GUTF8String::create( const uint32_t *buf, const unsigned int bufsize )
{
  return GUTF8String(buf,bufsize);
}

inline GNativeString::GNativeString(void) {}

#if !HAS_WCHAR
// For Windows CE, GNativeString is essentially GUTF8String

inline
GNativeString::GNativeString(const GUTF8String &str)
: GUTF8String(str) {}

inline
GNativeString::GNativeString(const GP<GStringRep> &str)
: GUTF8String(str) {}

inline
GNativeString::GNativeString(const char dat)
: GUTF8String(dat) {}

inline
GNativeString::GNativeString(const char *str)
: GUTF8String(str) {}

inline
GNativeString::GNativeString(const unsigned char *str)
: GUTF8String(str) {}

inline
GNativeString::GNativeString(const uint16_t *str)
: GUTF8String(str) {}

inline
GNativeString::GNativeString(const uint32_t *str)
: GUTF8String(str) {}

inline
GNativeString::GNativeString(const char *dat, unsigned int len)
: GUTF8String(dat,len) {}

inline
GNativeString::GNativeString(const uint16_t *dat, unsigned int len)
: GUTF8String(dat,len) {}

inline
GNativeString::GNativeString(const uint32_t *dat, unsigned int len)
: GUTF8String(dat,len) {}

inline
GNativeString::GNativeString(const GNativeString &str)
: GUTF8String(str) {}

inline
GNativeString::GNativeString(const int number)
: GUTF8String(number) {}

inline
GNativeString::GNativeString(const double number)
: GUTF8String(number) {}

inline
GNativeString::GNativeString(const GNativeString &fmt, va_list &args)
: GUTF8String(fmt,args) {}

#else // HAS_WCHAR

/// Initialize this string class
inline void
GNativeString::init(void)
{ GBaseString::init(); }

/// Initialize this string class
inline GNativeString &
GNativeString::init(const GP<GStringRep> &rep)
{
  GP<GStringRep>::operator=(rep?rep->toNative(GStringRep::NOT_ESCAPED):rep);
  init();
  return *this;
}

inline GNativeString 
GNativeString::substr(int from, int len) const
{ return GNativeString(*this, from, len); }

inline GNativeString &
GNativeString::vformat(const GNativeString &fmt, va_list &args)
{ return (*this = (fmt.ptr?GNativeString(fmt,args):fmt)); }

inline GNativeString
GNativeString::toEscaped( const bool tosevenbit ) const
{ return ptr?GNativeString((*this)->toEscaped(tosevenbit)):(*this); }

inline
GNativeString::GNativeString(const GUTF8String &str)
{
  if (str.length())
    init(str->toNative(GStringRep::NOT_ESCAPED));
  else
    init((GP<GStringRep>)str);
}

inline
GNativeString::GNativeString(const GP<GStringRep> &str)
{
  if (str)
    init(str->toNative(GStringRep::NOT_ESCAPED));
  else
    init(str);
}

inline
GNativeString::GNativeString(const GBaseString &str)
{
  if (str.length())
    init(str->toNative(GStringRep::NOT_ESCAPED));
  else
    init((GP<GStringRep>)str);
}


inline
GNativeString::GNativeString(const GNativeString &fmt, va_list &args)
{
  if (fmt.ptr)
    init(fmt->vformat(args));
  else
    init(fmt);
}

inline GNativeString
GNativeString::create( const char *buf, const unsigned int bufsize )
{
  return GNativeString(buf,bufsize);
}

inline GNativeString
GNativeString::create( const uint16_t *buf, const unsigned int bufsize )
{
  return GNativeString(buf,bufsize);
}

inline GNativeString
GNativeString::create( const uint32_t *buf, const unsigned int bufsize )
{
  return GNativeString(buf,bufsize);
}

inline GNativeString&
GNativeString::operator= (const GP<GStringRep> &str)
{ return init(str); }

inline GNativeString&
GNativeString::operator= (const GBaseString &str)
{ return init(str); }

inline GNativeString&
GNativeString::operator= (const GUTF8String &str)
{ return init(str); }

inline GNativeString&
GNativeString::operator= (const GNativeString &str)
{ return init(str); }

inline GNativeString
GNativeString::upcase( void ) const
{
  if (ptr) return (*this)->upcase();
  return *this;
}

inline GNativeString
GNativeString::downcase( void ) const
{
  if (ptr) return (*this)->downcase();
  return *this;
}

#endif // HAS_WCHAR

inline bool
operator==(const char *s1, const GBaseString &s2)
{ return !s2.cmp(s1); }

inline bool
operator!=(const char *s1, const GBaseString &s2)
{ return !!s2.cmp(s1); }

inline bool
operator>=(const char    *s1, const GBaseString &s2)
{ return (s2.cmp(s1)<=0); }

inline bool
operator>=(const char s1, const GBaseString &s2)
{ return (s2.cmp(s1)<=0); }

inline bool
operator<(const char *s1, const GBaseString &s2)
{ return (s2.cmp(s1)>0); }

inline bool
operator<(const char s1, const GBaseString &s2)
{ return (s2.cmp(s1)>0); }

inline bool
operator> (const char    *s1, const GBaseString &s2)
{ return (s2.cmp(s1)<0); }

inline bool
operator> (const char s1, const GBaseString &s2)
{ return (s2.cmp(s1)<0); }

inline bool
operator<=(const char    *s1, const GBaseString &s2)
{ return !(s1>s2); }

inline bool
operator<=(const char    s1, const GBaseString &s2)
{ return !(s1>s2); }

// ------------------- The end


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif

