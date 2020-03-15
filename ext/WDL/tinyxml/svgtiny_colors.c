/* ANSI-C code produced by gperf version 3.0.1 */
/* Command-line: gperf colors.gperf  */
/* Computed positions: -k'1,3,6-8,12-13' */

#if 0
#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif
#endif

//#line 15 "colors.gperf"

#include <string.h>
#include <stdio.h>

//#include "svgtiny.h"
//#include "svgtiny_internal.h"

typedef int svgtiny_colour; // == LICE_pixel
#define svgtiny_RGB(r,g,b) (((b)&0xFF)|(((g)&0xFF)<<8)|(((r)&0xFF)<<16)|(0xFF<<24))

struct svgtiny_named_color
{
	const char *name;
	svgtiny_colour color;
};

//#line 21 "colors.gperf"

//struct svgtiny_named_color;

#define TOTAL_KEYWORDS 147
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 20
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 565
/* maximum key range = 562, duplicates = 0 */

static unsigned int
svgtiny_color_hash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566,   5,  55,   0,
       35,   0,  75,  10,   5,   0, 566, 250,  10,  40,
       85,  60,  70, 144,   0,  20,  45,  10,  30, 185,
       95, 195, 566,   0, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566, 566, 566,
      566, 566, 566, 566, 566, 566, 566, 566
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[12]];
      /*FALLTHROUGH*/
      case 12:
        hval += asso_values[(unsigned char)str[11]];
      /*FALLTHROUGH*/
      case 11:
      case 10:
      case 9:
      case 8:
        hval += asso_values[(unsigned char)str[7]];
      /*FALLTHROUGH*/
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
        hval += asso_values[(unsigned char)str[5]];
      /*FALLTHROUGH*/
      case 5:
      case 4:
      case 3:
        hval += asso_values[(unsigned char)str[2]+2];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval;
}

static const struct svgtiny_named_color *
svgtiny_color_lookup (register const char *str, register unsigned int len)
{
  static const struct svgtiny_named_color wordlist[] =
    {
//#line 43 "colors.gperf"
      {"cyan",		svgtiny_RGB(  0, 255, 255)},
//#line 76 "colors.gperf"
      {"gray",		svgtiny_RGB(128, 128, 128)},
//#line 37 "colors.gperf"
      {"chartreuse",	svgtiny_RGB(127, 255,   0)},
//#line 77 "colors.gperf"
      {"grey",		svgtiny_RGB(128, 128, 128)},
//#line 78 "colors.gperf"
      {"green",		svgtiny_RGB(  0, 128,   0)},
//#line 96 "colors.gperf"
      {"lightgrey",	svgtiny_RGB(211, 211, 211)},
//#line 95 "colors.gperf"
      {"lightgreen",	svgtiny_RGB(144, 238, 144)},
//#line 94 "colors.gperf"
      {"lightgray",	svgtiny_RGB(211, 211, 211)},
//#line 152 "colors.gperf"
      {"skyblue",	svgtiny_RGB(135, 206, 235)},
//#line 155 "colors.gperf"
      {"slategrey",	svgtiny_RGB(112, 128, 144)},
//#line 150 "colors.gperf"
      {"sienna",		svgtiny_RGB(160,  82,  45)},
//#line 154 "colors.gperf"
      {"slategray",	svgtiny_RGB(112, 128, 144)},
//#line 149 "colors.gperf"
      {"seashell",	svgtiny_RGB(255, 245, 238)},
//#line 160 "colors.gperf"
      {"teal",		svgtiny_RGB(  0, 128, 128)},
//#line 39 "colors.gperf"
      {"coral",		svgtiny_RGB(255, 127,  80)},
//#line 98 "colors.gperf"
      {"lightsalmon",	svgtiny_RGB(255, 160, 122)},
//#line 102 "colors.gperf"
      {"lightslategrey",	svgtiny_RGB(119, 136, 153)},
//#line 30 "colors.gperf"
      {"black",		svgtiny_RGB(  0,   0,   0)},
//#line 101 "colors.gperf"
      {"lightslategray",	svgtiny_RGB(119, 136, 153)},
//#line 128 "colors.gperf"
      {"orange",		svgtiny_RGB(255, 165,   0)},
//#line 129 "colors.gperf"
      {"orangered",	svgtiny_RGB(255,  69,   0)},
//#line 29 "colors.gperf"
      {"bisque",		svgtiny_RGB(255, 228, 196)},
//#line 105 "colors.gperf"
      {"lime",		svgtiny_RGB(  0, 255,   0)},
//#line 142 "colors.gperf"
      {"red",		svgtiny_RGB(255,   0,   0)},
//#line 106 "colors.gperf"
      {"limegreen",	svgtiny_RGB( 50, 205,  50)},
//#line 91 "colors.gperf"
      {"lightcoral",	svgtiny_RGB(240, 128, 128)},
//#line 144 "colors.gperf"
      {"royalblue",	svgtiny_RGB( 65, 105, 225)},
//#line 107 "colors.gperf"
      {"linen",		svgtiny_RGB(250, 240, 230)},
//#line 71 "colors.gperf"
      {"fuchsia",	svgtiny_RGB(255,   0, 255)},
//#line 48 "colors.gperf"
      {"darkgreen",	svgtiny_RGB(  0, 100,   0)},
//#line 90 "colors.gperf"
      {"lightblue",	svgtiny_RGB(173, 216, 230)},
//#line 54 "colors.gperf"
      {"darkorchid",	svgtiny_RGB(153,  50, 204)},
//#line 157 "colors.gperf"
      {"springgreen",	svgtiny_RGB(  0, 255, 127)},
//#line 108 "colors.gperf"
      {"magenta",	svgtiny_RGB(255,   0, 255)},
//#line 74 "colors.gperf"
      {"gold",		svgtiny_RGB(255, 215,   0)},
//#line 130 "colors.gperf"
      {"orchid",		svgtiny_RGB(218, 112, 214)},
//#line 153 "colors.gperf"
      {"slateblue",	svgtiny_RGB(106,  90, 205)},
//#line 51 "colors.gperf"
      {"darkmagenta",	svgtiny_RGB(139, 0,   139)},
//#line 44 "colors.gperf"
      {"darkblue",	svgtiny_RGB(  0,   0, 139)},
//#line 103 "colors.gperf"
      {"lightsteelblue",	svgtiny_RGB(176, 196, 222)},
//#line 151 "colors.gperf"
      {"silver",		svgtiny_RGB(192, 192, 192)},
//#line 148 "colors.gperf"
      {"seagreen",	svgtiny_RGB( 46, 139,  87)},
//#line 158 "colors.gperf"
      {"steelblue",	svgtiny_RGB( 70, 130, 180)},
//#line 159 "colors.gperf"
      {"tan",		svgtiny_RGB(210, 180, 140)},
//#line 137 "colors.gperf"
      {"peru",		svgtiny_RGB(205, 133,  63)},
//#line 141 "colors.gperf"
      {"purple",		svgtiny_RGB(128,   0, 128)},
//#line 55 "colors.gperf"
      {"darkred",	svgtiny_RGB(139,   0,   0)},
//#line 120 "colors.gperf"
      {"mintcream",	svgtiny_RGB(245, 255, 250)},
//#line 68 "colors.gperf"
      {"firebrick",	svgtiny_RGB(178,  34,  34)},
//#line 99 "colors.gperf"
      {"lightseagreen",	svgtiny_RGB( 32, 178, 170)},
//#line 52 "colors.gperf"
      {"darkolivegreen",	svgtiny_RGB( 85, 107,  47)},
//#line 121 "colors.gperf"
      {"mistyrose",	svgtiny_RGB(255, 228, 225)},
//#line 83 "colors.gperf"
      {"indigo",		svgtiny_RGB( 75,   0, 130)},
//#line 125 "colors.gperf"
      {"oldlace",	svgtiny_RGB(253, 245, 230)},
//#line 138 "colors.gperf"
      {"pink",		svgtiny_RGB(255, 192, 203)},
//#line 56 "colors.gperf"
      {"darksalmon",	svgtiny_RGB(233, 150, 122)},
//#line 86 "colors.gperf"
      {"lavender",	svgtiny_RGB(230, 230, 250)},
//#line 84 "colors.gperf"
      {"ivory",		svgtiny_RGB(255, 255, 240)},
//#line 122 "colors.gperf"
      {"moccasin",	svgtiny_RGB(255, 228, 181)},
//#line 36 "colors.gperf"
      {"cadetblue",	svgtiny_RGB( 95, 158, 160)},
//#line 62 "colors.gperf"
      {"darkviolet",	svgtiny_RGB(148,   0, 211)},
//#line 145 "colors.gperf"
      {"saddlebrown",	svgtiny_RGB(139,  69,  19)},
//#line 58 "colors.gperf"
      {"darkslateblue",	svgtiny_RGB( 72,  61, 139)},
//#line 132 "colors.gperf"
      {"palegreen",	svgtiny_RGB(152, 251, 152)},
//#line 156 "colors.gperf"
      {"snow",		svgtiny_RGB(255, 250, 250)},
//#line 82 "colors.gperf"
      {"indianred",	svgtiny_RGB(205,  92,  92)},
//#line 93 "colors.gperf"
      {"lightgoldenrodyellow",	svgtiny_RGB(250, 250, 210)},
//#line 162 "colors.gperf"
      {"tomato",		svgtiny_RGB(255,  99,  71)},
//#line 89 "colors.gperf"
      {"lemonchiffon",	svgtiny_RGB(255, 250, 205)},
//#line 97 "colors.gperf"
      {"lightpink",	svgtiny_RGB(255, 182, 193)},
//#line 109 "colors.gperf"
      {"maroon",		svgtiny_RGB(128,   0,   0)},
//#line 87 "colors.gperf"
      {"lavenderblush",	svgtiny_RGB(255, 240, 245)},
//#line 163 "colors.gperf"
      {"turquoise",	svgtiny_RGB( 64, 224, 208)},
//#line 53 "colors.gperf"
      {"darkorange",	svgtiny_RGB(255, 140,   0)},
//#line 124 "colors.gperf"
      {"navy",		svgtiny_RGB(  0,   0, 128)},
//#line 67 "colors.gperf"
      {"dodgerblue",	svgtiny_RGB( 30, 144, 255)},
//#line 70 "colors.gperf"
      {"forestgreen",	svgtiny_RGB( 34, 139,  34)},
//#line 119 "colors.gperf"
      {"midnightblue",	svgtiny_RGB( 25,  25, 112)},
//#line 114 "colors.gperf"
      {"mediumseagreen",	svgtiny_RGB( 60, 179, 113)},
//#line 57 "colors.gperf"
      {"darkseagreen",	svgtiny_RGB(143, 188, 143)},
//#line 25 "colors.gperf"
      {"aqua",		svgtiny_RGB(  0, 255, 255)},
//#line 27 "colors.gperf"
      {"azure",		svgtiny_RGB(240, 255, 255)},
//#line 146 "colors.gperf"
      {"salmon",		svgtiny_RGB(250, 128, 114)},
//#line 165 "colors.gperf"
      {"wheat",		svgtiny_RGB(245, 222, 179)},
//#line 34 "colors.gperf"
      {"brown",		svgtiny_RGB(165,  42,  42)},
//#line 26 "colors.gperf"
      {"aquamarine",	svgtiny_RGB(127, 255, 212)},
//#line 38 "colors.gperf"
      {"chocolate",	svgtiny_RGB(210, 105,  30)},
//#line 88 "colors.gperf"
      {"lawngreen",	svgtiny_RGB(124, 252,   0)},
//#line 147 "colors.gperf"
      {"sandybrown",	svgtiny_RGB(244, 164,  96)},
//#line 92 "colors.gperf"
      {"lightcyan",	svgtiny_RGB(224, 255, 255)},
//#line 164 "colors.gperf"
      {"violet",		svgtiny_RGB(238, 130, 238)},
//#line 104 "colors.gperf"
      {"lightyellow",	svgtiny_RGB(255, 255, 224)},
//#line 111 "colors.gperf"
      {"mediumblue",	svgtiny_RGB(  0,   0, 205)},
//#line 136 "colors.gperf"
      {"peachpuff",	svgtiny_RGB(255, 218, 185)},
//#line 79 "colors.gperf"
      {"greenyellow",	svgtiny_RGB(173, 255,  47)},
//#line 24 "colors.gperf"
      {"antiquewhite",	svgtiny_RGB(250, 235, 215)},
//#line 32 "colors.gperf"
      {"blue",		svgtiny_RGB(  0,   0, 255)},
//#line 118 "colors.gperf"
      {"mediumvioletred",	svgtiny_RGB(199,  21, 133)},
//#line 113 "colors.gperf"
      {"mediumpurple",	svgtiny_RGB(147, 112, 219)},
//#line 75 "colors.gperf"
      {"goldenrod",	svgtiny_RGB(218, 165,  32)},
//#line 31 "colors.gperf"
      {"blanchedalmond",	svgtiny_RGB(255, 235, 205)},
//#line 85 "colors.gperf"
      {"khaki",		svgtiny_RGB(240, 230, 140)},
//#line 139 "colors.gperf"
      {"plum",		svgtiny_RGB(221, 160, 221)},
//#line 112 "colors.gperf"
      {"mediumorchid",	svgtiny_RGB(186,  85, 211)},
//#line 143 "colors.gperf"
      {"rosybrown",	svgtiny_RGB(188, 143, 143)},
//#line 115 "colors.gperf"
      {"mediumslateblue",	svgtiny_RGB(123, 104, 238)},
//#line 61 "colors.gperf"
      {"darkturquoise",	svgtiny_RGB(  0, 206, 209)},
//#line 134 "colors.gperf"
      {"palevioletred",	svgtiny_RGB(219, 112, 147)},
//#line 135 "colors.gperf"
      {"papayawhip",	svgtiny_RGB(255, 239, 213)},
//#line 116 "colors.gperf"
      {"mediumspringgreen",	svgtiny_RGB(  0, 250, 154)},
//#line 49 "colors.gperf"
      {"darkgrey",	svgtiny_RGB(169, 169, 169)},
//#line 117 "colors.gperf"
      {"mediumturquoise",	svgtiny_RGB( 72, 209, 204)},
//#line 47 "colors.gperf"
      {"darkgray",	svgtiny_RGB(169, 169, 169)},
//#line 46 "colors.gperf"
      {"darkgoldenrod",	svgtiny_RGB(184, 134,  11)},
//#line 66 "colors.gperf"
      {"dimgrey",	svgtiny_RGB(105, 105, 105)},
//#line 65 "colors.gperf"
      {"dimgray",	svgtiny_RGB(105, 105, 105)},
//#line 80 "colors.gperf"
      {"honeydew",	svgtiny_RGB(240, 255, 240)},
//#line 28 "colors.gperf"
      {"beige",		svgtiny_RGB(245, 245, 220)},
//#line 161 "colors.gperf"
      {"thistle",	svgtiny_RGB(216, 191, 216)},
//#line 41 "colors.gperf"
      {"cornsilk",	svgtiny_RGB(255, 248, 220)},
//#line 126 "colors.gperf"
      {"olive",		svgtiny_RGB(128, 128,   0)},
//#line 33 "colors.gperf"
      {"blueviolet",	svgtiny_RGB(138,  43, 226)},
//#line 110 "colors.gperf"
      {"mediumaquamarine",	svgtiny_RGB(102, 205, 170)},
//#line 40 "colors.gperf"
      {"cornflowerblue",	svgtiny_RGB(100, 149, 237)},
//#line 23 "colors.gperf"
      {"aliceblue",	svgtiny_RGB(240, 248, 255)},
//#line 140 "colors.gperf"
      {"powderblue",	svgtiny_RGB(176, 224, 230)},
//#line 133 "colors.gperf"
      {"paleturquoise",	svgtiny_RGB(175, 238, 238)},
//#line 60 "colors.gperf"
      {"darkslategrey",	svgtiny_RGB( 47,  79,  79)},
//#line 50 "colors.gperf"
      {"darkkhaki",	svgtiny_RGB(189, 183, 107)},
//#line 59 "colors.gperf"
      {"darkslategray",	svgtiny_RGB( 47,  79,  79)},
//#line 73 "colors.gperf"
      {"ghostwhite",	svgtiny_RGB(248, 248, 255)},
//#line 127 "colors.gperf"
      {"olivedrab",	svgtiny_RGB(107, 142,  35)},
//#line 131 "colors.gperf"
      {"palegoldenrod",	svgtiny_RGB(238, 232, 170)},
//#line 45 "colors.gperf"
      {"darkcyan",	svgtiny_RGB(  0, 139, 139)},
//#line 81 "colors.gperf"
      {"hotpink",	svgtiny_RGB(255, 105, 180)},
//#line 72 "colors.gperf"
      {"gainsboro",	svgtiny_RGB(220, 220, 220)},
//#line 63 "colors.gperf"
      {"deeppink",	svgtiny_RGB(255,  20, 147)},
//#line 42 "colors.gperf"
      {"crimson",	svgtiny_RGB(220,  20,  60)},
//#line 35 "colors.gperf"
      {"burlywood",	svgtiny_RGB(222, 184, 135)},
//#line 69 "colors.gperf"
      {"floralwhite",	svgtiny_RGB(255, 250, 240)},
//#line 166 "colors.gperf"
      {"white",		svgtiny_RGB(255, 255, 255)},
//#line 123 "colors.gperf"
      {"navajowhite",	svgtiny_RGB(255, 222, 173)},
//#line 168 "colors.gperf"
      {"yellow",		svgtiny_RGB(255, 255,   0)},
//#line 169 "colors.gperf"
      {"yellowgreen",	svgtiny_RGB(154, 205,  50)},
//#line 100 "colors.gperf"
      {"lightskyblue",	svgtiny_RGB(135, 206, 250)},
//#line 64 "colors.gperf"
      {"deepskyblue",	svgtiny_RGB(  0, 191, 255)},
//#line 167 "colors.gperf"
      {"whitesmoke",	svgtiny_RGB(245, 245, 245)}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = svgtiny_color_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const struct svgtiny_named_color *resword;

          switch (key - 4)
            {
              case 0:
                resword = &wordlist[0];
                goto compare;
              case 10:
                resword = &wordlist[1];
                goto compare;
              case 16:
                resword = &wordlist[2];
                goto compare;
              case 20:
                resword = &wordlist[3];
                goto compare;
              case 21:
                resword = &wordlist[4];
                goto compare;
              case 25:
                resword = &wordlist[5];
                goto compare;
              case 26:
                resword = &wordlist[6];
                goto compare;
              case 30:
                resword = &wordlist[7];
                goto compare;
              case 33:
                resword = &wordlist[8];
                goto compare;
              case 35:
                resword = &wordlist[9];
                goto compare;
              case 37:
                resword = &wordlist[10];
                goto compare;
              case 40:
                resword = &wordlist[11];
                goto compare;
              case 44:
                resword = &wordlist[12];
                goto compare;
              case 45:
                resword = &wordlist[13];
                goto compare;
              case 46:
                resword = &wordlist[14];
                goto compare;
              case 52:
                resword = &wordlist[15];
                goto compare;
              case 55:
                resword = &wordlist[16];
                goto compare;
              case 56:
                resword = &wordlist[17];
                goto compare;
              case 60:
                resword = &wordlist[18];
                goto compare;
              case 62:
                resword = &wordlist[19];
                goto compare;
              case 65:
                resword = &wordlist[20];
                goto compare;
              case 67:
                resword = &wordlist[21];
                goto compare;
              case 70:
                resword = &wordlist[22];
                goto compare;
              case 74:
                resword = &wordlist[23];
                goto compare;
              case 75:
                resword = &wordlist[24];
                goto compare;
              case 76:
                resword = &wordlist[25];
                goto compare;
              case 80:
                resword = &wordlist[26];
                goto compare;
              case 81:
                resword = &wordlist[27];
                goto compare;
              case 83:
                resword = &wordlist[28];
                goto compare;
              case 85:
                resword = &wordlist[29];
                goto compare;
              case 90:
                resword = &wordlist[30];
                goto compare;
              case 91:
                resword = &wordlist[31];
                goto compare;
              case 92:
                resword = &wordlist[32];
                goto compare;
              case 93:
                resword = &wordlist[33];
                goto compare;
              case 95:
                resword = &wordlist[34];
                goto compare;
              case 97:
                resword = &wordlist[35];
                goto compare;
              case 100:
                resword = &wordlist[36];
                goto compare;
              case 102:
                resword = &wordlist[37];
                goto compare;
              case 104:
                resword = &wordlist[38];
                goto compare;
              case 105:
                resword = &wordlist[39];
                goto compare;
              case 107:
                resword = &wordlist[40];
                goto compare;
              case 109:
                resword = &wordlist[41];
                goto compare;
              case 110:
                resword = &wordlist[42];
                goto compare;
              case 114:
                resword = &wordlist[43];
                goto compare;
              case 115:
                resword = &wordlist[44];
                goto compare;
              case 117:
                resword = &wordlist[45];
                goto compare;
              case 118:
                resword = &wordlist[46];
                goto compare;
              case 120:
                resword = &wordlist[47];
                goto compare;
              case 125:
                resword = &wordlist[48];
                goto compare;
              case 129:
                resword = &wordlist[49];
                goto compare;
              case 130:
                resword = &wordlist[50];
                goto compare;
              case 135:
                resword = &wordlist[51];
                goto compare;
              case 137:
                resword = &wordlist[52];
                goto compare;
              case 138:
                resword = &wordlist[53];
                goto compare;
              case 140:
                resword = &wordlist[54];
                goto compare;
              case 141:
                resword = &wordlist[55];
                goto compare;
              case 144:
                resword = &wordlist[56];
                goto compare;
              case 145:
                resword = &wordlist[57];
                goto compare;
              case 149:
                resword = &wordlist[58];
                goto compare;
              case 155:
                resword = &wordlist[59];
                goto compare;
              case 156:
                resword = &wordlist[60];
                goto compare;
              case 157:
                resword = &wordlist[61];
                goto compare;
              case 159:
                resword = &wordlist[62];
                goto compare;
              case 160:
                resword = &wordlist[63];
                goto compare;
              case 164:
                resword = &wordlist[64];
                goto compare;
              case 165:
                resword = &wordlist[65];
                goto compare;
              case 166:
                resword = &wordlist[66];
                goto compare;
              case 167:
                resword = &wordlist[67];
                goto compare;
              case 168:
                resword = &wordlist[68];
                goto compare;
              case 170:
                resword = &wordlist[69];
                goto compare;
              case 172:
                resword = &wordlist[70];
                goto compare;
              case 174:
                resword = &wordlist[71];
                goto compare;
              case 175:
                resword = &wordlist[72];
                goto compare;
              case 176:
                resword = &wordlist[73];
                goto compare;
              case 180:
                resword = &wordlist[74];
                goto compare;
              case 181:
                resword = &wordlist[75];
                goto compare;
              case 182:
                resword = &wordlist[76];
                goto compare;
              case 183:
                resword = &wordlist[77];
                goto compare;
              case 185:
                resword = &wordlist[78];
                goto compare;
              case 188:
                resword = &wordlist[79];
                goto compare;
              case 190:
                resword = &wordlist[80];
                goto compare;
              case 191:
                resword = &wordlist[81];
                goto compare;
              case 192:
                resword = &wordlist[82];
                goto compare;
              case 196:
                resword = &wordlist[83];
                goto compare;
              case 200:
                resword = &wordlist[84];
                goto compare;
              case 201:
                resword = &wordlist[85];
                goto compare;
              case 209:
                resword = &wordlist[86];
                goto compare;
              case 210:
                resword = &wordlist[87];
                goto compare;
              case 211:
                resword = &wordlist[88];
                goto compare;
              case 215:
                resword = &wordlist[89];
                goto compare;
              case 221:
                resword = &wordlist[90];
                goto compare;
              case 222:
                resword = &wordlist[91];
                goto compare;
              case 226:
                resword = &wordlist[92];
                goto compare;
              case 230:
                resword = &wordlist[93];
                goto compare;
              case 232:
                resword = &wordlist[94];
                goto compare;
              case 238:
                resword = &wordlist[95];
                goto compare;
              case 240:
                resword = &wordlist[96];
                goto compare;
              case 241:
                resword = &wordlist[97];
                goto compare;
              case 243:
                resword = &wordlist[98];
                goto compare;
              case 245:
                resword = &wordlist[99];
                goto compare;
              case 250:
                resword = &wordlist[100];
                goto compare;
              case 251:
                resword = &wordlist[101];
                goto compare;
              case 255:
                resword = &wordlist[102];
                goto compare;
              case 258:
                resword = &wordlist[103];
                goto compare;
              case 260:
                resword = &wordlist[104];
                goto compare;
              case 261:
                resword = &wordlist[105];
                goto compare;
              case 263:
                resword = &wordlist[106];
                goto compare;
              case 269:
                resword = &wordlist[107];
                goto compare;
              case 271:
                resword = &wordlist[108];
                goto compare;
              case 278:
                resword = &wordlist[109];
                goto compare;
              case 279:
                resword = &wordlist[110];
                goto compare;
              case 281:
                resword = &wordlist[111];
                goto compare;
              case 284:
                resword = &wordlist[112];
                goto compare;
              case 289:
                resword = &wordlist[113];
                goto compare;
              case 293:
                resword = &wordlist[114];
                goto compare;
              case 298:
                resword = &wordlist[115];
                goto compare;
              case 299:
                resword = &wordlist[116];
                goto compare;
              case 306:
                resword = &wordlist[117];
                goto compare;
              case 308:
                resword = &wordlist[118];
                goto compare;
              case 309:
                resword = &wordlist[119];
                goto compare;
              case 311:
                resword = &wordlist[120];
                goto compare;
              case 316:
                resword = &wordlist[121];
                goto compare;
              case 321:
                resword = &wordlist[122];
                goto compare;
              case 330:
                resword = &wordlist[123];
                goto compare;
              case 335:
                resword = &wordlist[124];
                goto compare;
              case 336:
                resword = &wordlist[125];
                goto compare;
              case 338:
                resword = &wordlist[126];
                goto compare;
              case 344:
                resword = &wordlist[127];
                goto compare;
              case 345:
                resword = &wordlist[128];
                goto compare;
              case 349:
                resword = &wordlist[129];
                goto compare;
              case 350:
                resword = &wordlist[130];
                goto compare;
              case 355:
                resword = &wordlist[131];
                goto compare;
              case 364:
                resword = &wordlist[132];
                goto compare;
              case 369:
                resword = &wordlist[133];
                goto compare;
              case 373:
                resword = &wordlist[134];
                goto compare;
              case 380:
                resword = &wordlist[135];
                goto compare;
              case 384:
                resword = &wordlist[136];
                goto compare;
              case 398:
                resword = &wordlist[137];
                goto compare;
              case 410:
                resword = &wordlist[138];
                goto compare;
              case 426:
                resword = &wordlist[139];
                goto compare;
              case 436:
                resword = &wordlist[140];
                goto compare;
              case 437:
                resword = &wordlist[141];
                goto compare;
              case 467:
                resword = &wordlist[142];
                goto compare;
              case 482:
                resword = &wordlist[143];
                goto compare;
              case 483:
                resword = &wordlist[144];
                goto compare;
              case 552:
                resword = &wordlist[145];
                goto compare;
              case 561:
                resword = &wordlist[146];
                goto compare;
            }
          return 0;
        compare:
          {
            register const char *s = resword->name;

            if (*str == *s && !strcmp (str + 1, s + 1))
              return resword;
          }
        }
    }
  return 0;
}

int LICE_RGBA_from_SVG(const char* s,int len)  // returns LICE_pixel
{
  int r = 0, g = 0, b = 0;
  float rf = 0.0f, gf = 0.0f, bf = 0.0f;
  const struct svgtiny_named_color* c = 0;

#ifdef _WIN32
  if (len == 4 && !strnicmp(s, "none", 4)) return 0;
#else
  if (len == 4 && !strncasecmp(s, "none", 4)) return 0;
#endif
  
  if (len == 4 && s[0] == '#') 
  {
		if (sscanf(s+1, "%1x%1x%1x", &r, &g, &b) == 3)
    {
      return svgtiny_RGB(r<<4,g<<4,b<<4);
	  }
  }
  else if (len == 7 && s[0] == '#')
  {
    if (sscanf(s+1, "%2x%2x%2x", &r, &g, &b) == 3)
    {
      return svgtiny_RGB(r, g, b);
    }
  }
  else if (10 <= len && s[0] == 'r' && s[1] == 'g' && s[2] == 'b' && s[3] == '(' && s[len - 1] == ')') 
  {
		if (sscanf(s+4, "%i,%i,%i", &r, &g, &b) == 3)
    {
			return svgtiny_RGB(r, g, b);
    }
		else 
    {
      if (sscanf(s+4, "%f%%,%f%%,%f%%", &rf, &gf, &bf) == 3) 
      {
			  b = (int) (bf*2.55);
			  g = (int) (gf*2.55);
			  r = (int) (rf*2.55);
			  return svgtiny_RGB(r, g, b);
		  }
    }
  }
  else
  {
    c = svgtiny_color_lookup(s, len);
    if (c) return c->color;
  }

  return 0;
}
