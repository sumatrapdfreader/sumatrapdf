/*
 * PDF cleaning tool: general purpose pdf syntax washer.
 *
 * Rewrite PDF with pretty printed objects.
 * Garbage collect unreachable objects.
 * Inflate compressed streams.
 * Encrypt output.
 */

#include "fitz.h"
#include "mupdf.h"

static pdf_xref *xref = NULL;
static fz_obj *id = NULL;

static pdf_crypt *outcrypt = NULL;
static FILE *out = NULL;

static char *uselist = NULL;
static int *ofslist = NULL;
static int *genlist = NULL;

int doencrypt = 0;
int dogarbage = 0;
int doexpand = 0;

void die(fz_error error)
{
    fz_catch(error, "aborting");
    exit(1);
}

void openxref(char *filename, char *password)
{
    fz_error error;
    fz_obj *obj;

    error = pdf_newxref(&xref);
    if (error)
	die(error);

    error = pdf_loadxref(xref, filename);
    if (error)
    {
	fz_catch(error, "trying to repair");
	error = pdf_repairxref(xref, filename);
	if (error)
	    die(error);
    }

    error = pdf_decryptxref(xref);
    if (error)
	die(error);

    if (xref->crypt)
    {
	int okay = pdf_setpassword(xref->crypt, password);
	if (!okay)
	    die(fz_throw("invalid password"));
    }

    /* TODO: move into mupdf lib, see pdfapp_open in pdfapp.c */
    obj = fz_dictgets(xref->trailer, "Root");
    if (!obj)
	die(error);

    error = pdf_loadindirect(&xref->root, xref, obj);
    if (error)
	die(error);

    obj = fz_dictgets(xref->trailer, "Info");
    if (obj)
    {
	error = pdf_loadindirect(&xref->info, xref, obj);
	if (error)
	    die(error);
    }
}

/*
 * Garbage collect objects not reachable from the trailer.
 */

static fz_error sweepref(pdf_xref *xref, fz_obj *ref);

static fz_error sweepobj(pdf_xref *xref, fz_obj *obj)
{
    fz_error error;
    int i;

    if (fz_isdict(obj))
    {
	for (i = 0; i < fz_dictlen(obj); i++)
	{
	    error = sweepobj(xref, fz_dictgetval(obj, i));
	    if (error)
		return error; /* too deeply nested for rethrow */
	}
    }

    if (fz_isarray(obj))
    {
	for (i = 0; i < fz_arraylen(obj); i++)
	{
	    error = sweepobj(xref, fz_arrayget(obj, i));
	    if (error)
		return error; /* too deeply nested for rethrow */
	}
    }

    if (fz_isindirect(obj))
	return sweepref(xref, obj);

    return fz_okay;
}

static fz_error sweepref(pdf_xref *xref, fz_obj *ref)
{
    fz_error error;
    fz_obj *obj;
    fz_obj *len;
    int oid, gen;

    oid = fz_tonum(ref);
    gen = fz_tonum(ref);

    if (oid < 0 || oid >= xref->len)
	return fz_throw("object out of range (%d %d R)", oid, gen);

    if (uselist[oid])
	return fz_okay;

    uselist[oid] = 1;

    error = pdf_loadindirect(&obj, xref, ref);
    if (error)
	return fz_rethrow(error, "cannot load indirect object");

    /* Bake in /Length in stream objects */
    if (xref->table[oid].stmofs)
    {
	len = fz_dictgets(obj, "Length");
	if (fz_isindirect(len))
	{
	    pdf_loadindirect(&len, xref, len);
	    fz_dictputs(obj, "Length", len);
	}
    }

    error = sweepobj(xref, obj);
    if (error)
    {
	fz_dropobj(obj);
	return error; /* too deeply nested for rethrow */
    }

    fz_dropobj(obj);
    return fz_okay;
}

void preloadobjstms(void)
{
    fz_error error;
    fz_obj *obj;
    int oid;

    for (oid = 0; oid < xref->len; oid++)
    {
	if (xref->table[oid].type == 'o')
	{
	    error = pdf_loadobject(&obj, xref, oid, 0);
	    if (error)
		die(error);
	    fz_dropobj(obj);
	}
    }
}

void copystream(fz_obj *obj, int oid, int gen)
{
    fz_error error;
    fz_buffer *buf;

    error = pdf_loadrawstream(&buf, xref, oid, gen);
    if (error)
	die(error);

    if (doencrypt)
	pdf_cryptbuffer(outcrypt, buf, oid, gen);

    fprintf(out, "%d %d obj\n", oid, gen);
    if (doencrypt)
	pdf_cryptobj(outcrypt, obj, oid, gen);
    fz_fprintobj(out, obj, !doexpand);
    if (doencrypt)
	pdf_cryptobj(outcrypt, obj, oid, gen);
    fprintf(out, "stream\n");
    fwrite(buf->rp, 1, buf->wp - buf->rp, out);
    fprintf(out, "endstream\nendobj\n\n");

    fz_dropbuffer(buf);
}

void expandstream(fz_obj *obj, int oid, int gen)
{
    fz_error error;
    fz_buffer *buf;
    fz_obj *newdict, *newlen;

    error = pdf_loadstream(&buf, xref, oid, gen);
    if (error)
	die(error);

    if (doencrypt)
	pdf_cryptbuffer(outcrypt, buf, oid, gen);

    fz_copydict(&newdict, obj);
    fz_dictdels(newdict, "Filter");
    fz_dictdels(newdict, "DecodeParms");

    fz_newint(&newlen, buf->wp - buf->rp);
    fz_dictputs(newdict, "Length", newlen);
    fz_dropobj(newlen);

    fprintf(out, "%d %d obj\n", oid, gen);
    if (doencrypt)
	pdf_cryptobj(outcrypt, obj, oid, gen);
    fz_fprintobj(out, newdict, !doexpand);
    if (doencrypt)
	pdf_cryptobj(outcrypt, obj, oid, gen);
    fprintf(out, "stream\n");
    fwrite(buf->rp, 1, buf->wp - buf->rp, out);
    fprintf(out, "endstream\nendobj\n\n");

    fz_dropobj(newdict);

    fz_dropbuffer(buf);
}

void saveobject(int oid, int gen)
{
    fz_error error;
    fz_obj *obj;
    fz_obj *type;

    error = pdf_loadobject(&obj, xref, oid, gen);
    if (error)
	die(error);

    /* skip ObjStm and XRef objects */
    if (fz_isdict(obj))
    {
	type = fz_dictgets(obj, "Type");
	if (fz_isname(type) && !strcmp(fz_toname(type), "ObjStm"))
	{
	    uselist[oid] = 0;
	    fz_dropobj(obj);
	    return;
	}
	if (fz_isname(type) && !strcmp(fz_toname(type), "XRef"))
	{
	    uselist[oid] = 0;
	    fz_dropobj(obj);
	    return;
	}
    }


    if (!xref->table[oid].stmofs)
    {
	fprintf(out, "%d %d obj\n", oid, gen);
	if (doencrypt)
	    pdf_cryptobj(outcrypt, obj, oid, gen);
	fz_fprintobj(out, obj, !doexpand);
	if (doencrypt)
	    pdf_cryptobj(outcrypt, obj, oid, gen);
	fprintf(out, "endobj\n\n");
    }
    else
    {
	if (doexpand)
	    expandstream(obj, oid, gen);
	else
	    copystream(obj, oid, gen);
    }


    fz_dropobj(obj);
}

void savexref(void)
{
    fz_obj *trailer;
    fz_obj *obj;
    int startxref;
    int oid;

    startxref = ftell(out);

    fprintf(out, "xref\n0 %d\n", xref->len + doencrypt);
    for (oid = 0; oid < xref->len + doencrypt; oid++)
    {
	if (uselist[oid])
	    fprintf(out, "%010d %05d n \n", ofslist[oid], genlist[oid]);
	else
	    fprintf(out, "%010d %05d f \n", ofslist[oid], genlist[oid]);
    }
    fprintf(out, "\n");

    fz_newdict(&trailer, 5);

    fz_newint(&obj, xref->len);
    fz_dictputs(trailer, "Size", obj);
    fz_dropobj(obj);

    obj = fz_dictgets(xref->trailer, "Info");
    if (obj)
	fz_dictputs(trailer, "Info", obj);

    obj = fz_dictgets(xref->trailer, "Root");
    if (obj)
	fz_dictputs(trailer, "Root", obj);

    if (doencrypt)
    {
	fz_newindirect(&obj, xref->len, 0);
	fz_dictputs(trailer, "Encrypt", obj);
	fz_dropobj(obj);
    }

    fz_dictputs(trailer, "ID", id);

    fprintf(out, "trailer\n");
    fz_fprintobj(out, trailer, !doexpand);
    fprintf(out, "\n");

    fprintf(out, "startxref\n%d\n%%%%EOF\n", startxref);
}

void cleanusage(void)
{
    fprintf(stderr,
	    "usage: pdfclean [options] input.pdf [outfile.pdf]\n"
	    "  -d -\tpassword for decryption\n"
	    "  -g  \tgarbage collect unused objects\n"
	    "  -x  \texpand compressed streams\n"
	    "  -e  \tencrypt output\n"
	    "    -u -\tset user password for encryption\n"
	    "    -o -\tset owner password\n"
	    "    -p -\tset permissions (combine letters 'pmca')\n"
	    "    -n -\tkey length in bits: 40 <= n <= 128\n");
    exit(1);
}

int main(int argc, char **argv)
{
    char *infile;
    char *outfile = "out.pdf";
    char *userpw = "";
    char *ownerpw = "";
    unsigned perms = 0xfffff0c0;	/* nothing allowed */
    int keylen = 40;
    char *password = "";
    fz_error error;
    int c, oid;
    int lastfree;


    while ((c = getopt(argc, argv, "d:egn:o:p:u:x")) != -1)
    {
	switch (c)
	{
	    case 'p':
		/* see TABLE 3.15 User access permissions */
		perms = 0xfffff0c0;
		if (strchr(optarg, 'p')) /* print */
		    perms |= (1 << 2) | (1 << 11);
		if (strchr(optarg, 'm')) /* modify */
		    perms |= (1 << 3) | (1 << 10);
		if (strchr(optarg, 'c')) /* copy */
		    perms |= (1 << 4) | (1 << 9);
		if (strchr(optarg, 'a')) /* annotate / forms */
		    perms |= (1 << 5) | (1 << 8);
		break;
	    case 'd': password = optarg; break;
	    case 'e': doencrypt ++; break;
	    case 'g': dogarbage ++; break;
	    case 'n': keylen = atoi(optarg); break;
	    case 'o': ownerpw = optarg; break;
	    case 'u': userpw = optarg; break;
	    case 'x': doexpand ++; break;
	    default: cleanusage(); break;
	}
    }

    if (argc - optind < 1)
	cleanusage();

    infile = argv[optind++];
    if (argc - optind > 0)
	outfile = argv[optind++];

    openxref(infile, password);

    id = fz_dictgets(xref->trailer, "ID");
    if (!id)
    {
	error = fz_packobj(&id, "[(ABCDEFGHIJKLMNOP)(ABCDEFGHIJKLMNOP)]");
	if (error)
	    die(error);
    }
    else
    {
	fz_keepobj(id);
    }

    if (doencrypt)
    {
	error = pdf_newencrypt(&outcrypt, userpw, ownerpw, perms, keylen, id);
	if (error)
	    die(error);
    }

    out = fopen(outfile, "wb");
    if (!out)
	die(fz_throw("cannot open output file '%s'", outfile));

    fprintf(out, "%%PDF-%d.%d\n", xref->version / 10, xref->version % 10);
    fprintf(out, "%%\342\343\317\323\n\n");

    uselist = malloc(sizeof (char) * (xref->len + 1));
    ofslist = malloc(sizeof (int) * (xref->len + 1));
    genlist = malloc(sizeof (int) * (xref->len + 1));

    for (oid = 0; oid < xref->len; oid++)
    {
	uselist[oid] = 0;
	ofslist[oid] = 0;
	genlist[oid] = 0;
    }

    /* Make sure any objects hidden in compressed streams have been loaded */
    preloadobjstms();

    /* Sweep & mark objects from the trailer */
    error = sweepobj(xref, xref->trailer);
    if (error)
	die(fz_rethrow(error, "cannot mark used objects"));

    for (oid = 0; oid < xref->len; oid++)
    {
	if (xref->table[oid].type == 'f')
	    uselist[oid] = 0;

	if (xref->table[oid].type == 'f')
	    genlist[oid] = xref->table[oid].gen;
	if (xref->table[oid].type == 'n')
	    genlist[oid] = xref->table[oid].gen;
	if (xref->table[oid].type == 'o')
	    genlist[oid] = 0;

	if (dogarbage && !uselist[oid])
	    continue;

	if (xref->table[oid].type == 'n' || xref->table[oid].type == 'o')
	{
	    ofslist[oid] = ftell(out);
	    saveobject(oid, genlist[oid]);
	}
    }

    /* add new encryption dictionary to xref */
    if (doencrypt)
    {
	ofslist[xref->len] = ftell(out);
	genlist[xref->len] = 0;
	uselist[xref->len] = 1;
	fprintf(out, "%d %d obj\n", xref->len, 0);
	fz_fprintobj(out, outcrypt->encrypt, !doexpand);
	fprintf(out, "endobj\n\n");
    }

    /* construct linked list of free object slots */
    lastfree = 0;
    for (oid = 0; oid < xref->len + doencrypt; oid++)
    {
	if (!uselist[oid])
	{
	    genlist[oid]++;
	    ofslist[lastfree] = oid;
	    lastfree = oid;
	}
    }

    savexref();

    pdf_closexref(xref);
}

