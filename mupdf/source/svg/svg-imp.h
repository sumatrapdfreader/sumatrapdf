#ifndef SOURCE_SVG_IMP_H
#define SOURCE_SVG_IMP_H

typedef struct svg_document_s svg_document;

struct svg_document_s
{
	fz_document super;
	fz_xml_doc *xml;
	fz_xml *root;
	fz_tree *idmap;
	float width;
	float height;
	fz_archive *zip; /* for locating external resources */
	char base_uri[2048];
};

const char *svg_lex_number(float *fp, const char *str);
float svg_parse_number(const char *str, float min, float max, float inherit);
float svg_parse_length(const char *str, float percent, float font_size);
float svg_parse_angle(const char *str);

void svg_parse_color_from_style(fz_context *ctx, svg_document *doc, const char *str,
	int *fill_is_set, float fill[3], int *stroke_is_set, float stroke[3]);
void svg_parse_color(fz_context *ctx, svg_document *doc, const char *str, float *rgb);
fz_matrix svg_parse_transform(fz_context *ctx, svg_document *doc, const char *str, fz_matrix transform);

int svg_is_whitespace_or_comma(int c);
int svg_is_whitespace(int c);
int svg_is_alpha(int c);
int svg_is_digit(int c);

void svg_parse_document_bounds(fz_context *ctx, svg_document *doc, fz_xml *root);
void svg_run_document(fz_context *ctx, svg_document *doc, fz_xml *root, fz_device *dev, fz_matrix ctm);

#endif
