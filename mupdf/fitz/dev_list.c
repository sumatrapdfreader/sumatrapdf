#include "fitz.h"

static fz_displaynode *
fz_newdisplaynode(fz_displaycommand cmd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_displaynode *node;
	int i;

	node = fz_malloc(sizeof(fz_displaynode));
	node->cmd = cmd;
	node->next = nil;
	node->item.path = nil;
	node->stroke = nil;
	node->flag = 0;
	node->ctm = ctm;
	if (colorspace)
	{
		node->colorspace = fz_keepcolorspace(colorspace);
		for (i = 0; i < node->colorspace->n; i++)
			node->color[i] = color[i];
	}
	else
	{
		node->colorspace = nil;
	}
	node->alpha = alpha;

	return node;
}

static fz_strokestate *
fz_clonestrokestate(fz_strokestate *stroke)
{
	fz_strokestate *newstroke = fz_malloc(sizeof(fz_strokestate));
	*newstroke = *stroke;
	return newstroke;
}

static void
fz_appenddisplaynode(fz_displaylist *list, fz_displaynode *node)
{
	if (!list->first)
	{
		list->first = node;
		list->last = node;
	}
	else
	{
		list->last->next = node;
		list->last = node;
	}
}

static void
fz_freedisplaynode(fz_displaynode *node)
{
	switch (node->cmd)
	{
	case FZ_CMDFILLPATH:
	case FZ_CMDSTROKEPATH:
	case FZ_CMDCLIPPATH:
	case FZ_CMDCLIPSTROKEPATH:
		fz_freepath(node->item.path);
		break;
	case FZ_CMDFILLTEXT:
	case FZ_CMDSTROKETEXT:
	case FZ_CMDCLIPTEXT:
	case FZ_CMDCLIPSTROKETEXT:
	case FZ_CMDIGNORETEXT:
		fz_freetext(node->item.text);
		break;
	case FZ_CMDFILLSHADE:
		fz_dropshade(node->item.shade);
		break;
	case FZ_CMDFILLIMAGE:
	case FZ_CMDFILLIMAGEMASK:
	case FZ_CMDCLIPIMAGEMASK:
		fz_droppixmap(node->item.image);
		break;
	case FZ_CMDPOPCLIP:
		break;
	}
	if (node->stroke)
		fz_free(node->stroke);
	if (node->colorspace)
		fz_dropcolorspace(node->colorspace);
	fz_free(node);
}

static void
fz_listfillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDFILLPATH, ctm, colorspace, color, alpha);
	node->item.path = fz_clonepath(path);
	node->flag = evenodd;
	fz_appenddisplaynode(user, node);
}

static void
fz_liststrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDSTROKEPATH, ctm, colorspace, color, alpha);
	node->item.path = fz_clonepath(path);
	node->stroke = fz_clonestrokestate(stroke);
	fz_appenddisplaynode(user, node);
}

static void
fz_listclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDCLIPPATH, ctm, nil, nil, 0.0);
	node->item.path = fz_clonepath(path);
	node->flag = evenodd;
	fz_appenddisplaynode(user, node);
}

static void
fz_listclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDCLIPSTROKEPATH, ctm, nil, nil, 0.0);
	node->item.path = fz_clonepath(path);
	node->stroke = fz_clonestrokestate(stroke);
	fz_appenddisplaynode(user, node);
}

static void
fz_listfilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDFILLTEXT, ctm, colorspace, color, alpha);
	node->item.text = fz_clonetext(text);
	fz_appenddisplaynode(user, node);
}

static void
fz_liststroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDSTROKETEXT, ctm, colorspace, color, alpha);
	node->item.text = fz_clonetext(text);
	node->stroke = fz_clonestrokestate(stroke);
	fz_appenddisplaynode(user, node);
}

static void
fz_listcliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDCLIPTEXT, ctm, nil, nil, 0.0);
	node->item.text = fz_clonetext(text);
	node->flag = accumulate;
	fz_appenddisplaynode(user, node);
}

static void
fz_listclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDCLIPSTROKETEXT, ctm, nil, nil, 0.0);
	node->item.text = fz_clonetext(text);
	node->stroke = fz_clonestrokestate(stroke);
	fz_appenddisplaynode(user, node);
}

static void
fz_listignoretext(void *user, fz_text *text, fz_matrix ctm)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDIGNORETEXT, ctm, nil, nil, 0.0);
	node->item.text = fz_clonetext(text);
	fz_appenddisplaynode(user, node);
}

static void
fz_listpopclip(void *user)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDPOPCLIP, fz_identity(), nil, nil, 0.0);
	fz_appenddisplaynode(user, node);
}

static void
fz_listfillshade(void *user, fz_shade *shade, fz_matrix ctm)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDFILLSHADE, ctm, nil, nil, 0.0);
	node->item.shade = fz_keepshade(shade);
	fz_appenddisplaynode(user, node);
}

static void
fz_listfillimage(void *user, fz_pixmap *image, fz_matrix ctm)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDFILLIMAGE, ctm, nil, nil, 0.0);
	node->item.image = fz_keeppixmap(image);
	fz_appenddisplaynode(user, node);
}

static void
fz_listfillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDFILLIMAGEMASK, ctm, colorspace, color, alpha);
	node->item.image = fz_keeppixmap(image);
	fz_appenddisplaynode(user, node);
}

static void
fz_listclipimagemask(void *user, fz_pixmap *image, fz_matrix ctm)
{
	fz_displaynode *node;
	node = fz_newdisplaynode(FZ_CMDCLIPIMAGEMASK, ctm, nil, nil, 0.0);
	node->item.image = fz_keeppixmap(image);
	fz_appenddisplaynode(user, node);
}

fz_device *
fz_newlistdevice(fz_displaylist *list)
{
	fz_device *dev = fz_newdevice(list);

	dev->fillpath = fz_listfillpath;
	dev->strokepath = fz_liststrokepath;
	dev->clippath = fz_listclippath;
	dev->clipstrokepath = fz_listclipstrokepath;

	dev->filltext = fz_listfilltext;
	dev->stroketext = fz_liststroketext;
	dev->cliptext = fz_listcliptext;
	dev->clipstroketext = fz_listclipstroketext;
	dev->ignoretext = fz_listignoretext;

	dev->fillshade = fz_listfillshade;
	dev->fillimage = fz_listfillimage;
	dev->fillimagemask = fz_listfillimagemask;
	dev->clipimagemask = fz_listclipimagemask;

	dev->popclip = fz_listpopclip;

	return dev;
}

fz_displaylist *
fz_newdisplaylist(void)
{
	fz_displaylist *list = fz_malloc(sizeof(fz_displaylist));
	list->first = nil;
	list->last = nil;
	return list;
}

void
fz_freedisplaylist(fz_displaylist *list)
{
	fz_displaynode *node = list->first;
	while (node)
	{
		fz_displaynode *next = node->next;
		fz_freedisplaynode(node);
		node = next;
	}
}

void
fz_executedisplaylist(fz_displaylist *list, fz_device *dev, fz_matrix topctm)
{
	fz_displaynode *node;
	for (node = list->first; node; node = node->next)
	{
		fz_matrix ctm = fz_concat(node->ctm, topctm);
		switch (node->cmd)
		{
		case FZ_CMDFILLPATH:
			dev->fillpath(dev->user, node->item.path, node->flag, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMDSTROKEPATH:
			dev->strokepath(dev->user, node->item.path, node->stroke, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMDCLIPPATH:
			dev->clippath(dev->user, node->item.path, node->flag, ctm);
			break;
		case FZ_CMDCLIPSTROKEPATH:
			dev->clipstrokepath(dev->user, node->item.path, node->stroke, ctm);
			break;
		case FZ_CMDFILLTEXT:
			dev->filltext(dev->user, node->item.text, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMDSTROKETEXT:
			dev->stroketext(dev->user, node->item.text, node->stroke, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMDCLIPTEXT:
			dev->cliptext(dev->user, node->item.text, ctm, node->flag);
			break;
		case FZ_CMDCLIPSTROKETEXT:
			dev->clipstroketext(dev->user, node->item.text, node->stroke, ctm);
			break;
		case FZ_CMDIGNORETEXT:
			dev->ignoretext(dev->user, node->item.text, ctm);
			break;
		case FZ_CMDFILLSHADE:
			dev->fillshade(dev->user, node->item.shade, ctm);
			break;
		case FZ_CMDFILLIMAGE:
			dev->fillimage(dev->user, node->item.image, ctm);
			break;
		case FZ_CMDFILLIMAGEMASK:
			dev->fillimagemask(dev->user, node->item.image, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMDCLIPIMAGEMASK:
			dev->clipimagemask(dev->user, node->item.image, ctm);
			break;
		case FZ_CMDPOPCLIP:
			dev->popclip(dev->user);
			break;
		}
	}
}

