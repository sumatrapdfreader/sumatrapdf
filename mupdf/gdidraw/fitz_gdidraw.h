fz_device *fz_newgdidevice(HDC hDC);
HBITMAP fz_pixtobitmap(HDC hDC, fz_pixmap *pixmap, BOOL paletted);
void fz_pixmaptodc(HDC hDC, fz_pixmap *pixmap, fz_rect *dest);
