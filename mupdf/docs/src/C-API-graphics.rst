.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


Graphics API
-----------------------------------------



This module is a grab bag of various graphics related objects and functions.

Colors
-----------------------------------------

Colors throughout :title:`MuPDF` are represented as a float vector associated with a `colorspace` object. `Colorspaces` come in many variants, the most common of which are :title:`Gray`, :title:`RGB`, and :title:`CMYK`. We also support :title:`Indexed` (palette), :title:`L*a*b*`, :title:`Separation`, :title:`DeviceN`, and :title:`ICC` based colorspaces.


.. code-block:: c

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



Most colorspace` have color components between `0` and `1`. The number of components a color uses is given by the 'n' field of the colorspace. This is at most `FZ_MAX_COLORS`.

:title:`L*a*b*` colors have a range of `0..100` for the :title:`L*` and `-128..127` for :title:`a*` and :title:`b*`.

:title:`CMYK`, :title:`Separation`, and :title:`DeviceN` colorspaces are subtractive colorspaces. :title:`Separation` and :title:`DeviceN` colorspaces also have a tint transform function and a base colorspace used to represent the colors when rendering to a device that does not have these specific colorants.


Color management engine
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:title:`MuPDF` is usually built with a color management engine, but this can be turned off at runtime if you need faster performance at the cost of much more inaccurate color conversions.

.. code-block:: c

   int fz_enable_icc(fz_context *ctx);
   int fz_disable_icc(fz_context *ctx);

The various color conversion functions also take certain parameters that the color management engine uses, such as rendering intent and overprint control.

.. code-block:: c

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



:title:`Device` colorspaces
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When you need to define a color, and don't care too much about the calibration, you can use the :title:`Device` colorspaces. When :title:`MuPDF` is built with a color management engine and :title:`ICC` is enabled the :title:`Gray` and :title:`RGB` colorspaces use an :title:`sRGB` :title:`ICC` profile.

.. code-block:: c

   fz_colorspace *fz_device_gray(fz_context *ctx);
   fz_colorspace *fz_device_rgb(fz_context *ctx);
   fz_colorspace *fz_device_bgr(fz_context *ctx);
   fz_colorspace *fz_device_cmyk(fz_context *ctx);
   fz_colorspace *fz_device_lab(fz_context *ctx);

:title:`BGR` is present to allow you to render to pixmaps that have the :title:`RGB` components in a different order, so that the data can be passed directly to the operating system for drawing without needing yet another conversion step.


:title:`Indexed` colorspaces
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:title:`Indexed` colors have a range of `0..N` where `N` is one less than the number of colors in the palette. An indexed colorspace also has a base colorspace, which is used to define the palette of colors used.


.. code-block:: c

   fz_colorspace *fz_new_indexed_colorspace(fz_context *ctx,
      fz_colorspace *base,
      int high,
      unsigned char *lookup);

High is the maximum value in the palette; i.e. one less than the number of colors.

The lookup argument is a packed array of color values in the base colorspace, represented as bytes mapped to the range of `0..255`.



:title:`ICC` colorspaces
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can create :title:`ICC` colorspaces from a buffer containing the :title:`ICC` profile.


.. code-block:: c

   fz_colorspace *fz_new_icc_colorspace(fz_context *ctx,
      enum fz_colorspace_type type,
      int flags,
      const char *name,
      fz_buffer *buf);


The `type` argument can be `NONE` if you want to automatically infer the colorspace type from the profile data. If the type is anything else, then an error will be thrown if the profile does not match the type.



Color converters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are several ways to convert colors. The easiest is to call a function, but if you are converting many colors at once, it will be faster to use a color converter object.


.. code-block:: c

   void fz_convert_color(fz_context *ctx,
   fz_colorspace *src_colorspace,
   const float *src_color,
   fz_colorspace *dst_colorspace,
   float *dst_color,
   fz_colorspace *proof_colorspace,
   const fz_color_params params);
   typedef struct {
   void (*convert)(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst);
   private internal fields
   } fz_color_converter;

   void fz_find_color_converter(fz_context *ctx, fz_color_converter *cc,
   fz_colorspace *src_colorspace,
   fz_colorspace *dst_colorspace,
   fz_colorspace *proof_colorspace,
   fz_color_params params);
   void fz_drop_color_converter(fz_context *ctx, fz_color_converter *cc);


Here is some sample code to do a one-off :title:`CMYK` to :title:`RGB` color conversion.


.. code-block:: c

   float cmyk[4] = { 1, 0, 0, 0 };
   float rgb[3];
   fz_convert_color(ctx, fz_device_cmyk(ctx), cmyk, fz_device_rgb(ctx), rgb, NULL, fz_default_color_params(ctx));


Here is some sample code to do repeated :title:`CMYK` to :title:`RGB` color conversions on many colors using a color converter object.


.. code-block:: c

   float cmyk[100][4] = { {1,0,0,0}, ...
   float rgb[100][3];
   int i;
   fz_color_converter cc;
   fz_find_color_converter(ctx, &cc, fz_device_cmyk(ctx), fz_device_rgb(ctx), NULL, fz_default_color_params(ctx));
   for (i = 0; i < 100; ++i)
      cc.convert(ctx, &cc, cmyk[i], rgb[i]);
   fz_drop_color_converter(ctx, &cc);


Pixmaps
--------------

A pixmap is an 8-bit per component raster image. Each pixel is packed with process colorants, spot colors, and alpha channel in that order. If an alpha channel is present, the process colorants are pre-multiplied with the alpha value.



.. code-block:: c

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

.. code-block:: c

   fz_pixmap *fz_new_pixmap(fz_context *ctx, fz_colorspace *cs, int w, int h, fz_separations *seps, int alpha);


A newly created pixmap has uninitialized data. The samples must either be cleared or overwritten with existing data before the pixmap can be safely used.


`void fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix);`
   Clear the pixmap to black.

`void fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value);`
   Clear the pixmap to a grayscale value `0..255`, where `0` is black and `255` is white. The value is automatically inverted for subtractive colorspaces.

`void fz_fill_pixmap_with_color(fz_context *ctx, fz_pixmap *pix, fz_colorspace *colorspace, float *color, fz_color_params color_params);`
   Fill the pixmap with a solid color.

`void fz_unpack_tile(fz_context *ctx, fz_pixmap *dst, unsigned char *src, int n, int depth, size_t stride, int scale);`
   Unpack pixel values from source data to fill in the pixmap samples. `n` is the number of samples per pixel, `depth` is the bit depth (`1`, `2`, `4`, `8`, `16`, `24`, or `32`), `stride` is the number of bytes per row. If `scale` is non-zero, it is the scaling factor to apply to the input samples to map them to the 8-bpc pixmap range. Pass `1` to the scale for indexed images, and `0` for everything else. If there are more components in the source data than the destination, they will be dropped. If there are fewer components in the source data, the pixmap will be padded with `255`.



Some functions can create a pixmap and initialize its samples in one go:

.. code-block:: c

   fz_pixmap *fz_new_pixmap_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *data, int stride);
   fz_pixmap *fz_new_pixmap_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *data, int stride);


Pixmaps can be tinted, inverted, scaled, gamma corrected, and converted to other colorspaces.



`void fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix);`
   Invert the pixmap samples.

`void fz_tint_pixmap(fz_context *ctx, fz_pixmap *pix, int black, int white);`
   Map black to black and white to white. The black and white colors are represented as a packed :title:`RGB` integer. `0xFFFFFF` is white, `0xFF0000` is red, and `0x000000` is black.

`void fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma);`
   Apply a gamma correction curve on the samples. A typical use is to adjust the gamma curve on an inverted image by applying a correction factor of 1/1.4.

`fz_pixmap *fz_convert_pixmap(fz_context *ctx, fz_pixmap *source_pixmap, fz_colorspace *destination_colorspace, fz_colorspace *proof_colorspace, fz_default_colorspaces *default_cs, fz_color_params color_params, int keep_alpha);`
   Convert the source pixmap into the destination colorspace. Pass `NULL` for the `default_cs` parameter.

`fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip);`
   Scale the pixmap up or down in size to fit the rectangle. Will return `NULL` if the scaling factors are out of range. This applies fancy filtering and will anti-alias the edges for subpixel positioning if using non-integer coordinates. If the clip rectangle is set, the returned pixmap may be subset to fit the clip rectangle. Pass `NULL` to the clip if you want the whole pixmap scaled.
