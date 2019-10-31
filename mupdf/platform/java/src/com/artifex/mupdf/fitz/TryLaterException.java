package com.artifex.mupdf.fitz;

public class TryLaterException extends RuntimeException
{
	TryLaterException(String message) {
		super(message);
	}
}
