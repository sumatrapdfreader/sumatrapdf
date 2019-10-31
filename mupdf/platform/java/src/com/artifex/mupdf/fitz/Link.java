package com.artifex.mupdf.fitz;

public class Link
{
	public Rect bounds;
	public String uri;

	public Link(Rect bounds, String uri) {
		this.bounds = bounds;
		this.uri = uri;
	}

	public boolean isExternal() {
		for (int i = 0; i < uri.length(); i++)
		{
			char c = uri.charAt(i);
			if (c >= 'a' && c <= 'z')
				continue;
			else
				return c == ':';
		}
		return false;
	}

	public String toString() {
		return "Link(bounds="+bounds+",uri="+uri+")";
	}
}
