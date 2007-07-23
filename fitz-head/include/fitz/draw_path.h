typedef struct fz_edge_s fz_edge;
typedef struct fz_gel_s fz_gel;
typedef struct fz_ael_s fz_ael;
typedef struct fz_scanargs_s fz_scanargs;

struct fz_edge_s
{
	int x, e, h, y;
	int adjup, adjdown;
	int xmove;
	int xdir, ydir;     /* -1 or +1 */
};

struct fz_gel_s
{
	fz_irect clip;
	fz_irect bbox;
	int cap;
	int len;
	fz_edge *edges;
};

struct fz_ael_s
{
	int cap;
	int len;
	fz_edge **edges;
};

struct fz_scanargs_s
{
	void (*blit)(fz_scanargs *args, unsigned char *bucket, int x, int y, int w, int h, int s);
	fz_pixmap *dest;
	int nt;
	int t0[FZ_MAXCOLORS];
	int dtdx[FZ_MAXCOLORS];
	int dtdy[FZ_MAXCOLORS];
	unsigned char solid[FZ_MAXCOLORS + 2];
	/* shade color lookup function */
};

fz_error *fz_newgel(fz_gel **gelp);
fz_error *fz_insertgel(fz_gel *gel, float x0, float y0, float x1, float y1);
fz_irect fz_boundgel(fz_gel *gel);
void fz_resetgel(fz_gel *gel, fz_irect clip);
void fz_sortgel(fz_gel *gel);
void fz_dropgel(fz_gel *gel);

fz_error *fz_newael(fz_ael **aelp);
void fz_dropael(fz_ael *ael);

fz_error *fz_scanconvert(fz_gel *gel, fz_ael *ael, int eofill, fz_scanargs *args);

fz_error *fz_fillpath(fz_gel *gel, fz_pathnode *path, fz_matrix ctm, float flatness);
fz_error *fz_strokepath(fz_gel *gel, fz_pathnode *path, fz_matrix ctm, float flatness);
fz_error *fz_dashpath(fz_gel *gel, fz_pathnode *path, fz_matrix ctm, float flatness);

