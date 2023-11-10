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
		fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), rgb, 0.9 /*alpha*/, fz_default_color_params);
		fz_drop_path(ctx, path);
	}
	else
	{
		float rgb[3] = { 0.125, 0.25, 0.75};
		fz_moveto(ctx, path, 50, 50);
		fz_lineto(ctx, path, 50, 200);
		fz_lineto(ctx, path, 100, 50);
		fz_closepath(ctx, path);
		fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), rgb, 0.9 /*alpha*/, fz_default_color_params);
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
	fz_try(ctx)
	{
		writer = fz_new_pdf_writer(ctx, "out.pdf", "");

		buf = fz_new_buffer_from_copied_data(ctx, snark, strlen(snark)+1);

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

		buf = fz_new_buffer_from_copied_data(ctx, festival_template, strlen(festival_template)+1);
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

	fz_drop_context(ctx);

	return 0;
}
