#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <assert.h>
#include <string.h>

#undef CHECK_SPLAY
#undef DUMP_SPLAY

/*
 * Allocate, destroy and simple parameters.
 */

void
pdf_drop_cmap_imp(fz_context *ctx, fz_storable *cmap_)
{
	pdf_cmap *cmap = (pdf_cmap *)cmap_;
	pdf_drop_cmap(ctx, cmap->usecmap);
	fz_free(ctx, cmap->ranges);
	fz_free(ctx, cmap->xranges);
	fz_free(ctx, cmap->mranges);
	fz_free(ctx, cmap->dict);
	fz_free(ctx, cmap->tree);
	fz_free(ctx, cmap);
}

pdf_cmap *
pdf_new_cmap(fz_context *ctx)
{
	pdf_cmap *cmap = fz_malloc_struct(ctx, pdf_cmap);
	FZ_INIT_STORABLE(cmap, 1, pdf_drop_cmap_imp);
	return cmap;
}

/* Could be a macro for speed */
pdf_cmap *
pdf_keep_cmap(fz_context *ctx, pdf_cmap *cmap)
{
	return fz_keep_storable(ctx, &cmap->storable);
}

/* Could be a macro for speed */
void
pdf_drop_cmap(fz_context *ctx, pdf_cmap *cmap)
{
	fz_drop_storable(ctx, &cmap->storable);
}

void
pdf_set_usecmap(fz_context *ctx, pdf_cmap *cmap, pdf_cmap *usecmap)
{
	int i;

	pdf_drop_cmap(ctx, cmap->usecmap);
	cmap->usecmap = pdf_keep_cmap(ctx, usecmap);

	if (cmap->codespace_len == 0)
	{
		cmap->codespace_len = usecmap->codespace_len;
		for (i = 0; i < usecmap->codespace_len; i++)
			cmap->codespace[i] = usecmap->codespace[i];
	}
}

int
pdf_cmap_wmode(fz_context *ctx, pdf_cmap *cmap)
{
	return cmap->wmode;
}

void
pdf_set_cmap_wmode(fz_context *ctx, pdf_cmap *cmap, int wmode)
{
	cmap->wmode = wmode;
}

/*
 * Add a codespacerange section.
 * These ranges are used by pdf_decode_cmap to decode
 * multi-byte encoded strings.
 */
void
pdf_add_codespace(fz_context *ctx, pdf_cmap *cmap, unsigned int low, unsigned int high, size_t n)
{
	if (cmap->codespace_len + 1 == nelem(cmap->codespace))
	{
		fz_warn(ctx, "assert: too many code space ranges");
		return;
	}

	if ((uint32_t)n != n)
	{
		fz_warn(ctx, "assert: code space range too large");
		return;
	}

	cmap->codespace[cmap->codespace_len].n = (int)n;
	cmap->codespace[cmap->codespace_len].low = low;
	cmap->codespace[cmap->codespace_len].high = high;
	cmap->codespace_len ++;
}

struct cmap_splay_s {
	unsigned int low;
	unsigned int high;
	unsigned int out;
	unsigned int left;
	unsigned int right;
	unsigned int parent : 31;
	unsigned int many : 1;
};

#define EMPTY ((unsigned int)0x40000000)

/*
	The splaying steps used:

	Case 1:	|              z              x
		|          y       D  =>  A       y
		|        x   C                  B   z
		|       A B                        C D

	Case 2:	|         z              x
		|     y       D  =>   y     z
		|   A   x            A B   C D
		|  B C

	Case 3:	|     y                 x
		|  x     C       =>   A     y
		| A B                      B C
*/

static void
move_to_root(cmap_splay *tree, unsigned int x)
{
	if (x == EMPTY)
		return;
	do
	{
		unsigned int z, zp;
		unsigned int y = tree[x].parent;
		if (y == EMPTY)
			break;
		z = tree[y].parent;
		if (z == EMPTY)
		{
			/* Case 3 */
			tree[x].parent = EMPTY;
			tree[y].parent = x;
			if (tree[y].left == x)
			{
				/* Case 3 */
				tree[y].left = tree[x].right;
				if (tree[y].left != EMPTY)
					tree[tree[y].left].parent = y;
				tree[x].right = y;
			}
			else
			{
				/* Case 3 - reflected */
				assert(tree[y].right == x);
				tree[y].right = tree[x].left;
				if (tree[y].right != EMPTY)
					tree[tree[y].right].parent = y;
				tree[x].left = y;
			}
			break;
		}

		zp = tree[z].parent;
		tree[x].parent = zp;
		if (zp != EMPTY) {
			if (tree[zp].left == z)
				tree[zp].left = x;
			else
			{
				assert(tree[zp].right == z);
				tree[zp].right = x;
			}
		}
		tree[y].parent = x;
		if (tree[y].left == x)
		{
			tree[y].left = tree[x].right;
			if (tree[y].left != EMPTY)
				tree[tree[y].left].parent = y;
			tree[x].right = y;
			if (tree[z].left == y)
			{
				/* Case 1 */
				tree[z].parent = y;
				tree[z].left = tree[y].right;
				if (tree[z].left != EMPTY)
					tree[tree[z].left].parent = z;
				tree[y].right = z;
			}
			else
			{
				/* Case 2 - reflected */
				assert(tree[z].right == y);
				tree[z].parent = x;
				tree[z].right = tree[x].left;
				if (tree[z].right != EMPTY)
					tree[tree[z].right].parent = z;
				tree[x].left = z;
			}
		}
		else
		{
			assert(tree[y].right == x);
			tree[y].right = tree[x].left;
			if (tree[y].right != EMPTY)
				tree[tree[y].right].parent = y;
			tree[x].left = y;
			if (tree[z].left == y)
			{
				/* Case 2 */
				tree[z].parent = x;
				tree[z].left = tree[x].right;
				if (tree[z].left != EMPTY)
					tree[tree[z].left].parent = z;
				tree[x].right = z;
			}
			else
			{
				/* Case 1 - reflected */
				assert(tree[z].right == y);
				tree[z].parent = y;
				tree[z].right = tree[y].left;
				if (tree[z].right != EMPTY)
					tree[tree[z].right].parent = z;
				tree[y].left = z;
			}
		}
	} while (1);
}

static unsigned int delete_node(pdf_cmap *cmap, unsigned int current)
{
	cmap_splay *tree = cmap->tree;
	unsigned int parent;
	unsigned int replacement;

	assert(current != EMPTY);

	parent = tree[current].parent;
	if (tree[current].right == EMPTY)
	{
		if (parent == EMPTY)
		{
			replacement = cmap->ttop = tree[current].left;
		}
		else if (tree[parent].left == current)
		{
			replacement = tree[parent].left = tree[current].left;
		}
		else
		{
			assert(tree[parent].right == current);
			replacement = tree[parent].right = tree[current].left;
		}
		if (replacement != EMPTY)
			tree[replacement].parent = parent;
		else
			replacement = parent;
	}
	else if (tree[current].left == EMPTY)
	{
		if (parent == EMPTY)
		{
			replacement = cmap->ttop = tree[current].right;
		}
		else if (tree[parent].left == current)
		{
			replacement = tree[parent].left = tree[current].right;
		}
		else
		{
			assert(tree[parent].right == current);
			replacement = tree[parent].right = tree[current].right;
		}
		if (replacement != EMPTY)
			tree[replacement].parent = parent;
		else
			replacement = parent;
	}
	else
	{
		/* Hard case, find the in-order predecessor of current */
		unsigned int amputee = current;
		replacement = tree[current].left;
		while (tree[replacement].right != EMPTY) {
			amputee = replacement;
			replacement = tree[replacement].right;
		}
		/* Remove replacement from the tree */
		if (amputee == current)
		{
			tree[amputee].left = tree[replacement].left;
			if (tree[amputee].left != EMPTY)
				tree[tree[amputee].left].parent = amputee;
		}
		else
		{
			tree[amputee].right = tree[replacement].left;
			if (tree[amputee].right != EMPTY)
				tree[tree[amputee].right].parent = amputee;
		}
		/* Insert replacement in place of current */
		tree[replacement].parent = parent;
		if (parent == EMPTY)
		{
			tree[replacement].parent = EMPTY;
			cmap->ttop = replacement;
		}
		else if (tree[parent].left == current)
			tree[parent].left = replacement;
		else
		{
			assert(tree[parent].right == current);
			tree[parent].right = replacement;
		}
		tree[replacement].left = tree[current].left;
		if (tree[replacement].left != EMPTY)
			tree[tree[replacement].left].parent = replacement;
		tree[replacement].right = tree[current].right;
		if (tree[replacement].right != EMPTY)
			tree[tree[replacement].right].parent = replacement;
	}

	/* current is now unlinked. We need to remove it from our array. */
	cmap->tlen--;
	if (current != (unsigned int) cmap->tlen)
	{
		if (replacement == (unsigned int) cmap->tlen)
			replacement = current;
		tree[current] = tree[cmap->tlen];
		parent = tree[current].parent;
		if (parent == EMPTY)
			cmap->ttop = current;
		else if (tree[parent].left == (unsigned int) cmap->tlen)
			tree[parent].left = current;
		else
		{
			assert(tree[parent].right == (unsigned int) cmap->tlen);
			tree[parent].right = current;
		}
		if (tree[current].left != EMPTY)
		{
			assert(tree[tree[current].left].parent == (unsigned int) cmap->tlen);
			tree[tree[current].left].parent = current;
		}
		if (tree[current].right != EMPTY)
		{
			assert(tree[tree[current].right].parent == (unsigned int) cmap->tlen);
			tree[tree[current].right].parent = current;
		}
	}

	/* Return the node that we should continue searching from */
	return replacement;
}

#ifdef DUMP_SPLAY
static void
dump_splay(cmap_splay *tree, unsigned int node, int depth, const char *pre)
{
	int i;

	if (tree == NULL || node == EMPTY)
		return;

	for (i = 0; i < depth; i++)
		fprintf(stderr, " ");
	fprintf(stderr, "%s%d:", pre, node);
	if (tree[node].parent == EMPTY)
		fprintf(stderr, "^EMPTY");
	else
		fprintf(stderr, "^%d", tree[node].parent);
	if (tree[node].left == EMPTY)
		fprintf(stderr, "<EMPTY");
	else
		fprintf(stderr, "<%d", tree[node].left);
	if (tree[node].right == EMPTY)
		fprintf(stderr, ">EMPTY");
	else
		fprintf(stderr, ">%d", tree[node].right);
	fprintf(stderr, "(%x,%x,%x,%d)\n", tree[node].low, tree[node].high, tree[node].out, tree[node].many);
	assert(tree[node].parent == EMPTY || depth);
	assert(tree[node].left == EMPTY || tree[tree[node].left].parent == node);
	assert(tree[node].right == EMPTY || tree[tree[node].right].parent == node);
	dump_splay(tree, tree[node].left, depth+1, "L");
	dump_splay(tree, tree[node].right, depth+1, "R");
}
#endif

enum
{
	TOP = 0,
	LEFT = 1,
	RIGHT = 2
};

static void walk_splay(cmap_splay *tree, unsigned int node, void (*fn)(cmap_splay *, void *), void *arg)
{
	int from = TOP;

	while (node != EMPTY)
	{
		switch (from)
		{
		case TOP:
			if (tree[node].left != EMPTY)
			{
				node = tree[node].left;
				from = TOP;
				break;
			}
			/* fallthrough */
		case LEFT:
			fn(&tree[node], arg);
			if (tree[node].right != EMPTY)
			{
				node = tree[node].right;
				from = TOP;
				break;
			}
			/* fallthrough */
		case RIGHT:
			{
				unsigned int parent = tree[node].parent;
				if (parent == EMPTY)
					return;
				if (tree[parent].left == node)
					from = LEFT;
				else
				{
					assert(tree[parent].right == node);
					from = RIGHT;
				}
				node = parent;
			}
		}
	}
}

#ifdef CHECK_SPLAY

static int
tree_has_overlap(cmap_splay *tree, int node, int low, int high)
{
	if (tree[node].left != EMPTY)
		if (tree_has_overlap(tree, tree[node].left, low, high))
			return 1;
	if (tree[node].right != EMPTY)
		if (tree_has_overlap(tree, tree[node].right, low, high))
			return 1;
	return (tree[node].low < low && low < tree[node].high) || (tree[node].low < high && high < tree[node].high);
}

static void
do_check(cmap_splay *node, void *arg)
{
	cmap_splay *tree = arg;
	unsigned int num = node - tree;
	assert(!node->many || node->low == node->high);
	assert(node->low <= node->high);
	assert((node->left == EMPTY) || (tree[node->left].parent == num &&
		tree[node->left].high < node->low));
	assert(node->right == EMPTY || (tree[node->right].parent == num &&
		node->high < tree[node->right].low));
	assert(!tree_has_overlap(tree, num, node->low, node->high));
}

static void
check_splay(cmap_splay *tree, unsigned int node, int depth)
{
	if (node == EMPTY)
		return;
	assert(tree[node].parent == EMPTY);
	walk_splay(tree, node, do_check, tree);
}
#endif

/*
 * Add a range.
 */
static void
add_range(fz_context *ctx, pdf_cmap *cmap, unsigned int low, unsigned int high, unsigned int out, int check_for_overlap, int many)
{
	int current;
	cmap_splay *tree;
	int i;
	int inrange = 0;
	unsigned int k, count;

	if (low > high)
	{
		fz_warn(ctx, "range limits out of range in cmap %s", cmap->cmap_name);
		return;
	}

	count = high - low + 1;
	for (k = 0; k < count; k++) {
		unsigned int c = low + k;

		inrange = 0;
		for (i = 0; i < cmap->codespace_len; i++) {
			if (cmap->codespace[i].low <= c && c <= cmap->codespace[i].high)
				inrange = 1;
		}
		if (!inrange)
		{
			fz_warn(ctx, "ignoring CMap range (%u-%u) that is outside of the codespace", low, high);
			return;
		}
	}

	tree = cmap->tree;

	if (cmap->tlen)
	{
		unsigned int move = cmap->ttop;
		unsigned int gt = EMPTY;
		unsigned int lt = EMPTY;
		if (check_for_overlap)
		{
			/* Check for collision with the current node */
			do
			{
				current = move;
				/* Cases we might meet:
				 * tree[i]:        <----->
				 * case 0:     <->
				 * case 1:     <------->
				 * case 2:     <------------->
				 * case 3:           <->
				 * case 4:           <------->
				 * case 5:                 <->
				 */
				if (low <= tree[current].low && tree[current].low <= high)
				{
					/* case 1, reduces to case 0 */
					/* or case 2, deleting the node */
					tree[current].out += high + 1 - tree[current].low;
					tree[current].low = high + 1;
					if (tree[current].low > tree[current].high)
					{
						/* update lt/gt references that will be moved/stale after deleting current */
						if (gt == (unsigned int) cmap->tlen - 1)
							gt = current;
						if (lt == (unsigned int) cmap->tlen - 1)
							lt = current;
						/* delete_node() moves the element at cmap->tlen-1 into current */
						move = delete_node(cmap, current);
						current = EMPTY;
						continue;
					}
				}
				else if (low <= tree[current].high && tree[current].high <= high)
				{
					/* case 4, reduces to case 5 */
					tree[current].high = low - 1;
					assert(tree[current].low <= tree[current].high);
				}
				else if (tree[current].low < low && high < tree[current].high)
				{
					/* case 3, reduces to case 5 */
					int new_high = tree[current].high;
					tree[current].high = low-1;
					add_range(ctx, cmap, high+1, new_high, tree[current].out + high + 1 - tree[current].low, 0, tree[current].many);
					tree = cmap->tree;
				}
				/* Now look for where to move to next (left for case 0, right for case 5) */
				if (tree[current].low > high) {
					move = tree[current].left;
					gt = current;
				}
				else
				{
					move = tree[current].right;
					lt = current;
				}
			}
			while (move != EMPTY);
		}
		else
		{
			do
			{
				current = move;
				if (tree[current].low > high)
				{
					move = tree[current].left;
					gt = current;
				}
				else
				{
					move = tree[current].right;
					lt = current;
				}
			} while (move != EMPTY);
		}
		/* current is now the node to which we would be adding the new node */
		/* lt is the last node we traversed which is lt the new node. */
		/* gt is the last node we traversed which is gt the new node. */

		if (!many)
		{
			/* Check for the 'merge' cases. */
			if (lt != EMPTY && !tree[lt].many && tree[lt].high == low-1 && tree[lt].out - tree[lt].low == out - low)
			{
				tree[lt].high = high;
				if (gt != EMPTY && !tree[gt].many && tree[gt].low == high+1 && tree[gt].out - tree[gt].low == out - low)
				{
					tree[lt].high = tree[gt].high;
					delete_node(cmap, gt);
				}
				goto exit;
			}
			if (gt != EMPTY && !tree[gt].many && tree[gt].low == high+1 && tree[gt].out - tree[gt].low == out - low)
			{
				tree[gt].low = low;
				tree[gt].out = out;
				goto exit;
			}
		}
	}
	else
		current = EMPTY;

	if (cmap->tlen == cmap->tcap)
	{
		int new_cap = cmap->tcap ? cmap->tcap * 2 : 256;
		tree = cmap->tree = fz_realloc_array(ctx, cmap->tree, new_cap, cmap_splay);
		cmap->tcap = new_cap;
	}
	tree[cmap->tlen].low = low;
	tree[cmap->tlen].high = high;
	tree[cmap->tlen].out = out;
	tree[cmap->tlen].parent = current;
	tree[cmap->tlen].left = EMPTY;
	tree[cmap->tlen].right = EMPTY;
	tree[cmap->tlen].many = many;
	cmap->tlen++;
	if (current == EMPTY)
		cmap->ttop = 0;
	else if (tree[current].low > high)
		tree[current].left = cmap->tlen-1;
	else
	{
		assert(tree[current].high < low);
		tree[current].right = cmap->tlen-1;
	}
	move_to_root(tree, cmap->tlen-1);
	cmap->ttop = cmap->tlen-1;
exit:
	{}
#ifdef CHECK_SPLAY
	check_splay(cmap->tree, cmap->ttop, 0);
#endif
#ifdef DUMP_SPLAY
	dump_splay(cmap->tree, cmap->ttop, 0, "");
#endif
}

/*
 * Add a one-to-many mapping.
 */
static void
add_mrange(fz_context *ctx, pdf_cmap *cmap, unsigned int low, int *out, int len)
{
	int out_pos;

	if (cmap->dlen + len + 1 > cmap->dcap)
	{
		int new_cap = cmap->dcap ? cmap->dcap * 2 : 256;
		cmap->dict = fz_realloc_array(ctx, cmap->dict, new_cap, int);
		cmap->dcap = new_cap;
	}
	out_pos = cmap->dlen;
	cmap->dict[out_pos] = len;
	memcpy(&cmap->dict[out_pos+1], out, sizeof(int)*len);
	cmap->dlen += len + 1;

	add_range(ctx, cmap, low, low, out_pos, 1, 1);
}

/*
 * Add a range of contiguous one-to-one mappings (ie 1..5 maps to 21..25)
 */
void
pdf_map_range_to_range(fz_context *ctx, pdf_cmap *cmap, unsigned int low, unsigned int high, int out)
{
	add_range(ctx, cmap, low, high, out, 1, 0);
}

/*
 * Add a single one-to-many mapping.
 */
void
pdf_map_one_to_many(fz_context *ctx, pdf_cmap *cmap, unsigned int low, int *values, size_t len)
{
	if (len == 1)
	{
		add_range(ctx, cmap, low, low, values[0], 1, 0);
		return;
	}

	/* Decode unicode surrogate pairs. */
	/* Only the *-UCS2 CMaps use one-to-many mappings, so assuming unicode should be safe. */
	if (len == 2 &&
		values[0] >= 0xD800 && values[0] <= 0xDBFF &&
		values[1] >= 0xDC00 && values[1] <= 0xDFFF)
	{
		int rune = ((values[0] - 0xD800) << 10) + (values[1] - 0xDC00) + 0x10000;
		add_range(ctx, cmap, low, low, rune, 1, 0);
		return;
	}

	if (len > PDF_MRANGE_CAP)
	{
		fz_warn(ctx, "ignoring one to many mapping in cmap %s", cmap->cmap_name);
		return;
	}

	add_mrange(ctx, cmap, low, values, (int)len);
}

static void
count_node_types(cmap_splay *node, void *arg)
{
	int *counts = (int *)arg;

	if (node->many)
		counts[2]++;
	else if (node->low <= 0xffff && node->high <= 0xFFFF && node->out <= 0xFFFF)
		counts[0]++;
	else
		counts[1]++;
}

static void
copy_node_types(cmap_splay *node, void *arg)
{
	pdf_cmap *cmap = (pdf_cmap *)arg;

	if (node->many)
	{
		assert(node->low == node->high);
		cmap->mranges[cmap->mlen].low = node->low;
		cmap->mranges[cmap->mlen].out = node->out;
		cmap->mlen++;
	}
	else if (node->low <= 0xffff && node->high <= 0xFFFF && node->out <= 0xFFFF)
	{
		cmap->ranges[cmap->rlen].low = node->low;
		cmap->ranges[cmap->rlen].high = node->high;
		cmap->ranges[cmap->rlen].out = node->out;
		cmap->rlen++;
	}
	else
	{
		cmap->xranges[cmap->xlen].low = node->low;
		cmap->xranges[cmap->xlen].high = node->high;
		cmap->xranges[cmap->xlen].out = node->out;
		cmap->xlen++;
	}
}

void
pdf_sort_cmap(fz_context *ctx, pdf_cmap *cmap)
{
	int counts[3];

	if (cmap->tree == NULL)
		return;

	counts[0] = 0;
	counts[1] = 0;
	counts[2] = 0;
	walk_splay(cmap->tree, cmap->ttop, count_node_types, &counts);

	cmap->ranges = Memento_label(fz_malloc_array(ctx, counts[0], pdf_range), "cmap_range");
	cmap->rcap = counts[0];
	cmap->xranges = Memento_label(fz_malloc_array(ctx, counts[1], pdf_xrange), "cmap_xrange");
	cmap->xcap = counts[1];
	cmap->mranges = Memento_label(fz_malloc_array(ctx, counts[2], pdf_mrange), "cmap_mrange");
	cmap->mcap = counts[2];

	walk_splay(cmap->tree, cmap->ttop, copy_node_types, cmap);

	fz_free(ctx, cmap->tree);
	cmap->tree = NULL;
}

/*
 * Lookup the mapping of a codepoint.
 */
int
pdf_lookup_cmap(pdf_cmap *cmap, unsigned int cpt)
{
	pdf_range *ranges = cmap->ranges;
	pdf_xrange *xranges = cmap->xranges;
	int l, r, m;

	l = 0;
	r = cmap->rlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < ranges[m].low)
			r = m - 1;
		else if (cpt > ranges[m].high)
			l = m + 1;
		else
			return cpt - ranges[m].low + ranges[m].out;
	}

	l = 0;
	r = cmap->xlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < xranges[m].low)
			r = m - 1;
		else if (cpt > xranges[m].high)
			l = m + 1;
		else
			return cpt - xranges[m].low + xranges[m].out;
	}

	if (cmap->usecmap)
		return pdf_lookup_cmap(cmap->usecmap, cpt);

	return -1;
}

int
pdf_lookup_cmap_full(pdf_cmap *cmap, unsigned int cpt, int *out)
{
	pdf_range *ranges = cmap->ranges;
	pdf_xrange *xranges = cmap->xranges;
	pdf_mrange *mranges = cmap->mranges;
	unsigned int i;
	int l, r, m;

	l = 0;
	r = cmap->rlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < ranges[m].low)
			r = m - 1;
		else if (cpt > ranges[m].high)
			l = m + 1;
		else
		{
			out[0] = cpt - ranges[m].low + ranges[m].out;
			return 1;
		}
	}

	l = 0;
	r = cmap->xlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < xranges[m].low)
			r = m - 1;
		else if (cpt > xranges[m].high)
			l = m + 1;
		else
		{
			out[0] = cpt - xranges[m].low + xranges[m].out;
			return 1;
		}
	}

	l = 0;
	r = cmap->mlen - 1;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cpt < mranges[m].low)
			r = m - 1;
		else if (cpt > mranges[m].low)
			l = m + 1;
		else
		{
			int *ptr = &cmap->dict[cmap->mranges[m].out];
			unsigned int len = (unsigned int)*ptr++;
			for (i = 0; i < len; ++i)
				out[i] = *ptr++;
			return len;
		}
	}

	if (cmap->usecmap)
		return pdf_lookup_cmap_full(cmap->usecmap, cpt, out);

	return 0;
}

/*
 * Use the codespace ranges to extract a codepoint from a
 * multi-byte encoded string.
 */
int
pdf_decode_cmap(pdf_cmap *cmap, unsigned char *buf, unsigned char *end, unsigned int *cpt)
{
	unsigned int c;
	int k, n;
	int len = end - buf;

	if (len > 4)
		len = 4;

	c = 0;
	for (n = 0; n < len; n++)
	{
		c = (c << 8) | buf[n];
		for (k = 0; k < cmap->codespace_len; k++)
		{
			if (cmap->codespace[k].n == n + 1)
			{
				if (c >= cmap->codespace[k].low && c <= cmap->codespace[k].high)
				{
					*cpt = c;
					return n + 1;
				}
			}
		}
	}

	*cpt = 0;
	return 1;
}
