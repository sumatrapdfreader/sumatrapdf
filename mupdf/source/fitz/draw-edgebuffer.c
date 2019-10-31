#include "mupdf/fitz.h"
#include "draw-imp.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#undef DEBUG_SCAN_CONVERTER

/* Define ourselves a 'fixed' type for clarity */
typedef int fixed;

#define fixed_shift 8
#define float2fixed(x) ((int)((x)*(1<<fixed_shift)))
#define fixed2int(x) ((int)((x)>>fixed_shift))
#define fixed_half (1<<(fixed_shift-1))
#define fixed_1 (1<<fixed_shift)
#define int2fixed(x) ((x)<<fixed_shift)

enum
{
	DIRN_UNSET = -1,
	DIRN_UP = 0,
	DIRN_DOWN = 1
};

typedef struct
{
	fixed left;
	fixed right;
	fixed y;
	signed char d; /* 0 up (or horiz), 1 down, -1 uninited */

	/* unset == 1, iff the values in the above are unset */
	unsigned char unset;
	/* can_save == 1, iff we are eligible to 'save'. i.e. if we
	 * have not yet output a cursor, and have not detected
	 * any line segments completely out of range. */
	unsigned char can_save;
	unsigned char saved;

	fixed save_left;
	fixed save_right;
	int save_iy;
	int save_d;
}
cursor_t;

typedef struct fz_edgebuffer_s
{
	fz_rasterizer super;
	int app;
	int sorted;
	int n;
	int index_cap;
	int *index;
	int table_cap;
	int *table;

	/* cursor section, for use with any part of pixel mode */
	cursor_t cursor[3];
} fz_edgebuffer;

static fz_rasterizer_insert_fn fz_insert_edgebuffer_app;
static fz_rasterizer_insert_fn fz_insert_edgebuffer;

#ifdef DEBUG_SCAN_CONVERTER
int debugging_scan_converter = 1;

static void
fz_edgebuffer_print(fz_context *ctx, fz_output *out, fz_edgebuffer * edgebuffer)
{
	int i;
	int height = edgebuffer->super.clip.y1 - edgebuffer->super.clip.y0;

	fz_write_printf(ctx, out, "Edgebuffer %x\n", edgebuffer);
	fz_write_printf(ctx, out, "xmin=%x xmax=%x base=%x height=%x\n",
			  edgebuffer->super.clip.x0, edgebuffer->super.clip.x1, edgebuffer->super.clip.y0, height);
	for (i=0; i < height; i++) {
		int  offset = edgebuffer->index[i];
		int *row = &edgebuffer->table[offset];
		int count = *row++;
		assert ((count & 1) == 0);
		fz_write_printf(ctx, out, "%x @ %x: %d =", i, offset, count);
		while (count-- > 0) {
			int v = *row++;
			fz_write_printf(ctx, out, " %x:%d", v&~1, v&1);
		}
		fz_write_printf(ctx, out, "\n");
	}
}

static void
fz_edgebuffer_print_app(fz_context *ctx, fz_output *out, fz_edgebuffer * edgebuffer)
{
	int i;
	int height = edgebuffer->super.clip.y1 - edgebuffer->super.clip.y0;

	fz_write_printf(ctx, out, "Edgebuffer %x\n", edgebuffer);
	fz_write_printf(ctx, out, "xmin=%x xmax=%x base=%x height=%x\n",
			  edgebuffer->super.clip.x0, edgebuffer->super.clip.x1, edgebuffer->super.clip.y0, height);
	if (edgebuffer->table == NULL)
		return;
	for (i=0; i < height; i++) {
		int  offset = edgebuffer->index[i];
		int *row = &edgebuffer->table[offset];
		int count = *row++;
		int count0 = count;
		fz_write_printf(ctx, out, "%x @ %x: %d =", i, offset, count);
		while (count-- > 0) {
			int l = *row++;
			int r = *row++;
			fz_write_printf(ctx, out, " %x:%x", l, r);
		}
		assert((count0 & 1) == 0); (void)count0;
		fz_write_printf(ctx, out, "\n");
	}
}
#endif

static void fz_drop_edgebuffer(fz_context *ctx, fz_rasterizer *r)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)r;

	if (eb)
	{
		fz_free(ctx, eb->index);
		fz_free(ctx, eb->table);
	}
	fz_free(ctx, eb);
}

static void index_edgebuffer_insert(fz_context *ctx, fz_rasterizer *ras, float fsx, float fsy, float fex, float fey, int rev)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)ras;
	int iminy, imaxy;
	int height = eb->super.clip.y1 - eb->super.clip.y0;

	if (fsy == fey)
		return;

	if (fsx < fex)
	{
		if (fsx < eb->super.bbox.x0) eb->super.bbox.x0 = fsx;
		if (fex > eb->super.bbox.x1) eb->super.bbox.x1 = fex;
	}
	else
	{
		if (fsx > eb->super.bbox.x1) eb->super.bbox.x1 = fsx;
		if (fex < eb->super.bbox.x0) eb->super.bbox.x0 = fex;
	}
	if (fsy < fey)
	{
		if (fsy < eb->super.bbox.y0) eb->super.bbox.y0 = fsy;
		if (fey > eb->super.bbox.y1) eb->super.bbox.y1 = fey;
	}
	else
	{
		if (fey < eb->super.bbox.y0) eb->super.bbox.y0 = fey;
		if (fsy > eb->super.bbox.y1) eb->super.bbox.y1 = fsy;
	}

	/* To strictly match, this should be:
	 * iminy = int2fixed(float2fixed(fsy))
	 * imaxy = int2fixed(float2fixed(fsx))
	 * but this is faster. It can round differently,
	 * (on some machines at least) hence the iminy--; below.
	 */
	iminy = (int)fsy;
	imaxy = (int)fey;

	if (iminy > imaxy)
	{
		int t;
		t = iminy; iminy = imaxy; imaxy = t;
	}
	imaxy++;
	iminy--;

	imaxy -= eb->super.clip.y0;
	if (imaxy < 0)
		return;
	iminy -= eb->super.clip.y0;
	if (iminy < 0)
		iminy = 0;
	else if (iminy > height)
		return;
	if (imaxy > height-1)
		imaxy = height-1;
#ifdef DEBUG_SCAN_CONVERTER
	if (debugging_scan_converter)
		fprintf(stderr, "%x->%x:%d\n", iminy, imaxy, eb->n);
#endif
	eb->index[iminy] += eb->n;
	eb->index[imaxy+1] -= eb->n;
}

static void fz_postindex_edgebuffer(fz_context *ctx, fz_rasterizer *r)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)r;
	int height = eb->super.clip.y1 - eb->super.clip.y0 + 1;
	int n = eb->n;
	int total = 0;
	int delta = 0;
	int i;

	eb->super.fns.insert = (eb->app ? fz_insert_edgebuffer_app : fz_insert_edgebuffer);

	for (i = 0; i < height; i++)
	{
		delta += eb->index[i];
		eb->index[i] = total;
		total += 1 + delta*n;
	}
	assert(delta == 0);

	if (eb->table_cap < total)
	{
		eb->table = fz_realloc_array(ctx, eb->table, total, int);
		eb->table_cap = total;
	}

	for (i = 0; i < height; i++)
	{
		eb->table[eb->index[i]] = 0;
	}
}

static int fz_reset_edgebuffer(fz_context *ctx, fz_rasterizer *r)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)r;
	int height = eb->super.clip.y1 - eb->super.clip.y0 + 1;
	int n;

	eb->sorted = 0;

	if (eb->index_cap < height)
	{
		eb->index = fz_realloc_array(ctx, eb->index, height, int);
		eb->index_cap = height;
	}
	memset(eb->index, 0, sizeof(int) * height);

	n = 1;

	if (eb->app)
	{
		n = 2;
		eb->cursor[0].saved = 0;
		eb->cursor[0].unset = 1;
		eb->cursor[0].can_save = 1;
		eb->cursor[0].d = DIRN_UNSET;
		eb->cursor[1].saved = 0;
		eb->cursor[1].unset = 1;
		eb->cursor[1].can_save = 1;
		eb->cursor[1].d = DIRN_UNSET;
		eb->cursor[2].saved = 0;
		eb->cursor[2].unset = 1;
		eb->cursor[2].can_save = 1;
		eb->cursor[2].d = DIRN_UNSET;
	}

	eb->n = n;

	eb->super.fns.insert = index_edgebuffer_insert;
	return 1;
}

static void mark_line(fz_context *ctx, fz_edgebuffer *eb, fixed sx, fixed sy, fixed ex, fixed ey)
{
	int base_y = eb->super.clip.y0;
	int height = eb->super.clip.y1 - eb->super.clip.y0;
	int *table = eb->table;
	int *index = eb->index;
	int delta;
	int iy, ih;
	fixed clip_sy, clip_ey;
	int dirn = DIRN_UP;
	int *row;

	if (fixed2int(sy + fixed_half-1) == fixed2int(ey + fixed_half-1))
		return;
	if (sy > ey) {
		int t;
		t = sy; sy = ey; ey = t;
		t = sx; sx = ex; ex = t;
		dirn = DIRN_DOWN;
	}

	if (fixed2int(sx) < eb->super.bbox.x0)
		eb->super.bbox.x0 = fixed2int(sx);
	if (fixed2int(sx + fixed_1 - 1) > eb->super.bbox.x1)
		eb->super.bbox.x1 = fixed2int(sx + fixed_1 - 1);
	if (fixed2int(ex) < eb->super.bbox.x0)
		eb->super.bbox.x0 = fixed2int(ex);
	if (fixed2int(ex + fixed_1 - 1) > eb->super.bbox.x1)
		eb->super.bbox.x1 = fixed2int(ex + fixed_1 - 1);

	if (fixed2int(sy) < eb->super.bbox.y0)
		eb->super.bbox.y0 = fixed2int(sy);
	if (fixed2int(ey + fixed_1 - 1) > eb->super.bbox.y1)
		eb->super.bbox.y1 = fixed2int(ey + fixed_1 - 1);

	/* Lines go from sy to ey, closed at the start, open at the end. */
	/* We clip them to a region to make them closed at both ends. */
	/* Thus the unset scanline marked (>= sy) is: */
	clip_sy = ((sy + fixed_half - 1) & ~(fixed_1-1)) | fixed_half;
	/* The last scanline marked (< ey) is: */
	clip_ey = ((ey - fixed_half - 1) & ~(fixed_1-1)) | fixed_half;
	/* Now allow for banding */
	if (clip_sy < int2fixed(base_y) + fixed_half)
		clip_sy = int2fixed(base_y) + fixed_half;
	if (ey <= clip_sy)
		return;
	if (clip_ey > int2fixed(base_y + height - 1) + fixed_half)
		clip_ey = int2fixed(base_y + height - 1) + fixed_half;
	if (sy > clip_ey)
		return;
	delta = clip_sy - sy;
	if (delta > 0)
	{
		int dx = ex - sx;
		int dy = ey - sy;
		int advance = (int)(((int64_t)dx * delta + (dy>>1)) / dy);
		sx += advance;
		sy += delta;
	}
	ex -= sx;
	ey -= sy;
	clip_ey -= clip_sy;
	delta = ey - clip_ey;
	if (delta > 0)
	{
		int advance = (int)(((int64_t)ex * delta + (ey>>1)) / ey);
		ex -= advance;
		ey -= delta;
	}
	ih = fixed2int(ey);
	assert(ih >= 0);
	iy = fixed2int(sy) - base_y;
#ifdef DEBUG_SCAN_CONVERTER
	if (debugging_scan_converter)
		fz_write_printf(ctx, fz_stderr(ctx), "	iy=%x ih=%x\n", iy, ih);
#endif
	assert(iy >= 0 && iy < height);
	/* We always cross at least one scanline */
	row = &table[index[iy]];
	*row = (*row)+1; /* Increment the count */
	row[*row] = (sx&~1) | dirn;
	if (ih == 0)
		return;
	if (ex >= 0) {
		int x_inc, n_inc, f;

		/* We want to change sx by ex in ih steps. So each step, we add
		 * ex/ih to sx. That's x_inc + n_inc/ih.
		 */
		x_inc = ex/ih;
		n_inc = ex-(x_inc*ih);
		f	 = ih>>1;
		delta = ih;
		do {
			int count;
			iy++;
			sx += x_inc;
			f  -= n_inc;
			if (f < 0) {
				f += ih;
				sx++;
			}
			assert(iy >= 0 && iy < height);
			row = &table[index[iy]];
			count = *row = (*row)+1; /* Increment the count */
			row[count] = (sx&~1) | dirn;
		} while (--delta);
	} else {
		int x_dec, n_dec, f;

		ex = -ex;
		/* We want to change sx by ex in ih steps. So each step, we subtract
		 * ex/ih from sx. That's x_dec + n_dec/ih.
		 */
		x_dec = ex/ih;
		n_dec = ex-(x_dec*ih);
		f	 = ih>>1;
		delta = ih;
		do {
			int count;
			iy++;
			sx -= x_dec;
			f  -= n_dec;
			if (f < 0) {
				f += ih;
				sx--;
			}
			assert(iy >= 0 && iy < height);
			row = &table[index[iy]];
			count = *row = (*row)+1; /* Increment the count */
			row[count] = (sx&~1) | dirn;
		} while (--delta);
	}
}

static void fz_insert_edgebuffer(fz_context *ctx, fz_rasterizer *ras, float fsx, float fsy, float fex, float fey, int rev)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)ras;
	fixed sx = float2fixed(fsx);
	fixed sy = float2fixed(fsy);
	fixed ex = float2fixed(fex);
	fixed ey = float2fixed(fey);

	mark_line(ctx, eb, sx, sy, ex, ey);
}

static inline void
cursor_output(fz_edgebuffer * FZ_RESTRICT eb, int rev, int iy)
{
	int *row;
	int count;
	int height = eb->super.clip.y1 - eb->super.clip.y0;
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	rev &= 1; /* Edge label 0 is forwards, 1 and 2 are reverse */

	if (iy >= 0 && iy < height) {
		if (cr->can_save) {
			/* Save it for later in case we join up */
			cr->save_left = cr->left;
			cr->save_right = cr->right;
			cr->save_iy = iy;
			cr->save_d = cr->d;
			cr->saved = 1;
		} else {
			/* Enter it into the table */
			row = &eb->table[eb->index[iy]];
			if (cr->d == DIRN_UNSET)
			{
				/* Move 0 0; line 10 0; line 0 0; */
				/* FIXME */
			}
			else
			{
				*row = count = (*row)+1; /* Increment the count */
#ifdef DEBUG_SCAN_CONVERTER
				if (debugging_scan_converter)
					fprintf(stderr, "row: %x: %x->%x %c\n", iy, cr->left, cr->right, (cr->d^rev) == DIRN_UP ? '^' : (cr->d^rev) == DIRN_DOWN ? 'v' : '-');
#endif
				assert(count <= (eb->index[iy+1] - eb->index[iy] - 1)/2);
				row[2 * count - 1] = (cr->left&~1) | (cr->d ^ rev);
				row[2 * count] = cr->right;
			}
		}
	}
	cr->can_save = 0;
}

static inline void
cursor_output_inrange(fz_edgebuffer * FZ_RESTRICT eb, int rev, int iy)
{
	int *row;
	int count;
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	rev &= 1; /* Edge label 0 is forwards, 1 and 2 are reverse */

	assert(iy >= 0 && iy < eb->super.clip.y1 - eb->super.clip.y0);
	if (cr->can_save) {
		/* Save it for later in case we join up */
		cr->save_left = cr->left;
		cr->save_right = cr->right;
		cr->save_iy = iy;
		cr->save_d = cr->d;
		cr->saved = 1;
	} else {
		/* Enter it into the table */
		assert(cr->d != DIRN_UNSET);

		row = &eb->table[eb->index[iy]];
		*row = count = (*row)+1; /* Increment the count */
#ifdef DEBUG_SCAN_CONVERTER
			if (debugging_scan_converter)
				printf("row= %x: %x->%x %c\n", iy, cr->left, cr->right, (cr->d^rev) == DIRN_UP ? '^' : (cr->d^rev) == DIRN_DOWN ? 'v' : '-');
#endif
		row[2 * count - 1] = (cr->left&~1) | (cr->d ^ rev);
		row[2 * count] = cr->right;
	}
	cr->can_save = 0;
}

/* Step the cursor in y, allowing for maybe crossing a scanline */
static inline void
cursor_step(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed dy, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];
	int new_iy;
	int base = eb->super.clip.y0;
	int iy = fixed2int(cr->y) - base;

	cr->y += dy;
	new_iy = fixed2int(cr->y) - base;
	if (new_iy != iy) {
		cursor_output(eb, rev, iy);
		cr->left = x;
		cr->right = x;
	} else {
		if (x < cr->left)
			cr->left = x;
		if (x > cr->right)
			cr->right = x;
	}
}

/* Step the cursor in y, never by enough to cross a scanline. */
static inline void
cursor_never_step_vertical(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed dy, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

	cr->y += dy;
}

/* Step the cursor in y, never by enough to cross a scanline,
 * knowing that we are moving left, and that the right edge
 * has already been accounted for. */
static inline void
cursor_never_step_left(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed dy, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

	if (x < cr->left)
		cr->left = x;
	cr->y += dy;
}

/* Step the cursor in y, never by enough to cross a scanline,
 * knowing that we are moving right, and that the left edge
 * has already been accounted for. */
static inline void
cursor_never_step_right(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed dy, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	assert(fixed2int(cr->y+dy) == fixed2int(cr->y));

	if (x > cr->right)
		cr->right = x;
	cr->y += dy;
}

/* Step the cursor in y, always by enough to cross a scanline. */
static inline void
cursor_always_step(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed dy, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];
	int base = eb->super.clip.y0;
	int iy = fixed2int(cr->y) - base;

	cursor_output(eb, rev, iy);
	cr->y += dy;
	cr->left = x;
	cr->right = x;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a vertical line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_step_inrange_vertical(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed dy, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];
	int base = eb->super.clip.y0;
	int iy = fixed2int(cr->y) - base;

	cursor_output(eb, rev, iy);
	cr->y += dy;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a left moving line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_inrange_step_left(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed dy, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];
	int base = eb->super.clip.y0;
	int iy = fixed2int(cr->y) - base;

	cr->y += dy;
	cursor_output_inrange(eb, rev, iy);
	cr->right = x;
}

/* Step the cursor in y, always by enough to cross a scanline, as
 * part of a right moving line, knowing that we are moving from a
 * position guaranteed to be in the valid y range. */
static inline void
cursor_always_inrange_step_right(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed dy, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];
	int base = eb->super.clip.y0;
	int iy = fixed2int(cr->y) - base;

	cr->y += dy;
	cursor_output_inrange(eb, rev, iy);
	cr->left = x;
}

static inline void cursor_init(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed y, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	assert(y >= int2fixed(eb->super.clip.y0) && y <= int2fixed(eb->super.clip.y1));

	cr->y = y;
	cr->left = x;
	cr->right = x;
	cr->d = DIRN_UNSET;
}

static inline void cursor_left_merge(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	if (x < cr->left)
		cr->left = x;
}

static inline void cursor_left(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	cr->left = x;
}

static inline void cursor_right_merge(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	if (x > cr->right)
		cr->right = x;
}

static inline void cursor_right(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	cr->right = x;
}

static inline void cursor_down(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];
	int base = eb->super.clip.y0;

	if (cr->d == DIRN_UP)
	{
		cursor_output(eb, rev, fixed2int(cr->y) - base);
		cr->left = x;
		cr->right = x;
	}
	cr->d = DIRN_DOWN;
}

static inline void cursor_up(fz_edgebuffer * FZ_RESTRICT eb, int rev, fixed x)
{
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];
	int base = eb->super.clip.y0;

	if (cr->d == DIRN_DOWN)
	{
		cursor_output(eb, rev, fixed2int(cr->y) - base);
		cr->left = x;
		cr->right = x;
	}
	cr->d = DIRN_UP;
}

static inline int dirns_match(int d0, int d1)
{
	return d0 == d1 || d0 == DIRN_UNSET || d1 == DIRN_UNSET;
}

static inline int dirn_flip(int d)
{
	return d < 0 ? d : d^1;
}

static inline int dirns_merge(int d0, int d1)
{
	if (d0 == DIRN_UNSET)
		return d1;
	assert(dirns_match(d0, d1));
	return d0;
}

static void
cursor_flush(fz_edgebuffer * FZ_RESTRICT eb)
{
	int base = eb->super.clip.y0;
	int iy0, iy1, iy2;
	cursor_t * FZ_RESTRICT cr0 = &eb->cursor[0];
	cursor_t * FZ_RESTRICT cr1 = &eb->cursor[1];
	cursor_t * FZ_RESTRICT cr2 = &eb->cursor[2];

	if (cr0->unset)
	{
		assert(cr1->unset && cr2->unset);
		return;
	}

	iy0 = fixed2int(cr0->y) - base;
	iy1 = fixed2int(cr1->y) - base;
	if (!cr2->unset)
	{
		assert(!cr1->unset);
		iy2 = fixed2int(cr2->y) - base;
		/* Try to merge the end of cursor 0 with the end of cursor 1 */
		if (iy0 == iy1 && dirns_match(cr0->d, dirn_flip(cr1->d)))
		{
			/* Succeeded! Just one to output. */
			cr0->d = dirns_merge(cr0->d, dirn_flip(cr1->d));
			if (cr0->left > cr1->left)
				cr0->left = cr1->left;
			if (cr0->right < cr1->right)
				cr0->right = cr1->right;
			cr1->unset = 1; /* Stop us outputting cursor 1 later */
		}

		/* Try to merge the end of cursor 2 with the start of cursor 0 */
		if (cr0->saved)
		{
			if (cr0->save_iy == iy2 && dirns_match(cr0->save_d, cr2->d))
			{
				cr0->save_d = dirns_merge(cr0->save_d, cr2->d);
				if (cr0->save_left > cr2->left)
					cr0->save_left = cr2->left;
				if (cr0->save_right > cr2->right)
					cr0->save_right = cr2->right;
				cr2->unset = 1; /* Stop us outputting cursor 2 later */
			}
		}
		else
		{
			/* Maybe cursor 0 never moved from the original pixel */
			if (iy0 == iy2 && dirns_match(cr0->d, cr2->d))
			{
				cr0->d = dirns_merge(cr0->d, cr2->d);
				if (cr0->left > cr2->left)
					cr0->left = cr2->left;
				if (cr0->right > cr2->right)
					cr0->right = cr2->right;
				cr2->unset = 1; /* Stop us outputting cursor 2 later */
			}
		}

		/* Try to merge the start of cursor 2 with the start of cursor 1 */
		if (cr1->saved)
		{
			if (cr2->saved)
			{
				if (cr2->save_iy == cr1->save_iy && dirns_match(cr2->save_d, dirn_flip(cr1->save_d)))
				{
					cr2->save_d = dirns_merge(cr2->save_d, dirn_flip(cr1->save_d));
					if (cr2->save_left > cr1->save_left)
						cr2->save_left = cr1->save_left;
					if (cr2->save_right > cr1->save_right)
						cr2->save_right = cr1->save_right;
					cr1->saved = 0; /* Don't output cr1->saved again later */
				}
			}
			else if (!cr2->unset)
			{
				/* Maybe cursor 2 never moved from the original pixel */
				if (iy2 == cr1->save_iy && dirns_match(cr2->d, dirn_flip(cr1->save_d)))
				{
					cr2->d = dirns_merge(cr2->d, dirn_flip(cr1->save_d));
					if (cr2->left > cr1->save_left)
						cr2->left = cr1->save_left;
					if (cr2->right > cr1->save_right)
						cr2->right = cr1->save_right;
					cr1->saved = 0; /* Don't output cr1->saved again later */
				}
			}
		}
		else if (!cr1->unset)
		{
			/* Cursor 1 might not have moved from the original pixel, hence nothing saved */
			if (cr2->saved)
			{
				if (cr2->save_iy == iy1 && dirns_match(cr2->save_d, dirn_flip(cr1->d)))
				{
					cr2->save_d = dirns_merge(cr2->save_d, dirn_flip(cr1->d));
					if (cr2->save_left > cr1->left)
						cr2->save_left = cr1->left;
					if (cr2->save_right > cr1->right)
						cr2->save_right = cr1->right;
					cr1->unset = 1; /* Stop us outputting cursor 1 later */
				}
			}
			else if (!cr2->unset)
			{
				/* Maybe cursor 2 never moved from the original pixel */
				if (iy2 == iy1 && dirns_match(cr2->d, dirn_flip(cr1->d)))
				{
					cr2->d = dirns_merge(cr2->d, dirn_flip(cr1->d));
					if (cr2->left > cr1->left)
						cr2->left = cr1->left;
					if (cr2->right > cr1->right)
						cr2->right = cr1->right;
					cr1->unset = 1; /* Stop us outputting cursor 1 later */
				}
			}
		}
		else
		{
			/* Cursor 1 might not have moved from the original pixel, hence nothing saved,
			 * AND we might have merged it with cursor 0 already! */
			if (cr2->saved)
			{
				if (iy0 == cr2->save_iy && dirns_match(cr0->d, cr2->save_d))
				{
					cr0->d = dirns_merge(cr0->d, cr2->save_d);
					if (cr0->left > cr2->save_left)
						cr0->left = cr2->save_left;
					if (cr0->right > cr2->save_right)
						cr0->right = cr2->save_right;
					cr2->saved = 0; /* Stop us outputting saved cursor 2 later */
				}
			}
			else if (!cr2->unset)
			{
				/* Maybe cursor 2 never moved from the original pixel */
				if (iy0 == iy2 && dirns_match(cr0->d, cr2->d))
				{
					cr0->d = dirns_merge(cr0->d, cr2->d);
					if (cr0->left > cr2->left)
						cr0->left = cr2->left;
					if (cr0->right > cr2->right)
						cr0->right = cr2->right;
					cr2->unset = 1; /* Stop us outputting cursor 2 later */
				}
			}
		}
	}
	else
	{
		/* Try to merge the end of cursor 0 with the start of cursor 0 */
		if (cr0->saved)
		{
			if (iy0 == cr0->save_iy && dirns_match(cr0->d, cr0->save_d))
			{
				cr0->d = dirns_merge(cr0->d, cr0->save_d);
				if (cr0->left > cr0->save_left)
					cr0->left = cr0->save_left;
				if (cr0->right > cr0->save_right)
					cr0->right = cr0->save_right;
				cr0->saved = 0; /* Stop us outputting saved cursor 0 later */
			}
		}

		if (!cr1->unset)
		{
			/* Try to merge the end of cursor 1 with the start of cursor 1 */
			if (cr1->saved)
			{
				if (iy1 == cr1->save_iy && dirns_match(cr1->d, cr1->save_d))
				{
					cr1->d = dirns_merge(cr1->d, cr1->save_d);
					if (cr1->left > cr1->save_left)
						cr1->left = cr1->save_left;
					if (cr1->right > cr1->save_right)
						cr1->right = cr1->save_right;
					cr1->saved = 0; /* Stop us outputting saved cursor 1 later */
				}
			}
		}
	}

	if (!cr0->unset)
		cursor_output(eb, 0, iy0);
	if (cr0->saved)
	{
		cr0->left = cr0->save_left;
		cr0->right = cr0->save_right;
		cr0->d = cr0->save_d;
		cursor_output(eb, 0, cr0->save_iy);
	}
	if (!cr1->unset)
		cursor_output(eb, 1, iy1);
	if (cr1->saved)
	{
		cr1->left = cr1->save_left;
		cr1->right = cr1->save_right;
		cr1->d = cr1->save_d;
		cursor_output(eb, 1, cr1->save_iy);
	}
	if (!cr2->unset)
		cursor_output(eb, 2, iy2);
	if (cr2->saved)
	{
		cr2->left = cr2->save_left;
		cr2->right = cr2->save_right;
		cr2->d = cr2->save_d;
		cursor_output(eb, 2, cr2->save_iy);
	}
}

static void do_mark_line_app(fz_context *ctx, fz_edgebuffer *eb, fixed sx, fixed sy, fixed ex, fixed ey, int rev)
{
	int base_y = eb->super.clip.y0;
	int height = eb->super.clip.y1 - eb->super.clip.y0;
	int isy, iey;
	fixed y_steps;
	fixed save_sy = sy;
	fixed save_ex = ex;
	fixed save_ey = ey;
	int truncated;
	cursor_t * FZ_RESTRICT cr = &eb->cursor[rev];

	if (cr->unset)
		cr->y = sy, cr->left = sx, cr->right = sx, cr->unset = 0;

	/* Floating point inaccuracies can cause these not *quite* to be true. */
	assert(cr->y == sy && cr->left <= sx && cr->right >= sx && cr->d >= DIRN_UNSET && cr->d <= DIRN_DOWN);
	sy = cr->y;
	if (cr->left > sx)
		sx = cr->left;
	else if (cr->right < sx)
		sx = cr->right;

	if (sx == ex && sy == ey)
		return;

	isy = fixed2int(sy) - base_y;
	iey = fixed2int(ey) - base_y;
#ifdef DEBUG_SCAN_CONVERTER
	if (debugging_scan_converter)
		fz_write_printf(ctx, fz_stderr(ctx), "Marking line (app) from %x,%x to %x,%x (%x,%x) %d\n", sx, sy, ex, ey, isy, iey, rev);
#endif

	if (isy < iey) {
		/* Rising line */
		if (iey < 0 || isy >= height) {
			/* All line is outside. */
			cr->y = ey;
			cr->left = ex;
			cr->right = ex;
			cr->can_save = 0;
			return;
		}
		if (isy < 0) {
			/* Move sy up */
			int y = ey - sy;
			int new_sy = int2fixed(base_y);
			int dy = new_sy - sy;
			sx += (int)((((int64_t)(ex-sx))*dy + y/2)/y);
			sy = new_sy;
			cursor_init(eb, rev, sy, sx);
			isy = 0;
		}
		truncated = iey > height;
		if (truncated) {
			/* Move ey down */
			int y = ey - sy;
			int new_ey = int2fixed(base_y + height);
			int dy = ey - new_ey;
			save_ex = ex;
			save_ey = ey;
			ex -= (int)((((int64_t)(ex-sx))*dy + y/2)/y);
			ey = new_ey;
			iey = height;
		}
	} else {
		/* Falling line */
		if (isy < 0 || iey >= height) {
			/* All line is outside. */
			cr->y = ey;
			cr->left = ex;
			cr->right = ex;
			cr->can_save = 0;
			return;
		}
		truncated = iey < 0;
		if (truncated) {
			/* Move ey up */
			int y = ey - sy;
			int new_ey = int2fixed(base_y);
			int dy = ey - new_ey;
			ex -= (int)((((int64_t)(ex-sx))*dy + y/2)/y);
			ey = new_ey;
			iey = 0;
		}
		if (isy >= height) {
			/* Move sy down */
			int y = ey - sy;
			if (y) {
				int new_sy = int2fixed(base_y + height);
				int dy = new_sy - sy;
				sx += (int)((((int64_t)(ex-sx))*dy + y/2)/y);
				sy = new_sy;
				cursor_init(eb, rev, sy, sx);
				isy = height;
			}
		}
	}

	assert(cr->left <= sx);
	assert(cr->right >= sx);
	assert(cr->y == sy);

	/* A note: The code below used to be of the form:
	 *   if (isy == iey)   ... deal with horizontal lines
	 *   else if (ey > sy) {
	 *	 fixed y_steps = ey - sy;
	 *	  ... deal with rising lines ...
	 *   } else {
	 *	 fixed y_steps = ey - sy;
	 *	 ... deal with falling lines
	 *   }
	 * but that lead to problems, for instance, an example seen
	 * has sx=2aa8e, sy=8aee7, ex=7ffc1686, ey=8003e97a.
	 * Thus isy=84f, iey=ff80038a. We can see that ey < sy, but
	 * sy - ey < 0!
	 * We therefore rejig our code so that the choice between
	 * cases is done based on the sign of y_steps rather than
	 * the relative size of ey and sy.
	 */

	/* First, deal with lines that don't change scanline.
	 * This accommodates horizontal lines. */
	if (isy == iey) {
		if (save_sy == save_ey) {
			/* Horizontal line. Don't change cr->d, don't flush. */
		} else if (save_sy > save_ey) {
			/* Falling line, flush if previous was rising */
			cursor_down(eb, rev, sx);
		} else {
			/* Rising line, flush if previous was falling */
			cursor_up(eb, rev, sx);
		}
		if (sx <= ex) {
			cursor_left_merge(eb, rev, sx);
			cursor_right_merge(eb, rev, ex);
		} else {
			cursor_left_merge(eb, rev, ex);
			cursor_right_merge(eb, rev, sx);
		}
		cr->y = ey;
		if (sy > save_ey)
			goto endFalling;
	} else if ((y_steps = ey - sy) > 0) {
		/* We want to change from sy to ey, which are guaranteed to be on
		 * different scanlines. We do this in 3 phases.
		 * Phase 1 gets us from sy to the next scanline boundary.
		 * Phase 2 gets us all the way to the last scanline boundary.
		 * Phase 3 gets us from the last scanline boundary to ey.
		 */
		/* We want to change from sy to ey, which are guaranteed to be on
		 * different scanlines. We do this in 3 phases.
		 * Phase 1 gets us from sy to the next scanline boundary. (We may exit after phase 1).
		 * Phase 2 gets us all the way to the last scanline boundary. (This may be a null operation)
		 * Phase 3 gets us from the last scanline boundary to ey. (We are guaranteed to have output the cursor at least once before phase 3).
		 */
		int phase1_y_steps = (-sy) & (fixed_1 - 1);
		int phase3_y_steps = ey & (fixed_1 - 1);

		cursor_up(eb, rev, sx);

		if (sx == ex) {
			/* Vertical line. (Rising) */

			/* Phase 1: */
			cursor_left_merge(eb, rev, sx);
			cursor_right_merge(eb, rev, sx);
			if (phase1_y_steps) {
				/* If phase 1 will move us into a new scanline, then we must
				 * flush it before we move. */
				cursor_step(eb, rev, phase1_y_steps, sx);
				sy += phase1_y_steps;
				y_steps -= phase1_y_steps;
				if (y_steps == 0)
					goto end;
			}

			/* Phase 3: precalculation */
			y_steps -= phase3_y_steps;

			/* Phase 2: */
			y_steps = fixed2int(y_steps);
			assert(y_steps >= 0);
			if (y_steps > 0) {
				cursor_always_step(eb, rev, fixed_1, sx);
				y_steps--;
				while (y_steps) {
					cursor_always_step_inrange_vertical(eb, rev, fixed_1, sx);
					y_steps--;
				}
			}

			/* Phase 3 */
			assert(cr->left == sx && cr->right == sx);
			cr->y += phase3_y_steps;
		} else if (sx < ex) {
			/* Lines increasing in x. (Rightwards, rising) */
			int phase1_x_steps, phase3_x_steps;
			fixed x_steps = ex - sx;

			/* Phase 1: */
			cursor_left_merge(eb, rev, sx);
			if (phase1_y_steps) {
				phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
				sx += phase1_x_steps;
				cursor_right_merge(eb, rev, sx);
				x_steps -= phase1_x_steps;
				cursor_step(eb, rev, phase1_y_steps, sx);
				sy += phase1_y_steps;
				y_steps -= phase1_y_steps;
				if (y_steps == 0)
					goto end;
			}

			/* Phase 3: precalculation */
			phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
			x_steps -= phase3_x_steps;
			y_steps -= phase3_y_steps;
			assert((y_steps & (fixed_1 - 1)) == 0);

			/* Phase 2: */
			y_steps = fixed2int(y_steps);
			assert(y_steps >= 0);
			if (y_steps) {
				/* We want to change sx by x_steps in y_steps steps.
				 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/y_steps. */
				int x_inc = x_steps/y_steps;
				int n_inc = x_steps - (x_inc * y_steps);
				int f = y_steps/2;
				int d = y_steps;

				/* Special casing the unset iteration, allows us to simplify
				 * the following loop. */
				sx += x_inc;
				f -= n_inc;
				if (f < 0)
					f += d, sx++;
				cursor_right_merge(eb, rev, sx);
				cursor_always_step(eb, rev, fixed_1, sx);
				y_steps--;

				while (y_steps) {
					sx += x_inc;
					f -= n_inc;
					if (f < 0)
						f += d, sx++;
					cursor_right(eb, rev, sx);
					cursor_always_inrange_step_right(eb, rev, fixed_1, sx);
					y_steps--;
				};
			}

			/* Phase 3 */
			assert(cr->left <= ex && cr->right >= sx);
			cursor_right(eb, rev, ex);
			cr->y += phase3_y_steps;
		} else {
			/* Lines decreasing in x. (Leftwards, rising) */
			int phase1_x_steps, phase3_x_steps;
			fixed x_steps = sx - ex;

			/* Phase 1: */
			cursor_right_merge(eb, rev, sx);
			if (phase1_y_steps) {
				phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
				x_steps -= phase1_x_steps;
				sx -= phase1_x_steps;
				cursor_left_merge(eb, rev, sx);
				cursor_step(eb, rev, phase1_y_steps, sx);
				sy += phase1_y_steps;
				y_steps -= phase1_y_steps;
				if (y_steps == 0)
					goto end;
			}

			/* Phase 3: precalculation */
			phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
			x_steps -= phase3_x_steps;
			y_steps -= phase3_y_steps;
			assert((y_steps & (fixed_1 - 1)) == 0);

			/* Phase 2: */
			y_steps = fixed2int(y_steps);
			assert(y_steps >= 0);
			if (y_steps) {
				/* We want to change sx by x_steps in y_steps steps.
				 * So each step, we sub x_steps/y_steps from sx. That's x_inc + n_inc/ey. */
				int x_inc = x_steps/y_steps;
				int n_inc = x_steps - (x_inc * y_steps);
				int f = y_steps/2;
				int d = y_steps;

				/* Special casing the unset iteration, allows us to simplify
				 * the following loop. */
				sx -= x_inc;
				f -= n_inc;
				if (f < 0)
					f += d, sx--;
				cursor_left_merge(eb, rev, sx);
				cursor_always_step(eb, rev, fixed_1, sx);
				y_steps--;

				while (y_steps) {
					sx -= x_inc;
					f -= n_inc;
					if (f < 0)
						f += d, sx--;
					cursor_left(eb, rev, sx);
					cursor_always_inrange_step_left(eb, rev, fixed_1, sx);
					y_steps--;
				}
			}

			/* Phase 3 */
			assert(cr->right >= ex && cr->left <= sx);
			cursor_left(eb, rev, ex);
			cr->y += phase3_y_steps;
		}
	} else {
		/* So lines decreasing in y. */
		/* We want to change from sy to ey, which are guaranteed to be on
		 * different scanlines. We do this in 3 phases.
		 * Phase 1 gets us from sy to the next scanline boundary. This never causes an output.
		 * Phase 2 gets us all the way to the last scanline boundary. This is guaranteed to cause an output.
		 * Phase 3 gets us from the last scanline boundary to ey. We are guaranteed to have outputted by now.
		 */
		int phase1_y_steps = sy & (fixed_1 - 1);
		int phase3_y_steps = (-ey) & (fixed_1 - 1);

		y_steps = -y_steps;
		/* Cope with the awkward 0x80000000 case. */
		if (y_steps < 0)
		{
			int mx, my;
			mx = sx + ((ex-sx)>>1);
			my = sy + ((ey-sy)>>1);
			do_mark_line_app(ctx, eb, sx, sy, mx, my, rev);
			do_mark_line_app(ctx, eb, mx, my, ex, ey, rev);
			return;
		}

		cursor_down(eb, rev, sx);

		if (sx == ex) {
			/* Vertical line. (Falling) */

			/* Phase 1: */
			cursor_left_merge(eb, rev, sx);
			cursor_right_merge(eb, rev, sx);
			if (phase1_y_steps) {
				/* Phase 1 in a falling line never moves us into a new scanline. */
				cursor_never_step_vertical(eb, rev, -phase1_y_steps, sx);
				sy -= phase1_y_steps;
				y_steps -= phase1_y_steps;
				if (y_steps == 0)
					goto endFalling;
			}

			/* Phase 3: precalculation */
			y_steps -= phase3_y_steps;
			assert((y_steps & (fixed_1 - 1)) == 0);

			/* Phase 2: */
			y_steps = fixed2int(y_steps);
			assert(y_steps >= 0);
			if (y_steps) {
				cursor_always_step(eb, rev, -fixed_1, sx);
				y_steps--;
				while (y_steps) {
					cursor_always_step_inrange_vertical(eb, rev, -fixed_1, sx);
					y_steps--;
				}
			}

			/* Phase 3 */
			if (phase3_y_steps > 0) {
				cursor_step(eb, rev, -phase3_y_steps, sx);
				assert(cr->left == sx && cr->right == sx);
			}
		} else if (sx < ex) {
			/* Lines increasing in x. (Rightwards, falling) */
			int phase1_x_steps, phase3_x_steps;
			fixed x_steps = ex - sx;

			/* Phase 1: */
			cursor_left_merge(eb, rev, sx);
			if (phase1_y_steps) {
				phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
				x_steps -= phase1_x_steps;
				sx += phase1_x_steps;
				/* Phase 1 in a falling line never moves us into a new scanline. */
				cursor_never_step_right(eb, rev, -phase1_y_steps, sx);
				sy -= phase1_y_steps;
				y_steps -= phase1_y_steps;
				if (y_steps == 0)
					goto endFalling;
			} else
				cursor_right_merge(eb, rev, sx);

			/* Phase 3: precalculation */
			phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
			x_steps -= phase3_x_steps;
			y_steps -= phase3_y_steps;
			assert((y_steps & (fixed_1 - 1)) == 0);

			/* Phase 2: */
			y_steps = fixed2int(y_steps);
			assert(y_steps >= 0);
			if (y_steps) {
				/* We want to change sx by x_steps in y_steps steps.
				 * So each step, we add x_steps/y_steps to sx. That's x_inc + n_inc/ey. */
				int x_inc = x_steps/y_steps;
				int n_inc = x_steps - (x_inc * y_steps);
				int f = y_steps/2;
				int d = y_steps;

				cursor_always_step(eb, rev, -fixed_1, sx);
				sx += x_inc;
				f -= n_inc;
				if (f < 0)
					f += d, sx++;
				cursor_right(eb, rev, sx);
				y_steps--;

				while (y_steps) {
					cursor_always_inrange_step_right(eb, rev, -fixed_1, sx);
					sx += x_inc;
					f -= n_inc;
					if (f < 0)
						f += d, sx++;
					cursor_right(eb, rev, sx);
					y_steps--;
				}
			}

			/* Phase 3 */
			if (phase3_y_steps > 0) {
				cursor_step(eb, rev, -phase3_y_steps, sx);
				cursor_right(eb, rev, ex);
				assert(cr->left == sx && cr->right == ex);
			}
		} else {
			/* Lines decreasing in x. (Falling) */
			int phase1_x_steps, phase3_x_steps;
			fixed x_steps = sx - ex;

			/* Phase 1: */
			cursor_right_merge(eb, rev, sx);
			if (phase1_y_steps) {
				phase1_x_steps = (int)(((int64_t)x_steps * phase1_y_steps + y_steps/2) / y_steps);
				x_steps -= phase1_x_steps;
				sx -= phase1_x_steps;
				/* Phase 1 in a falling line never moves us into a new scanline. */
				cursor_never_step_left(eb, rev, -phase1_y_steps, sx);
				sy -= phase1_y_steps;
				y_steps -= phase1_y_steps;
				if (y_steps == 0)
					goto endFalling;
			} else
				cursor_left_merge(eb, rev, sx);

			/* Phase 3: precalculation */
			phase3_x_steps = (int)(((int64_t)x_steps * phase3_y_steps + y_steps/2) / y_steps);
			x_steps -= phase3_x_steps;
			y_steps -= phase3_y_steps;
			assert((y_steps & (fixed_1 - 1)) == 0);

			/* Phase 2: */
			y_steps = fixed2int(y_steps);
			assert(y_steps >= 0);
			if (y_steps) {
				/* We want to change sx by x_steps in y_steps steps.
				 * So each step, we sub x_steps/y_steps from sx. That's x_inc + n_inc/ey. */
				int x_inc = x_steps/y_steps;
				int n_inc = x_steps - (x_inc * y_steps);
				int f = y_steps/2;
				int d = y_steps;

				cursor_always_step(eb, rev, -fixed_1, sx);
				sx -= x_inc;
				f -= n_inc;
				if (f < 0)
					f += d, sx--;
				cursor_left(eb, rev, sx);
				y_steps--;

				while (y_steps) {
					cursor_always_inrange_step_left(eb, rev, -fixed_1, sx);
					sx -= x_inc;
					f -= n_inc;
					if (f < 0)
						f += d, sx--;
					cursor_left(eb, rev, sx);
					y_steps--;
				}
			}

			/* Phase 3 */
			if (phase3_y_steps > 0) {
				cursor_step(eb, rev, -phase3_y_steps, sx);
				cursor_left(eb, rev, ex);
				assert(cr->left == ex && cr->right == sx);
			}
		}
endFalling:
		if (truncated)
			cursor_output(eb, rev, fixed2int(cr->y) - base_y);
	}

end:
	if (truncated) {
		cr->left = save_ex;
		cr->right = save_ex;
		cr->y = save_ey;
	}
}

static void mark_line_app(fz_context *ctx, fz_edgebuffer *eb, fixed sx, fixed sy, fixed ex, fixed ey, int rev)
{
	if (rev == 1)
	{
		fixed t;
		t = sx, sx = ex, ex = t;
		t = sy, sy = ey, ey = t;
	}
	do_mark_line_app(ctx, eb, sx, sy, ex, ey, rev);
}

static void fz_insert_edgebuffer_app(fz_context *ctx, fz_rasterizer *ras, float fsx, float fsy, float fex, float fey, int rev)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)ras;
	fixed sx = float2fixed(fsx);
	fixed sy = float2fixed(fsy);
	fixed ex = float2fixed(fex);
	fixed ey = float2fixed(fey);

	if (fsx < fex)
	{
		if (fsx < eb->super.bbox.x0) eb->super.bbox.x0 = fsx;
		if (fex > eb->super.bbox.x1) eb->super.bbox.x1 = fex;
	}
	else
	{
		if (fsx > eb->super.bbox.x1) eb->super.bbox.x1 = fsx;
		if (fex < eb->super.bbox.x0) eb->super.bbox.x0 = fex;
	}
	if (fsy < fey)
	{
		if (fsy < eb->super.bbox.y0) eb->super.bbox.y0 = fsy;
		if (fey > eb->super.bbox.y1) eb->super.bbox.y1 = fey;
	}
	else
	{
		if (fey < eb->super.bbox.y0) eb->super.bbox.y0 = fey;
		if (fsy > eb->super.bbox.y1) eb->super.bbox.y1 = fsy;
	}

	mark_line_app(ctx, eb, sx, sy, ex, ey, rev);
}

static int intcmp(const void *a, const void *b)
{
	return *((int*)a) - *((int *)b);
}

static void fz_convert_edgebuffer(fz_context *ctx, fz_rasterizer *ras, int eofill, const fz_irect *clip, fz_pixmap *pix, unsigned char *color, fz_overprint *eop)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)ras;
	int scanlines = ras->clip.y1 - ras->clip.y0;
	int i, n, a, pl, pr;
	int *table = eb->table;
	int *index = eb->index;
	uint8_t *out;
	fz_solid_color_painter_t *fn;

	fn = fz_get_solid_color_painter(pix->n, color, pix->alpha, eop);
	assert(fn);
	if (fn == NULL)
		return;

#ifdef DEBUG_SCAN_CONVERTER
	if (debugging_scan_converter)
	{
		fz_output *err = fz_stderr(ctx);
		fz_write_printf(ctx, err, "Before sort:\n");
		fz_edgebuffer_print(ctx, err, eb);
	}
#endif

	if (!eb->sorted)
	{
		eb->sorted = 1;
		for (i = 0; i < scanlines; i++)
		{
			int *row = &table[index[i]];
			int rowlen = *row++;

			/* Bubblesort short runs, qsort longer ones. */
			/* FIXME: Check "6" below */
			if (rowlen <= 6) {
				int j, k;
				for (j = 0; j < rowlen-1; j++)
				{
					int t = row[j];
					for (k = j+1; k < rowlen; k++)
					{
						int s = row[k];
						if (t > s)
							row[k] = t, t = row[j] = s;
					}
				}
			} else
				qsort(row, rowlen, sizeof(int), intcmp);
		}

#ifdef DEBUG_SCAN_CONVERTER
		if (debugging_scan_converter)
		{
			fz_output *err = fz_stderr(ctx);
			fz_write_printf(ctx, err, "Before filter: %s\n", eofill ? "EO" : "NZ");
			fz_edgebuffer_print(ctx, err, eb);
		}
#endif

		for (i=0; i < scanlines; i++) {
			int *row = &table[index[i]];
			int *rowstart = row;
			int rowlen = *row++;
			int *rowout = row;

			while (rowlen > 0)
			{
				int left, right;

				if (eofill) {
					/* Even Odd */
					left  = (*row++)&~1;
					right = (*row++)&~1;
					rowlen -= 2;
				} else {
					/* Non-Zero */
					int w;

					left = *row++;
					w = ((left&1)-1) | (left&1);
					rowlen--;
					do {
						right  = *row++;
						rowlen--;
						w += ((right&1)-1) | (right&1);
					} while (w != 0);
					left &= ~1;
					right &= ~1;
				}

				if (right > left) {
					*rowout++ = left;
					*rowout++ = right;
				}
			}
			*rowstart = (rowout-rowstart)-1;
		}
	}

#ifdef DEBUG_SCAN_CONVERTER
	if (debugging_scan_converter)
	{
		fz_output *err = fz_stderr(ctx);
		fz_write_printf(ctx, err, "Before render:\n");
		fz_edgebuffer_print(ctx, err, eb);
	}
#endif

	n = pix->n;
	a = pix->alpha;
	pl = fz_maxi(ras->clip.x0, pix->x);
	pr = fz_mini(ras->clip.x1, pix->x + pix->w);
	pr -= pl;
	out = pix->samples + pix->stride * fz_maxi(ras->clip.y0 - pix->y, 0) + fz_maxi(ras->clip.x0 - pix->x, 0) * n;
	if (scanlines > pix->y + pix->h - ras->clip.y0)
		scanlines = pix->y + pix->h - ras->clip.y0;
	for (i = fz_maxi(pix->y - ras->clip.y0, 0); i < scanlines; i++) {
		int *row = &table[index[i]];
		int  rowlen = *row++;

		while (rowlen > 0) {
			int left, right;

			left  = *row++;
			right = *row++;
			rowlen -= 2;
			left  = fixed2int(left + fixed_half) - pl;
			right = fixed2int(right + fixed_half) - pl;

			if (right <= 0)
				continue;
			if (left >= pr)
				continue;
			if (right > pr)
				right = pr;
			if (left < 0)
				left = 0;
			right -= left;
			if (right > 0) {
				(*fn)(out + left*n, n, right, color, a, eop);
			}
		}
		out += pix->stride;
	}
}

static int edgecmp(const void *a, const void *b)
{
	int left  = ((int*)a)[0];
	int right = ((int*)b)[0];
	left -= right;
	if (left)
		return left;
	return ((int*)a)[1] - ((int*)b)[1];
}

static void fz_convert_edgebuffer_app(fz_context *ctx, fz_rasterizer *ras, int eofill, const fz_irect *clip, fz_pixmap *pix, unsigned char *color, fz_overprint *eop)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)ras;
	int scanlines = ras->clip.y1 - ras->clip.y0;
	int i, n, a, pl, pr;
	int *table = eb->table;
	int *index = eb->index;
	uint8_t *out;
	fz_solid_color_painter_t *fn;

	fn = fz_get_solid_color_painter(pix->n, color, pix->alpha, eop);
	assert(fn);
	if (fn == NULL)
		return;

#ifdef DEBUG_SCAN_CONVERTER
	if (debugging_scan_converter)
	{
		fz_output *err = fz_stderr(ctx);
		fz_write_printf(ctx, err, "Before sort:\n");
		fz_edgebuffer_print_app(ctx, err, eb);
	}
#endif

	if (!eb->sorted)
	{
		eb->sorted = 1;
		for (i = 0; i < scanlines; i++)
		{
			int *row = &table[index[i]];
			int rowlen = *row++;

			/* Bubblesort short runs, qsort longer ones. */
			/* FIXME: Check "6" below */
			if (rowlen <= 6) {
				int j, k;
				for (j = 0; j < rowlen-1; j++) {
					int * FZ_RESTRICT t = &row[j<<1];
					for (k = j+1; k < rowlen; k++) {
						int * FZ_RESTRICT s = &row[k<<1];
						int tmp;
						if (t[0] < s[0])
							continue;
						if (t[0] > s[0])
							tmp = t[0], t[0] = s[0], s[0] = tmp;
						else if (t[0] <= s[1])
							continue;
						tmp = t[1]; t[1] = s[1]; s[1] = tmp;
					}
				}
			} else
				qsort(row, rowlen, 2*sizeof(int), edgecmp);
		}

#ifdef DEBUG_SCAN_CONVERTER
		if (debugging_scan_converter)
		{
			fz_output *err = fz_stderr(ctx);
			fz_write_printf(ctx, err, "Before filter: %s\n", eofill ? "EO" : "NZ");
			fz_edgebuffer_print_app(ctx, err, eb);
		}
#endif

		for (i=0; i < scanlines; i++) {
			int *row = &table[index[i]];
			int rowlen = *row++;
			int *rowstart = row;
			int *rowout = row;
			int  ll, lr, rl, rr, wind, marked_to;

			/* Avoid double setting pixels, by keeping where we have marked to. */
			marked_to = int2fixed(clip->x0);
			while (rowlen > 0) {
				if (eofill) {
					/* Even Odd */
					ll = (*row++)&~1;
					lr = *row;
					row += 2;
					rowlen-=2;

					/* We will fill solidly from ll to at least lr, possibly further */
					assert(rowlen >= 0);
					rr = (*row++);
					if (rr > lr)
						lr = rr;
				} else {
					/* Non-Zero */
					int w;

					ll = *row++;
					lr = *row++;
					wind = -(ll&1) | 1;
					ll &= ~1;
					rowlen--;

					assert(rowlen > 0);
					do {
						rl = *row++;
						rr = *row++;
						w = -(rl&1) | 1;
						rl &= ~1;
						rowlen--;
						if (rr > lr)
							lr = rr;
						wind += w;
						if (wind == 0)
							break;
					} while (rowlen > 0);
				}

				if (marked_to >= lr)
					continue;

				if (marked_to >= ll) {
					if (rowout == rowstart)
						ll = marked_to;
					else {
						rowout -= 2;
						ll = *rowout;
					}
				}

				if (lr > ll) {
					*rowout++ = ll;
					*rowout++ = lr;
					marked_to = lr;
				}
			}
			rowstart[-1] = rowout-rowstart;
		}
	}

#ifdef DEBUG_SCAN_CONVERTER
	if (debugging_scan_converter)
	{
		fz_output *err = fz_stderr(ctx);
		fz_write_printf(ctx, err, "Before render:\n");
		fz_edgebuffer_print_app(ctx, err, eb);
	}
#endif

	n = pix->n;
	a = pix->alpha;
	pl = clip->x0;
	pr = clip->x1 - pl;
	out = pix->samples + pix->stride * (clip->y0 - pix->y) + (clip->x0 - pix->x) * n;
	if (scanlines > clip->y1 - ras->clip.y0)
		scanlines = clip->y1 - ras->clip.y0;

	i = (clip->y0 - ras->clip.y0);
	if (i < 0)
		return;
	for (; i < scanlines; i++) {
		int *row = &table[index[i]];
		int  rowlen = *row++;

		while (rowlen > 0) {
			int left, right;

			left  = *row++;
			right = *row++;
			rowlen -= 2;
			left  = fixed2int(left + fixed_half) - pl;
			right = fixed2int(right + fixed_half) - pl;

			if (right <= 0)
				continue;
			if (left >= pr)
				break;
			if (right > pr)
				right = pr;
			if (left < 0)
				left = 0;
			right -= left;
			if (right > 0) {
				(*fn)(out + left*n, n, right, color, a, eop);
			}
		}
		out += pix->stride;
	}
}

static void fz_gap_edgebuffer(fz_context *ctx, fz_rasterizer *ras)
{
	fz_edgebuffer *eb = (fz_edgebuffer *)ras;

	if (eb->app)
	{
#ifdef DEBUG_SCAN_CONVERTER
		if (0 && debugging_scan_converter)
		{
			fz_output *err = fz_stderr(ctx);
			fz_write_printf(ctx, fz_stderr(ctx), "Pen up move.\n");
			fz_write_printf(ctx, err, "Before flush:\n");
			fz_edgebuffer_print_app(ctx, err, eb);
		}
#endif
		cursor_flush(eb);
		eb->cursor[0].saved = 0;
		eb->cursor[0].unset = 1;
		eb->cursor[0].can_save = 1;
		eb->cursor[0].d = DIRN_UNSET;
		eb->cursor[1].saved = 0;
		eb->cursor[1].unset = 1;
		eb->cursor[1].can_save = 1;
		eb->cursor[1].d = DIRN_UNSET;
		eb->cursor[2].saved = 0;
		eb->cursor[2].unset = 1;
		eb->cursor[2].can_save = 1;
		eb->cursor[2].d = DIRN_UNSET;
	}
}

static int fz_is_rect_edgebuffer(fz_context *ctx, fz_rasterizer *r)
{
	return 0;
}

static const fz_rasterizer_fns edgebuffer_app =
{
	fz_drop_edgebuffer,
	fz_reset_edgebuffer,
	fz_postindex_edgebuffer,
	fz_insert_edgebuffer_app,
	NULL,
	fz_gap_edgebuffer,
	fz_convert_edgebuffer_app,
	fz_is_rect_edgebuffer,
	1 /* Reusable */
};

static const fz_rasterizer_fns edgebuffer_cop =
{
	fz_drop_edgebuffer,
	fz_reset_edgebuffer,
	fz_postindex_edgebuffer,
	fz_insert_edgebuffer,
	NULL,
	NULL, /* gap */
	fz_convert_edgebuffer,
	fz_is_rect_edgebuffer,
	1 /* Reusable */
};

fz_rasterizer *
fz_new_edgebuffer(fz_context *ctx, fz_edgebuffer_rule rule)
{
	fz_edgebuffer *eb;
	eb = fz_new_derived_rasterizer(ctx, fz_edgebuffer, rule == FZ_EDGEBUFFER_ANY_PART_OF_PIXEL ? &edgebuffer_app : &edgebuffer_cop);
	eb->app = rule == FZ_EDGEBUFFER_ANY_PART_OF_PIXEL;
	return &eb->super;
}
