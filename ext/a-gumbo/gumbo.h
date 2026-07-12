#ifndef GUMBO_GUMBO_H_
#define GUMBO_GUMBO_H_

#ifdef _MSC_VER
#define fileno _fileno
#endif

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned int line;
  unsigned int column;
  unsigned int offset;
} GumboSourcePosition;

extern const GumboSourcePosition kGumboEmptySourcePosition;

typedef struct {

  const char* data;

  size_t length;
} GumboStringPiece;

extern const GumboStringPiece kGumboEmptyString;

bool gumbo_string_equals(
    const GumboStringPiece* str1, const GumboStringPiece* str2);

bool gumbo_string_equals_ignore_case(
    const GumboStringPiece* str1, const GumboStringPiece* str2);

typedef struct {

  void** data;

  unsigned int length;

  unsigned int capacity;
} GumboVector;

extern const GumboVector kGumboEmptyVector;

int gumbo_vector_index_of(GumboVector* vector, const void* element);

typedef enum {

GUMBO_TAG_HTML,
GUMBO_TAG_HEAD,
GUMBO_TAG_TITLE,
GUMBO_TAG_BASE,
GUMBO_TAG_LINK,
GUMBO_TAG_META,
GUMBO_TAG_STYLE,
GUMBO_TAG_SCRIPT,
GUMBO_TAG_NOSCRIPT,
GUMBO_TAG_TEMPLATE,
GUMBO_TAG_BODY,
GUMBO_TAG_ARTICLE,
GUMBO_TAG_SECTION,
GUMBO_TAG_NAV,
GUMBO_TAG_ASIDE,
GUMBO_TAG_H1,
GUMBO_TAG_H2,
GUMBO_TAG_H3,
GUMBO_TAG_H4,
GUMBO_TAG_H5,
GUMBO_TAG_H6,
GUMBO_TAG_HGROUP,
GUMBO_TAG_HEADER,
GUMBO_TAG_FOOTER,
GUMBO_TAG_ADDRESS,
GUMBO_TAG_P,
GUMBO_TAG_HR,
GUMBO_TAG_PRE,
GUMBO_TAG_BLOCKQUOTE,
GUMBO_TAG_OL,
GUMBO_TAG_UL,
GUMBO_TAG_LI,
GUMBO_TAG_DL,
GUMBO_TAG_DT,
GUMBO_TAG_DD,
GUMBO_TAG_FIGURE,
GUMBO_TAG_FIGCAPTION,
GUMBO_TAG_MAIN,
GUMBO_TAG_DIV,
GUMBO_TAG_A,
GUMBO_TAG_EM,
GUMBO_TAG_STRONG,
GUMBO_TAG_SMALL,
GUMBO_TAG_S,
GUMBO_TAG_CITE,
GUMBO_TAG_Q,
GUMBO_TAG_DFN,
GUMBO_TAG_ABBR,
GUMBO_TAG_DATA,
GUMBO_TAG_TIME,
GUMBO_TAG_CODE,
GUMBO_TAG_VAR,
GUMBO_TAG_SAMP,
GUMBO_TAG_KBD,
GUMBO_TAG_SUB,
GUMBO_TAG_SUP,
GUMBO_TAG_I,
GUMBO_TAG_B,
GUMBO_TAG_U,
GUMBO_TAG_MARK,
GUMBO_TAG_RUBY,
GUMBO_TAG_RT,
GUMBO_TAG_RP,
GUMBO_TAG_BDI,
GUMBO_TAG_BDO,
GUMBO_TAG_SPAN,
GUMBO_TAG_BR,
GUMBO_TAG_WBR,
GUMBO_TAG_INS,
GUMBO_TAG_DEL,
GUMBO_TAG_IMAGE,
GUMBO_TAG_IMG,
GUMBO_TAG_IFRAME,
GUMBO_TAG_EMBED,
GUMBO_TAG_OBJECT,
GUMBO_TAG_PARAM,
GUMBO_TAG_VIDEO,
GUMBO_TAG_AUDIO,
GUMBO_TAG_SOURCE,
GUMBO_TAG_TRACK,
GUMBO_TAG_CANVAS,
GUMBO_TAG_MAP,
GUMBO_TAG_AREA,
GUMBO_TAG_MATH,
GUMBO_TAG_MI,
GUMBO_TAG_MO,
GUMBO_TAG_MN,
GUMBO_TAG_MS,
GUMBO_TAG_MTEXT,
GUMBO_TAG_MGLYPH,
GUMBO_TAG_MALIGNMARK,
GUMBO_TAG_ANNOTATION_XML,
GUMBO_TAG_SVG,
GUMBO_TAG_FOREIGNOBJECT,
GUMBO_TAG_DESC,
GUMBO_TAG_TABLE,
GUMBO_TAG_CAPTION,
GUMBO_TAG_COLGROUP,
GUMBO_TAG_COL,
GUMBO_TAG_TBODY,
GUMBO_TAG_THEAD,
GUMBO_TAG_TFOOT,
GUMBO_TAG_TR,
GUMBO_TAG_TD,
GUMBO_TAG_TH,
GUMBO_TAG_FORM,
GUMBO_TAG_FIELDSET,
GUMBO_TAG_LEGEND,
GUMBO_TAG_LABEL,
GUMBO_TAG_INPUT,
GUMBO_TAG_BUTTON,
GUMBO_TAG_SELECT,
GUMBO_TAG_DATALIST,
GUMBO_TAG_OPTGROUP,
GUMBO_TAG_OPTION,
GUMBO_TAG_TEXTAREA,
GUMBO_TAG_KEYGEN,
GUMBO_TAG_OUTPUT,
GUMBO_TAG_PROGRESS,
GUMBO_TAG_METER,
GUMBO_TAG_DETAILS,
GUMBO_TAG_SUMMARY,
GUMBO_TAG_MENU,
GUMBO_TAG_MENUITEM,
GUMBO_TAG_APPLET,
GUMBO_TAG_ACRONYM,
GUMBO_TAG_BGSOUND,
GUMBO_TAG_DIR,
GUMBO_TAG_FRAME,
GUMBO_TAG_FRAMESET,
GUMBO_TAG_NOFRAMES,
GUMBO_TAG_ISINDEX,
GUMBO_TAG_LISTING,
GUMBO_TAG_XMP,
GUMBO_TAG_NEXTID,
GUMBO_TAG_NOEMBED,
GUMBO_TAG_PLAINTEXT,
GUMBO_TAG_RB,
GUMBO_TAG_STRIKE,
GUMBO_TAG_BASEFONT,
GUMBO_TAG_BIG,
GUMBO_TAG_BLINK,
GUMBO_TAG_CENTER,
GUMBO_TAG_FONT,
GUMBO_TAG_MARQUEE,
GUMBO_TAG_MULTICOL,
GUMBO_TAG_NOBR,
GUMBO_TAG_SPACER,
GUMBO_TAG_TT,
GUMBO_TAG_RTC,

  GUMBO_TAG_UNKNOWN,

  GUMBO_TAG_LAST,
} GumboTag;

const char* gumbo_normalized_tagname(GumboTag tag);

void gumbo_tag_from_original_text(GumboStringPiece* text);

const char* gumbo_normalize_svg_tagname(const GumboStringPiece* tagname);

GumboTag gumbo_tag_enum(const char* tagname);
GumboTag gumbo_tagn_enum(const char* tagname, unsigned int length);

typedef enum {
  GUMBO_ATTR_NAMESPACE_NONE,
  GUMBO_ATTR_NAMESPACE_XLINK,
  GUMBO_ATTR_NAMESPACE_XML,
  GUMBO_ATTR_NAMESPACE_XMLNS,
} GumboAttributeNamespaceEnum;

typedef struct {

  GumboAttributeNamespaceEnum attr_namespace;

  const char* name;

  GumboStringPiece original_name;

  const char* value;

  GumboStringPiece original_value;

  GumboSourcePosition name_start;

  GumboSourcePosition name_end;

  GumboSourcePosition value_start;

  GumboSourcePosition value_end;
} GumboAttribute;

GumboAttribute* gumbo_get_attribute(const GumboVector* attrs, const char* name);

typedef enum {

  GUMBO_NODE_DOCUMENT,

  GUMBO_NODE_ELEMENT,

  GUMBO_NODE_TEXT,

  GUMBO_NODE_CDATA,

  GUMBO_NODE_COMMENT,

  GUMBO_NODE_WHITESPACE,

  GUMBO_NODE_TEMPLATE
} GumboNodeType;

typedef struct GumboInternalNode GumboNode;

typedef enum {
  GUMBO_DOCTYPE_NO_QUIRKS,
  GUMBO_DOCTYPE_QUIRKS,
  GUMBO_DOCTYPE_LIMITED_QUIRKS
} GumboQuirksModeEnum;

typedef enum {
  GUMBO_NAMESPACE_HTML,
  GUMBO_NAMESPACE_SVG,
  GUMBO_NAMESPACE_MATHML
} GumboNamespaceEnum;

typedef enum {

  GUMBO_INSERTION_NORMAL = 0,

  GUMBO_INSERTION_BY_PARSER = 1 << 0,

  GUMBO_INSERTION_IMPLICIT_END_TAG = 1 << 1,

  GUMBO_INSERTION_IMPLIED = 1 << 3,

  GUMBO_INSERTION_CONVERTED_FROM_END_TAG = 1 << 4,

  GUMBO_INSERTION_FROM_ISINDEX = 1 << 5,

  GUMBO_INSERTION_FROM_IMAGE = 1 << 6,

  GUMBO_INSERTION_RECONSTRUCTED_FORMATTING_ELEMENT = 1 << 7,

  GUMBO_INSERTION_ADOPTION_AGENCY_CLONED = 1 << 8,

  GUMBO_INSERTION_ADOPTION_AGENCY_MOVED = 1 << 9,

  GUMBO_INSERTION_FOSTER_PARENTED = 1 << 10,
} GumboParseFlags;

typedef struct {

  GumboVector  children;

  bool has_doctype;

  const char* name;
  const char* public_identifier;
  const char* system_identifier;

  GumboQuirksModeEnum doc_type_quirks_mode;
} GumboDocument;

typedef struct {

  const char* text;

  GumboStringPiece original_text;

  GumboSourcePosition start_pos;
} GumboText;

typedef struct {

  GumboVector  children;

  GumboTag tag;

  GumboNamespaceEnum tag_namespace;

  GumboStringPiece original_tag;

  GumboStringPiece original_end_tag;

  GumboSourcePosition start_pos;

  GumboSourcePosition end_pos;

  GumboVector  attributes;
} GumboElement;

struct GumboInternalNode {

  GumboNodeType type;

  GumboNode* parent;

  size_t index_within_parent;

  GumboParseFlags parse_flags;

  union {
    GumboDocument document;
    GumboElement element;
    GumboText text;
  } v;
};

typedef void* (*GumboAllocatorFunction)(void* userdata, size_t size);

typedef void (*GumboDeallocatorFunction)(void* userdata, void* ptr);

typedef struct GumboInternalOptions {

  GumboAllocatorFunction allocator;

  GumboDeallocatorFunction deallocator;

  void* userdata;

  int tab_stop;

  bool stop_on_first_error;

  int max_errors;

  GumboTag fragment_context;

  GumboNamespaceEnum fragment_namespace;
} GumboOptions;

extern const GumboOptions kGumboDefaultOptions;

typedef struct GumboInternalOutput {

  GumboNode* document;

  GumboNode* root;

  GumboVector  errors;
} GumboOutput;

GumboOutput* gumbo_parse(const char* buffer);

GumboOutput* gumbo_parse_with_options(
    const GumboOptions* options, const char* buffer, size_t buffer_length);

void gumbo_destroy_output(const GumboOptions* options, GumboOutput* output);

#ifdef __cplusplus
}
#endif

void gumbo_destroy_node_iter(GumboOptions* options, GumboNode* node);
void gumbo_destroy_output_iter(const GumboOptions* options, GumboOutput* output);
#endif
