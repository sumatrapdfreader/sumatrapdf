@ imagescalearm.s is a hand tuned assembler version
@ of some of the imagescale functions targetted
@ for ARM based systems (any architecture version).
@
@ Copyright (C) 2010 Robin Watts for Artifex Software LLC.
@ <robin.watts@artifex.com> or <robin.watts@wss.co.uk>

        .file "imagescalearm.s"
        .global	fz_srow4_arm
        .global	fz_scol4_arm

	.type	fz_srow4_arm, %function
        .text

	@ r0 = src
	@ r1 = dst
	@ r2 = w
	@ r3 = denom
fz_srow4_arm:
	STMFD	r13!,{r3-r7,r9-r10,r14}

	MOV	r12,#1<<16		@ r12 = (1<<16)
	MOV	r14,#0			@ r14 = will contain invdenom

	@ r14 = r12/r3
	CMP	r12,r3, LSL #16
	SUBHS	r12,r12,r3, LSL #16
	ADDHS	r14,r14,#1<<16
	CMP	r12,r3, LSL #15
	SUBHS	r12,r12,r3, LSL #15
	ADDHS	r14,r14,#1<<15
	CMP	r12,r3, LSL #14
	SUBHS	r12,r12,r3, LSL #14
	ADDHS	r14,r14,#1<<14
	CMP	r12,r3, LSL #13
	SUBHS	r12,r12,r3, LSL #13
	ADDHS	r14,r14,#1<<13
	CMP	r12,r3, LSL #12
	SUBHS	r12,r12,r3, LSL #12
	ADDHS	r14,r14,#1<<12
	CMP	r12,r3, LSL #11
	SUBHS	r12,r12,r3, LSL #11
	ADDHS	r14,r14,#1<<11
	CMP	r12,r3, LSL #10
	SUBHS	r12,r12,r3, LSL #10
	ADDHS	r14,r14,#1<<10
	CMP	r12,r3, LSL #9
	SUBHS	r12,r12,r3, LSL #9
	ADDHS	r14,r14,#1<<9
	CMP	r12,r3, LSL #8
	SUBHS	r12,r12,r3, LSL #8
	ADDHS	r14,r14,#1<<8
	CMP	r12,r3, LSL #7
	SUBHS	r12,r12,r3, LSL #7
	ADDHS	r14,r14,#1<<7
	CMP	r12,r3, LSL #6
	SUBHS	r12,r12,r3, LSL #6
	ADDHS	r14,r14,#1<<6
	CMP	r12,r3, LSL #5
	SUBHS	r12,r12,r3, LSL #5
	ADDHS	r14,r14,#1<<5
	CMP	r12,r3, LSL #4
	SUBHS	r12,r12,r3, LSL #4
	ADDHS	r14,r14,#1<<4
	CMP	r12,r3, LSL #3
	SUBHS	r12,r12,r3, LSL #3
	ADDHS	r14,r14,#1<<3
	CMP	r12,r3, LSL #2
	SUBHS	r12,r12,r3, LSL #2
	ADDHS	r14,r14,#1<<2
	CMP	r12,r3, LSL #1
	SUBHS	r12,r12,r3, LSL #1
	ADDHS	r14,r14,#1<<1
	CMP	r12,r3
	SUBHS	r12,r12,r3
	ADDHS	r14,r14,#1

	@ r2 = x = w
	@ r3 = left = denom
	MOV	r10,#1<<15		@ r10= 1<<15
	B	.L_enter_loop_r4	@ Enter the loop
.L_store_r4:
	ADD	r7, r7, r12		@ r7 = sum3 += r12
	MLA	r4, r14,r4, r10		@ r4 = sum0 * invdenom + (1<<15)
	MLA	r5, r14,r5, r10		@ r5 = sum1 * invdenom + (1<<15)
	MLA	r6, r14,r6, r10		@ r6 = sum2 * invdenom + (1<<15)
	MLA	r7, r14,r7, r10		@ r7 = sum3 * invdenom + (1<<15)
	MOV	r4, r4, LSR #16		@ r4 = r4 >> 16
	MOV	r5, r5, LSR #16		@ r5 = r5 >> 16
	MOV	r6, r6, LSR #16		@ r6 = r6 >> 16
	MOV	r7, r7, LSR #16		@ r7 = r7 >> 16
	STRB	r4, [r1], #1		@ *dst++ = r4
	STRB	r5, [r1], #1		@ *dst++ = r5
	STRB	r6, [r1], #1		@ *dst++ = r6
	STRB	r7, [r1], #1		@ *dst++ = r7
	SUBS	r2, r2, #1		@ x--
	BEQ	.L_end_r4
	LDR	r3, [r13]		@ r3 = left = denom
.L_enter_loop_r4:
	MOV	r4, #0			@ r4 = sum0 = 0
	MOV	r5, #0			@ r5 = sum1 = 0
	MOV	r6, #0			@ r6 = sum2 = 0
	MOV	r7, #0			@ r7 = sum3 = 0
.L_x_loop_r4:
	LDRB	r9, [r0], #1		@ r9 = src++
	LDRB	r12,[r0], #1		@ r12= src++
	SUBS	r3, r3, #1		@ r3 = --left
	ADD	r4, r4, r9		@ r4 = sum0 += r9
	LDRB	r9, [r0], #1		@ r9 = src++
	ADD	r5, r5, r12		@ r5 = sum1 += r12
	LDRB	r12,[r0], #1		@ r12= src++
	ADD	r6, r6, r9		@ r9 = sum2 += r9
	BEQ	.L_store_r4
	ADD	r7, r7, r12		@ r7 = sum3 += r12
	SUBS	r2, r2, #1		@ x--
	BNE	.L_x_loop_r4

	@ Trailers
	LDR	r0, [r13]		@ r0 = denom
	MOV	r12,#1<<16		@ r12 = (1<<16)
	MOV	r14,#0			@ r14 = will contain invleft
	SUB	r3, r0, r3		@ r3 = denom-left
	CMP	r12,r3, LSL #16
	SUBHS	r12,r12,r3, LSL #16
	ADDHS	r14,r14,#1<<16
	CMP	r12,r3, LSL #15
	SUBHS	r12,r12,r3, LSL #15
	ADDHS	r14,r14,#1<<15
	CMP	r12,r3, LSL #14
	SUBHS	r12,r12,r3, LSL #14
	ADDHS	r14,r14,#1<<14
	CMP	r12,r3, LSL #13
	SUBHS	r12,r12,r3, LSL #13
	ADDHS	r14,r14,#1<<13
	CMP	r12,r3, LSL #12
	SUBHS	r12,r12,r3, LSL #12
	ADDHS	r14,r14,#1<<12
	CMP	r12,r3, LSL #11
	SUBHS	r12,r12,r3, LSL #11
	ADDHS	r14,r14,#1<<11
	CMP	r12,r3, LSL #10
	SUBHS	r12,r12,r3, LSL #10
	ADDHS	r14,r14,#1<<10
	CMP	r12,r3, LSL #9
	SUBHS	r12,r12,r3, LSL #9
	ADDHS	r14,r14,#1<<9
	CMP	r12,r3, LSL #8
	SUBHS	r12,r12,r3, LSL #8
	ADDHS	r14,r14,#1<<8
	CMP	r12,r3, LSL #7
	SUBHS	r12,r12,r3, LSL #7
	ADDHS	r14,r14,#1<<7
	CMP	r12,r3, LSL #6
	SUBHS	r12,r12,r3, LSL #6
	ADDHS	r14,r14,#1<<6
	CMP	r12,r3, LSL #5
	SUBHS	r12,r12,r3, LSL #5
	ADDHS	r14,r14,#1<<5
	CMP	r12,r3, LSL #4
	SUBHS	r12,r12,r3, LSL #4
	ADDHS	r14,r14,#1<<4
	CMP	r12,r3, LSL #3
	SUBHS	r12,r12,r3, LSL #3
	ADDHS	r14,r14,#1<<3
	CMP	r12,r3, LSL #2
	SUBHS	r12,r12,r3, LSL #2
	ADDHS	r14,r14,#1<<2
	CMP	r12,r3, LSL #1
	SUBHS	r12,r12,r3, LSL #1
	ADDHS	r14,r14,#1<<1
	CMP	r12,r3
	SUBHS	r12,r12,r3
	ADDHS	r14,r14,#1

	MLA	r4, r14,r4, r10		@ r4 = sum0 * invleft + (1<<15)
	MLA	r5, r14,r5, r10		@ r5 = sum1 * invleft + (1<<15)
	MLA	r6, r14,r6, r10		@ r6 = sum2 * invleft + (1<<15)
	MLA	r7, r14,r7, r10		@ r7 = sum3 * invleft + (1<<15)
	MOV	r4, r4, LSR #16		@ r4 = r4 >> 16
	MOV	r5, r5, LSR #16		@ r5 = r5 >> 16
	MOV	r6, r6, LSR #16		@ r6 = r6 >> 16
	MOV	r7, r7, LSR #16		@ r7 = r7 >> 16
	STRB	r4, [r1], #1		@ *dst++ = r4
	STRB	r5, [r1], #1		@ *dst++ = r5
	STRB	r6, [r1], #1		@ *dst++ = r6
	STRB	r7, [r1], #1		@ *dst++ = r7
.L_end_r4:
	LDMFD	r13!,{r3-r7,r9-r10,PC}

	.fnend
	.size	fz_srow4_arm, .-fz_srow4_arm

	.type	fz_scol4_arm, %function
        .text

	@ r0 = src
	@ r1 = dst
	@ r2 = w
	@ r3 = denom
fz_scol4_arm:
	STMFD	r13!,{r3-r7,r9-r11,r14}

	MOV	r12,#1<<16		@ r12 = (1<<16)
	MOV	r14,#0			@ r14 = will contain invdenom

	@ r14 = r12/r3
	CMP	r12,r3, LSL #16
	SUBHS	r12,r12,r3, LSL #16
	ADDHS	r14,r14,#1<<16
	CMP	r12,r3, LSL #15
	SUBHS	r12,r12,r3, LSL #15
	ADDHS	r14,r14,#1<<15
	CMP	r12,r3, LSL #14
	SUBHS	r12,r12,r3, LSL #14
	ADDHS	r14,r14,#1<<14
	CMP	r12,r3, LSL #13
	SUBHS	r12,r12,r3, LSL #13
	ADDHS	r14,r14,#1<<13
	CMP	r12,r3, LSL #12
	SUBHS	r12,r12,r3, LSL #12
	ADDHS	r14,r14,#1<<12
	CMP	r12,r3, LSL #11
	SUBHS	r12,r12,r3, LSL #11
	ADDHS	r14,r14,#1<<11
	CMP	r12,r3, LSL #10
	SUBHS	r12,r12,r3, LSL #10
	ADDHS	r14,r14,#1<<10
	CMP	r12,r3, LSL #9
	SUBHS	r12,r12,r3, LSL #9
	ADDHS	r14,r14,#1<<9
	CMP	r12,r3, LSL #8
	SUBHS	r12,r12,r3, LSL #8
	ADDHS	r14,r14,#1<<8
	CMP	r12,r3, LSL #7
	SUBHS	r12,r12,r3, LSL #7
	ADDHS	r14,r14,#1<<7
	CMP	r12,r3, LSL #6
	SUBHS	r12,r12,r3, LSL #6
	ADDHS	r14,r14,#1<<6
	CMP	r12,r3, LSL #5
	SUBHS	r12,r12,r3, LSL #5
	ADDHS	r14,r14,#1<<5
	CMP	r12,r3, LSL #4
	SUBHS	r12,r12,r3, LSL #4
	ADDHS	r14,r14,#1<<4
	CMP	r12,r3, LSL #3
	SUBHS	r12,r12,r3, LSL #3
	ADDHS	r14,r14,#1<<3
	CMP	r12,r3, LSL #2
	SUBHS	r12,r12,r3, LSL #2
	ADDHS	r14,r14,#1<<2
	CMP	r12,r3, LSL #1
	SUBHS	r12,r12,r3, LSL #1
	ADDHS	r14,r14,#1<<1
	CMP	r12,r3
	SUBHS	r12,r12,r3
	ADDHS	r14,r14,#1

	@ r2 = x = w
	@ r3 = y = denom
	MOV	r11,r2, LSL #2		@ r11= w = w*n
	RSB	r11,r11,#0		@ r11=-w
	MOV	r10,#1<<15		@ r10= 1<<15
.L_x_loop_c4:
	MOV	r4, #0			@ r4 = sum0 = 0
	MOV	r5, #0			@ r5 = sum1 = 0
	MOV	r6, #0			@ r6 = sum2 = 0
	MOV	r7, #0			@ r7 = sum3 = 0
.L_y_loop_c4:
	LDRB	r9, [r0, #1]		@ r9 = src[1]
	LDRB	r12,[r0, #2]		@ r12= src[2]
	SUBS	r3, r3, #1		@ r3 = y--
	ADD	r5, r5, r9		@ r4 = sum1 += r9
	LDRB	r9, [r0, #3]		@ r9 = src[3]
	ADD	r6, r6, r12		@ r5 = sum2 += r12
	LDRB	r12,[r0], -r11		@ r12= src[0]	src += w
	ADD	r7, r7, r9		@ r9 = sum3 += r9
	ADD	r4, r4, r12		@ r7 = sum0 += r12
	BGT	.L_y_loop_c4

	LDR	r3, [r13]		@ r3 = y = denom
	MLA	r4, r14,r4, r10		@ r4 = sum0 * invdenom + (1<<15)
	MLA	r5, r14,r5, r10		@ r5 = sum1 * invdenom + (1<<15)
	MLA	r6, r14,r6, r10		@ r6 = sum2 * invdenom + (1<<15)
	MLA	r7, r14,r7, r10		@ r7 = sum3 * invdenom + (1<<15)
	MLA	r0, r3, r11,r0		@ r0 = src += -denom*w
	MOV	r4, r4, LSR #16		@ r4 = r4 >> 16
	MOV	r5, r5, LSR #16		@ r5 = r5 >> 16
	MOV	r6, r6, LSR #16		@ r6 = r6 >> 16
	MOV	r7, r7, LSR #16		@ r7 = r7 >> 16
	ADD	r0, r0, #4		@ r0 = src += n
	STRB	r4, [r1], #1		@ *dst++ = r4
	STRB	r5, [r1], #1		@ *dst++ = r5
	STRB	r6, [r1], #1		@ *dst++ = r6
	STRB	r7, [r1], #1		@ *dst++ = r7
	SUBS	r2, r2, #1		@ x--
	BNE	.L_x_loop_c4

.L_end_c4:
	LDMFD	r13!,{r3-r7,r9-r11,PC}

	.fnend
	.size	fz_scol4_arm, .-fz_scol4_arm
