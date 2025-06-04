.. default-domain:: js

.. highlight:: javascript

Color
=====

Colors are specified as arrays with the appropriate number of components for
the associated `ColorSpace`. Each component value is stored as floating
point value between 0 and 1, where 0 means no colorant and 1
means maximum colorant.

For example:

- In the `ColorSpace.DeviceCMYK` color space the components are [Cyan, Magenta, Yellow, Black]. A full intensity magenta color would therefore be [0, 1, 0, 0].
- In the `ColorSpace.DeviceRGB` color space the components are [Red, Green, Blue]. A full intensity green color would therefore be [0, 1, 0].
- In the `ColorSpace.DeviceGray` color space the components are [Black]. A full intensity black color would therefore be [0].

In TypeScript this is defined as follows:

.. code-block::

	type Color = [number] | [number, number, number] | [number, number, number, number]
