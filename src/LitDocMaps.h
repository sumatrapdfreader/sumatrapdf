/* Microsoft LIT tag / attribute code tables, for reconstructing HTML and
   OPF from the tokenized binary form used inside .lit files.

   Generated from calibre's src/calibre/ebooks/lit/maps/{html,opf}.py (which
   are themselves copied from ConvertLIT). Do not edit by hand. */

struct LitAttrCode {
    u32 code;
    const char* name;
};

// tag code -> name, indexed by the code; "\x01" marks a code with no tag
static SeqStrings gLitHtmlTags =
    "\x01\0" "\x01\0" "\x01\0" "a\0" "acronym\0" "address\0" "applet\0" "area\0" "b\0" "base\0" "basefont\0" "bdo\0"
    "bgsound\0" "big\0" "blink\0" "blockquote\0" "body\0" "br\0" "button\0" "caption\0" "center\0" "cite\0" "code\0"
    "col\0" "colgroup\0" "\x01\0" "\x01\0" "dd\0" "del\0" "dfn\0" "dir\0" "div\0" "dl\0" "dt\0" "em\0" "embed\0"
    "fieldset\0" "font\0" "form\0" "frame\0" "frameset\0" "\x01\0" "h1\0" "h2\0" "h3\0" "h4\0" "h5\0" "h6\0" "head\0"
    "hr\0" "html\0" "i\0" "iframe\0" "img\0" "input\0" "ins\0" "kbd\0" "label\0" "legend\0" "li\0" "link\0" "tag61\0"
    "map\0" "tag63\0" "tag64\0" "meta\0" "nextid\0" "nobr\0" "noembed\0" "noframes\0" "noscript\0" "object\0" "ol\0"
    "option\0" "p\0" "param\0" "plaintext\0" "pre\0" "q\0" "rp\0" "rt\0" "ruby\0" "s\0" "samp\0" "script\0" "select\0"
    "small\0" "span\0" "strike\0" "strong\0" "style\0" "sub\0" "sup\0" "table\0" "tbody\0" "tc\0" "td\0" "textarea\0"
    "tfoot\0" "th\0" "thead\0" "title\0" "tr\0" "tt\0" "u\0" "ul\0" "var\0" "wbr\0" "\x01\0" "\0";

static const LitAttrCode gLitHtmlAttrs[] = {
    {0x8010, "tabindex"}, {0x8046, "title"}, {0x804b, "style"}, {0x804d, "disabled"}, {0x83ea, "class"},
    {0x83eb, "id"}, {0x83fe, "datafld"}, {0x83ff, "datasrc"}, {0x8400, "dataformatas"}, {0x87d6, "accesskey"},
    {0x9392, "lang"}, {0x93ed, "language"}, {0x93fe, "dir"}, {0x9771, "onmouseover"}, {0x9772, "onmouseout"},
    {0x9773, "onmousedown"}, {0x9774, "onmouseup"}, {0x9775, "onmousemove"}, {0x9776, "onkeydown"},
    {0x9777, "onkeyup"}, {0x9778, "onkeypress"}, {0x9779, "onclick"}, {0x977a, "ondblclick"}, {0x977e, "onhelp"},
    {0x977f, "onfocus"}, {0x9780, "onblur"}, {0x9783, "onrowexit"}, {0x9784, "onrowenter"},
    {0x9786, "onbeforeupdate"}, {0x9787, "onafterupdate"}, {0x978a, "onreadystatechange"}, {0x9790, "onscroll"},
    {0x9794, "ondragstart"}, {0x9795, "onresize"}, {0x9796, "onselectstart"}, {0x9797, "onerrorupdate"},
    {0x9799, "ondatasetchanged"}, {0x979a, "ondataavailable"}, {0x979b, "ondatasetcomplete"},
    {0x979c, "onfilterchange"}, {0x979f, "onlosecapture"}, {0x97a0, "onpropertychange"}, {0x97a2, "ondrag"},
    {0x97a3, "ondragend"}, {0x97a4, "ondragenter"}, {0x97a5, "ondragover"}, {0x97a6, "ondragleave"},
    {0x97a7, "ondrop"}, {0x97a8, "oncut"}, {0x97a9, "oncopy"}, {0x97aa, "onpaste"}, {0x97ab, "onbeforecut"},
    {0x97ac, "onbeforecopy"}, {0x97ad, "onbeforepaste"}, {0x97af, "onrowsdelete"}, {0x97b0, "onrowsinserted"},
    {0x97b1, "oncellchange"}, {0x97b2, "oncontextmenu"}, {0x97b6, "onbeforeeditfocus"},
};

static const LitAttrCode gLitHtmlATTRS3[] = {
    {0x1, "href"}, {0x3ec, "target"}, {0x3ee, "rel"}, {0x3ef, "rev"}, {0x3f0, "urn"}, {0x3f1, "methods"},
    {0x8001, "name"}, {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS5[] = {
    {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS6[] = {
    {0x8001, "name"}, {0x8006, "width"}, {0x8007, "height"}, {0x804a, "align"}, {0x8bbb, "classid"}, {0x8bbc, "data"},
    {0x8bbf, "codebase"}, {0x8bc0, "codetype"}, {0x8bc1, "code"}, {0x8bc2, "type"}, {0x8bc5, "vspace"},
    {0x8bc6, "hspace"}, {0x978e, "onerror"},
};

static const LitAttrCode gLitHtmlATTRS7[] = {
    {0x1, "href"}, {0x3ea, "shape"}, {0x3eb, "coords"}, {0x3ed, "target"}, {0x3ee, "alt"}, {0x3ef, "nohref"},
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS8[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS9[] = {
    {0x3ec, "href"}, {0x3ed, "target"},
};

static const LitAttrCode gLitHtmlATTRS10[] = {
    {0x938b, "color"}, {0x939b, "face"}, {0x93a3, "size"},
};

static const LitAttrCode gLitHtmlATTRS12[] = {
    {0x3ea, "src"}, {0x3eb, "loop"}, {0x3ec, "volume"}, {0x3ed, "balance"},
};

static const LitAttrCode gLitHtmlATTRS13[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS15[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS16[] = {
    {0x7db, "link"}, {0x7dc, "alink"}, {0x7dd, "vlink"}, {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"},
    {0x83eb, "id"}, {0x938a, "background"}, {0x938b, "text"}, {0x938e, "nowrap"}, {0x93ae, "topmargin"},
    {0x93af, "rightmargin"}, {0x93b0, "bottommargin"}, {0x93b1, "leftmargin"}, {0x93b6, "bgproperties"},
    {0x93d8, "scroll"}, {0x977b, "onselect"}, {0x9791, "onload"}, {0x9792, "onunload"}, {0x9798, "onbeforeunload"},
    {0x97b3, "onbeforeprint"}, {0x97b4, "onafterprint"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS17[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS18[] = {
    {0x7d1, "type"}, {0x8001, "name"},
};

static const LitAttrCode gLitHtmlATTRS19[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x93a8, "valign"},
};

static const LitAttrCode gLitHtmlATTRS20[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS21[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS22[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS23[] = {
    {0x3ea, "span"}, {0x8006, "width"}, {0x8049, "align"}, {0x93a8, "valign"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS24[] = {
    {0x3ea, "span"}, {0x8006, "width"}, {0x8049, "align"}, {0x93a8, "valign"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS27[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x938e, "nowrap"},
};

static const LitAttrCode gLitHtmlATTRS29[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS31[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x938e, "nowrap"},
};

static const LitAttrCode gLitHtmlATTRS32[] = {
    {0x3ea, "compact"}, {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS33[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x938e, "nowrap"},
};

static const LitAttrCode gLitHtmlATTRS34[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS35[] = {
    {0x8001, "name"}, {0x8006, "width"}, {0x8007, "height"}, {0x804a, "align"}, {0x8bbd, "palette"},
    {0x8bbe, "pluginspage"}, {0x8bbf, "src"}, {0x8bc1, "units"}, {0x8bc2, "type"}, {0x8bc3, "hidden"},
};

static const LitAttrCode gLitHtmlATTRS36[] = {
    {0x804a, "align"},
};

static const LitAttrCode gLitHtmlATTRS37[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x938b, "color"}, {0x939b, "face"},
    {0x939c, "size"},
};

static const LitAttrCode gLitHtmlATTRS38[] = {
    {0x3ea, "action"}, {0x3ec, "enctype"}, {0x3ed, "method"}, {0x3ef, "target"}, {0x3f4, "accept-charset"},
    {0x8001, "name"}, {0x977c, "onsubmit"}, {0x977d, "onreset"},
};

static const LitAttrCode gLitHtmlATTRS39[] = {
    {0x8000, "align"}, {0x8001, "name"}, {0x8bb9, "src"}, {0x8bbb, "border"}, {0x8bbc, "frameborder"},
    {0x8bbd, "framespacing"}, {0x8bbe, "marginwidth"}, {0x8bbf, "marginheight"}, {0x8bc0, "noresize"},
    {0x8bc1, "scrolling"}, {0x8fa2, "bordercolor"},
};

static const LitAttrCode gLitHtmlATTRS40[] = {
    {0x3e9, "rows"}, {0x3ea, "cols"}, {0x3eb, "border"}, {0x3ec, "bordercolor"}, {0x3ed, "frameborder"},
    {0x3ee, "framespacing"}, {0x8001, "name"}, {0x9791, "onload"}, {0x9792, "onunload"}, {0x9798, "onbeforeunload"},
    {0x97b3, "onbeforeprint"}, {0x97b4, "onafterprint"},
};

static const LitAttrCode gLitHtmlATTRS42[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS43[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS44[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS45[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS46[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS47[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS49[] = {
    {0x3ea, "noshade"}, {0x8006, "width"}, {0x8007, "size"}, {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"},
    {0x83ea, "class"}, {0x83eb, "id"}, {0x938b, "color"},
};

static const LitAttrCode gLitHtmlATTRS51[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS52[] = {
    {0x8001, "name"}, {0x8006, "width"}, {0x8007, "height"}, {0x804a, "align"}, {0x8bb9, "src"}, {0x8bbb, "border"},
    {0x8bbc, "frameborder"}, {0x8bbd, "framespacing"}, {0x8bbe, "marginwidth"}, {0x8bbf, "marginheight"},
    {0x8bc0, "noresize"}, {0x8bc1, "scrolling"}, {0x8fa2, "vspace"}, {0x8fa3, "hspace"},
};

static const LitAttrCode gLitHtmlATTRS53[] = {
    {0x3eb, "alt"}, {0x3ec, "src"}, {0x3ed, "border"}, {0x3ee, "vspace"}, {0x3ef, "hspace"}, {0x3f0, "lowsrc"},
    {0x3f1, "vrml"}, {0x3f2, "dynsrc"}, {0x3f4, "loop"}, {0x3f6, "start"}, {0x7d3, "ismap"}, {0x7d9, "usemap"},
    {0x8001, "name"}, {0x8006, "width"}, {0x8007, "height"}, {0x8046, "title"}, {0x804a, "align"}, {0x804b, "style"},
    {0x83ea, "class"}, {0x83eb, "id"}, {0x978d, "onabort"}, {0x978e, "onerror"}, {0x9791, "onload"},
};

static const LitAttrCode gLitHtmlATTRS54[] = {
    {0x7d1, "type"}, {0x7d3, "size"}, {0x7d4, "maxlength"}, {0x7d6, "readonly"}, {0x7d8, "indeterminate"},
    {0x7da, "checked"}, {0x7db, "alt"}, {0x7dc, "src"}, {0x7dd, "border"}, {0x7de, "vspace"}, {0x7df, "hspace"},
    {0x7e0, "lowsrc"}, {0x7e1, "vrml"}, {0x7e2, "dynsrc"}, {0x7e4, "loop"}, {0x7e5, "start"}, {0x8001, "name"},
    {0x8006, "width"}, {0x8007, "height"}, {0x804a, "align"}, {0x93ee, "value"}, {0x977b, "onselect"},
    {0x978d, "onabort"}, {0x978e, "onerror"}, {0x978f, "onchange"}, {0x9791, "onload"},
};

static const LitAttrCode gLitHtmlATTRS56[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS57[] = {
    {0x3e9, "for"},
};

static const LitAttrCode gLitHtmlATTRS58[] = {
    {0x804a, "align"},
};

static const LitAttrCode gLitHtmlATTRS59[] = {
    {0x3ea, "value"}, {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x939a, "type"},
};

static const LitAttrCode gLitHtmlATTRS60[] = {
    {0x3ee, "href"}, {0x3ef, "rel"}, {0x3f0, "rev"}, {0x3f1, "type"}, {0x3f9, "media"}, {0x3fa, "target"},
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x978e, "onerror"}, {0x9791, "onload"},
};

static const LitAttrCode gLitHtmlATTRS61[] = {
    {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS62[] = {
    {0x8001, "name"}, {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS63[] = {
    {0x1771, "scrolldelay"}, {0x1772, "direction"}, {0x1773, "behavior"}, {0x1774, "scrollamount"}, {0x1775, "loop"},
    {0x1776, "vspace"}, {0x1777, "hspace"}, {0x1778, "truespeed"}, {0x8006, "width"}, {0x8007, "height"},
    {0x9785, "onbounce"}, {0x978b, "onfinish"}, {0x978c, "onstart"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS65[] = {
    {0x3ea, "http-equiv"}, {0x3eb, "content"}, {0x3ec, "url"}, {0x3f6, "charset"}, {0x8001, "name"},
};

static const LitAttrCode gLitHtmlATTRS66[] = {
    {0x3f5, "n"},
};

static const LitAttrCode gLitHtmlATTRS71[] = {
    {0x8000, "usemap"}, {0x8001, "name"}, {0x8006, "width"}, {0x8007, "height"}, {0x8046, "title"}, {0x804a, "align"},
    {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x8bbb, "classid"}, {0x8bbc, "data"}, {0x8bbf, "codebase"},
    {0x8bc0, "codetype"}, {0x8bc1, "code"}, {0x8bc2, "type"}, {0x8bc5, "vspace"}, {0x8bc6, "hspace"},
    {0x978e, "onerror"},
};

static const LitAttrCode gLitHtmlATTRS72[] = {
    {0x3eb, "compact"}, {0x3ec, "start"}, {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
    {0x939a, "type"},
};

static const LitAttrCode gLitHtmlATTRS73[] = {
    {0x3ea, "selected"}, {0x3eb, "value"},
};

static const LitAttrCode gLitHtmlATTRS74[] = {
    {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS75[] = {
    {0x8000, "type"},
};

static const LitAttrCode gLitHtmlATTRS76[] = {
    {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS77[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x9399, "clear"},
};

static const LitAttrCode gLitHtmlATTRS78[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS82[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS83[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS84[] = {
    {0x3ea, "src"}, {0x3ed, "for"}, {0x3ee, "event"}, {0x3f0, "defer"}, {0x3f2, "type"}, {0x978e, "onerror"},
};

static const LitAttrCode gLitHtmlATTRS85[] = {
    {0x3eb, "size"}, {0x3ec, "multiple"}, {0x8000, "align"}, {0x8001, "name"}, {0x978f, "onchange"},
};

static const LitAttrCode gLitHtmlATTRS86[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS87[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS88[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS89[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS90[] = {
    {0x3eb, "type"}, {0x3ef, "media"}, {0x8046, "title"}, {0x978e, "onerror"}, {0x9791, "onload"},
};

static const LitAttrCode gLitHtmlATTRS91[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS92[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS93[] = {
    {0x3ea, "cols"}, {0x3eb, "border"}, {0x3ec, "rules"}, {0x3ed, "frame"}, {0x3ee, "cellspacing"},
    {0x3ef, "cellpadding"}, {0x3fa, "datapagesize"}, {0x8006, "width"}, {0x8007, "height"}, {0x8046, "title"},
    {0x804a, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x938a, "background"},
    {0x93a5, "bordercolor"}, {0x93a6, "bordercolorlight"}, {0x93a7, "bordercolordark"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS94[] = {
    {0x8049, "align"}, {0x93a8, "valign"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS95[] = {
    {0x8049, "align"}, {0x93a8, "valign"},
};

static const LitAttrCode gLitHtmlATTRS96[] = {
    {0x7d2, "rowspan"}, {0x7d3, "colspan"}, {0x8006, "width"}, {0x8007, "height"}, {0x8046, "title"},
    {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x938a, "background"},
    {0x938e, "nowrap"}, {0x93a5, "bordercolor"}, {0x93a6, "bordercolorlight"}, {0x93a7, "bordercolordark"},
    {0x93a8, "valign"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS97[] = {
    {0x1b5a, "rows"}, {0x1b5b, "cols"}, {0x1b5c, "wrap"}, {0x1b5d, "readonly"}, {0x8001, "name"},
    {0x977b, "onselect"}, {0x978f, "onchange"},
};

static const LitAttrCode gLitHtmlATTRS98[] = {
    {0x8049, "align"}, {0x93a8, "valign"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS99[] = {
    {0x7d2, "rowspan"}, {0x7d3, "colspan"}, {0x8006, "width"}, {0x8007, "height"}, {0x8046, "title"},
    {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x938a, "background"},
    {0x938e, "nowrap"}, {0x93a5, "bordercolor"}, {0x93a6, "bordercolorlight"}, {0x93a7, "bordercolordark"},
    {0x93a8, "valign"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS100[] = {
    {0x8049, "align"}, {0x93a8, "valign"}, {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS102[] = {
    {0x8007, "height"}, {0x8046, "title"}, {0x8049, "align"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
    {0x93a5, "bordercolor"}, {0x93a6, "bordercolorlight"}, {0x93a7, "bordercolordark"}, {0x93a8, "valign"},
    {0xfe0c, "bgcolor"},
};

static const LitAttrCode gLitHtmlATTRS103[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS104[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

static const LitAttrCode gLitHtmlATTRS105[] = {
    {0x3eb, "compact"}, {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"}, {0x939a, "type"},
};

static const LitAttrCode gLitHtmlATTRS106[] = {
    {0x8046, "title"}, {0x804b, "style"}, {0x83ea, "class"}, {0x83eb, "id"},
};

struct LitTagAttrList {
    const LitAttrCode* attrs;
    int n;
};

static const LitTagAttrList gLitHtmlTagAttrs[] = {
    {nullptr, 0}, // 0
    {nullptr, 0}, // 1
    {nullptr, 0}, // 2
    {gLitHtmlATTRS3, dimofi(gLitHtmlATTRS3)}, // a
    {nullptr, 0}, // acronym
    {gLitHtmlATTRS5, dimofi(gLitHtmlATTRS5)}, // address
    {gLitHtmlATTRS6, dimofi(gLitHtmlATTRS6)}, // applet
    {gLitHtmlATTRS7, dimofi(gLitHtmlATTRS7)}, // area
    {gLitHtmlATTRS8, dimofi(gLitHtmlATTRS8)}, // b
    {gLitHtmlATTRS9, dimofi(gLitHtmlATTRS9)}, // base
    {gLitHtmlATTRS10, dimofi(gLitHtmlATTRS10)}, // basefont
    {nullptr, 0}, // bdo
    {gLitHtmlATTRS12, dimofi(gLitHtmlATTRS12)}, // bgsound
    {gLitHtmlATTRS13, dimofi(gLitHtmlATTRS13)}, // big
    {nullptr, 0}, // blink
    {gLitHtmlATTRS15, dimofi(gLitHtmlATTRS15)}, // blockquote
    {gLitHtmlATTRS16, dimofi(gLitHtmlATTRS16)}, // body
    {gLitHtmlATTRS17, dimofi(gLitHtmlATTRS17)}, // br
    {gLitHtmlATTRS18, dimofi(gLitHtmlATTRS18)}, // button
    {gLitHtmlATTRS19, dimofi(gLitHtmlATTRS19)}, // caption
    {gLitHtmlATTRS20, dimofi(gLitHtmlATTRS20)}, // center
    {gLitHtmlATTRS21, dimofi(gLitHtmlATTRS21)}, // cite
    {gLitHtmlATTRS22, dimofi(gLitHtmlATTRS22)}, // code
    {gLitHtmlATTRS23, dimofi(gLitHtmlATTRS23)}, // col
    {gLitHtmlATTRS24, dimofi(gLitHtmlATTRS24)}, // colgroup
    {nullptr, 0}, // 25
    {nullptr, 0}, // 26
    {gLitHtmlATTRS27, dimofi(gLitHtmlATTRS27)}, // dd
    {nullptr, 0}, // del
    {gLitHtmlATTRS29, dimofi(gLitHtmlATTRS29)}, // dfn
    {nullptr, 0}, // dir
    {gLitHtmlATTRS31, dimofi(gLitHtmlATTRS31)}, // div
    {gLitHtmlATTRS32, dimofi(gLitHtmlATTRS32)}, // dl
    {gLitHtmlATTRS33, dimofi(gLitHtmlATTRS33)}, // dt
    {gLitHtmlATTRS34, dimofi(gLitHtmlATTRS34)}, // em
    {gLitHtmlATTRS35, dimofi(gLitHtmlATTRS35)}, // embed
    {gLitHtmlATTRS36, dimofi(gLitHtmlATTRS36)}, // fieldset
    {gLitHtmlATTRS37, dimofi(gLitHtmlATTRS37)}, // font
    {gLitHtmlATTRS38, dimofi(gLitHtmlATTRS38)}, // form
    {gLitHtmlATTRS39, dimofi(gLitHtmlATTRS39)}, // frame
    {gLitHtmlATTRS40, dimofi(gLitHtmlATTRS40)}, // frameset
    {nullptr, 0}, // 41
    {gLitHtmlATTRS42, dimofi(gLitHtmlATTRS42)}, // h1
    {gLitHtmlATTRS43, dimofi(gLitHtmlATTRS43)}, // h2
    {gLitHtmlATTRS44, dimofi(gLitHtmlATTRS44)}, // h3
    {gLitHtmlATTRS45, dimofi(gLitHtmlATTRS45)}, // h4
    {gLitHtmlATTRS46, dimofi(gLitHtmlATTRS46)}, // h5
    {gLitHtmlATTRS47, dimofi(gLitHtmlATTRS47)}, // h6
    {nullptr, 0}, // head
    {gLitHtmlATTRS49, dimofi(gLitHtmlATTRS49)}, // hr
    {nullptr, 0}, // html
    {gLitHtmlATTRS51, dimofi(gLitHtmlATTRS51)}, // i
    {gLitHtmlATTRS52, dimofi(gLitHtmlATTRS52)}, // iframe
    {gLitHtmlATTRS53, dimofi(gLitHtmlATTRS53)}, // img
    {gLitHtmlATTRS54, dimofi(gLitHtmlATTRS54)}, // input
    {nullptr, 0}, // ins
    {gLitHtmlATTRS56, dimofi(gLitHtmlATTRS56)}, // kbd
    {gLitHtmlATTRS57, dimofi(gLitHtmlATTRS57)}, // label
    {gLitHtmlATTRS58, dimofi(gLitHtmlATTRS58)}, // legend
    {gLitHtmlATTRS59, dimofi(gLitHtmlATTRS59)}, // li
    {gLitHtmlATTRS60, dimofi(gLitHtmlATTRS60)}, // link
    {gLitHtmlATTRS61, dimofi(gLitHtmlATTRS61)}, // tag61
    {gLitHtmlATTRS62, dimofi(gLitHtmlATTRS62)}, // map
    {gLitHtmlATTRS63, dimofi(gLitHtmlATTRS63)}, // tag63
    {nullptr, 0}, // tag64
    {gLitHtmlATTRS65, dimofi(gLitHtmlATTRS65)}, // meta
    {gLitHtmlATTRS66, dimofi(gLitHtmlATTRS66)}, // nextid
    {nullptr, 0}, // nobr
    {nullptr, 0}, // noembed
    {nullptr, 0}, // noframes
    {nullptr, 0}, // noscript
    {gLitHtmlATTRS71, dimofi(gLitHtmlATTRS71)}, // object
    {gLitHtmlATTRS72, dimofi(gLitHtmlATTRS72)}, // ol
    {gLitHtmlATTRS73, dimofi(gLitHtmlATTRS73)}, // option
    {gLitHtmlATTRS74, dimofi(gLitHtmlATTRS74)}, // p
    {gLitHtmlATTRS75, dimofi(gLitHtmlATTRS75)}, // param
    {gLitHtmlATTRS76, dimofi(gLitHtmlATTRS76)}, // plaintext
    {gLitHtmlATTRS77, dimofi(gLitHtmlATTRS77)}, // pre
    {gLitHtmlATTRS78, dimofi(gLitHtmlATTRS78)}, // q
    {nullptr, 0}, // rp
    {nullptr, 0}, // rt
    {nullptr, 0}, // ruby
    {gLitHtmlATTRS82, dimofi(gLitHtmlATTRS82)}, // s
    {gLitHtmlATTRS83, dimofi(gLitHtmlATTRS83)}, // samp
    {gLitHtmlATTRS84, dimofi(gLitHtmlATTRS84)}, // script
    {gLitHtmlATTRS85, dimofi(gLitHtmlATTRS85)}, // select
    {gLitHtmlATTRS86, dimofi(gLitHtmlATTRS86)}, // small
    {gLitHtmlATTRS87, dimofi(gLitHtmlATTRS87)}, // span
    {gLitHtmlATTRS88, dimofi(gLitHtmlATTRS88)}, // strike
    {gLitHtmlATTRS89, dimofi(gLitHtmlATTRS89)}, // strong
    {gLitHtmlATTRS90, dimofi(gLitHtmlATTRS90)}, // style
    {gLitHtmlATTRS91, dimofi(gLitHtmlATTRS91)}, // sub
    {gLitHtmlATTRS92, dimofi(gLitHtmlATTRS92)}, // sup
    {gLitHtmlATTRS93, dimofi(gLitHtmlATTRS93)}, // table
    {gLitHtmlATTRS94, dimofi(gLitHtmlATTRS94)}, // tbody
    {gLitHtmlATTRS95, dimofi(gLitHtmlATTRS95)}, // tc
    {gLitHtmlATTRS96, dimofi(gLitHtmlATTRS96)}, // td
    {gLitHtmlATTRS97, dimofi(gLitHtmlATTRS97)}, // textarea
    {gLitHtmlATTRS98, dimofi(gLitHtmlATTRS98)}, // tfoot
    {gLitHtmlATTRS99, dimofi(gLitHtmlATTRS99)}, // th
    {gLitHtmlATTRS100, dimofi(gLitHtmlATTRS100)}, // thead
    {nullptr, 0}, // title
    {gLitHtmlATTRS102, dimofi(gLitHtmlATTRS102)}, // tr
    {gLitHtmlATTRS103, dimofi(gLitHtmlATTRS103)}, // tt
    {gLitHtmlATTRS104, dimofi(gLitHtmlATTRS104)}, // u
    {gLitHtmlATTRS105, dimofi(gLitHtmlATTRS105)}, // ul
    {gLitHtmlATTRS106, dimofi(gLitHtmlATTRS106)}, // var
    {nullptr, 0}, // wbr
    {nullptr, 0}, // 108
};

static const char* gLitOpfTags[] = {
    nullptr, "package", "dc:Title", "dc:Creator", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, "manifest", "item", "spine", "itemref", "metadata", "dc-metadata",
    "dc:Subject", "dc:Description", "dc:Publisher", "dc:Contributor", "dc:Date", "dc:Type", "dc:Format",
    "dc:Identifier", "dc:Source", "dc:Language", "dc:Relation", "dc:Coverage", "dc:Rights", "x-metadata", "meta",
    "tours", "tour", "site", "guide", "reference", nullptr,
};

static const LitAttrCode gLitOpfAttrs[] = {
    {0x1, "href"}, {0x2, "%never-used"}, {0x3, "%guid"}, {0x4, "%minimum_level"}, {0x5, "%attr5"}, {0x6, "id"},
    {0x7, "href"}, {0x8, "media-type"}, {0x9, "fallback"}, {0xa, "idref"}, {0xb, "xmlns:dc"},
    {0xc, "xmlns:oebpackage"}, {0xd, "role"}, {0xe, "file-as"}, {0xf, "event"}, {0x10, "scheme"}, {0x11, "title"},
    {0x12, "type"}, {0x13, "unique-identifier"}, {0x14, "name"}, {0x15, "content"}, {0x16, "xml:lang"},
};
