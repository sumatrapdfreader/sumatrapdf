# Pixmaps

A pixmap is an 8-bit per component raster image. Each pixel is packed with
process colorants, spot colors, and alpha channel in that order. If an alpha
channel is present, the process colorants are pre-multiplied with the alpha
value.

	typedef struct {
		int w, h; // Width and height
		int x, y; // X and Y offset
		int n; // Number of components in total (colors + spots + alpha)
		int s; // Number of components that are spot colors
		int alpha; // True if alpha channel is present
		int stride; // Number of bytes per row
		int xres, yres; // Resolution in dots per inch.
		fz_colorspace *colorspace; // Colorspace of samples, or NULL if alpha only pixmap.
		unsigned char *samples;
		private internal fields
	} fz_pixmap;

	fz_pixmap *fz_keep_pixmap(fz_context *ctx, fz_pixmap *pix);
	void fz_drop_pixmap(fz_context *ctx, fz_pixmap *pix);

There are too many pixmap constructors. Here is the only one you should need.

	fz_pixmap *fz_new_pixmap(fz_context *ctx, fz_colorspace *cs, int w, int h, fz_separations *seps, int alpha);


A newly created pixmap has uninitialized data. The samples must either be
cleared or overwritten with existing data before the pixmap can be safely used.

`void fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix);`
:	Clear the pixmap to black.

`void fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value);`
:	Clear the pixmap to a grayscale value `0..255`, where `0` is black and
	`255` is white. The value is automatically inverted for subtractive
	colorspaces.

`void fz_fill_pixmap_with_color(fz_context *ctx, fz_pixmap *pix, fz_colorspace *colorspace, float *color, fz_color_params color_params);`
:	Fill the pixmap with a solid color.

`void fz_unpack_tile(fz_context *ctx, fz_pixmap *dst, unsigned char *src, int n, int depth, size_t stride, int scale);`
:	Unpack pixel values from source data to fill in the pixmap samples. `n`
	is the number of samples per pixel, `depth` is the bit depth (`1`, `2`,
	`4`, `8`, `16`, `24`, or `32`), `stride` is the number of bytes per
	row. If `scale` is non-zero, it is the scaling factor to apply to the
	input samples to map them to the 8-bpc pixmap range. Pass `1` to the
	scale for indexed images, and `0` for everything else. If there are
	more components in the source data than the destination, they will be
	dropped. If there are fewer components in the source data, the pixmap
	will be padded with `255`.



Some functions can create a pixmap and initialize its samples in one go:

.. code-block:: c

	fz_pixmap *fz_new_pixmap_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *data, int stride);
	fz_pixmap *fz_new_pixmap_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *data, int stride);


Pixmaps can be tinted, inverted, scaled, gamma corrected, and converted to other colorspaces.



`void fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix);`
:	Invert the pixmap samples.

`void fz_tint_pixmap(fz_context *ctx, fz_pixmap *pix, int black, int white);`
:	Map black to black and white to white. The black and white colors are
	represented as a packed RGB integer. `0xFFFFFF` is white, `0xFF0000` is
	red, and `0x000000` is black.

`void fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma);`
:	Apply a gamma correction curve on the samples. A typical use is to
	adjust the gamma curve on an inverted image by applying a correction
	factor of 1/1.4.

`fz_pixmap *fz_convert_pixmap(fz_context *ctx, fz_pixmap *source_pixmap, fz_colorspace *destination_colorspace, fz_colorspace *proof_colorspace, fz_default_colorspaces *default_cs, fz_color_params color_params, int keep_alpha);`
:	Convert the source pixmap into the destination colorspace. Pass `NULL`
	for the `default_cs` parameter.

`fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip);`
:	Scale the pixmap up or down in size to fit the rectangle. Will return
	`NULL` if the scaling factors are out of range. This applies fancy
	filtering and will anti-alias the edges for subpixel positioning if
	using non-integer coordinates. If the clip rectangle is set, the
	returned pixmap may be subset to fit the clip rectangle. Pass `NULL` to
	the clip if you want the whole pixmap scaled.
