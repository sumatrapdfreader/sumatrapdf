/* These extract_docx_*() functions generate docx content and docx zip archive
data.

Caller must call things in a sensible order to create valid content -
e.g. don't call docx_paragraph_start() twice without intervening call to
docx_paragraph_finish(). */

#include "extract/extract.h"

#include "docx_template.h"

#include "astring.h"
#include "document.h"
#include "docx.h"
#include "mem.h"
#include "memento.h"
#include "outf.h"
#include "sys.h"
#include "text.h"
#include "zip.h"

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>


static int
docx_paragraph_start(extract_alloc_t *alloc, extract_astring_t *output)
{
	return extract_astring_cat(alloc, output, "\n\n<w:p>");
}

static int
docx_paragraph_finish(extract_alloc_t *alloc, extract_astring_t *output)
{
	return extract_astring_cat(alloc, output, "\n</w:p>");
}

/* Starts a new run. Caller must ensure that docx_run_finish() was
called to terminate any previous run. */
static int
docx_run_start(	extract_alloc_t   *alloc,
		extract_astring_t *output,
		content_state_t   *content_state)
{
	int e = 0;

	if (!e) e = extract_astring_cat(alloc, output, "\n<w:r><w:rPr><w:rFonts w:ascii=\"");
	if (!e) e = extract_astring_cat(alloc, output, content_state->font.name);
	if (!e) e = extract_astring_cat(alloc, output, "\" w:hAnsi=\"");
	if (!e) e = extract_astring_cat(alloc, output, content_state->font.name);
	if (!e) e = extract_astring_cat(alloc, output, "\"/>");
	if (!e && content_state->font.bold) e = extract_astring_cat(alloc, output, "<w:b/>");
	if (!e && content_state->font.italic) e = extract_astring_cat(alloc, output, "<w:i/>");
	{
		char font_size_text[32];

		if (!e) e = extract_astring_cat(alloc, output, "<w:sz w:val=\"");
		snprintf(font_size_text, sizeof(font_size_text), "%f", content_state->font.size * 2);
		extract_astring_cat(alloc, output, font_size_text);
		extract_astring_cat(alloc, output, "\"/>");

		if (!e) e = extract_astring_cat(alloc, output, "<w:szCs w:val=\"");
		snprintf(font_size_text, sizeof(font_size_text), "%f", content_state->font.size * 2);
		extract_astring_cat(alloc, output, font_size_text);
		extract_astring_cat(alloc, output, "\"/>");
	}
	if (!e) e = extract_astring_cat(alloc, output, "</w:rPr><w:t xml:space=\"preserve\">");

	return e;
}

static int
docx_run_finish(extract_alloc_t   *alloc,
		content_state_t   *state,
		extract_astring_t *output)
{
	if (state) state->font.name = NULL;

	return extract_astring_cat(alloc, output, "</w:t></w:r>");
}

/* Append an empty paragraph to *content. */
static int
docx_paragraph_empty(
		extract_alloc_t   *alloc,
		extract_astring_t *output)
{
	int e = -1;
	static char fontname[] = "OpenSans";
	content_state_t content_state = {0};

	if (docx_paragraph_start(alloc, output)) goto end;
	/* It seems like our choice of font size here doesn't make any difference
	 * to the ammount of vertical space, unless we include a non-space
	 * character. Presumably something to do with the styles in the template
	 * document. */
	content_state.font.name = fontname;
	content_state.font.size = 10;
	content_state.font.bold = 0;
	content_state.font.italic = 0;

	if (docx_run_start(alloc, output, &content_state)) goto end;
	//docx_char_append_string(output, "&#160;");   /* &#160; is non-break space. */
	if (docx_run_finish(alloc, NULL /*state*/, output)) goto end;
	if (docx_paragraph_finish(alloc, output)) goto end;

	e = 0;
end:

    return e;
}


/* Removes last char if it is <c>. */
static int
docx_char_truncate_if(extract_astring_t *output, char c)
{
	if (output->chars_num && output->chars[output->chars_num-1] == c)
		extract_astring_truncate(output, 1);

	return 0;
}


/* Append docx xml for <paragraph> to <content>. Updates *state if we change
font. */
static int
document_to_docx_content_paragraph(
		extract_alloc_t   *alloc,
		content_state_t   *content_state,
		paragraph_t       *paragraph,
		extract_astring_t *content)
{
	int                    e = -1;
	content_line_iterator  lit;
	line_t                *line;

	if (docx_paragraph_start(alloc, content)) goto end;

	if ((paragraph->line_flags & paragraph_not_fully_justified) == 0)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"both\"/></w:pPr>"))
			goto end;
	}
	else if ((paragraph->line_flags & paragraph_not_centred) == 0)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"center\"/></w:pPr>"))
			goto end;
	}
	else if ((paragraph->line_flags & (paragraph_not_aligned_left | paragraph_not_aligned_right)) == paragraph_not_aligned_left)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"right\"/></w:pPr>"))
			goto end;
	}
	else if ((paragraph->line_flags & (paragraph_not_aligned_left | paragraph_not_aligned_right)) == paragraph_not_aligned_right)
	{
		if (extract_astring_cat(alloc, content, "<w:pPr><w:jc w:val=\"left\"/></w:pPr>"))
			goto end;
	}

	for (line = content_line_iterator_init(&lit, &paragraph->content); line != NULL; line = content_line_iterator_next(&lit))
	{
		content_span_iterator  sit;
		span_t                *span;

		for (span = content_span_iterator_init(&sit, &line->content); span != NULL; span = content_span_iterator_next(&sit))
		{
			int si;
			double font_size_new;

			content_state->ctm_prev = &span->ctm;
			font_size_new = extract_font_size(&span->ctm);
			if (!content_state->font.name
				|| strcmp(span->font_name, content_state->font.name)
				|| span->flags.font_bold != content_state->font.bold
				|| span->flags.font_italic != content_state->font.italic
				|| font_size_new != content_state->font.size)
			{
				if (content_state->font.name)
					if (docx_run_finish(alloc, content_state, content))
						goto end;

				content_state->font.name = span->font_name;
				content_state->font.bold = span->flags.font_bold;
				content_state->font.italic = span->flags.font_italic;
				content_state->font.size = font_size_new;
				if (docx_run_start(alloc, content, content_state))
					goto end;
			}

			for (si=0; si<span->chars_num; ++si)
			{
				char_t* char_ = &span->chars[si];
				int c = char_->ucs;
				if (extract_astring_catc_unicode_xml(alloc, content, c))
					goto end;
			}
			/* Remove any trailing '-' at end of line. */
			if (docx_char_truncate_if(content, '-'))
				goto end;
		}
		if (paragraph->line_flags & paragraph_breaks_strangely)
		{
			if (extract_astring_cat(alloc, content, "<w:br/>"))
				goto end;
		}
	}
	if (content_state->font.name)
	{
		if (docx_run_finish(alloc, content_state, content)) goto
			end;
	}
	if (docx_paragraph_finish(alloc, content))
		goto end;

	e = 0;
end:

	return e;
}

/* Write reference to image into docx content. */
static int
docx_append_image(
		extract_alloc_t   *alloc,
		extract_astring_t *output,
		image_t           *image)
{
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "     <w:p>\n");
	extract_astring_cat(alloc, output, "       <w:r>\n");
	extract_astring_cat(alloc, output, "         <w:rPr>\n");
	extract_astring_cat(alloc, output, "           <w:noProof/>\n");
	extract_astring_cat(alloc, output, "         </w:rPr>\n");
	extract_astring_cat(alloc, output, "         <w:drawing>\n");
	extract_astring_cat(alloc, output, "           <wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\" wp14:anchorId=\"7057A832\" wp14:editId=\"466EB3FB\">\n");
	//extract_astring_cat(alloc, output, "             <wp:extent cx=\"2933700\" cy=\"2200275\"/>\n");
	//extract_astring_cat(alloc, output, "             <wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"9525\"/>\n");
	extract_astring_cat(alloc, output, "             <wp:docPr id=\"1\" name=\"Picture 1\"/>\n");
	extract_astring_cat(alloc, output, "             <wp:cNvGraphicFramePr>\n");
	extract_astring_cat(alloc, output, "               <a:graphicFrameLocks xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" noChangeAspect=\"1\"/>\n");
	extract_astring_cat(alloc, output, "             </wp:cNvGraphicFramePr>\n");
	extract_astring_cat(alloc, output, "             <a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
	extract_astring_cat(alloc, output, "               <a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n");
	extract_astring_cat(alloc, output, "                 <pic:pic xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n");
	extract_astring_cat(alloc, output, "                   <pic:nvPicPr>\n");
	extract_astring_cat(alloc, output, "                     <pic:cNvPr id=\"1\" name=\"Picture 1\"/>\n");
	extract_astring_cat(alloc, output, "                     <pic:cNvPicPr>\n");
	extract_astring_cat(alloc, output, "                       <a:picLocks noChangeAspect=\"1\" noChangeArrowheads=\"1\"/>\n");
	extract_astring_cat(alloc, output, "                     </pic:cNvPicPr>\n");
	extract_astring_cat(alloc, output, "                   </pic:nvPicPr>\n");
	extract_astring_cat(alloc, output, "                   <pic:blipFill>\n");
	extract_astring_catf(alloc, output,"                     <a:blip r:embed=\"%s\">\n", image->id);
	extract_astring_cat(alloc, output, "                       <a:extLst>\n");
	extract_astring_cat(alloc, output, "                         <a:ext uri=\"{28A0092B-C50C-407E-A947-70E740481C1C}\">\n");
	extract_astring_cat(alloc, output, "                           <a14:useLocalDpi xmlns:a14=\"http://schemas.microsoft.com/office/drawing/2010/main\" val=\"0\"/>\n");
	extract_astring_cat(alloc, output, "                         </a:ext>\n");
	extract_astring_cat(alloc, output, "                       </a:extLst>\n");
	extract_astring_cat(alloc, output, "                     </a:blip>\n");
	//extract_astring_cat(alloc, output, "                     <a:srcRect/>\n");
	extract_astring_cat(alloc, output, "                     <a:stretch>\n");
	extract_astring_cat(alloc, output, "                       <a:fillRect/>\n");
	extract_astring_cat(alloc, output, "                     </a:stretch>\n");
	extract_astring_cat(alloc, output, "                   </pic:blipFill>\n");
	extract_astring_cat(alloc, output, "                   <pic:spPr bwMode=\"auto\">\n");
	extract_astring_cat(alloc, output, "                     <a:xfrm>\n");
	extract_astring_cat(alloc, output, "                       <a:off x=\"0\" y=\"0\"/>\n");
	//extract_astring_cat(alloc, output, "                       <a:ext cx=\"2933700\" cy=\"2200275\"/>\n");
	extract_astring_cat(alloc, output, "                     </a:xfrm>\n");
	extract_astring_cat(alloc, output, "                     <a:prstGeom prst=\"rect\">\n");
	extract_astring_cat(alloc, output, "                       <a:avLst/>\n");
	extract_astring_cat(alloc, output, "                     </a:prstGeom>\n");
	extract_astring_cat(alloc, output, "                     <a:noFill/>\n");
	extract_astring_cat(alloc, output, "                     <a:ln>\n");
	extract_astring_cat(alloc, output, "                       <a:noFill/>\n");
	extract_astring_cat(alloc, output, "                     </a:ln>\n");
	extract_astring_cat(alloc, output, "                   </pic:spPr>\n");
	extract_astring_cat(alloc, output, "                 </pic:pic>\n");
	extract_astring_cat(alloc, output, "               </a:graphicData>\n");
	extract_astring_cat(alloc, output, "             </a:graphic>\n");
	extract_astring_cat(alloc, output, "           </wp:inline>\n");
	extract_astring_cat(alloc, output, "         </w:drawing>\n");
	extract_astring_cat(alloc, output, "       </w:r>\n");
	extract_astring_cat(alloc, output, "     </w:p>\n");
	extract_astring_cat(alloc, output, "\n");

	return 0;
}


/* Writes paragraph to content inside rotated text box. */
static int
docx_output_rotated_paragraphs(
		extract_alloc_t   *alloc,
		block_t           *block,
		int                rot,
		int                x,
		int                y,
		int                w,
		int                h,
		int                text_box_id,
		extract_astring_t *output,
		content_state_t   *state)
{
	int                         e = -1;
	paragraph_t                *paragraph;
	content_paragraph_iterator  pit;

	outf("x,y=%ik,%ik = %i,%i", x/1000, y/1000, x, y);
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "<w:p>\n");
	extract_astring_cat(alloc, output, "  <w:r>\n");
	extract_astring_cat(alloc, output, "    <mc:AlternateContent>\n");
	extract_astring_cat(alloc, output, "      <mc:Choice Requires=\"wps\">\n");
	extract_astring_cat(alloc, output, "        <w:drawing>\n");
	extract_astring_cat(alloc, output, "          <wp:anchor distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\" simplePos=\"0\" relativeHeight=\"0\" behindDoc=\"0\" locked=\"0\" layoutInCell=\"1\" allowOverlap=\"1\" wp14:anchorId=\"53A210D1\" wp14:editId=\"2B7E8016\">\n");
	extract_astring_cat(alloc, output, "            <wp:simplePos x=\"0\" y=\"0\"/>\n");
	extract_astring_cat(alloc, output, "            <wp:positionH relativeFrom=\"page\">\n");
	extract_astring_catf(alloc, output,"              <wp:posOffset>%i</wp:posOffset>\n", x);
	extract_astring_cat(alloc, output, "            </wp:positionH>\n");
	extract_astring_cat(alloc, output, "            <wp:positionV relativeFrom=\"page\">\n");
	extract_astring_catf(alloc, output,"              <wp:posOffset>%i</wp:posOffset>\n", y);
	extract_astring_cat(alloc, output, "            </wp:positionV>\n");
	extract_astring_catf(alloc, output,"            <wp:extent cx=\"%i\" cy=\"%i\"/>\n", w, h);
	//extract_astring_cat(alloc, output, "            <wp:effectExtent l=\"381000\" t=\"723900\" r=\"371475\" b=\"723900\"/>\n");
	extract_astring_cat(alloc, output, "            <wp:wrapNone/>\n");
	extract_astring_catf(alloc, output,"            <wp:docPr id=\"%i\" name=\"Text Box %i\"/>\n", text_box_id, text_box_id);
	extract_astring_cat(alloc, output, "            <wp:cNvGraphicFramePr/>\n");
	extract_astring_cat(alloc, output, "            <a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
	extract_astring_cat(alloc, output, "              <a:graphicData uri=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\">\n");
	extract_astring_cat(alloc, output, "                <wps:wsp>\n");
	extract_astring_cat(alloc, output, "                  <wps:cNvSpPr txBox=\"1\"/>\n");
	extract_astring_cat(alloc, output, "                  <wps:spPr>\n");
	extract_astring_catf(alloc, output,"                    <a:xfrm rot=\"%i\">\n", rot);
	extract_astring_cat(alloc, output, "                      <a:off x=\"0\" y=\"0\"/>\n");
	//extract_astring_cat(alloc, output, "                      <a:ext cx=\"3228975\" cy=\"2286000\"/>\n");
	extract_astring_cat(alloc, output, "                    </a:xfrm>\n");
	extract_astring_cat(alloc, output, "                    <a:prstGeom prst=\"rect\">\n");
	extract_astring_cat(alloc, output, "                      <a:avLst/>\n");
	extract_astring_cat(alloc, output, "                    </a:prstGeom>\n");

	/* Give box a solid background. */
	if (0) {
		extract_astring_cat(alloc, output, "                    <a:solidFill>\n");
		extract_astring_cat(alloc, output, "                      <a:schemeClr val=\"lt1\"/>\n");
		extract_astring_cat(alloc, output, "                    </a:solidFill>\n");
	}

	/* Draw line around box. */
	if (0) {
		extract_astring_cat(alloc, output, "                    <a:ln w=\"175\">\n");
		extract_astring_cat(alloc, output, "                      <a:solidFill>\n");
		extract_astring_cat(alloc, output, "                        <a:prstClr val=\"black\"/>\n");
		extract_astring_cat(alloc, output, "                      </a:solidFill>\n");
		extract_astring_cat(alloc, output, "                    </a:ln>\n");
	}

	extract_astring_cat(alloc, output, "                  </wps:spPr>\n");
	extract_astring_cat(alloc, output, "                  <wps:txbx>\n");
	extract_astring_cat(alloc, output, "                    <w:txbxContent>");

#if 0
	if (0) {
		/* Output inline text describing the rotation. */
		extract_astring_catf(content, "<w:p>\n"
			"<w:r><w:rPr><w:rFonts w:ascii=\"OpenSans\" w:hAnsi=\"OpenSans\"/><w:sz w:val=\"20.000000\"/><w:szCs w:val=\"15.000000\"/></w:rPr><w:t xml:space=\"preserve\">*** rotate: %f rad, %f deg. rot=%i</w:t></w:r>\n"
			"</w:p>\n",
			rotate,
			rotate * 180 / pi,
			rot
			);
	}
#endif

	/* Output paragraphs p0..p2-1. */
	for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
		if (document_to_docx_content_paragraph(alloc, state, paragraph, output)) goto end;

	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "                    </w:txbxContent>\n");
	extract_astring_cat(alloc, output, "                  </wps:txbx>\n");
	extract_astring_cat(alloc, output, "                  <wps:bodyPr rot=\"0\" spcFirstLastPara=\"0\" vertOverflow=\"overflow\" horzOverflow=\"overflow\" vert=\"horz\" wrap=\"square\" lIns=\"91440\" tIns=\"45720\" rIns=\"91440\" bIns=\"45720\" numCol=\"1\" spcCol=\"0\" rtlCol=\"0\" fromWordArt=\"0\" anchor=\"t\" anchorCtr=\"0\" forceAA=\"0\" compatLnSpc=\"1\">\n");
	extract_astring_cat(alloc, output, "                    <a:prstTxWarp prst=\"textNoShape\">\n");
	extract_astring_cat(alloc, output, "                      <a:avLst/>\n");
	extract_astring_cat(alloc, output, "                    </a:prstTxWarp>\n");
	extract_astring_cat(alloc, output, "                    <a:noAutofit/>\n");
	extract_astring_cat(alloc, output, "                  </wps:bodyPr>\n");
	extract_astring_cat(alloc, output, "                </wps:wsp>\n");
	extract_astring_cat(alloc, output, "              </a:graphicData>\n");
	extract_astring_cat(alloc, output, "            </a:graphic>\n");
	extract_astring_cat(alloc, output, "          </wp:anchor>\n");
	extract_astring_cat(alloc, output, "        </w:drawing>\n");
	extract_astring_cat(alloc, output, "      </mc:Choice>\n");

#if 0
	/* This fallback is copied from a real Word document. Not sure
	whether it works - both Libreoffice and Word use the above
	choice. */
	extract_astring_cat(alloc, output, "      <mc:Fallback>\n");
	extract_astring_cat(alloc, output, "        <w:pict>\n");
	extract_astring_cat(alloc, output, "          <v:shapetype w14:anchorId=\"53A210D1\" id=\"_x0000_t202\" coordsize=\"21600,21600\" o:spt=\"202\" path=\"m,l,21600r21600,l21600,xe\">\n");
	extract_astring_cat(alloc, output, "            <v:stroke joinstyle=\"miter\"/>\n");
	extract_astring_cat(alloc, output, "            <v:path gradientshapeok=\"t\" o:connecttype=\"rect\"/>\n");
	extract_astring_cat(alloc, output, "          </v:shapetype>\n");
	extract_astring_catf(alloc, output,"          <v:shape id=\"Text Box %i\" o:spid=\"_x0000_s1026\" type=\"#_x0000_t202\" style=\"position:absolute;margin-left:71.25pt;margin-top:48.75pt;width:254.25pt;height:180pt;rotation:-2241476fd;z-index:251659264;visibility:visible;mso-wrap-style:square;mso-wrap-distance-left:9pt;mso-wrap-distance-top:0;mso-wrap-distance-right:9pt;mso-wrap-distance-bottom:0;mso-position-horizontal:absolute;mso-position-horizontal-relative:text;mso-position-vertical:absolute;mso-position-vertical-relative:text;v-text-anchor:top\" o:gfxdata=\"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&#10;bFBLBQYAAAAABAAEAPMAAAC8BQAAAAA=&#10;\" fillcolor=\"white [3201]\" strokeweight=\".5pt\">\n", text_box_id);
	extract_astring_cat(alloc, output, "            <v:textbox>\n");
	extract_astring_cat(alloc, output, "              <w:txbxContent>");

	for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
		if (document_to_docx_content_paragraph(alloc, state, paragraph, output)) goto end;

	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "\n");
	extract_astring_cat(alloc, output, "              </w:txbxContent>\n");
	extract_astring_cat(alloc, output, "            </v:textbox>\n");
	extract_astring_cat(alloc, output, "          </v:shape>\n");
	extract_astring_cat(alloc, output, "        </w:pict>\n");
	extract_astring_cat(alloc, output, "      </mc:Fallback>\n");
#endif
	extract_astring_cat(alloc, output, "    </mc:AlternateContent>\n");
	extract_astring_cat(alloc, output, "  </w:r>\n");
	extract_astring_cat(alloc, output, "</w:p>");

	e = 0;
end:

	return e;
}


/* Appends table to content.

We do not fix the size of the table or its columns and rows, but instead leave layout up
to the application. */
static int
docx_append_table(
		extract_alloc_t   *alloc,
		table_t           *table,
		extract_astring_t *output)
{
	int e = -1;
	int y;

	if (extract_astring_cat(alloc, output,
				"\n"
				"    <w:tbl>\n"
				"        <w:tblLayout w:type=\"autofit\"/>\n"))
		goto end;

	for (y=0; y<table->cells_num_y; ++y)
	{
		int x;
		if (extract_astring_cat(alloc, output,
					"        <w:tr>\n"
					"            <w:trPr/>\n")) goto end;

		for (x=0; x<table->cells_num_x; ++x)
		{
			cell_t* cell = table->cells[y*table->cells_num_x + x];
			if (!cell->left) continue;

			if (extract_astring_cat(alloc, output, "            <w:tc>\n"))
				goto end;

			/* Write cell properties. */
			{
				if (extract_astring_cat(alloc, output,
							"                <w:tcPr>\n"
							"                    <w:tcBorders>\n"
							"                        <w:top w:val=\"double\" w:sz=\"2\" w:space=\"0\" w:color=\"808080\"/>\n"
							"                        <w:start w:val=\"double\" w:sz=\"2\" w:space=\"0\" w:color=\"808080\"/>\n"
							"                        <w:bottom w:val=\"double\" w:sz=\"2\" w:space=\"0\" w:color=\"808080\"/>\n"
							"                        <w:end w:val=\"double\" w:sz=\"2\" w:space=\"0\" w:color=\"808080\"/>\n"
							"                    </w:tcBorders>\n"))
					goto end;
				if (cell->extend_right > 1)
				{
					if (extract_astring_catf(alloc, output, "                    <w:gridSpan w:val=\"%i\"/>\n", cell->extend_right))
						goto end;
				}
				if (cell->above)
				{
					if (cell->extend_down > 1)
					{
						if (extract_astring_catf(alloc, output, "                    <w:vMerge w:val=\"restart\"/>\n", cell->extend_down))
							goto end;
					}
				}
				else
				{
					if (extract_astring_catf(alloc, output, "                    <w:vMerge w:val=\"continue\"/>\n"))
						goto end;
				}
				if (extract_astring_cat(alloc, output, "                </w:tcPr>\n"))
					goto end;
			}

			/* Write contents of this cell. */
			{
				content_paragraph_iterator  pit;
				paragraph_t                *paragraph;
				size_t                      chars_num_old = output->chars_num;
				content_state_t             content_state = {0};

				content_state.font.name = NULL;
				content_state.ctm_prev = NULL;
				for (paragraph = content_paragraph_iterator_init(&pit, &cell->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
					if (document_to_docx_content_paragraph(alloc, &content_state, paragraph, output))
						goto end;

				if (content_state.font.name)
					if (docx_run_finish(alloc, &content_state, output)) goto end;

				/* Need to write out at least an empty paragraph in each
				 * cell, otherwise Word/Libreoffice fail to show table at
				 * all; the OOXML spec says "If a table cell does not
				 * include at least one block-level element, then this
				 * document shall be considered corrupt." */
				if (output->chars_num == chars_num_old)
					if (extract_astring_catf(alloc, output, "<w:p/>\n"))
						goto end;
			}
			if (extract_astring_cat(alloc, output, "            </w:tc>\n"))
				goto end;
		}
		if (extract_astring_cat(alloc, output, "        </w:tr>\n"))
			goto end;
	}
	if (extract_astring_cat(alloc, output, "    </w:tbl>\n"))
		goto end;

	e = 0;
end:

	return e;
}

/* Appends a block of content with same rotation. */
static int
docx_append_rotated_paragraphs(
		extract_alloc_t    *alloc,
		content_state_t    *state,
		block_t            *block,
		int                *text_box_id,
		double              angle,
		extract_astring_t  *output)
{
	/* Find extent of paragraphs with this same rotation. extent
	will contain max width and max height of paragraphs, in units
	before application of ctm, i.e. before rotation. */
	int               e           = -1;
	rect_t            bounds;

	bounds = extract_block_pre_rotation_bounds(block, angle);

	outf("angle=%f pre-transform box is: (%f %f) to (%f %f)",
		angle, bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y);

	/* All the paragraphs have same rotation. We output them into
	 * a single rotated text box. */

	/* We need unique id for text box. */
	*text_box_id += 1;

	{
		/* Angles are in units of 1/60,000 degree. */
		int rot = (int) (angle * 180 / pi * 60000);

		/* <wp:anchor distT=\.. etc are in EMU - 1/360,000 of a cm.
		 * relativeHeight is z-ordering. (wp:positionV:wp:posOffset,
		 * wp:positionV:wp:posOffset) is position of origin of box in
		* EMU. */
		double point_to_emu = 12700;    /* https://en.wikipedia.org/wiki/Office_Open_XML_file_formats#DrawingML */
		int x = (int) (bounds.min.x * point_to_emu);
		int y = (int) (bounds.min.y * point_to_emu);
		int w = (int) ((bounds.max.x - bounds.min.x) * point_to_emu);
		int h = (int) ((bounds.max.y - bounds.min.y) * point_to_emu);

		if (0) outf("rotate: %f rad, %f deg. rot=%i", angle, angle*180/pi, rot);

		if (docx_output_rotated_paragraphs(alloc, block, rot, x, y, w, h, *text_box_id, output, state))
			goto end;
	}

	e = 0;
end:

	return e;
}

int
extract_document_to_docx_content(
		extract_alloc_t   *alloc,
		document_t        *document,
		int                spacing,
		int                rotation,
		int                images,
		extract_astring_t *output)
{
	int e = -1;
	int text_box_id = 0;
	int p;

	/* Write paragraphs into <content>. */
	for (p=0; p<document->pages_num; ++p)
	{
		extract_page_t *page = document->pages[p];
		int c;

		for (c=0; c<page->subpages_num; ++c)
		{
			subpage_t                  *subpage = page->subpages[c];
			content_iterator            cit;
			content_t                  *content;
			content_table_iterator      tit;
			table_t                    *table;

			content_state_t content_state;
			content_state.font.name = NULL;
			content_state.font.size = 0;
			content_state.font.bold = 0;
			content_state.font.italic = 0;
			content_state.ctm_prev = NULL;

			/* Output paragraphs and tables in order of y coordinate. */
			content = content_iterator_init(&cit, &subpage->content);
			table = content_table_iterator_init(&tit, &subpage->tables);
			while (1)
			{
				double y_paragraph;
				double y_table;
				/* Next block or NULL if none. */
				block_t *block = (content && content->type == content_block) ? (block_t *)content : NULL;
				/* Next paragraph or NULL if none. */
				paragraph_t *paragraph = (content && content->type == content_paragraph) ? (paragraph_t *)content : (block ? content_first_paragraph(&block->content) : NULL);
				line_t *first_line = paragraph ? content_first_line(&paragraph->content) : NULL;
				span_t *first_span = first_line ? content_head_as_span(&first_line->content) : NULL;

				if (!paragraph && !table) break;

				y_paragraph = (first_span) ? first_span->chars[0].y : DBL_MAX;
				y_table = (table) ? table->pos.y : DBL_MAX;

				if (first_span && y_paragraph < y_table)
				{
					const matrix4_t *ctm   = &first_span->ctm;
					double           angle = extract_baseline_angle(ctm);

					if (spacing
						&& content_state.ctm_prev
						&& first_line
						&& first_span
						&& extract_matrix4_cmp(content_state.ctm_prev,
									&first_span->ctm))
					{
						/* Extra vertical space between paragraphs that
						 * were at different angles in the original
						 * document. */
						if (docx_paragraph_empty(alloc, output))
							goto end;
					}

					/* Extra vertical space between paragraphs. */
					if (spacing)
						if (docx_paragraph_empty(alloc, output))
							goto end;

					if (rotation && angle != 0)
					{
						assert(block);
						if (docx_append_rotated_paragraphs(alloc, &content_state, block, &text_box_id, angle, output))
							goto end;
					}
					else if (block)
					{
						content_paragraph_iterator pit;
						int                        first = 1;

						for (paragraph = content_paragraph_iterator_init(&pit, &block->content); paragraph != NULL; paragraph = content_paragraph_iterator_next(&pit))
						{
							if (spacing && !first)
							{
								/* Extra vertical space between paragraphs. */
								if (docx_paragraph_empty(alloc, output))
									goto end;
							}
							first = 0;

							if (document_to_docx_content_paragraph(alloc, &content_state, paragraph, output)) goto end;
						}
					}
					else
					{
						if (document_to_docx_content_paragraph(alloc, &content_state, paragraph, output))
							goto end;
					}
					content = content_iterator_next(&cit);
				}
				else if (table)
				{
					if (docx_append_table(alloc, table, output))
						goto end;
					table = content_table_iterator_next(&tit);
				}
			}

			if (images)
			{
				content_image_iterator  iit;
				image_t                *image;

				for (image = content_image_iterator_init(&iit, &subpage->content); image != NULL; image = content_image_iterator_next(&iit))
					docx_append_image(alloc, output, image);
			}
		}
	}

	e = 0;
end:

	return e;
}


/* Sets *o_begin to end of first occurrence of <begin> in <text>, and *o_end to
 * beginning of first occurtence of <end> in <text>. */
static int
find_mid(
	const char  *text,
	const char  *begin,
	const char  *end,
	const char **o_begin,
	const char **o_end)
{
	*o_begin = strstr(text, begin);
	if (*o_begin == NULL)
		goto fail;
	*o_begin += strlen(begin);
	*o_end = strstr(*o_begin, end);
	if (*o_end == NULL)
		goto fail;

	return 0;

fail:
	errno = ESRCH;
	return -1;
}


int
extract_docx_content_item(
		extract_alloc_t    *alloc,
		extract_astring_t  *contentss,
		int                 contentss_num,
		images_t           *images,
		const char         *name,
		const char         *text,
		char              **text2)
{
	int               e    = -1;
	extract_astring_t temp = { 0 };

	*text2 = NULL;

	if (0)
	{}
	else if (!strcmp(name, "[Content_Types].xml"))
	{
		/* Add information about all image types that we are going to use. */
		const char *begin;
		const char *end;
		const char *insert;
		int it;

		extract_astring_free(alloc, &temp);
		outf("text: %s", text);
		if (find_mid(text, "<Types ", "</Types>", &begin, &end)) goto end;

		insert = begin;
		insert = strchr(insert, '>');
		assert(insert);
		insert += 1;

		if (extract_astring_catl(alloc, &temp, text, insert - text)) goto end;
		outf("images->imagetypes_num=%i", images->imagetypes_num);
		for (it=0; it<images->imagetypes_num; ++it) {
			const char *imagetype = images->imagetypes[it];
			if (extract_astring_cat(alloc, &temp, "<Default Extension=\"")) goto end;
			if (extract_astring_cat(alloc, &temp, imagetype)) goto end;
			if (extract_astring_cat(alloc, &temp, "\" ContentType=\"image/")) goto end;
			if (extract_astring_cat(alloc, &temp, imagetype)) goto end;
			if (extract_astring_cat(alloc, &temp, "\"/>")) goto end;
		}
		if (extract_astring_cat(alloc, &temp, insert)) goto end;
		*text2 = temp.chars;
		extract_astring_init(&temp);
	}
	else if (!strcmp(name, "word/_rels/document.xml.rels"))
	{
		/* Add relationships between image ids and image names within docx
		 * archive. */
		const char *begin;
		const char *end;
		int         j;

		extract_astring_free(alloc, &temp);
		if (find_mid(text, "<Relationships", "</Relationships>", &begin, &end)) goto end;
		if (extract_astring_catl(alloc, &temp, text, end - text)) goto end;
		outf("images.images_num=%i", images->images_num);
		for (j=0; j<images->images_num; ++j) {
			image_t* image = images->images[j];
			if (extract_astring_cat(alloc, &temp, "<Relationship Id=\"")) goto end;
			if (extract_astring_cat(alloc, &temp, image->id)) goto end;
			if (extract_astring_cat(alloc, &temp, "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/")) goto end;
			if (extract_astring_cat(alloc, &temp, image->name)) goto end;
			if (extract_astring_cat(alloc, &temp, "\"/>")) goto end;
		}
		if (extract_astring_cat(alloc, &temp, end)) goto end;
		*text2 = temp.chars;
		extract_astring_init(&temp);
	}
	else if (!strcmp(name, "word/document.xml"))
	{
		/* Insert paragraphs content. */
		if (extract_content_insert(alloc,
				text,
				NULL /*single*/,
				"<w:body>",
				"</w:body>",
				contentss,
				contentss_num,
				text2)) goto end;
	}
	else
	{
	*text2 = NULL;
	}

	e = 0;
end:

	if (e)
	{
		/* We might have set <text2> to new content. */
		extract_free(alloc, text2);
		/* We might have used <temp> as a temporary buffer. */
		extract_astring_free(alloc, &temp);
	}
	extract_astring_init(&temp);

	return e;
}



int
extract_docx_write_template(
		extract_alloc_t   *alloc,
		extract_astring_t *contentss,
		int                contentss_num,
		images_t          *images,
		const char        *path_template,
		const char        *path_out,
		int                preserve_dir)
{
	int   e = -1;
	int   i;
	char *path_tempdir = NULL;
	char *path = NULL;
	char *text = NULL;
	char *text2 = NULL;

	assert(path_out);
	assert(path_template);

	if (extract_check_path_shell_safe(path_out))
	{
		outf("path_out is unsafe: %s", path_out);
		goto end;
	}

	outf("images->images_num=%i", images->images_num);
	if (extract_asprintf(alloc, &path_tempdir, "%s.dir", path_out) < 0) goto end;
	if (extract_systemf(alloc, "rm -r '%s' 2>/dev/null", path_tempdir) < 0) goto end;

	if (extract_mkdir(path_tempdir, 0777)) {
		outf("Failed to create directory: %s", path_tempdir);
		goto end;
	}

	outf("Unzipping template document '%s' to tempdir: %s",
		path_template, path_tempdir);
	if (extract_systemf(alloc, "unzip -q -d '%s' '%s'", path_tempdir, path_template))
	{
		outf("Failed to unzip %s into %s",
			path_template, path_tempdir);
		goto end;
	}

	/* Might be nice to iterate through all items in path_tempdir, but for now
	 * we look at just the items that we know extract_docx_content_item() will
	 * modify. */

	{
		const char *names[] = {
			"word/document.xml",
			"[Content_Types].xml",
			"word/_rels/document.xml.rels",
		};
		int names_num = sizeof(names) / sizeof(names[0]);
		for (i=0; i<names_num; ++i) {
			const char* name = names[i];
			extract_free(alloc, &path);
			extract_free(alloc, &text);
			extract_free(alloc, &text2);
			if (extract_asprintf(alloc, &path, "%s/%s", path_tempdir, name) < 0) goto end;
			if (extract_read_all_path(alloc, path, &text)) goto end;

			if (extract_docx_content_item(alloc,
					contentss,
					contentss_num,
					images,
					name,
					text,
					&text2)) goto end;

			{
				const char *text3 = (text2) ? text2 : text;
				if (extract_write_all(text3, strlen(text3), path)) goto end;
			}
		}
	}

	/* Copy images into <path_tempdir>/media/. */
	extract_free(alloc, &path);
	if (extract_asprintf(alloc, &path, "%s/word/media", path_tempdir) < 0) goto end;
	if (extract_mkdir(path, 0777)) goto end;

	for (i=0; i<images->images_num; ++i) {
		image_t* image = images->images[i];
		extract_free(alloc, &path);
		if (extract_asprintf(alloc, &path, "%s/word/media/%s", path_tempdir, image->name) < 0) goto end;
		if (extract_write_all(image->data, image->data_size, path)) goto end;
	}

	outf("Zipping tempdir to create %s", path_out);
	{
		const char *path_out_leaf = strrchr(path_out, '/');
		if (!path_out_leaf) path_out_leaf = path_out;
		if (extract_systemf(alloc, "cd '%s' && zip -q -r -D '../%s' .", path_tempdir, path_out_leaf))
		{
			outf("Zip command failed to convert '%s' directory into output file: %s",
				path_tempdir, path_out);
			goto end;
		}
	}

	if (!preserve_dir) {
		if (extract_remove_directory(alloc, path_tempdir)) goto end;
	}

	e = 0;
end:

	outf("e=%i", e);
	extract_free(alloc, &path_tempdir);
	extract_free(alloc, &path);
	extract_free(alloc, &text);
	extract_free(alloc, &text2);

	if (e)
	{
		outf("Failed to create %s", path_out);
	}

	return e;
}
