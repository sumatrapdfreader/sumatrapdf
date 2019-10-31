package com.artifex.mupdf.fitz;

public final class ColorParams
{
	public enum RenderingIntent {
		PERCEPTUAL,
		RELATIVE_COLORIMETRIC,
		SATURATION,
		ABSOLUTE_COLORIMETRIC
	}
	public static final int BP = 32;
	public static final int OP = 64;
	public static final int OPM = 128;

	public static RenderingIntent RI(int flags) {
		switch (flags & 3) {
			default:
			case 0: return RenderingIntent.PERCEPTUAL;
			case 1: return RenderingIntent.RELATIVE_COLORIMETRIC;
			case 2: return RenderingIntent.SATURATION;
			case 3: return RenderingIntent.ABSOLUTE_COLORIMETRIC;
		}
	}

	public static boolean BP(int flags) {
		return (flags & BP) != 0;
	}

	public static boolean OP(int flags) {
		return (flags & OP) != 0;
	}

	public static boolean OPM(int flags) {
		return (flags & OPM) != 0;
	}

	public static int pack(RenderingIntent ri, boolean bp, boolean op, boolean opm) {
		int flags;
		switch (ri) {
		default:
		case PERCEPTUAL: flags = 0; break;
		case RELATIVE_COLORIMETRIC: flags = 1; break;
		case SATURATION: flags = 2; break;
		case ABSOLUTE_COLORIMETRIC: flags = 3; break;
		}
		if (bp) flags |= BP;
		if (op) flags |= OP;
		if (opm) flags |= OPM;
		return flags;
	}
}
