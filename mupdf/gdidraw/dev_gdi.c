#include <windows.h>
#include <fitz.h>
#include <fitz_gdidraw.h>

fz_device *
fz_newgdidevice(HDC hDC)
{
#ifndef _DEBUG
	return NULL; // don't make release builds depend on GDI+ yet
#else
	return fz_newgdiplusdevice(hDC);
#endif
}

HBITMAP
fz_pixtobitmap(HDC hDC, fz_pixmap *pixmap, BOOL paletted)
{
	int w, h, rows8;
	int paletteSize = 0;
	BOOL hasPalette = FALSE;
	int i, j, k;
	BITMAPINFO *bmi;
	HBITMAP hbmp = NULL;
	unsigned char *bmpData, *source, *dest;
	fz_pixmap *bgrPixmap;
	
	w = pixmap->w;
	h = pixmap->h;
	
	/* abgr is a GDI compatible format */
	bgrPixmap = fz_newpixmap(fz_devicebgr, pixmap->x, pixmap->y, w, h);
	fz_convertpixmap(pixmap, bgrPixmap);
	pixmap = bgrPixmap;
	
	assert(pixmap->n == 4);
	
	bmi = fz_malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	memset(bmi, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	
	if (paletted)
	{
		rows8 = ((w + 3) / 4) * 4;	
		dest = bmpData = fz_malloc(rows8 * h);
		source = pixmap->samples;
		
		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				RGBQUAD c = { 0 };
				
				c.rgbBlue = *source++;
				c.rgbGreen = *source++;
				c.rgbRed = *source++;
				source++;
				
				/* find this color in the palette */
				for (k = 0; k < paletteSize; k++)
					if (*(int *)&bmi->bmiColors[k] == *(int *)&c)
						break;
				/* add it to the palette if it isn't in there and if there's still space left */
				if (k == paletteSize)
				{
					if (k >= 256)
						goto ProducingPaletteDone;
					*(int *)&bmi->bmiColors[paletteSize] = *(int *)&c;
					paletteSize++;
				}
				/* 8-bit data consists of indices into the color palette */
				*dest++ = k;
			}
			dest += rows8 - w;
		}
ProducingPaletteDone:
		hasPalette = paletteSize <= 256;
		if (!hasPalette)
			fz_free(bmpData);
	}
	if (!hasPalette)
		bmpData = pixmap->samples;
	
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = w;
	bmi->bmiHeader.biHeight = -h;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biBitCount = hasPalette ? 8 : 32;
	bmi->bmiHeader.biSizeImage = h * (hasPalette ? rows8 : w * 4);
	bmi->bmiHeader.biClrUsed = hasPalette ? paletteSize : 0;
	
	hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT, bmpData, bmi, DIB_RGB_COLORS);
	
	fz_droppixmap(bgrPixmap);
	if (hasPalette)
		fz_free(bmpData);
	fz_free(bmi);
	
	return hbmp;
}

void
fz_pixmaptodc(HDC hDC, fz_pixmap *pixmap, fz_rect *dest)
{
	// Try to extract a 256 color palette so that we can use 8-bit images instead of 24-bit ones.
	// This should e.g. speed up printing for most monochrome documents by saving spool resources.
	HBITMAP hbmp = fz_pixtobitmap(hDC, pixmap, TRUE);
	HDC bmpDC = CreateCompatibleDC(hDC);
	
	SelectObject(bmpDC, hbmp);
	if (!dest)
		BitBlt(hDC, 0, 0, pixmap->w, pixmap->h, bmpDC, 0, 0, SRCCOPY);
	else if (dest->x1 - dest->x0 == pixmap->w && dest->y1 - dest->y0 == pixmap->h)
		BitBlt(hDC, dest->x0, dest->y0, pixmap->w, pixmap->h, bmpDC, 0, 0, SRCCOPY);
	else
		StretchBlt(hDC, dest->x0, dest->y0, dest->x1 - dest->x0, dest->y1 - dest->y0,
			bmpDC, 0, 0, pixmap->w, pixmap->h, SRCCOPY);
	
	DeleteDC(bmpDC);
	DeleteObject(hbmp);
}
