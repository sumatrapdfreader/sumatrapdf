#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <memory.h>

const char snark[] =
	"<!DOCTYPE html>"
	"<style>"
	"#a { margin: 30px; }"
	"#b { margin: 20px; }"
	"#c { margin: 5px; }"
	"#a { border: 1px solid red; }"
	"#b { border: 1px solid green; }"
	"#c { border: 1px solid blue; }"
	"</style>"
	"<body>"
	"<div id=\"a\">"
	"A"
	"</div>"
	"<div id=\"b\">"
	"<div id=\"c\">"
	"C"
	"</div>"
	"</div>"
	"<div style=\"text-align:center;\"><IMG width=\"300\" src=\"docs/examples/SnarkFront.svg\"></div>"
	"<H1>The hunting of the Snark</H1>"
	"<div style=\"text-align:center\"><IMG width=\"30\" src=\"docs/examples/huntingofthesnark.png\"></div>"
	"<p>\"Just the place for a Snark!\" the Bellman cried,<br>"
	"As he landed his crew with care;<br>"
	"Supporting each man on the top of the tide<br>"
	"By a finger entwined in his hair.</p>"

	"<P>Just the place for a Snark! I have said it twice:<br>"
	"That alone should encourage the crew.<br>"
	"Just the place for a Snark! I have said it thrice:<br>"
	"What I tell you three times is true.</p>"

	"<p>The crew was complete: it included a Boots-<br>"
	"A maker of Bonnets and Hoods-<br>"
	"A Barrister, brought to arrange their disputes-<br>"
	"And a Broker, to value their goods.</p>"

	"<p>A Billiard-marker, whose skill was immense,<br>"
	"Might perhaps have won more than his share-<br>"
	"But a Banker, engaged at enormous expense,<br>"
	"Had the whole of their cash in his care.</p>"

	"<p>There was also a Beaver, that paced on the deck,<br>"
	"Or would sit making lace in the bow:<br>"
	"And had often (the Bellman said) saved them from wreck,<br>"
	"Though none of the sailors knew how.</p>"

	"<p>There was one who was famed for the number of things<br>"
	"He forgot when he entered the ship:<br>"
	"His umbrella, his watch, all his jewels and rings,<br>"
	"And the clothes he had bought for the trip.</p>"
	"<div id=\"a\">"
	"<p>He had forty-two boxes, all carefully packed,<br>"
	"With his name painted clearly on each:<br>"
	"But, since he omitted to mention the fact,<br>"
	"They were all left behind on the beach.</p>"
	"</div>"

	"<p>The loss of his clothes hardly mattered, because<br>"
	"He had seven coats on when he came,<br>"
	"With three pair of boots-but the worst of it was,<br>"
	"He had wholly forgotten his name.</p>"

	"<p>He would answer to \"Hi!\" or to any loud cry,<br>"
	"Such as \"Fry me!\" or \"Fritter my wig!\"<br>"
	"To \"What-you-may-call-um!\" or \"What-was-his-name!\"<br>"
	"But especially \"Thing-um-a-jig!\"</p>"

	"<p>While, for those who preferred a more forcible word,<br>"
	"He had different names from these:<br>"
	"His intimate friends called him \"Candle-ends,\"<br>"
	"And his enemies \"Toasted-cheese.\"</p>"

	"<p>\"His form is ungainly-his intellect small-\"<br>"
	"(So the Bellman would often remark)<br>"
	"\"But his courage is perfect! And that, after all,<br>"
	"Is the thing that one needs with a Snark.\"</p>"

	"<p>He would joke with hyenas, returning their stare<br>"
	"With an impudent wag of the head:<br>"
	"And he once went a walk, paw-in-paw, with a bear,<br>"
	"\"Just to keep up its spirits,\" he said.</p>"

	"<p>He came as a Baker: but owned, when too late-<br>"
	"And it drove the poor Bellman half-mad-<br>"
	"He could only bake Bride-cake-for which, I may state,<br>"
	"No materials were to be had.</p>"

	"<p>The last of the crew needs especial remark,<br>"
	"Though he looked an incredible dunce:<br>"
	"He had just one idea-but, that one being \"Snark,\"<br>"
	"The good Bellman engaged him at once.</p>"

	"<p>He came as a Butcher: but gravely declared,<br>"
	"When the ship had been sailing a week,<br>"
	"He could only kill Beavers. The Bellman looked scared,<br>"
	"And was almost too frightened to speak:</p>"

	"<p>But at length he explained, in a tremulous tone,<br>"
	"There was only one Beaver on board;<br>"
	"And that was a tame one he had of his own,<br>"
	"Whose death would be deeply deplored.</p>"

	"<div id=\"b\">"
	"<p>The Beaver, who happened to hear the remark,<br>"
	"Protested, with tears in its eyes,<br>"
	"That not even the rapture of hunting the Snark<br>"
	"Could atone for that dismal surprise!</p>"
	"</div>"

	"<p style=\"-mupdf-leading:7pt;\">It strongly advised that the Butcher should be<br>"
	"Conveyed in a separate ship:<br>"
	"But the Bellman declared that would never agree<br>"
	"With the plans he had made for the trip:</p>"

	"<p style=\"-mupdf-leading:11pt;\">Navigation was always a difficult art,<br>"
	"Though with only one ship and one bell:<br>"
	"And he feared he must really decline, for his part,<br>"
	"Undertaking another as well.</p>"

	"<p style=\"-mupdf-leading:15pt;\">The Beaver's best course was, no doubt, to procure<br>"
	"A second-hand dagger-proof coat-<br>"
	"So the Baker advised it-and next, to insure<br>"
	"Its life in some Office of note:</p>"

	"<p style=\"-mupdf-leading:20pt;\">This the Banker suggested, and offered for hire<br>"
	"(On moderate terms), or for sale,<br>"
	"Two excellent Policies, one Against Fire,<br>"
	"And one Against Damage From Hail.</p>"

	"<p style=\"-mupdf-leading:30pt;\">Yet still, ever after that sorrowful day,<br>"
	"Whenever the Butcher was by,<br>"
	"The Beaver kept looking the opposite way,<br>"
	"And appeared unaccountably shy.</p>"
;

#define MAX_CAST_MEMBERS 32

typedef struct {
	const char *title;
	const char *director;
	const char *year;
	const char *cast[MAX_CAST_MEMBERS];
} film_t;

static const film_t films[] =
{
	{
		"Pulp Fiction",
		"Quentin Tarantino",
		"1994",
		{
			"John Travolta",
			"Samuel L Jackson",
			"Uma Thurman",
			"Bruce Willis",
			"Ving Rhames",
			"Harvey Keitel",
			"Tim Roth",
			"Bridget Fonda"
		}
	},

	{
		"The Usual Suspects",
		"Bryan Singer",
		"1995",
		{
			"Kevin Spacey",
			"Gabriel Byrne",
			"Chazz Palminteri",
			"Benicio Del Toro",
			"Kevin Pollak",
			"Pete Postlethwaite",
			"Steven Baldwin"
		}

	},

	{
		"Fight Club",
		"David Fincher",
		"1999",
		{
			"Brad Pitt",
			"Edward Norton",
			"Helena Bonham Carter"
		}
	}
};

const char *festival_template =
	"<html><head><title>Why do we have a title? Why not?</title></head>"
	"<body><h1 style=\"text-align:center\">Hook Norton Film Festival</h1>"
	"<ol>"
	"<li id=\"filmtemplate\">"
	"<b id=\"filmtitle\"></b>"
	"<dl>"
	"<dt>Director<dd id=\"director\">"
	"<dt>Release Year<dd id=\"filmyear\">"
	"<dt>Cast<dd id=\"cast\">"
	"</dl>"
	"</li>"
	"<ul>"
	"</body></html";


static int toc_rectfn(fz_context *ctx, void *ref, int num, fz_rect filled, fz_rect *rect, fz_matrix *ctm, fz_rect *mediabox)
{
	if (num == 0)
	{
		rect->x0 = 100;
		rect->y0 = 200;
		rect->x1 = 175;
		rect->y1 = 300;
	}
	else
	{
		rect->x0 = 10;
		rect->y0 = 50;
		rect->x1 = 290;
		rect->y1 = 350;
	}
	mediabox->x0 = 0;
	mediabox->y0 = 0;
	mediabox->x1 = 300;
	mediabox->y1 = 400;
	return 1;
}

static void toc_contentfn(fz_context *ctx, void *ref, const fz_write_story_positions *positions, fz_buffer *buffer)
{
	int i;
	fz_append_string(ctx, buffer,
			"<!DOCTYPE html>\n"
			"<style>\n"
			"#a { margin: 30px; }\n"
			"#b { margin: 20px; }\n"
			"#c { margin: 5px; }\n"
			"#a { border: 1px solid red; }\n"
			"#b { border: 1px solid green; }\n"
			"#c { border: 1px solid blue; }\n"
			"</style>\n"
			"<body>\n"
			"<h2>Contents</h2>\n"
			"<ol>\n"
			);
	for (i=0; i<positions->num; ++i)
	{
		fz_write_story_position *position = &positions->positions[i];

		fz_append_printf(ctx, buffer,
				"	<li>page=%i depth=%i heading=%i id='%s' rect=(%f %f %f %f) text='<b>%s</b>' open_close=%i\n",
				position->page_num,
				position->element.depth,
				position->element.heading,
				(position->element.id) ? position->element.id : "",
				position->element.rect.x0,
				position->element.rect.y0,
				position->element.rect.x1,
				position->element.rect.y1,
				(position->element.text) ? position->element.text : "",
				position->element.open_close
				);
	}
	fz_append_string(ctx, buffer, "</ol>\n");
	fz_append_string(ctx, buffer,
			"<h1>Section the first</h1>\n"
			"<p>Blah.\n"
			"<h1>Section the second</h1>\n"
			"<p>Blah blah.\n"
			"<h2>Subsection</h2>\n"
			"<p>Blah blah blah.\n"
			"<p>Blah blah blah.\n"
			"<p>Blah blah blah.\n"
			"<h1>Section the third</h1>\n"
			"<p>Blah blah.\n"
			"</body>\n"
			);
	{
		const char *data = fz_string_from_buffer(ctx, buffer);
		printf( "======== Html content: ========\n");
		printf( "%s", data);
		printf( "========\n");
	}
}

static void toc_pagefn(fz_context *ctx, void *ref, int page_num, fz_rect mediabox, fz_device *dev, int after)
{
	fz_path *path = fz_new_path(ctx);
	fz_matrix ctm = fz_identity;
	printf("toc_pagefn(): ref=%p page_num=%i dev=%p after=%i\n", ref, page_num, dev, after);
	if (after)
	{
		float rgb[3] = { 0.75, 0.25, 0.125};
		fz_moveto(ctx, path, 50, 50);
		fz_lineto(ctx, path, 100, 200);
		fz_lineto(ctx, path, 50, 200);
		fz_closepath(ctx, path);
		fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), rgb, 0.9f /*alpha*/, fz_default_color_params);
		fz_drop_path(ctx, path);
	}
	else
	{
		float rgb[3] = { 0.125, 0.25, 0.75};
		fz_moveto(ctx, path, 50, 50);
		fz_lineto(ctx, path, 50, 200);
		fz_lineto(ctx, path, 100, 50);
		fz_closepath(ctx, path);
		fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), rgb, 0.9f /*alpha*/, fz_default_color_params);
		fz_drop_path(ctx, path);
	}
}

static void test_write_stabilized_story(fz_context *ctx)
{
	fz_document_writer *writer = fz_new_pdf_writer(ctx, "out_toc.pdf", "");
	fz_write_stabilized_story(
			ctx,
			writer,
			"" /*user_css*/,
			11 /*em*/,
			toc_contentfn,
			NULL /*contentfn_ref*/,
			toc_rectfn,
			NULL /*rectfn_ref*/,
			toc_pagefn /*pagefn*/,
			NULL /*pagefn_ref*/,
			NULL /* archive */
			);
	fz_close_document_writer(ctx, writer);
	fz_drop_document_writer(ctx, writer);
}

static void
test_story(fz_context *ctx, const char *filename, const char *options, const char *storytext)
{
	fz_document_writer *writer = NULL;
	fz_story *story = NULL;
	fz_buffer *buf = NULL;
	fz_device *dev = NULL;
	fz_archive *archive = NULL;
	fz_rect mediabox = { 0, 0, 512, 640 };
	float margin = 10;
	int more;

	fz_var(writer);
	fz_var(story);
	fz_var(buf);
	fz_var(dev);
	fz_var(archive);

	fz_try(ctx)
	{
		writer = fz_new_pdf_writer(ctx, filename, options);

		buf = fz_new_buffer_from_copied_data(ctx, (unsigned char *)storytext, strlen(storytext)+1);

		archive = fz_open_directory(ctx, ".");

		story = fz_new_story(ctx, buf, "", 11, archive);

		do
		{
			fz_rect where;
			fz_rect filled;

			where.x0 = mediabox.x0 + margin;
			where.y0 = mediabox.y0 + margin;
			where.x1 = mediabox.x1 - margin;
			where.y1 = mediabox.y1 - margin;

			dev = fz_begin_page(ctx, writer, mediabox);

			more = fz_place_story(ctx, story, where, &filled);

			fz_draw_story(ctx, story, dev, fz_identity);

			fz_end_page(ctx, writer);
		}
		while (more);

		fz_close_document_writer(ctx, writer);
	}
	fz_always(ctx)
	{
		fz_drop_story(ctx, story);
		fz_drop_buffer(ctx, buf);
		fz_drop_document_writer(ctx, writer);
		fz_drop_archive(ctx, archive);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
	}
}

static void
test_positions(fz_context *ctx)
{
	const char *pos_static =
		"<!DOCTYPE html><html><head><style>\n"
		"div.test {\n"
		"  width: 400px;\n"
		"  border: 3px solid #808080;\n"
		"}\n"
		"p.test {\n"
		"  top: 10px;"
		"  left: 20px;"
		"}\n"
		"p.test2 {\n"
		"  bottom: 10px;"
		"  right: 20px;"
		"}\n"
		"</style></head><body>\n"
		"<div class=\"test\">"
		"<p>This is text 1.</p>\n"
		"<p>This is text 2.</p>\n"
		"<p class=\"test\">This is text 3 (static).</p>\n"
		"<p>This is text 4.</p>\n"
		"<p class=\"test2\">This is text 5 (static).</p>\n"
		"<p>This is text 6.</p>\n"
		"<p>This is text 7.</p>\n"
		"</div>\n"
		"</body></html>\n";

	const char *pos_relative =
		"<!DOCTYPE html><html><head><style>\n"
		"div.test {\n"
		"  width: 400px;\n"
		"  border: 3px solid #808080;\n"
		"}\n"
		"p.reltest {\n"
		"  position: relative;"
		"  top: 10px;"
		"  left: 20px;"
		"}\n"
		"p.reltest2 {\n"
		"  position: relative;"
		"  bottom: 10px;"
		"  right: 20px;"
		"}\n"
		"</style></head><body>\n"
		"<div class=\"test\">"
		"<p>This is text 1.</p>\n"
		"<p>This is text 2.</p>\n"
		"<p class=\"reltest\">This is text 3 (relative).</p>\n"
		"<p>This is text 4.</p>\n"
		"<p class=\"reltest2\">This is text 5 (relative).</p>\n"
		"<p>This is text 6.</p>\n"
		"<p>This is text 7.</p>\n"
		"</div>\n"
		"</body></html>\n";

	const char *pos_fixed =
		"<!DOCTYPE html><html><head><style>\n"
		"div.test {\n"
		"  width: 400px;\n"
		"  border: 3px solid #808080;\n"
		"}\n"
		"p.fixedtest {\n"
		"  position: fixed;"
		"  top: 10px;"
		"  left: 20px;"
		"}\n"
		"p.fixedtest2 {\n"
		"  position: fixed;"
		"  bottom: 10px;"
		"  right: 20px;"
		"}\n"
		"</style></head><body>\n"
		"<div class=\"test\">"
		"<p>This is text 1.</p>\n"
		"<p>This is text 2.</p>\n"
		"<p class=\"fixedtest\">This is text 3 (fixed).</p>\n"
		"<p>This is text 4.</p>\n"
		"<p class=\"fixedtest2\">This is text 5 (fixed).</p>\n"
		"<p>This is text 6.</p>\n"
		"<p>This is text 7.</p>\n"
		"</div>\n"
		"</body></html>\n";

	const char *pos_absolute =
		"<!DOCTYPE html><html><head><style>\n"
		"div.top {\n"
		"  width: 400px;\n"
		"  border: 3px solid #808080;\n"
		"}\n"
		"div.inner {\n"
		"  position: relative;\n"
		"  width: 300px;\n"
		"  border: 3px solid #808080;\n"
		"}\n"
		"p.absolutetest {\n"
		"  position: absolute;"
		"  top: 10px;"
		"  left: 20px;"
		"}\n"
		"p.absolutetest2 {\n"
		"  position: absolute;"
		"  bottom: 10px;"
		"  right: 20px;"
		"}\n"
		"</style></head><body>\n"
		"<div class=\"top\">"
		"<p>This is text 1.</p>\n"
		"<p>This is text 2.</p>\n"
		"<div class=\"inner\">\n"
		"<p class=\"absolutetest\">This is text 3 (absolute).</p>\n"
		"</div>\n"
		"<p>This is text 4.</p>\n"
		"<div class=\"inner\">\n"
		"<p class=\"absolutetest2\">This is text 5 (absolute).</p>\n"
		"</div>\n"
		"<p>This is text 6.</p>\n"
		"<p>This is text 7.</p>\n"
		"</div>\n"
		"</body></html>\n";

	const char *pos_fixed_sizes =
		"<!DOCTYPE html><html><head><style>"
		"div.test {"
		"  position: relative;"
		"  width: 450px;"
		"  height: 450px;"
		"  border: 3px solid #808080;"
		"}"
		"p.fltw {"
		"  position: fixed;"
		"  width: 100px;"
		"  height: 100px;"
		"  top: 20px;"
		"  left: 20px;"
		"  border: 3px solid #800000;"
		"}"
		"p.frtw {"
		"  position: fixed;"
		"  width: 100px;"
		"  height: 100px;"
		"  top: 20px;"
		"  right: 20px;"
		"  border: 3px solid #800000;"
		"}"
		"p.flbw {"
		"  position: fixed;"
		"  width: 100px;"
		"  height: 100px;"
		"  bottom: 20px;"
		"  left: 20px;"
		"  border: 3px solid #800000;"
		"}"
		"p.frbw {"
		"  position: fixed;"
		"  width: 100px;"
		"  height: 100px;"
		"  bottom: 20px;"
		"  right: 20px;"
		"  border: 3px solid #800000;"
		"}"
		"p.flt {"
		"  position: fixed;"
		"  top: 130px;"
		"  left: 130px;"
		"  border: 3px solid #800000;"
		"}"
		"p.frt {"
		"  position: fixed;"
		"  top: 130px;"
		"  right: 130px;"
		"  border: 3px solid #800000;"
		"}"
		"p.flb {"
		"  position: fixed;"
		"  bottom: 130px;"
		"  left: 130px;"
		"  border: 3px solid #800000;"
		"}"
		"p.frb {"
		"  position: fixed;"
		"  bottom: 130px;"
		"  right: 130px;"
		"  border: 3px solid #800000;"
		"}"
		"</style></head><body>"
		"<div class=\"test\">"
		"<p class=\"flt\">flt</p>"
		"<p class=\"frt\">frt</p>"
		"<p class=\"flb\">flb</p>"
		"<p class=\"frb\">frb</p>"
		"<p class=\"fltw\">fltw</p>"
		"<p class=\"frtw\">frtw</p>"
		"<p class=\"flbw\">flbw</p>"
		"<p class=\"frbw\">frbw</p>"
		"</div>"
		"</body></html>";

	const char *pos_absolute_sizes =
		"<!DOCTYPE html><html><head><style>"
		"div.test {"
		"  position: relative;"
		"  width: 450px;"
		"  height: 450px;"
		"  border: 3px solid #808080;"
		"}"
		"p.altw {"
		"  position: absolute;"
		"  width: 100px;"
		"  height: 100px;"
		"  top: 20px;"
		"  left: 20px;"
		"  border: 3px solid #800000;"
		"}"
		"p.artw {"
		"  position: absolute;"
		"  width: 100px;"
		"  height: 100px;"
		"  top: 20px;"
		"  right: 20px;"
		"  border: 3px solid #800000;"
		"}"
		"p.albw {"
		"  position: absolute;"
		"  width: 100px;"
		"  height: 100px;"
		"  bottom: 20px;"
		"  left: 20px;"
		"  border: 3px solid #800000;"
		"}"
		"p.arbw {"
		"  position: absolute;"
		"  width: 100px;"
		"  height: 100px;"
		"  bottom: 20px;"
		"  right: 20px;"
		"  border: 3px solid #800000;"
		"}"
		"p.alt {"
		"  position: absolute;"
		"  top: 130px;"
		"  left: 130px;"
		"  border: 3px solid #800000;"
		"}"
		"p.art {"
		"  position: absolute;"
		"  top: 130px;"
		"  right: 130px;"
		"  border: 3px solid #800000;"
		"}"
		"p.alb {"
		"  position: absolute;"
		"  bottom: 130px;"
		"  left: 130px;"
		"  border: 3px solid #800000;"
		"}"
		"p.arb {"
		"  position: absolute;"
		"  bottom: 130px;"
		"  right: 130px;"
		"  border: 3px solid #800000;"
		"}"
		"</style></head><body>"
		"<div class=\"test\">"
		"<p class=\"altw\">altw</p>"
		"<p class=\"artw\">artw</p>"
		"<p class=\"albw\">albw</p>"
		"<p class=\"arbw\">arbw</p>"
		"<p class=\"alt\">alt</p>"
		"<p class=\"art\">art</p>"
		"<p class=\"alb\">alb</p>"
		"<p class=\"arb\">arb</p>"
		"</div>"
		"</body></html>";

	test_story(ctx, "pos_fixed_sizes.pdf", "", pos_fixed_sizes);

	test_story(ctx, "pos_absolute_sizes.pdf", "", pos_absolute_sizes);

	test_story(ctx, "pos_static.pdf", "", pos_static);

	test_story(ctx, "pos_relative.pdf", "", pos_relative);

	test_story(ctx, "pos_fixed.pdf", "", pos_fixed);

	test_story(ctx, "pos_absolute.pdf", "", pos_absolute);
}

static void
test_tables(fz_context *ctx)
{
	const char *tables =
		"<!DOCTYPE html><html><head><style>\n"
		"div.test {\n"
		"  width: 450px;\n"
		"  height: 450px;"
		"  border: 3px solid #808080;\n"
		"}\n"
		"td {"
		"  border: 1px solid #800000;"
		"}"
		"</style></head><body>\n"
		"<div class=\"test\">"
		"<table>"
		"<colgroup>"
		"<col span=2 style=\"background-color: #80ffff; width: 100px;\">"
		"</colgroup>"
		"<tr style=\"height: 100px;\">"
		"<th>Head 1"
		"<th>Head 2"
		"<th width=150>Head 3"
		"<th border=1>Head 4"
		"<tr>"
		"<td>Contents 1"
		"<td>Contents 2"
		"<td>Contents 3"
		"<td>Contents 4"
		"<tr height=75>"
		"<td style=\"width: 50px\">Contents 5"
		"<td width=\"75\">Contents 6"
		"<td bgcolor=#ff8040>Contents 7"
		"<td bgcolor=\"green\">Contents 8"
		"</table>"
		"</div>\n"
		"</body></html>\n";

	test_story(ctx, "tables.pdf", "", tables);
}

static void
test_tablespans(fz_context *ctx)
{
	const char *tables =
		"<!DOCTYPE html><html><head><style>\n"
		"table, td, th {\n"
		"  border: 1px solid #808080;\n"
		"}\n"
		"</style></head><body>\n"
		"<p>Using attributes:</p>"
		"<table>"
		"<tr>"
		"<td align=center colspan=2 style=\"border: 4px solid red\">A</td>"
		"<td width=75>B</td>"
		"<td align=right width=50>C</td>"
		"</tr>"
		"<tr>"
		"<td valign=middle width=100 rowspan=2 style=\"border: 2px solid blue\">D</td>"
		"<td colspan=3>E</td>"
		"</tr>"
		"<tr>"
		"<td width=125>F</td>"
		"<td align=center valign=bottom rowspan=2 colspan=2 style=\"border: 3px solid green\">G</td>"
		"</tr>"
		"<tr>"
		"<td>H</td>"
		"<td>I</td>"
		"</tr>"
		"</table>"
		"<p>Using styles:</p>"
		"<table>"
		"<tr>"
		"<td colspan=2 style=\"border: 4px solid red; text-align: center\">A</td>"
		"<td style=\"width: 75px\">B</td>"
		"<td style=\"width: 50px; text-align: right\">C</td>"
		"</tr>"
		"<tr>"
		"<td rowspan=2 style=\"width: 100px; border: 2px solid blue; vertical-align: middle\">D</td>"
		"<td colspan=3>E</td>"
		"</tr>"
		"<tr>"
		"<td style=\"width: 125px\">F</td>"
		"<td rowspan=2 colspan=2 style=\"border: 3px solid green; text-align: center; vertical-align:bottom\">G</td>"
		"</tr>"
		"<tr>"
		"<td>H</td>"
		"<td>I</td>"
		"</tr>"
		"</table>"
		"</div>\n"
		"</body></html>\n";

	test_story(ctx, "tablespan.pdf", "", tables);
}

static void
test_tableborders(fz_context *ctx)
{
	const char *tables =
		"<!DOCTYPE html><html><head><style>\n"
		"table, td, th {\n"
		"  border: 1px solid #808080;\n"
		"}\n"
		"</style></head><body>\n"
		"<table>"
		"<tr>"
		"<td style=\"border: 8px outset #ff0000;\">F</td>"
		"<td style=\"border: 8px outset #ee0000;\">E</td>"
		"<td style=\"border: 8px outset #dd0000;\">D</td>"
		"<td style=\"border: 8px outset #cc0000;\">C</td>"
		"<td style=\"border: 8px outset #bb0000;\">B</td>"
		"<td style=\"border: 8px outset #aa0000;\">A</td>"
		"<td style=\"border: 8px outset #990000;\">9</td>"
		"<td style=\"border: 8px outset #880000;\">8</td>"
		"<td style=\"border: 8px outset #770000;\">7</td>"
		"<td style=\"border: 8px outset #660000;\">6</td>"
		"<td style=\"border: 8px outset #550000;\">5</td>"
		"<td style=\"border: 8px outset #440000;\">4</td>"
		"<td style=\"border: 8px outset #330000;\">3</td>"
		"<td style=\"border: 8px outset #220000;\">2</td>"
		"<td style=\"border: 8px outset #110000;\">1</td>"
		"<td style=\"border: 8px outset #000000;\">0</td>"
		"<td style=\"border: 8px #ff0000; border-style: none outset;\">F</td>"
		"<td style=\"border: 8px #ff0000; border-style: outset none;\">F</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 8px inset #00ff00;\">F</td>"
		"<td style=\"border: 8px inset #00ee00;\">E</td>"
		"<td style=\"border: 8px inset #00dd00;\">D</td>"
		"<td style=\"border: 8px inset #00cc00;\">C</td>"
		"<td style=\"border: 8px inset #00bb00;\">B</td>"
		"<td style=\"border: 8px inset #00aa00;\">A</td>"
		"<td style=\"border: 8px inset #009900;\">9</td>"
		"<td style=\"border: 8px inset #008800;\">8</td>"
		"<td style=\"border: 8px inset #007700;\">7</td>"
		"<td style=\"border: 8px inset #006600;\">6</td>"
		"<td style=\"border: 8px inset #005500;\">5</td>"
		"<td style=\"border: 8px inset #004400;\">4</td>"
		"<td style=\"border: 8px inset #003300;\">3</td>"
		"<td style=\"border: 8px inset #002200;\">2</td>"
		"<td style=\"border: 8px inset #001100;\">1</td>"
		"<td style=\"border: 8px inset #000000;\">0</td>"
		"<td style=\"border: 8px #00ff00; border-style: none inset;\">F</td>"
		"<td style=\"border: 8px #00ff00; border-style: inset none;\">F</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 8px ridge #0000ff;\">F</td>"
		"<td style=\"border: 8px ridge #0000ee;\">E</td>"
		"<td style=\"border: 8px ridge #0000dd;\">D</td>"
		"<td style=\"border: 8px ridge #0000cc;\">C</td>"
		"<td style=\"border: 8px ridge #0000bb;\">B</td>"
		"<td style=\"border: 8px ridge #0000aa;\">A</td>"
		"<td style=\"border: 8px ridge #000099;\">9</td>"
		"<td style=\"border: 8px ridge #000088;\">8</td>"
		"<td style=\"border: 8px ridge #000077;\">7</td>"
		"<td style=\"border: 8px ridge #000066;\">6</td>"
		"<td style=\"border: 8px ridge #000055;\">5</td>"
		"<td style=\"border: 8px ridge #000044;\">4</td>"
		"<td style=\"border: 8px ridge #000033;\">3</td>"
		"<td style=\"border: 8px ridge #000022;\">2</td>"
		"<td style=\"border: 8px ridge #000011;\">1</td>"
		"<td style=\"border: 8px ridge #000000;\">0</td>"
		"<td style=\"border: 8px #0000ff; border-style: none ridge;\">F</td>"
		"<td style=\"border: 8px #0000ff; border-style: ridge none;\">F</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 8px groove #7800ff;\">F</td>"
		"<td style=\"border: 8px groove #7000ee;\">E</td>"
		"<td style=\"border: 8px groove #6800dd;\">D</td>"
		"<td style=\"border: 8px groove #6000cc;\">C</td>"
		"<td style=\"border: 8px groove #5800bb;\">B</td>"
		"<td style=\"border: 8px groove #5000aa;\">A</td>"
		"<td style=\"border: 8px groove #480099;\">9</td>"
		"<td style=\"border: 8px groove #400088;\">8</td>"
		"<td style=\"border: 8px groove #380077;\">7</td>"
		"<td style=\"border: 8px groove #300066;\">6</td>"
		"<td style=\"border: 8px groove #280055;\">5</td>"
		"<td style=\"border: 8px groove #200044;\">4</td>"
		"<td style=\"border: 8px groove #180033;\">3</td>"
		"<td style=\"border: 8px groove #100022;\">2</td>"
		"<td style=\"border: 8px groove #080011;\">1</td>"
		"<td style=\"border: 8px groove #000000;\">0</td>"
		"<td style=\"border: 8px #7800ff; border-style: none groove;\">F</td>"
		"<td style=\"border: 8px #7800ff; border-style: groove none;\">F</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 8px double #ff7800;\">F</td>"
		"<td style=\"border: 8px double #ee7000;\">E</td>"
		"<td style=\"border: 8px double #dd6800;\">D</td>"
		"<td style=\"border: 8px double #cc6000;\">C</td>"
		"<td style=\"border: 8px double #bb5800;\">B</td>"
		"<td style=\"border: 8px double #aa5000;\">A</td>"
		"<td style=\"border: 8px double #994800;\">9</td>"
		"<td style=\"border: 8px double #884000;\">8</td>"
		"<td style=\"border: 8px double #773800;\">7</td>"
		"<td style=\"border: 8px double #663000;\">6</td>"
		"<td style=\"border: 8px double #552800;\">5</td>"
		"<td style=\"border: 8px double #442000;\">4</td>"
		"<td style=\"border: 8px double #331800;\">3</td>"
		"<td style=\"border: 8px double #221000;\">2</td>"
		"<td style=\"border: 8px double #110800;\">1</td>"
		"<td style=\"border: 8px double #000000;\">0</td>"
		"<td style=\"border: 8px #ff7800; border-style: none double;\">F</td>"
		"<td style=\"border: 8px #ff7800; border-style: double none;\">F</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 8px solid #ff0078;\">F</td>"
		"<td style=\"border: 8px solid #ee0070;\">E</td>"
		"<td style=\"border: 8px solid #dd0068;\">D</td>"
		"<td style=\"border: 8px solid #cc0060;\">C</td>"
		"<td style=\"border: 8px solid #bb0058;\">B</td>"
		"<td style=\"border: 8px solid #aa0050;\">A</td>"
		"<td style=\"border: 8px solid #990048;\">9</td>"
		"<td style=\"border: 8px solid #880040;\">8</td>"
		"<td style=\"border: 8px solid #770038;\">7</td>"
		"<td style=\"border: 8px solid #660030;\">6</td>"
		"<td style=\"border: 8px solid #550028;\">5</td>"
		"<td style=\"border: 8px solid #440020;\">4</td>"
		"<td style=\"border: 8px solid #330018;\">3</td>"
		"<td style=\"border: 8px solid #220010;\">2</td>"
		"<td style=\"border: 8px solid #110008;\">1</td>"
		"<td style=\"border: 8px solid #000000;\">0</td>"
		"<td style=\"border: 8px #ff0078; border-style: none solid;\">F</td>"
		"<td style=\"border: 8px #ff0078; border-style: solid none;\">F</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 8px dashed #00ff78;\">F</td>"
		"<td style=\"border: 8px dashed #00ee70;\">E</td>"
		"<td style=\"border: 8px dashed #00dd68;\">D</td>"
		"<td style=\"border: 8px dashed #00cc60;\">C</td>"
		"<td style=\"border: 8px dashed #00bb58;\">B</td>"
		"<td style=\"border: 8px dashed #00aa50;\">A</td>"
		"<td style=\"border: 8px dashed #009948;\">9</td>"
		"<td style=\"border: 8px dashed #008840;\">8</td>"
		"<td style=\"border: 8px dashed #007738;\">7</td>"
		"<td style=\"border: 8px dashed #006630;\">6</td>"
		"<td style=\"border: 8px dashed #005528;\">5</td>"
		"<td style=\"border: 8px dashed #004420;\">4</td>"
		"<td style=\"border: 8px dashed #003318;\">3</td>"
		"<td style=\"border: 8px dashed #002210;\">2</td>"
		"<td style=\"border: 8px dashed #001108;\">1</td>"
		"<td style=\"border: 8px dashed #000000;\">0</td>"
		"<td style=\"border: 8px #00ff78; border-style: none dashed;\">F</td>"
		"<td style=\"border: 8px #00ff78; border-style: dashed none;\">F</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 8px dotted #ffffff;\">F</td>"
		"<td style=\"border: 8px dotted #eeeeee;\">E</td>"
		"<td style=\"border: 8px dotted #dddddd;\">D</td>"
		"<td style=\"border: 8px dotted #cccccc;\">C</td>"
		"<td style=\"border: 8px dotted #bbbbbb;\">B</td>"
		"<td style=\"border: 8px dotted #aaaaaa;\">A</td>"
		"<td style=\"border: 8px dotted #999999;\">9</td>"
		"<td style=\"border: 8px dotted #888888;\">8</td>"
		"<td style=\"border: 8px dotted #777777;\">7</td>"
		"<td style=\"border: 8px dotted #666666;\">6</td>"
		"<td style=\"border: 8px dotted #555555;\">5</td>"
		"<td style=\"border: 8px dotted #444444;\">4</td>"
		"<td style=\"border: 8px dotted #333333;\">3</td>"
		"<td style=\"border: 8px dotted #222222;\">2</td>"
		"<td style=\"border: 8px dotted #111111;\">1</td>"
		"<td style=\"border: 8px dotted #000000;\">0</td>"
		"<td style=\"border: 8px #888888; border-style: none dotted;\">F</td>"
		"<td style=\"border: 8px #888888; border-style: dotted none;\">F</td>"
		"</tr>"
		"</table>"
		"<table>"
		"<tr>"
		"<td style=\"border: 2px dashed\">Some</td>"
		"<td style=\"border: 2px dashed\">longer</td>"
		"<td style=\"border: 2px dashed\">wi-----der</td>"
		"<td style=\"border: 2px dashed\">cells..............</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 2px dotted\">Some</td>"
		"<td style=\"border: 2px dotted\">longer</td>"
		"<td style=\"border: 2px dotted\">wi-----der</td>"
		"<td style=\"border: 2px dotted\">cells..............</td>"
		"</tr>"
		"</table>"
		"</div>\n"
		"</body></html>\n";

	test_story(ctx, "tableborders.pdf", "", tables);
}

static void
test_tableborderwidths(fz_context *ctx)
{
	const char *tables =
		"<!DOCTYPE html><html><head><style>\n"
		"td, th {\n"
		"  border: 1px solid black;\n"
		"}\n"
		"</style></head><body>\n"
		"<table>"
		"<tr>"
		"<td style=\"border: 1px #ff0000; border-style: solid; background-color: blue\">1</td>"
		"<td style=\"border: 2px #ff0000; border-style: solid\">2</td>"
		"<td style=\"border: 3px #ff0000; border-style: solid\">3</td>"
		"<td style=\"border: 4px #ff0000; border-style: solid\">4</td>"
		"<td style=\"border: 5px #ff0000; border-style: solid\">5</td>"
		"<td style=\"border: 6px #ff0000; border-style: solid\">6</td>"
		"<td style=\"border: 7px #ff0000; border-style: solid\">7</td>"
		"<td style=\"border: 8px #ff0000; border-style: solid\">8</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 8px #ff0000; border-style: solid\">8</td>"
		"<td style=\"border: 7px #ff0000; border-style: solid\">7</td>"
		"<td style=\"border: 6px #ff0000; border-style: solid\">6</td>"
		"<td style=\"border: 5px #ff0000; border-style: solid\">5</td>"
		"<td style=\"border: 4px #ff0000; border-style: solid\">4</td>"
		"<td style=\"border: 3px #ff0000; border-style: solid\">3</td>"
		"<td style=\"border: 2px #ff0000; border-style: solid\">2</td>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"<td style=\"border: 1px #ff0000; border-style: solid\">1</td>"
		"</tr>"
		"</table>"
		"<table>"
		"<tr>"
		"<td style=\"border-width: 1px; background-color: blue\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"borde-widthr: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"borde-widthr: 7px\">7</td>"
		"<td style=\"borde-widthr: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"</tr>"
		"<tr>"
		"<td style=\"borde-widthr: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"</tr>"
		"<tr>"
		"<td style=\"borde-widthr: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"borde-widthr: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"</tr>"
		"</table>"
		"</div>\n"
		"</body></html>\n";

	test_story(ctx, "tableborderwidths.pdf", "", tables);
}

static void
test_tablebordercollapse(fz_context *ctx)
{
	const char *tables =
		"<!DOCTYPE html><html><head><style>\n"
		"td, th {\n"
		"  border: 1px solid black;\n"
		"}\n"
		"</style></head><body>\n"
		"<table style=\"border-collapse:collapse\">"
		"<tr>"
		"<td style=\"border-width: 1px; background-color: blue\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 7px\">7</td>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"</tr>"
		"<tr>"
		"<td style=\"border-width: 8px\">8</td>"
		"<td style=\"border-width: 1px\">1</td>"
		"<td style=\"border-width: 2px\">2</td>"
		"<td style=\"border-width: 3px\">3</td>"
		"<td style=\"border-width: 4px\">4</td>"
		"<td style=\"border-width: 5px\">5</td>"
		"<td style=\"border-width: 6px\">6</td>"
		"<td style=\"border-width: 7px\">7</td>"
		"</tr>"
		"</table>"
		"</div>\n"
		"</body></html>\n";

	test_story(ctx, "tablebordercollapse.pdf", "", tables);
}

static void
test_tablewidths(fz_context *ctx)
{
	const char *tables =
		"<!DOCTYPE html><html><head><style>\n"
		"table, td, th {\n"
		"  border: 1px solid #808080;\n"
		"}\n"
		"</style></head><body>\n"
		"<table>"
		"<tr>"
		"<td>un</td>"
		"<td>widthed</td>"
		"<td>table</td>"
		"</tr>"
		"</table>"
		"<table width=10%>"
		"<tr>"
		"<td>10</td>"
		"<td>%</td>"
		"<td>table</td>"
		"</tr>"
		"</table>"
		"<table width=50%>"
		"<tr>"
		"<td>50</td>"
		"<td>%</td>"
		"<td>table</td>"
		"</tr>"
		"</table>"
		"<table width=90%>"
		"<tr>"
		"<td>90</td>"
		"<td>%</td>"
		"<td>table</td>"
		"</tr>"
		"</table>"
		"<table width=100%>"
		"<tr>"
		"<td>100</td>"
		"<td>%</td>"
		"<td>table</td>"
		"</tr>"
		"</table>"
		"</div>\n"
		"</body></html>\n";

	test_story(ctx, "tablewidths.pdf", "", tables);
}

static void
test_leftright(fz_context *ctx)
{
	const char *leftright =
		"<!DOCTYPE html><html><head><style>\n"
		"div.test {\n"
		"  width: 450px;\n"
		"  height: 450px;"
		"  border: 3px solid #808080;\n"
		"}\n"
		"td {"
		"  border: 1px solid #800000;"
		"}"
		"</style></head><body>\n"
		"<p>Right. No gap.</p>"
		// right -> left before
		"<p style=\"page-break-before: left\">Left. Then gap.</p>"
		// left -> left before
		"<p style=\"page-break-before: left\">Left.</p>"
		// left -> right before
		"<p style=\"page-break-before: right\">No gap. Right.</p>"
		// right -> right before
		"<p style=\"page-break-before: right\">Gap. Right.</p>"
		// right -> right after
		"<p style=\"page-break-after: right\">Same page.</p>"
		// right -> left after
		"<p style=\"page-break-after: left\">Gap. Right.</p>"
		// left -> left after
		"<p style=\"page-break-after: left\">No Gap. Left.</p>"
		// left- > right after
		"<p style=\"page-break-after: right\">Gap. Left.</p>"
		"<p>Right.</p>"
		"</body></html>\n";

	test_story(ctx, "leftright.pdf", "", leftright);
}

int main(int argc, const char *argv[])
{
	fz_context *ctx;
	fz_document_writer *writer = NULL;
	fz_story *story = NULL;
	fz_buffer *buf = NULL;
	fz_device *dev = NULL;
	fz_archive *archive = NULL;
	fz_rect mediabox = { 0, 0, 512, 640 };
	float margin = 10;
	int more;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (ctx == NULL)
	{
		fprintf(stderr, "Failed to create context");
		return 1;
	}

	fz_var(writer);
	fz_var(story);
	fz_var(buf);
	fz_var(dev);
	fz_var(archive);

	/* First one made with precooked content. */
	test_story(ctx, "out.pdf", "", snark);

	/* Now one made with programmatic content. */
	writer = NULL;
	buf = NULL;
	story = NULL;
	dev = NULL;

	fz_try(ctx)
	{
		fz_xml *dom, *body, *tmp;

		writer = fz_new_pdf_writer(ctx, "out2.pdf", "");

		story = fz_new_story(ctx, NULL, "", 11, NULL);

		dom = fz_story_document(ctx, story);

		body = fz_dom_body(ctx, dom);

		fz_dom_append_child(ctx, body, fz_dom_create_text_node(ctx, dom, "This is some text."));
		tmp = fz_dom_create_element(ctx, dom, "b");
		fz_dom_append_child(ctx, body, tmp);
		fz_dom_append_child(ctx, tmp, fz_dom_create_text_node(ctx, dom, "This is some bold text."));
		fz_dom_append_child(ctx, body, fz_dom_create_text_node(ctx, dom, "This is some normal text."));

		do
		{
			fz_rect where;
			fz_rect filled;

			where.x0 = mediabox.x0 + margin;
			where.y0 = mediabox.y0 + margin;
			where.x1 = mediabox.x1 - margin;
			where.y1 = mediabox.y1 - margin;

			dev = fz_begin_page(ctx, writer, mediabox);

			more = fz_place_story(ctx, story, where, &filled);

			fz_draw_story(ctx, story, dev, fz_identity);

			fz_end_page(ctx, writer);
		}
		while (more);

		fz_close_document_writer(ctx, writer);
	}
	fz_always(ctx)
	{
		fz_drop_story(ctx, story);
		fz_drop_buffer(ctx, buf);
		fz_drop_document_writer(ctx, writer);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
	}

	/* Now a combination of the two. */
	writer = NULL;
	buf = NULL;
	story = NULL;
	dev = NULL;

	fz_try(ctx)
	{
		fz_xml *dom, *body, *tmp, *templat;
		int i, j;

		writer = fz_new_pdf_writer(ctx, "out3.pdf", "");

		buf = fz_new_buffer_from_copied_data(ctx, (unsigned char *)festival_template, strlen(festival_template)+1);
		story = fz_new_story(ctx, buf, "", 11, NULL);

		dom = fz_story_document(ctx, story);

		body = fz_dom_body(ctx, dom);

		templat = fz_dom_find(ctx, body, NULL, "id", "filmtemplate");

		for (i = 0; i < nelem(films); i++)
		{
			fz_xml *film = fz_dom_clone(ctx, templat);

			/* Now fill in some of the template. */
			tmp = fz_dom_find(ctx, film, NULL, "id", "filmtitle");
			fz_dom_append_child(ctx, tmp, fz_dom_create_text_node(ctx, dom, films[i].title));

			tmp = fz_dom_find(ctx, film, NULL, "id", "director");
			fz_dom_append_child(ctx, tmp, fz_dom_create_text_node(ctx, dom, films[i].director));

			tmp = fz_dom_find(ctx, film, NULL, "id", "filmyear");
			fz_dom_append_child(ctx, tmp, fz_dom_create_text_node(ctx, dom, films[i].year));

			tmp = fz_dom_find(ctx, film, NULL, "id", "cast");
			for (j = 0; j < MAX_CAST_MEMBERS; j++)
			{
				if (films[i].cast[j] == NULL)
					break;
				fz_dom_append_child(ctx, tmp, fz_dom_create_text_node(ctx, dom, films[i].cast[j]));
				fz_dom_append_child(ctx, tmp, fz_dom_create_element(ctx, dom, "br"));
			}

			fz_dom_append_child(ctx, fz_dom_parent(ctx, templat), film);
		}

		/* Remove the template. */
		fz_dom_remove(ctx, templat);

		do
		{
			fz_rect where;
			fz_rect filled;

			where.x0 = mediabox.x0 + margin;
			where.y0 = mediabox.y0 + margin;
			where.x1 = mediabox.x1 - margin;
			where.y1 = mediabox.y1 - margin;

			dev = fz_begin_page(ctx, writer, mediabox);

			more = fz_place_story(ctx, story, where, &filled);

			fz_draw_story(ctx, story, dev, fz_identity);

			fz_end_page(ctx, writer);
		}
		while (more);

		fz_close_document_writer(ctx, writer);
	}
	fz_always(ctx)
	{
		fz_drop_story(ctx, story);
		fz_drop_buffer(ctx, buf);
		fz_drop_document_writer(ctx, writer);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
	}

	test_write_stabilized_story(ctx);

	test_positions(ctx);

	test_tables(ctx);

	test_tablespans(ctx);

	test_tablewidths(ctx);

	test_tableborders(ctx);

	test_tableborderwidths(ctx);

	test_tablebordercollapse(ctx);

	test_leftright(ctx);

	fz_drop_context(ctx);

	return 0;
}
