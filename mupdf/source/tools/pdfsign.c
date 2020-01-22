/*
 * PDF signature tool: verify and sign digital signatures in PDF files.
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "mupdf/helpers/pkcs7-check.h"
#include "mupdf/helpers/pkcs7-openssl.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char *infile = NULL;
static char *outfile = NULL;
static char *certificatefile = NULL;
static char *certificatepassword = "";
static int verify = 0;
static int clear = 0;
static int sign = 0;
static int list = 1;

static void usage(void)
{
	fprintf(stderr,
		"usage: mutool sign [options] input.pdf [signature object numbers]\n"
		"\t-p -\tpassword\n"
		"\t-v \tverify signature\n"
		"\t-c \tclear signatures\n"
		"\t-s -\tsign signatures using certificate file\n"
		"\t-P -\tcertificate password\n"
		"\t-o -\toutput file name\n"
		   );
	exit(1);
}

static void verify_signature(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	char name[500];
	pdf_signature_error err;
	int edits;

	printf("verifying signature %d\n", pdf_to_num(ctx, signature));

	if (!pdf_signature_is_signed(ctx, doc, signature))
	{
		printf("  Signature is not signed\n");
		return;
	}

	pdf_signature_designated_name(ctx, doc, signature, name, sizeof name);
	printf("  Designated name: %s\n", name);

	err = pdf_check_certificate(ctx, doc, signature);
	if (err)
		printf("  Certificate error: %s\n", pdf_signature_error_description(err));
	else
		printf("  Certificate is trusted.\n");

	fz_try(ctx)
	{
		err = pdf_check_digest(ctx, doc, signature);
		edits = pdf_signature_incremental_change_since_signing(ctx, doc, signature);
		if (err)
			printf("  Digest error: %s\n", pdf_signature_error_description(err));
		else if (edits)
			printf("  The signature is valid but there have been edits since signing.\n");
		else
			printf("  The document is unchanged since signing.\n");
	}
	fz_catch(ctx)
		printf("  Digest error: %s\n", fz_caught_message(ctx));
}

static void clear_signature(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	pdf_page *page = NULL;
	pdf_widget *widget;
	pdf_obj *parent;
	int pageno;

	fz_var(page);

	printf("clearing signature %d\n", pdf_to_num(ctx, signature));

	fz_try(ctx)
	{
		parent = pdf_dict_get(ctx, signature, PDF_NAME(P));
		pageno = pdf_lookup_page_number(ctx, doc, parent);
		page = pdf_load_page(ctx, doc, pageno);
		for (widget = pdf_first_widget(ctx, page); widget; widget = pdf_next_widget(ctx, widget))
			if (pdf_widget_type(ctx, widget) == PDF_WIDGET_TYPE_SIGNATURE && !pdf_objcmp_resolve(ctx, widget->obj, signature))
				pdf_clear_signature(ctx, doc, widget);
	}
	fz_always(ctx)
		fz_drop_page(ctx, (fz_page*)page);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void sign_signature(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	pdf_pkcs7_signer *signer = NULL;
	pdf_page *page = NULL;
	pdf_widget *widget;
	pdf_obj *parent;
	int pageno;

	fz_var(page);
	fz_var(signer);

	printf("signing signature %d\n", pdf_to_num(ctx, signature));

	fz_try(ctx)
	{
		signer = pkcs7_openssl_read_pfx(ctx, certificatefile, certificatepassword);

		parent = pdf_dict_get(ctx, signature, PDF_NAME(P));
		pageno = pdf_lookup_page_number(ctx, doc, parent);
		page = pdf_load_page(ctx, doc, pageno);
		for (widget = pdf_first_widget(ctx, page); widget; widget = pdf_next_widget(ctx, widget))
			if (pdf_widget_type(ctx, widget) == PDF_WIDGET_TYPE_SIGNATURE && !pdf_objcmp_resolve(ctx, widget->obj, signature))
				pdf_sign_signature(ctx, doc, widget, signer);
	}
	fz_always(ctx)
	{
		fz_drop_page(ctx, (fz_page*)page);
		if (signer)
			signer->drop(signer);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

}

static void list_signature(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	char name[500];
	pdf_signature_designated_name(ctx, doc, signature, name, sizeof name);
	printf("%5d: signature name: %s\n", pdf_to_num(ctx, signature), name);
}

static void process_field(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	if (pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT)) != PDF_NAME(Sig))
		fz_warn(ctx, "%d is not a signature, skipping", pdf_to_num(ctx, field));
	else
	{
		if (list)
			list_signature(ctx, doc, field);
		if (verify)
			verify_signature(ctx, doc, field);
		if (clear)
			clear_signature(ctx, doc, field);
		if (sign)
			sign_signature(ctx, doc, field);
	}
}

static void process_field_hierarchy(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
	if (kids)
	{
		int i, n;
		n = pdf_array_len(ctx, kids);
		for (i = 0; i < n; ++i)
		{
			pdf_obj *kid = pdf_array_get(ctx, kids, i);
			if (pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT)) == PDF_NAME(Sig))
				process_field_hierarchy(ctx, doc, kid);
		}
	}
	else if (pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT)) == PDF_NAME(Sig))
		process_field(ctx, doc, field);
}

static void process_acro_form(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *trailer = pdf_trailer(ctx, doc);
	pdf_obj *root = pdf_dict_get(ctx, trailer, PDF_NAME(Root));
	pdf_obj *acroform = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
	pdf_obj *fields = pdf_dict_get(ctx, acroform, PDF_NAME(Fields));
	int i, n = pdf_array_len(ctx, fields);
	for (i = 0; i < n; ++i)
		process_field_hierarchy(ctx, doc, pdf_array_get(ctx, fields, i));
}

int pdfsign_main(int argc, char **argv)
{
	fz_context *ctx;
	pdf_document *doc;
	char *password = "";
	int c;
	pdf_page *page = NULL;

	while ((c = fz_getopt(argc, argv, "co:p:s:vP:")) != -1)
	{
		switch (c)
		{
		case 'c': list = 0; clear = 1; break;
		case 'o': outfile = fz_optarg; break;
		case 'p': password = fz_optarg; break;
		case 'P': certificatepassword = fz_optarg; break;
		case 's': list = 0; sign = 1; certificatefile = fz_optarg; break;
		case 'v': list = 0; verify = 1; break;
		default: usage(); break;
		}
	}

	if (argc - fz_optind < 1)
		usage();

	infile = argv[fz_optind++];

	if (!clear && !sign && !verify && argc - fz_optind > 0)
	{
		list = 0;
		verify = 1;
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialize context\n");
		exit(1);
	}

	fz_var(page);

	doc = pdf_open_document(ctx, infile);
	fz_try(ctx)
	{
		if (pdf_needs_password(ctx, doc))
			if (!pdf_authenticate_password(ctx, doc, password))
				fz_warn(ctx, "cannot authenticate password: %s", infile);

		if (argc - fz_optind <= 0 || list)
			process_acro_form(ctx, doc);
		else
		{
			while (argc - fz_optind)
			{
				pdf_obj *field = pdf_new_indirect(ctx, doc, fz_atoi(argv[fz_optind]), 0);
				process_field(ctx, doc, field);
				pdf_drop_obj(ctx, field);
				fz_optind++;
			}
		}

		if (clear || sign)
		{
			if (!outfile)
				outfile = "out.pdf";
			pdf_save_document(ctx, doc, outfile, NULL);
		}
	}
	fz_always(ctx)
		pdf_drop_document(ctx, doc);
	fz_catch(ctx)
	{
		fz_drop_page(ctx, (fz_page*)page);
		fprintf(stderr, "error processing signatures: %s\n", fz_caught_message(ctx));
	}

	fz_flush_warnings(ctx);
	fz_drop_context(ctx);
	return 0;
}
