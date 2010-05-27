#include "fitz.h"

/* This C implementation was a prototype of the algorithm used
 * in the ARM code in draw/archarm.c. It is conceivable that on some
 * architectures/compilers this may be preferable to the vanilla
 * version below. */

static void
path_w4i1o4_32bit(unsigned char * restrict argb,
	unsigned char * restrict src, unsigned char cov, int len,
	unsigned char * restrict dst)
{
	/* COLOR * coverage + DST * (256-coverage) = (COLOR - DST)*coverage + DST*256 */
	unsigned int *dst32 = (unsigned int *)(void *)dst;
	int alpha = argb[0];
	unsigned int rb = argb[1] | argb[3] << 16;
	unsigned int ag = 255 | argb[2] << 16;
	const int MASK = 0xFF00FF00;

	/* sanity test */
	if (sizeof(int) != 4 || sizeof(unsigned int) != 4)
		abort();

	if (alpha != 255)
	{
		alpha += alpha>>7; /* alpha is now in the 0...256 range */
		while (len--)
		{
			unsigned int ca, drb, dag, crb, cag;
			cov += *src; *src++ = 0;
			ca = cov+(cov>>7); /* ca is in 0...256 range */
			ca = (ca*alpha)>>8; /* ca is is in 0...256 range */
			dag = *dst32++;
			if (ca != 0)
			{
				drb = dag & MASK;
				dag = (dag<<8) & MASK;
				crb = rb - (drb>>8);
				cag = ag - (dag>>8);
				drb += crb * ca;
				dag += cag * ca;
				drb = drb & MASK;
				dag = dag & MASK;
				dag = drb | (dag>>8);
				dst32[-1] = dag;
			}
		}
	}
	else
	{
		while (len--)
		{
			unsigned int ca, drb, dag, crb, cag;
			cov += *src; *src++ = 0;
			ca = cov+(cov>>7); /* ca is in 0...256 range */
			dag = *dst32++;
			if (ca == 0)
				continue;
			if (ca == 255)
			{
				dag = (rb<<8) | ag;
			}
			else
			{
				drb = dag & MASK;
				dag = (dag<<8) & MASK;
				crb = rb - (drb>>8);
				cag = ag - (dag>>8);
				drb += crb * ca;
				dag += cag * ca;
				drb = drb & MASK;
				dag = dag & MASK;
				dag = drb | (dag>>8);
			}
			dst32[-1] = dag;
		}
	}
}

void fz_accelerate(void)
{
	if (sizeof(int) == 4)
	{
		fz_path_w4i1o4 = path_w4i1o4_32bit;
	}

	if (sizeof(int) == 8)
	{
	}

#ifdef HAVE_CPUDEP
	fz_acceleratearch();
#endif
}
