# Document Writer Options

## Supported output formats

Raster formats:
: `cbz`, `png`, `pnm`, `pgm`, `ppm`, `pam`, `pbm`, `pkm`

Print-raster formats:
: `pcl`, `pclm`, `ps`, `pwg`

Vector formats:
: `pdf`, `svg`

Text formats:
: `html`, `xhtml`, `text`, `stext`

## Raster output options

rotate=N
: Rotate rendered pages N degrees counterclockwise.

resolution=N
: Set both X and Y resolution in pixels per inch.

x-resolution=N
: X resolution of rendered pages in pixels per inch.

y-resolution=N
: Y resolution of rendered pages in pixels per inch.

width=N
: Render pages to fit N pixels wide (ignore resolution option).

height=N
: Render pages to fit N pixels tall (ignore resolution option).

colorspace=(gray|rgb|cmyk)
: Render using specified colorspace.

alpha
: Render pages with alpha channel and transparent background.

graphics=(aaN|cop|app)
: Set the rasterizer to use for graphics.
	- `aaN` Antialias with N bits (0 to 8).
	- `cop` Center of pixel.
	- `app` Any part of pixel.

text=(aaN|cop|app)
: Set the rasterizer to use for text.
	- `aaN` Antialias with N bits (0 to 8).
	- `cop` Center of pixel.
	- `app` Any part of pixel.

## PCL output options

colorspace=mono
: Render 1-bit black and white page.

colorspace=rgb
: Render full color page.

preset
: Possible values are: generic|ljet4|dj500|fs600|lj|lj2|lj3|lj3d|lj4|lj4pl|lj4d|lp2563b|oce9050

spacing=0
: No vertical spacing capability.

spacing=1
: PCL 3 spacing (`<ESC>*p+\<n\>Y`).

spacing=2
: PCL 4 spacing (`<ESC>*b\<n\>Y`).

spacing=3
: PCL 5 spacing (`<ESC>*b\<n\>Y` and clear seed row).

mode2
: Enable mode 2 graphics compression.

mode3
: Enable mode 3 graphics compression.

eog\_reset
: End of graphics (`<ESC>*rB)` resets all parameters.

has\_duplex
: Duplex supported (`<ESC>&l\<duplex\>S`).

has\_papersize
: Papersize setting supported (`<ESC>&l\<sizecode\>A`).

has\_copies
: Number of copies supported (`<ESC>&l\<copies\>X`).

is\_ljet4pjl
: Disable/Enable HP 4PJL model-specific output.

is\_oce9050
: Disable/Enable Oce 9050 model-specific output.


## PCLm output options

compression=none
: No compression (default).

compression=flate
: Flate compression.

strip-height=N
: Strip height (default 16).


## PDF output options

See <a href="pdf-write-options.html">PDF Write Options</a>.

## PWG output options

media\_class=\<string\>
: Set the media\_class field.

media\_color=\<string\>
: Set the media\_color field.

media\_type=\<string\>
: Set the media\_type field.

output\_type=\<string\>
: Set the output\_type field.

rendering\_intent=\<string\>
: Set the rendering\_intent field.

page\_size\_name=\<string\>
: Set the page\_size\_name field.

advance\_distance=\<int\>
: Set the advance\_distance field.

advance\_media=\<int\>
: Set the advance\_media field.

collate=\<int\>
: Set the collate field.

cut\_media=\<int\>
: Set the cut\_media field.

duplex=\<int\>
: Set the duplex field.

insert\_sheet=\<int\>
: Set the insert\_sheet field.

jog=\<int\>
: Set the jog field.

leading\_edge=\<int\>
: Set the leading\_edge field.

manual\_feed=\<int\>
: Set the manual\_feed field.

media\_position=\<int\>
: Set the media\_position field.

media\_weight=\<int\>
: Set the media\_weight field.

mirror\_print=\<int\>
: Set the mirror\_print field.

negative\_print=\<int\>
: Set the negative\_print field.

num\_copies=\<int\>
: Set the num\_copies field.

orientation=\<int\>
: Set the orientation field.

output\_face\_up=\<int\>
: Set the output\_face\_up field.

page\_size\_x=\<int\>
: Set the page\_size\_x field.

page\_size\_y=\<int\>
: Set the page\_size\_y field.

separations=\<int\>
: Set the separations field.

tray\_switch=\<int\>
: Set the tray\_switch field.

tumble=\<int\>
: Set the tumble field.

media\_type\_num=\<int\>
: Set the media\_type\_num field.

compression=\<int\>
: Set the compression field.

row\_count=\<int\>
: Set the row\_count field.

row\_feed=\<int\>
: Set the row\_feed field.

row\_step=\<int\>
: Set the row\_step field.


## SVG output options

text=text
: Emit text as \<text\> elements (inaccurate fonts).

text=path
: Emit text as \<path\> elements (accurate fonts).

no-reuse-images
: Do not reuse images using \<symbol\> definitions.


## Text output options

See <a href="stext-options.html">Structured Text Options</a>.
