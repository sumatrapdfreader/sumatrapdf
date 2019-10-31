/*
	This file is #included by draw-paint.c multiple times to
	produce optimised plotters.
*/

#ifdef ALPHA
#define NAME alpha
#else
#define NAME solid
#endif

#ifdef DA
#define NAME2 _da
#else
#define NAME2
#endif

#ifdef EOP
#define NAME3 _op
#else
#define NAME3
#endif

#define FUNCTION_NAMER(NAME,N,NAME2,NAME3) fz_paint_glyph_##NAME##_##N##NAME2##NAME3
#define FUNCTION_NAME(NAME,N,NAME2,NAME3) FUNCTION_NAMER(NAME,N,NAME2,NAME3)

#ifdef EOP
#define IF_OVERPRINT_COMPONENT(k) if (fz_overprint_component(eop, k))
#else
#define IF_OVERPRINT_COMPONENT(k) if (1)
#endif

static inline void
FUNCTION_NAME(NAME,N,NAME2,NAME3)(const unsigned char * FZ_RESTRICT colorbv,
#ifndef N
				const int n1,
#endif
				int span, unsigned char * FZ_RESTRICT dp, const fz_glyph *glyph, int w, int h, int skip_x, int skip_y
#ifdef EOP
				, const fz_overprint *eop
#endif
				)
{
#ifdef N
	const int n1 = N;
#endif
#ifdef DA
	const int n = n1 + 1;
#else
	const int n = n1;
#endif
#ifdef ALPHA
	int sa = FZ_EXPAND(colorbv[n1]);
#else
#if defined(N) && N == 1 && defined(DA)
	const uint16_t color = *(const uint16_t *)colorbv;
#elif defined(N) && N == 3 && defined(DA)
	const uint32_t color = *(const uint32_t *)colorbv;
#elif defined(N) && N == 4 && !defined(DA)
	const uint32_t color = *(const uint32_t *)colorbv;
#endif
#endif
	TRACK_FN();
	while (h--)
	{
		int skip_xx, ww, len, extend;
		const unsigned char *runp;
		unsigned char *ddp = dp;
		int offset = ((int *)(glyph->data))[skip_y++];
		if (offset >= 0)
		{
			int eol = 0;
			runp = &glyph->data[offset];
			extend = 0;
			ww = w;
			skip_xx = skip_x;
			while (skip_xx)
			{
				int v = *runp++;
				switch (v & 3)
				{
				case 0: /* Extend */
					extend = v>>2;
					len = 0;
					break;
				case 1: /* Transparent */
					len = (v>>2) + 1 + (extend<<6);
					extend = 0;
					if (len > skip_xx)
					{
						len -= skip_xx;
						goto transparent_run;
					}
					break;
				case 2: /* Solid */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
					if (len > skip_xx)
					{
						len -= skip_xx;
						goto solid_run;
					}
					break;
				default: /* Intermediate */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
					if (len > skip_xx)
					{
						runp += skip_xx;
						len -= skip_xx;
						goto intermediate_run;
					}
					runp += len;
					break;
				}
				if (eol)
				{
					ww = 0;
					break;
				}
				skip_xx -= len;
			}
			while (ww > 0)
			{
				int v = *runp++;
				switch(v & 3)
				{
				case 0: /* Extend */
					extend = v>>2;
					break;
				case 1: /* Transparent */
					len = (v>>2) + 1 + (extend<<6);
					extend = 0;
transparent_run:
					if (len > ww)
						len = ww;
					ww -= len;
					ddp += len * n;
					break;
				case 2: /* Solid */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
solid_run:
					if (len > ww)
						len = ww;
					ww -= len;
					do
					{
#ifdef ALPHA
#if defined(N) && N == 1
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = FZ_BLEND(colorbv[0], ddp[0], sa);
#ifdef DA
						ddp[1] = FZ_BLEND(0xFF, ddp[1], sa);
						ddp += 2;
#else
						ddp++;
#endif
#elif defined(N) && N == 3
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = FZ_BLEND(colorbv[0], ddp[0], sa);
						IF_OVERPRINT_COMPONENT(1)
							ddp[1] = FZ_BLEND(colorbv[1], ddp[1], sa);
						IF_OVERPRINT_COMPONENT(2)
							ddp[2] = FZ_BLEND(colorbv[2], ddp[2], sa);
#ifdef DA
						ddp[3] = FZ_BLEND(0xFF, ddp[3], sa);
						ddp += 4;
#else
						ddp += 3;
#endif
#elif defined(N) && N == 4
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = FZ_BLEND(colorbv[0], ddp[0], sa);
						IF_OVERPRINT_COMPONENT(1)
							ddp[1] = FZ_BLEND(colorbv[1], ddp[1], sa);
						IF_OVERPRINT_COMPONENT(2)
							ddp[2] = FZ_BLEND(colorbv[2], ddp[2], sa);
						IF_OVERPRINT_COMPONENT(3)
							ddp[3] = FZ_BLEND(colorbv[3], ddp[3], sa);
#ifdef DA
						ddp[4] = FZ_BLEND(0xFF, ddp[4], sa);
						ddp += 5;
#else
						ddp += 4;
#endif
#else
						int k = 0;
						do
						{
							IF_OVERPRINT_COMPONENT(k)
								*ddp = FZ_BLEND(colorbv[k], *ddp, sa);
							k++;
							ddp++;
						}
						while (k != n1);
#ifdef DA
						*ddp = FZ_BLEND(0xFF, *ddp, sa);
						ddp++;
#endif
#endif
#else
#if defined(N) && N == 1
#ifdef DA
#ifdef EOP
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = colorbv[0];
						IF_OVERPRINT_COMPONENT(1)
							*ddp[1] = colorbv[1];
#else
						*(uint16_t *)ddp = color;
#endif
						ddp += 2;
#else
						*ddp++ = colorbv[0];
#endif
#elif defined(N) && N == 3
#ifdef DA
#ifdef EOP
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = colorbv[0];
						IF_OVERPRINT_COMPONENT(1)
							ddp[1] = colorbv[1];
						IF_OVERPRINT_COMPONENT(2)
							ddp[2] = colorbv[2];
						IF_OVERPRINT_COMPONENT(3)
#else
						*(uint32_t *)ddp = color;
#endif
						ddp += 4;
#else
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = colorbv[0];
						IF_OVERPRINT_COMPONENT(1)
							ddp[1] = colorbv[1];
						IF_OVERPRINT_COMPONENT(2)
							ddp[2] = colorbv[2];
						ddp += 3;
#endif
#elif defined(N) && N == 4
#ifdef DA
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = colorbv[0];
						IF_OVERPRINT_COMPONENT(1)
							ddp[1] = colorbv[1];
						IF_OVERPRINT_COMPONENT(2)
							ddp[2] = colorbv[2];
						IF_OVERPRINT_COMPONENT(3)
							ddp[3] = colorbv[3];
						ddp[4] = colorbv[4];
						ddp += 5;
#else
#ifdef EOP
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = colorbv[0];
						IF_OVERPRINT_COMPONENT(1)
							ddp[1] = colorbv[1];
						IF_OVERPRINT_COMPONENT(2)
							ddp[2] = colorbv[2];
						ddp[3] = colorbv[3];
#else
						*(uint32_t *)ddp = color;
#endif
						ddp += 4;
#endif
#else
						int k = 0;
						do
						{
							IF_OVERPRINT_COMPONENT(k)
								*ddp = colorbv[k];
							k++;
							ddp++;
						}
						while (k != n);
#endif
#endif
					}
					while (--len);
					break;
				default: /* Intermediate */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
intermediate_run:
					if (len > ww)
						len = ww;
					ww -= len;
					do
					{
						int k = 0;
						int a = *runp++;
#ifdef ALPHA
						a = FZ_COMBINE(sa, FZ_EXPAND(a));
#else
						a = FZ_EXPAND(a);
#endif
						(void)k;
#if defined(N) && N == 1
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = FZ_BLEND(colorbv[0], ddp[0], a);
#ifdef DA
						ddp[1] = FZ_BLEND(0xFF, ddp[1], a);
						ddp += 2;
#else
						ddp++;
#endif
#elif defined(N) && N == 3
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = FZ_BLEND(colorbv[0], ddp[0], a);
						IF_OVERPRINT_COMPONENT(1)
							ddp[1] = FZ_BLEND(colorbv[1], ddp[1], a);
						IF_OVERPRINT_COMPONENT(2)
							ddp[2] = FZ_BLEND(colorbv[2], ddp[2], a);
#ifdef DA
						ddp[3] = FZ_BLEND(0xFF, ddp[3], a);
						ddp += 4;
#else
						ddp += 3;
#endif
#elif defined(N) && N == 4
						IF_OVERPRINT_COMPONENT(0)
							ddp[0] = FZ_BLEND(colorbv[0], ddp[0], a);
						IF_OVERPRINT_COMPONENT(1)
							ddp[1] = FZ_BLEND(colorbv[1], ddp[1], a);
						IF_OVERPRINT_COMPONENT(2)
							ddp[2] = FZ_BLEND(colorbv[2], ddp[2], a);
						IF_OVERPRINT_COMPONENT(3)
							ddp[3] = FZ_BLEND(colorbv[3], ddp[3], a);
#ifdef DA
						ddp[4] = FZ_BLEND(0xFF, ddp[4], a);
						ddp += 5;
#else
						ddp += 4;
#endif
#else
						do
						{
							IF_OVERPRINT_COMPONENT(k)
								*ddp = FZ_BLEND(colorbv[k], *ddp, a);
							k++;
							ddp++;
						}
						while (k != n1);
#ifdef DA
						*ddp = FZ_BLEND(0xFF, *ddp, a);
						ddp++;
#endif
#endif
					}
					while (--len);
					break;
				}
				if (eol)
					break;
			}
		}
		dp += span;
	}
}

#undef NAME
#undef ALPHA
#undef NAME2
#undef NAME3
#undef EOP
#undef DA
#undef N
#undef FUNCTION_NAMER
#undef FUNCTION_NAME
#undef IF_OVERPRINT_COMPONENT
