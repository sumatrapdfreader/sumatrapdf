package com.artifex.mupdf.fitz;

public class Separation
{
	public String name;
	public int bgra;
	public int cmyk;

	public Separation(String name, int bgra, int cmyk)
	{
		this.name = name;
		this.bgra = bgra;
		this.cmyk = cmyk;
	}
}
