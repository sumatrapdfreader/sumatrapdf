# Colors

Colors throughout MuPDF are represented as a float vector associated with a
colorspace object. Colorspaces come in many variants, the most common of
which are Gray, RGB, and CMYK. We also support Indexed (palette), L\*a\*b,
Separation, DeviceN, and ICC based colorspaces.

	enum { FZ_MAX_COLORS = 32 };

	enum fz_colorspace_type {
		FZ_COLORSPACE_NONE,
		FZ_COLORSPACE_GRAY,
		FZ_COLORSPACE_RGB,
		FZ_COLORSPACE_BGR,
		FZ_COLORSPACE_CMYK,
		FZ_COLORSPACE_LAB,
		FZ_COLORSPACE_INDEXED,
		FZ_COLORSPACE_SEPARATION,
	};

	struct fz_colorspace_s {
		enum fz_colorspace_type type;
		int n;
		char *name;
		private internal fields
	};

	fz_colorspace *fz_keep_colorspace(fz_context *ctx, fz_colorspace *colorspace);
	void fz_drop_colorspace(fz_context *ctx, fz_colorspace *colorspace);

Most colorspaces have color components between `0` and `1`.
The number of components a color uses is given by the 'n' field of the colorspace.
This is at most `FZ_MAX_COLORS`.

L\*a\*b colors have a range of `0..100` for the L* and `-128..127` for a* and b*.

CMYK, Separation, and DeviceN colorspaces are subtractive colorspaces.
Separation and DeviceN colorspaces also have a tint transform function and a
base colorspace used to represent the colors when rendering to a device that
does not have these specific colorants.


## Color Management

MuPDF is usually built with a color management engine, but this can be turned
off at runtime if you need faster performance at the cost of much more
inaccurate color conversions.

	int fz_enable_icc(fz_context *ctx);
	int fz_disable_icc(fz_context *ctx);

The various color conversion functions also take certain parameters that the
color management engine uses, such as rendering intent and overprint control.

	enum {
		FZ_RI_PERCEPTUAL,
		FZ_RI_RELATIVE_COLORIMETRIC,
		FZ_RI_SATURATION,
		FZ_RI_ABSOLUTE_COLORIMETRIC,
	};

	typedef struct {
		int ri; // Rendering intent
		int bp; // Black point compensation
		int op;
		int opm;
	} fz_color_params;

	const fz_color_params fz_default_color_params = { FZ_RI_RELATIVE_COLORIMETRIC, 1, 0, 0 };

## Device colorspaces

When you need to define a color, and don't care too much about the calibration,
you can use the Device colorspaces. When MuPDF is built with a color management
engine and ICC is enabled the Gray and RGB colorspaces use an sRGB ICC profile.

	fz_colorspace *fz_device_gray(fz_context *ctx)
	fz_colorspace *fz_device_rgb(fz_context *ctx)
	fz_colorspace *fz_device_bgr(fz_context *ctx)
	fz_colorspace *fz_device_cmyk(fz_context *ctx)
	fz_colorspace *fz_device_lab(fz_context *ctx)

BGR is present to allow you to render to pixmaps that have the RGB components
in a different order, so that the data can be passed directly to the operating
system for drawing without needing yet another conversion step.


## Indexed colorspaces

Indexed colors have a range of `0..N` where `N` is one less than the number of
colors in the palette. An indexed colorspace also has a base colorspace, which
is used to define the palette of colors used.

This color space type is only used internally in PDF and some image formats.

	fz_colorspace *fz_new_indexed_colorspace(fz_context *ctx,
		fz_colorspace *base,
		int high,
		unsigned char *lookup
	)

High is the maximum value in the palette; i.e. one less than the number of colors.

The lookup argument is a packed array of color values in the base colorspace,
represented as bytes mapped to the range of `0..255`.

## ICC colorspaces

You can create ICC colorspaces from a buffer containing the ICC profile.

	fz_colorspace *fz_new_icc_colorspace(fz_context *ctx,
		enum fz_colorspace_type type,
		int flags,
		const char *name,
		fz_buffer *buf
	)


The `type` argument can be `NONE` if you want to automatically infer the colorspace type from the profile data. If the type is anything else, then an error will be thrown if the profile does not match the type.


## Color converters

If you have a color in one space, you often need it in another (such as when
rendering CMYK images to an RGB screen).

There are several ways to convert colors. The easiest is to call a function,
but if you are converting many colors at once, it will be faster to use a color
converter object.

	void fz_convert_color(fz_context *ctx,
		fz_colorspace *src_colorspace,
		const float *src_color,
		fz_colorspace *dst_colorspace,
		float *dst_color,
		fz_colorspace *proof_colorspace,
		const fz_color_params params
	);

	typedef struct {
		void (*convert)(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst);
		private internal fields
	} fz_color_converter;

	void fz_find_color_converter(fz_context *ctx, fz_color_converter *cc,
		fz_colorspace *src_colorspace,
		fz_colorspace *dst_colorspace,
		fz_colorspace *proof_colorspace,
		fz_color_params params
	);

	void fz_drop_color_converter(fz_context *ctx, fz_color_converter *cc);


Here is some sample code to do a one-off CMYK to RGB color conversion.

	float cmyk[4] = { 1, 0, 0, 0 };
	float rgb[3];
	fz_convert_color(ctx,
		fz_device_cmyk(ctx),
		cmyk,
		fz_device_rgb(ctx),
		rgb,
		NULL,
		fz_default_color_params(ctx)
	);

Here is some sample code to do repeated CMYK to RGB color conversions on many
colors using a color converter object.


	float cmyk[100][4] = { {1,0,0,0}, ...
	float rgb[100][3];
	fz_color_converter cc;
	int i;

	fz_find_color_converter(ctx,
		&cc,
		fz_device_cmyk(ctx),
		fz_device_rgb(ctx),
		NULL,
		fz_default_color_params(ctx)
	);
	for (i = 0; i < 100; ++i)
		cc.convert(ctx, &cc, cmyk[i], rgb[i]);
	fz_drop_color_converter(ctx, &cc);
