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
path_w4i1o4_arm(byte * restrict argb, byte * restrict src, byte cov, int len, byte * restrict dst)
{
	/* The ARM code here is a hand coded implementation
	 * of the optimized C version. */
	if (len <= 0)
		return;
	asm volatile(
	"ldr	%0, [%0]		@ %0 = argb			\n"
	"mov	r11,#0							\n"
	"mov	r8, #0xFF00						\n"
	"and	r14,%0,#255		@ r14= alpha			\n"
	"orr	%0, %0, #255		@ %0 = argb |= 255		\n"
	"orr	r8, r8, r8, LSL #16	@ r8 = 0xFF00FF00		\n"
	"adds	r14,r14,r14,LSR #7	@ r14 = alpha += alpha>>7	\n"
	"beq	9f			@ if (alpha == 0) bale		\n"
	"and	r6, %0, r8		@ r6 = rb<<8			\n"
	"bic	%0, %0, r8		@ %0 = ag			\n"
	"mov	r6, r6, LSR #8		@ r6 = rb			\n"
	"cmp	r14,#256		@ if (alpha == 256)		\n"
	"beq	4f			@     no-alpha loop		\n"
	"B	2f			@ enter the loop		\n"
	"1:	@ Loop used for when coverage*alpha == 0		\n"
	"subs	%3, %3, #1		@ len--				\n"
	"ble	9f							\n"
	"2:								\n"
	"ldrb	r12,[%1]		@ r12= *src			\n"
	"ldr	r9, [%4], #4		@ r9 = dag = *dst32++		\n"
	"strb	r11,[%1], #1		@ r11= *src++ = 0		\n"
	"add	%2, r12, %2		@ %2 = cov += r12		\n"
	"ands	%2, %2, #255		@ %2 = cov &= 255		\n"
	"beq	1b			@ if coverage == 0 loop back	\n"
	"add	r10,%2, %2, LSR #7	@ r10= ca = cov+(cov>>7)	\n"
	"mul	r10,r14,r10		@ r10= ca *= alpha		\n"
	"and	r7, r8, r9		@ r7 = drb =  dag     & MASK	\n"
	"mov	r10,r10,LSR #8		@ r10= ca >>= 8			\n"
	"and	r9, r8, r9, LSL #8	@ r9 = dag = (dag<<8) & MASK	\n"
	"sub	r12,r6, r7, LSR #8	@ r12= crb = rb - (drb>>8)	\n"
	"sub	r5, %0, r9, LSR #8	@ r5 = cag = ag - (dag>>8)	\n"
	"mla	r7, r12,r10,r7		@ r7 = drb += crb * ca		\n"
	"subs	%3, %3, #1		@ len--				\n"
	"mla	r9, r5, r10,r9		@ r9 = dag += cag * ca		\n"
	"and	r7, r8, r7		@ r7 = drb &= MASK		\n"
	"and	r9, r8, r9		@ r9 = dag &= MASK		\n"
	"orr	r9, r7, r9, LSR #8	@ r9 = dag = drb | (dag>>8)	\n"
	"str	r9, [%4, #-4]		@ dst32[-1] = r9		\n"
	"bgt	2b							\n"
	"b	9f							\n"
	"@ --- Solid alpha loop	---------------------------------------	\n"
	"3:	@ Loop used when coverage == 256			\n"
	"orr	r9, %0, r6, LSL #8	@ r9 = argb			\n"
	"str	r9, [%4, #-4]		@ dst32[-1] = r9		\n"
	"4:	@ Loop used for when coverage*alpha == 0		\n"
	"subs	%3, %3, #1		@ len--				\n"
	"ble	9f							\n"
	"5:								\n"
	"ldrb	r12,[%1]		@ r12= *src			\n"
	"ldr	r9, [%4], #4		@ r9 = dag = *dst32++		\n"
	"strb	r11,[%1], #1		@ r11= *src++ = 0		\n"
	"add	%2, r12, %2		@ %2 = cov += r12		\n"
	"ands	%2, %2, #255		@ %2 = cov &= 255		\n"
	"beq	4b			@ if coverage == 0 loop back	\n"
	"cmp	%2, #255		@ if coverage == solid		\n"
	"beq	3b			@    loop back			\n"
	"add	r10,%2, %2, LSR #7	@ r10= ca = cov+(cov>>7)	\n"
	"and	r7, r8, r9		@ r7 = drb =  dag     & MASK	\n"
	"and	r9, r8, r9, LSL #8	@ r9 = dag = (dag<<8) & MASK	\n"
	"sub	r12,r6, r7, LSR #8	@ r12= crb = rb - (drb>>8)	\n"
	"sub	r5, %0, r9, LSR #8	@ r5 = cag = ag - (dag>>8)	\n"
	"mla	r7, r12,r10,r7		@ r7 = drb += crb * ca		\n"
	"subs	%3, %3, #1		@ len--				\n"
	"mla	r9, r5, r10,r9		@ r9 = dag += cag * ca		\n"
	"and	r7, r8, r7		@ r7 = drb &= MASK		\n"
	"and	r9, r8, r9		@ r9 = dag &= MASK		\n"
	"orr	r9, r7, r9, LSR #8	@ r9 = dag = drb | (dag>>8)	\n"
	"str	r9, [%4, #-4]		@ dst32[-1] = r9		\n"
	"bgt	5b							\n"
	"9:				@ End				\n"
	:
	"+r" (argb),
	"+r" (src),
	"+r" (cov),
	"+r" (len),
	"+r" (dst)
	:
	:
	"r5","r6","r7","r8","r9","r10","r11","r12","r14","memory","cc"
	);
}

void
fz_acceleratearch(void)
{
	fz_path_w4i1o4 = path_w4i1o4_arm;
	fz_srow4 = fz_srow4_arm;
	fz_scol4 = fz_scol4_arm;
}

#endif
