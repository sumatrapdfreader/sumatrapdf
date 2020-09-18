/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf source/html/css-properties.gperf  */
/* Computed positions: -k'1-2,$' */

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
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "source/html/css-properties.gperf"
struct css_property_info { const char *name; int key; };

#define TOTAL_KEYWORDS 60
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 19
#define MIN_HASH_VALUE 11
#define MAX_HASH_VALUE 106
/* maximum key range = 96, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
css_property_hash (register const char *str, register size_t len)
{
  static unsigned char asso_values[] =
	{
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107,  35,   5,  55,
	   35,  10,  51,  31,  55,  22, 107, 107,   0,  20,
	   40,   0,   0,  10,   0,  45,  20,   5,  35,  20,
	  107,  30, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
	  107, 107, 107, 107, 107, 107
	};
  return (unsigned int)(len + asso_values[(unsigned char)str[1]] + asso_values[(unsigned char)str[0]] + asso_values[(unsigned char)str[len - 1]]);
}

static struct css_property_info css_property_list[] =
  {
	{""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
	{""}, {""},
#line 9 "source/html/css-properties.gperf"
	{"border",PRO_BORDER},
	{""}, {""}, {""},
#line 24 "source/html/css-properties.gperf"
	{"border-top",PRO_BORDER_TOP},
	{""},
#line 14 "source/html/css-properties.gperf"
	{"border-color",PRO_BORDER_COLOR},
	{""}, {""}, {""},
#line 25 "source/html/css-properties.gperf"
	{"border-top-color",PRO_BORDER_TOP_COLOR},
#line 16 "source/html/css-properties.gperf"
	{"border-left-color",PRO_BORDER_LEFT_COLOR},
#line 20 "source/html/css-properties.gperf"
	{"border-right-color",PRO_BORDER_RIGHT_COLOR},
#line 11 "source/html/css-properties.gperf"
	{"border-bottom-color",PRO_BORDER_BOTTOM_COLOR},
	{""}, {""},
#line 23 "source/html/css-properties.gperf"
	{"border-style",PRO_BORDER_STYLE},
	{""}, {""}, {""},
#line 26 "source/html/css-properties.gperf"
	{"border-top-style",PRO_BORDER_TOP_STYLE},
#line 17 "source/html/css-properties.gperf"
	{"border-left-style",PRO_BORDER_LEFT_STYLE},
#line 21 "source/html/css-properties.gperf"
	{"border-right-style",PRO_BORDER_RIGHT_STYLE},
#line 12 "source/html/css-properties.gperf"
	{"border-bottom-style",PRO_BORDER_BOTTOM_STYLE},
	{""},
#line 15 "source/html/css-properties.gperf"
	{"border-left",PRO_BORDER_LEFT},
#line 19 "source/html/css-properties.gperf"
	{"border-right",PRO_BORDER_RIGHT},
#line 10 "source/html/css-properties.gperf"
	{"border-bottom",PRO_BORDER_BOTTOM},
	{""}, {""}, {""},
#line 40 "source/html/css-properties.gperf"
	{"list-style",PRO_LIST_STYLE},
	{""}, {""}, {""},
#line 54 "source/html/css-properties.gperf"
	{"padding-top",PRO_PADDING_TOP},
#line 43 "source/html/css-properties.gperf"
	{"list-style-type",PRO_LIST_STYLE_TYPE},
#line 41 "source/html/css-properties.gperf"
	{"list-style-image",PRO_LIST_STYLE_IMAGE},
	{""}, {""},
#line 55 "source/html/css-properties.gperf"
	{"page-break-after",PRO_PAGE_BREAK_AFTER},
#line 49 "source/html/css-properties.gperf"
	{"orphans",PRO_ORPHANS},
#line 39 "source/html/css-properties.gperf"
	{"line-height",PRO_LINE_HEIGHT},
	{""},
#line 38 "source/html/css-properties.gperf"
	{"letter-spacing",PRO_LETTER_SPACING},
#line 8 "source/html/css-properties.gperf"
	{"background-color",PRO_BACKGROUND_COLOR},
	{""}, {""}, {""},
#line 29 "source/html/css-properties.gperf"
	{"color",PRO_COLOR},
#line 60 "source/html/css-properties.gperf"
	{"text-indent",PRO_TEXT_INDENT},
#line 56 "source/html/css-properties.gperf"
	{"page-break-before",PRO_PAGE_BREAK_BEFORE},
#line 67 "source/html/css-properties.gperf"
	{"word-spacing",PRO_WORD_SPACING},
#line 61 "source/html/css-properties.gperf"
	{"text-transform",PRO_TEXT_TRANSFORM},
#line 48 "source/html/css-properties.gperf"
	{"margin-top",PRO_MARGIN_TOP},
#line 57 "source/html/css-properties.gperf"
	{"quotes",PRO_QUOTES},
#line 52 "source/html/css-properties.gperf"
	{"padding-left",PRO_PADDING_LEFT},
#line 53 "source/html/css-properties.gperf"
	{"padding-right",PRO_PADDING_RIGHT},
#line 51 "source/html/css-properties.gperf"
	{"padding-bottom",PRO_PADDING_BOTTOM},
#line 33 "source/html/css-properties.gperf"
	{"font-size",PRO_FONT_SIZE},
#line 34 "source/html/css-properties.gperf"
	{"font-style",PRO_FONT_STYLE},
#line 28 "source/html/css-properties.gperf"
	{"border-width",PRO_BORDER_WIDTH},
#line 50 "source/html/css-properties.gperf"
	{"padding",PRO_PADDING},
	{""}, {""},
#line 27 "source/html/css-properties.gperf"
	{"border-top-width",PRO_BORDER_TOP_WIDTH},
#line 18 "source/html/css-properties.gperf"
	{"border-left-width",PRO_BORDER_LEFT_WIDTH},
#line 22 "source/html/css-properties.gperf"
	{"border-right-width",PRO_BORDER_RIGHT_WIDTH},
#line 13 "source/html/css-properties.gperf"
	{"border-bottom-width",PRO_BORDER_BOTTOM_WIDTH},
#line 59 "source/html/css-properties.gperf"
	{"text-align",PRO_TEXT_ALIGN},
#line 42 "source/html/css-properties.gperf"
	{"list-style-position",PRO_LIST_STYLE_POSITION},
#line 36 "source/html/css-properties.gperf"
	{"font-weight",PRO_FONT_WEIGHT},
#line 35 "source/html/css-properties.gperf"
	{"font-variant",PRO_FONT_VARIANT},
	{""}, {""},
#line 46 "source/html/css-properties.gperf"
	{"margin-left",PRO_MARGIN_LEFT},
#line 47 "source/html/css-properties.gperf"
	{"margin-right",PRO_MARGIN_RIGHT},
#line 45 "source/html/css-properties.gperf"
	{"margin-bottom",PRO_MARGIN_BOTTOM},
	{""}, {""},
#line 37 "source/html/css-properties.gperf"
	{"height",PRO_HEIGHT},
#line 32 "source/html/css-properties.gperf"
	{"font-family",PRO_FONT_FAMILY},
#line 65 "source/html/css-properties.gperf"
	{"widows",PRO_WIDOWS},
#line 31 "source/html/css-properties.gperf"
	{"display",PRO_DISPLAY},
	{""},
#line 64 "source/html/css-properties.gperf"
	{"white-space",PRO_WHITE_SPACE},
#line 63 "source/html/css-properties.gperf"
	{"visibility",PRO_VISIBILITY},
	{""},
#line 62 "source/html/css-properties.gperf"
	{"vertical-align",PRO_VERTICAL_ALIGN},
	{""},
#line 44 "source/html/css-properties.gperf"
	{"margin",PRO_MARGIN},
#line 66 "source/html/css-properties.gperf"
	{"width",PRO_WIDTH},
#line 58 "source/html/css-properties.gperf"
	{"src",PRO_SRC},
	{""}, {""},
#line 30 "source/html/css-properties.gperf"
	{"direction",PRO_DIRECTION}
  };

struct css_property_info *
css_property_lookup (register const char *str, register size_t len)
{
  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
	{
	  register unsigned int key = css_property_hash (str, len);

	  if (key <= MAX_HASH_VALUE)
		{
		  register const char *s = css_property_list[key].name;

		  if (*str == *s && !strcmp (str + 1, s + 1))
			return &css_property_list[key];
		}
	}
  return 0;
}
