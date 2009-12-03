#include "fitz_base.h"
#include "fitz_tree.h"

void fz_droplinknode(fz_linknode* node);
void fz_droppathnode(fz_pathnode* node);
void fz_dropsolidnode(fz_solidnode* node);
void fz_droptextnode(fz_textnode* node);
void fz_dropimagenode(fz_imagenode* node);
void fz_dropshadenode(fz_shadenode* node);

fz_rect fz_boundtransformnode(fz_transformnode* node, fz_matrix ctm);
fz_rect fz_boundovernode(fz_overnode* node, fz_matrix ctm);
fz_rect fz_boundmasknode(fz_masknode* node, fz_matrix ctm);
fz_rect fz_boundblendnode(fz_blendnode* node, fz_matrix ctm);
fz_rect fz_boundsolidnode(fz_solidnode* node, fz_matrix ctm);
fz_rect fz_boundpathnode(fz_pathnode* node, fz_matrix ctm);
fz_rect fz_boundtextnode(fz_textnode* node, fz_matrix ctm);
fz_rect fz_boundimagenode(fz_imagenode* node, fz_matrix ctm);
fz_rect fz_boundshadenode(fz_shadenode* node, fz_matrix ctm);
fz_rect fz_boundlinknode(fz_linknode* node, fz_matrix ctm);

void
fz_initnode(fz_node *node, fz_nodekind kind)
{
	node->kind = kind;
	node->parent = nil;
	node->first = nil;
	node->last = nil;
	node->next = nil;
}

void
fz_dropnode(fz_node *node)
{
	fz_node *next;

	while (node)
	{
		if (node->first)
			fz_dropnode(node->first);

		switch (node->kind)
		{
		case FZ_NTRANSFORM:
		case FZ_NOVER:
		case FZ_NMASK:
		case FZ_NBLEND:
			break;
		case FZ_NCOLOR:
			fz_dropsolidnode((fz_solidnode *) node);
			break;
		case FZ_NPATH:
			fz_droppathnode((fz_pathnode *) node);
			break;
		case FZ_NTEXT:
			fz_droptextnode((fz_textnode *) node);
			break;
		case FZ_NIMAGE:
			fz_dropimagenode((fz_imagenode *) node);
			break;
		case FZ_NSHADE:
			fz_dropshadenode((fz_shadenode *) node);
			break;
		case FZ_NLINK:
			fz_droplinknode((fz_linknode *) node);
			break;
		}

		next = node->next;
		fz_free(node);
		node = next;
	}
}

fz_rect
fz_boundnode(fz_node *node, fz_matrix ctm)
{
	switch (node->kind)
	{
	case FZ_NTRANSFORM:
		return fz_boundtransformnode((fz_transformnode *) node, ctm);
	case FZ_NOVER:
		return fz_boundovernode((fz_overnode *) node, ctm);
	case FZ_NMASK:
		return fz_boundmasknode((fz_masknode *) node, ctm);
	case FZ_NBLEND:
		return fz_boundblendnode((fz_blendnode *) node, ctm);
	case FZ_NCOLOR:
		return fz_boundsolidnode((fz_solidnode *) node, ctm);
	case FZ_NPATH:
		return fz_boundpathnode((fz_pathnode *) node, ctm);
	case FZ_NTEXT:
		return fz_boundtextnode((fz_textnode *) node, ctm);
	case FZ_NIMAGE:
		return fz_boundimagenode((fz_imagenode *) node, ctm);
	case FZ_NSHADE:
		return fz_boundshadenode((fz_shadenode *) node, ctm);
	case FZ_NLINK:
		return fz_boundlinknode((fz_linknode *) node, ctm);
	}
	return fz_emptyrect;
}

int
fz_istransformnode(fz_node *node)
{
	return node ? node->kind == FZ_NTRANSFORM : 0;
}

int
fz_isovernode(fz_node *node)
{
	return node ? node->kind == FZ_NOVER : 0;
}

int
fz_ismasknode(fz_node *node)
{
	return node ? node->kind == FZ_NMASK : 0;
}

int
fz_isblendnode(fz_node *node)
{
	return node ? node->kind == FZ_NBLEND : 0;
}

int
fz_issolidnode(fz_node *node)
{
	return node ? node->kind == FZ_NCOLOR : 0;
}

int
fz_ispathnode(fz_node *node)
{
	return node ? node->kind == FZ_NPATH : 0;
}

int
fz_istextnode(fz_node *node)
{
	return node ? node->kind == FZ_NTEXT : 0;
}

int
fz_isimagenode(fz_node *node)
{
	return node ? node->kind == FZ_NIMAGE : 0;
}

int
fz_isshadenode(fz_node *node)
{
	return node ? node->kind == FZ_NSHADE : 0;
}

int
fz_islinknode(fz_node *node)
{
	return node ? node->kind == FZ_NLINK : 0;
}

