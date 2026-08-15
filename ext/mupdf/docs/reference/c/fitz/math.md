# Math

We obviously need to deal with lots of points, rectangles, and transformations
in MuPDF.

## fz_point

Points are fairly self evident. The `fz_make_point` utility function is for use
with Visual Studio that doesn't yet support the C99 struct initializer syntax.

	typedef struct {
		float x, y;
	} fz_point;

	fz_point fz_make_point(float x, float y);

## fz_rect and fz_irect

Rectangles are represented by two pairs of coordinates. The `x0`, `y0` pair
have the smallest values, and in the normal coordinate space used by MuPDF that
is the upper left corner. The `x1`, `y1` pair have the largest values,
typically the lower right corner.

In order to represent an infinite unbounded area, we use an `x0` that is larger
than the `x1`.

	typedef struct {
		float x0, y0;
		float x1, y1;
	} fz_rect;

	const fz_rect fz_infinite_rect = { 1, 1, -1, -1 };
	const fz_rect fz_empty_rect = { 0, 0, 0, 0 };
	const fz_rect fz_unit_rect = { 0, 0, 1, 1 };

	fz_rect fz_make_rect(float x0, float y0, float x1, float y1);

## fz_matrix

Our matrix structure is a row-major 3x3 matrix with the last column always
`[ 0 0 1 ]`. This is represented as a struct with six fields, in the same order as
in PDF and Postscript. The identity matrix is a global constant, for easy
access.

	/ a b 0 \
	| c d 0 |
	\ e f 1 /

	typedef struct {
		float a, b, c, d, e, f;
	} fz_matrix;

	const fz_matrix fz_identity = { 1, 0, 0, 1, 0, 0 };

	fz_matrix fz_make_matrix(float a, float b, float c, float d, float e, float f);

## fz_quad

Sometimes we need to represent a non-axis aligned rectangular-ish area, such as
the area covered by some rotated text. For this we use a quad representation,
using a points for each of the upper/lower/left/right corners as seen from the
reading direction of the text represented.

	typedef struct {
		fz_point ul, ur, ll, lr;
	} fz_quad;

## List of math functions

These are simple mathematical operations that can not throw errors, so do not need a context argument.

`float fz_abs(float f)`
:	Abs for float.

`float fz_min(float a, float b)`
:	Min for float.

`float fz_max(float a, float b)`
:	Max for float.

`float fz_clamp(float f, float min, float max)`
:	Clamp for float.

`int fz_absi(int i)`
:	Abs for integer.

`int fz_mini(int a, int b)`
:	Min for integer.

`int fz_maxi(int a, int b)`
:	Max for integer.

`int fz_clampi(int i, int min, int max)`
:	Clamp for integer.

`int fz_is_empty_rect(fz_rect r)`
:	Returns whether the supplied `fz_rect` is empty.

`int fz_is_infinite_rect(fz_rect r)`
:	Returns whether the supplied `fz_rect` is infinite.

`fz_matrix fz_concat(fz_matrix left, fz_matrix right)`
:	Concat two matrices and returns a new matrix.

`fz_matrix fz_scale(float sx, float sy)`
:	Scale.

`fz_matrix fz_shear(float sx, float sy)`
:	Shear.

`fz_matrix fz_rotate(float degrees)`
:	Rotate.

`fz_matrix fz_translate(float tx, float ty)`
:	Translate.

`fz_matrix fz_invert_matrix(fz_matrix matrix)`
:	Invert a matrix.

`fz_point fz_transform_point(fz_point point, fz_matrix m)`
:	Transform a point according to the given matrix.

`fz_point fz_transform_vector(fz_point vector, fz_matrix m)`
:	Transform a vector according to the given matrix (ignores translation).

`fz_rect fz_transform_rect(fz_rect rect, fz_matrix m)`
:	Transform a `fz_rect` according to the given matrix.

`fz_quad fz_transform_quad(fz_quad q, fz_matrix m)`
:	Transform a `fz_quad` according to the given matrix.

`int fz_is_point_inside_rect(fz_point p, fz_rect r)`
:	Returns whether the point is inside the supplied `fz_rect`.

`int fz_is_point_inside_quad(fz_point p, fz_quad q)`
:	Returns whether the point is inside the supplied `fz_quad`.

`fz_matrix fz_transform_page(fz_rect mediabox, float resolution, float rotate)`
:	Create a transform matrix to draw a page at a given resolution and
	rotation. The scaling factors are adjusted so that the page covers a
	whole number of pixels. Resolution is given in dots per inch. Rotation
	is expressed in 90 degree increments (`0`, `90`, `180`, and `270` are
	valid values).
