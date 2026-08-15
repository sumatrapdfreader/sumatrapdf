# XML Parser

We have a rudimentary XML parser that handles well formed XML. It does not do
any namespace processing, and it does not validate the XML syntax.

The parser supports `UTF-8`, `UTF-16`, `iso-8859-1`, `iso-8859-7`, `koi8`,
`windows-1250`, `windows-1251`, and `windows-1252` encoded input.

If `preserve_white` is *false*, we will discard all *whitespace-only* text
elements. This is useful for parsing non-text documents such as XPS and SVG.
Preserving whitespace is useful for parsing XHTML.

	typedef struct { opaque } fz_xml_doc;
	typedef struct { opaque } fz_xml;

	fz_xml_doc *fz_parse_xml(fz_context *ctx, fz_buffer *buf, int preserve_white);
	void fz_drop_xml(fz_context *ctx, fz_xml_doc *xml);
	fz_xml *fz_xml_root(fz_xml_doc *xml);

	fz_xml *fz_xml_prev(fz_xml *item);
	fz_xml *fz_xml_next(fz_xml *item);
	fz_xml *fz_xml_up(fz_xml *item);
	fz_xml *fz_xml_down(fz_xml *item);

`int fz_xml_is_tag(fz_xml *item, const char *name);`
:	Returns *true* if the element is a tag with the given name.

`char *fz_xml_tag(fz_xml *item);`
:	Returns the tag name if the element is a tag, otherwise `NULL`.

`char *fz_xml_att(fz_xml *item, const char *att);`
:	Returns the value of the tag element's attribute, or `NULL` if not a tag or missing.

`char *fz_xml_text(fz_xml *item);`
:	Returns the `UTF-8` text of the text element, or `NULL` if not a text element.

`fz_xml *fz_xml_find(fz_xml *item, const char *tag);`
:	Find the next element with the given tag name. Returns the element
	itself if it matches, or the first sibling if it doesn't. Returns
	`NULL` if there is no sibling with that tag name.

`fz_xml *fz_xml_find_next(fz_xml *item, const char *tag);`
:	Find the next sibling element with the given tag name, or `NULL` if none.

`fz_xml *fz_xml_find_down(fz_xml *item, const char *tag);`
:	Find the first child element with the given tag name, or `NULL` if none.
