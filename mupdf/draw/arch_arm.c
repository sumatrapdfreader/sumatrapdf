/*
 * ARM specific render optims live here
 */

#include "fitz.h"

typedef unsigned char byte;

/* always surround cpu specific code with HAVE_XXX */
#ifdef ARCH_ARM

/* from imagescalearm.s */
extern void fz_srow4_arm(byte *src, byte *dst, int w, int denom);
extern void fz_scol4_arm(byte *src, byte *dst, int w, int denom);

static void
path_w4i1o4_arm(byte * restrict rgba, byte * restrict src, byte cov, int len, byte * restrict dst)
{
	/* The ARM code here is a hand coded implementation of the optimized C version. */

	if (len <= 0)
		return;

	asm volatile(
	"ldr	%0, [%0]		@ %0 = rgba			\n"
	"mov	r11,#0							\n"
	"mov	r8, #0xFF00						\n"
	"mov	r14,%0,lsr #24		@ r14= alpha			\n"
	"orr	%0, %0, #0xFF000000	@ %0 = rgba |= 0xFF000000	\n"
	"orr	r8, r8, r8, LSL #16	@ r8 = 0xFF00FF00		\n"
	"adds	r14,r14,r14,LSR #7	@ r14 = alpha += alpha>>7	\n"
	"beq	9f			@ if (alpha == 0) bale		\n"
	"and	r6, %0, r8		@ r6 = ga<<8			\n"
	"bic	%0, %0, r8		@ %0 = rb			\n"
	"mov	r6, r6, LSR #8		@ r6 = ga			\n"
	"cmp	r14,#256		@ if (alpha == 256)		\n"
	"beq	4f			@	no-alpha loop		\n"
	"B	2f			@ enter the loop		\n"
	"1:	@ Loop used for when coverage*alpha == 0		\n"
	"subs	%3, %3, #1		@ len--				\n"
	"ble	9f							\n"
	"2:								\n"
	"ldrb	r12,[%1]		@ r12= *src			\n"
	"ldr	r9, [%4], #4		@ r9 = drb = *dst32++		\n"
	"strb	r11,[%1], #1		@ r11= *src++ = 0		\n"
	"add	%2, r12, %2		@ %2 = cov += r12		\n"
	"ands	%2, %2, #255		@ %2 = cov &= 255		\n"
	"beq	1b			@ if coverage == 0 loop back	\n"
	"add	r10,%2, %2, LSR #7	@ r10= ca = cov+(cov>>7)	\n"
	"mul	r10,r14,r10		@ r10= ca *= alpha		\n"
	"and	r7, r8, r9		@ r7 = dga = drb & MASK		\n"
	"mov	r10,r10,LSR #8		@ r10= ca >>= 8			\n"
	"and	r9, r8, r9, LSL #8	@ r9 = drb = (drb<<8) & MASK	\n"
	"sub	r12,r6, r7, LSR #8	@ r12= cga = ga - (dga>>8)	\n"
	"sub	r5, %0, r9, LSR #8	@ r5 = crb = rb - (drb>>8)	\n"
	"mla	r7, r12,r10,r7		@ r7 = dga += cga * ca		\n"
	"subs	%3, %3, #1		@ len--				\n"
	"mla	r9, r5, r10,r9		@ r9 = drb += crb * ca		\n"
	"and	r7, r8, r7		@ r7 = dga &= MASK		\n"
	"and	r9, r8, r9		@ r9 = drb &= MASK		\n"
	"orr	r9, r7, r9, LSR #8	@ r9 = drb = dga | (drb>>8)	\n"
	"str	r9, [%4, #-4]		@ dst32[-1] = r9		\n"
	"bgt	2b							\n"
	"b	9f							\n"
	"@ --- Solid alpha loop	---------------------------------------	\n"
	"3:	@ Loop used when coverage == 256			\n"
	"orr	r9, %0, r6, LSL #8	@ r9 = rgba			\n"
	"str	r9, [%4, #-4]		@ dst32[-1] = r9		\n"
	"4:	@ Loop used for when coverage*alpha == 0		\n"
	"subs	%3, %3, #1		@ len--				\n"
	"ble	9f							\n"
	"5:								\n"
	"ldrb	r12,[%1]		@ r12= *src			\n"
	"ldr	r9, [%4], #4		@ r9 = drb = *dst32++		\n"
	"strb	r11,[%1], #1		@ r11= *src++ = 0		\n"
	"add	%2, r12, %2		@ %2 = cov += r12		\n"
	"ands	%2, %2, #255		@ %2 = cov &= 255		\n"
	"beq	4b			@ if coverage == 0 loop back	\n"
	"cmp	%2, #255		@ if coverage == solid		\n"
	"beq	3b			@	loop back		\n"
	"add	r10,%2, %2, LSR #7	@ r10= ca = cov+(cov>>7)	\n"
	"and	r7, r8, r9		@ r7 = dga = drb & MASK		\n"
	"and	r9, r8, r9, LSL #8	@ r9 = dga = (drb<<8) & MASK	\n"
	"sub	r12,r6, r7, LSR #8	@ r12= cga = ga - (dga>>8)	\n"
	"sub	r5, %0, r9, LSR #8	@ r5 = crb = rb - (drb>>8)	\n"
	"mla	r7, r12,r10,r7		@ r7 = dga += cga * ca		\n"
	"subs	%3, %3, #1		@ len--				\n"
	"mla	r9, r5, r10,r9		@ r9 = drb += crb * ca		\n"
	"and	r7, r8, r7		@ r7 = dga &= MASK		\n"
	"and	r9, r8, r9		@ r9 = drb &= MASK		\n"
	"orr	r9, r7, r9, LSR #8	@ r9 = drb = dga | (drb>>8)	\n"
	"str	r9, [%4, #-4]		@ dst32[-1] = r9		\n"
	"bgt	5b							\n"
	"9:				@ End				\n"
	:
	"+r" (rgba),
	"+r" (src),
	"+r" (cov),
	"+r" (len),
	"+r" (dst)
	:
	:
	"r5","r6","r7","r8","r9","r10","r11","r12","r14","memory","cc"
	);
}

static void load_tile8_arm(byte * restrict src, int sw, byte * restrict dst, int dw, int w, int h, int pad)
{
	if ((h == 0) || (w == 0))
		return;

	switch (pad)
	{
	case 0:
		while (h--)
		{
			memcpy(dst, src, w);
			src += sw;
			dst += dw;
		}
		break;

	case 1:
		sw -= w;
		dw -= w<<1;
		asm volatile(
			"MOV	r11,#255				\n"
			"1:						\n"
			"MOV	r5, %[w]		@ r5 = x = w	\n"
			"2:						\n"
			"LDRB	r4, [%[src]], #1	@ r4 = *src++	\n"
			"SUBS	r5, r5, #1				\n"
			"STRB	r4, [%[dst]], #1	@ *dst++ = r4	\n"
			"STRB	r11,[%[dst]], #1	@ *dst++ = 255	\n"
			"BGT	2b					\n"
			"ADD	%[src],%[src],%[sw]	@ src += sw	\n"
			"ADD	%[dst],%[dst],%[dw]	@ dst += dw	\n"
			"SUBS	%[h],%[h],#1				\n"
			"BGT	1b					\n"
			:
			[src]	"+r" (src),
			[sw]	"+r" (sw),
			[dst]	"+r" (dst),
			[dw]	"+r" (dw),
			[h]	"+r" (h),
			[w]	"+r" (w)
			:
			:
			"r4","r5","r11","memory","cc"
			);
		break;

	case 3:
		sw -= w;
		asm volatile(
			"MOV	r11,#255				\n"
			"1:						\n"
			"MOV	r5, %[w]		@ r5 = x = w	\n"
			"MOV	r8, %[dst]		@ r8 = dp = dst	\n"
			"2:						\n"
			"LDRB	r4, [%[src]], #1	@ r4 = *src++	\n"
			"LDRB	r6, [%[src]], #1	@ r6 = *src++	\n"
			"LDRB	r7, [%[src]], #1	@ r7 = *src++	\n"
			"SUBS	r5, r5, #3				\n"
			"STRB	r4, [r8], #1		@ *dp++ = r4	\n"
			"STRB	r6, [r8], #1		@ *dp++ = r6	\n"
			"STRB	r7, [r8], #1		@ *dp++ = r7	\n"
			"STRB	r11,[r8], #1		@ *dp++ = 255	\n"
			"BGT	2b					\n"
			"ADD	%[src],%[src],%[sw]	@ src += sw	\n"
			"ADD	%[dst],%[dst],%[dw]	@ dst += dw	\n"
			"SUBS	%[h],%[h],#1				\n"
			"BGT	1b					\n"
			:
			[src]	"+r" (src),
			[sw]	"+r" (sw),
			[dst]	"+r" (dst),
			[dw]	"+r" (dw),
			[h]	"+r" (h),
			[w]	"+r" (w)
			:
			:
			"r4","r5","r6","r7","r8","r11","memory","cc"
			);
		break;

	default:
		sw -= w;
		asm volatile(
			"mov	r9,#255					\n"
			"1:						\n"
			"mov	r7, %[dst]	@ r7 = dp = dst		\n"
			"mov	r8, #1		@ r8 = tpad = 1		\n"
			"mov	r14,%[w]	@ r11= x = w		\n"
			"2:						\n"
			"ldrb	r10,[%[src]],#1				\n"
			"subs	r8, r8, #1				\n"
			"moveq	r8, %[pad]				\n"
			"streqb	r9, [r7], #1				\n"
			"strb	r10,[r7], #1				\n"
			"subs	r14,r14, #1				\n"
			"bgt	2b					\n"
			"add	%[src],%[src],%[sw]			\n"
			"add	%[dst],%[dst],%[dw]			\n"
			"subs	%[h], %[h], #1				\n"
			"bgt	1b					\n"
			:
			[src]	"+r" (src),
			[sw]	"+r" (sw),
			[dst]	"+r" (dst),
			[dw]	"+r" (dw),
			[h]	"+r" (h),
			[w]	"+r" (w),
			[pad]	"+r" (pad)
			:
			:
			"r7","r8","r9","r10","r14","memory","cc"
			);
		break;
	}
}

void
fz_accelerate_arch(void)
{
	fz_path_w4i1o4 = path_w4i1o4_arm;
	fz_loadtile8 = load_tile8_arm;
	fz_srow4 = fz_srow4_arm;
	fz_scol4 = fz_scol4_arm;
}

#endif
