.. default-domain:: js

.. highlight:: javascript

PDFProcessor
======================

A PDF processor object provides callbacks that will be called for each
PDF operator when it is passed to `PDFAnnotation.prototype.process` and
`PDFPage.prototype.process`. The callbacks correspond to the equivalent
PDF operator. Refer to the PDF specification's section on `graphic
operators
<https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf#G9.3859555>`_
for what these do and what the callback arguments are.

Constructors
------------

.. class:: PDFProcessor(callbacks)

	|interface_type|

Special resource tracking
-------------------------

These are not operators per se, but are called when the
current resource dictionary used changes such as when
executing XObject forms.

- push_resources(resources)
- pop_resources()

General graphics state callbacks
-------------------------------------------

- op_w(lineWidth: number)
- op_j(lineJoin: number)
- op_J(lineCap: number)
- op_M(miterLimit: number)
- op_d(dashPattern: Array of number, phase: number)
- op_ri(intent: string)
- op_i(flatness: number)
- op_gs(name: string, extGState: `PDFObject`)

Special graphics state
-------------------------------------------

- op_q()
- op_Q()
- op_cm(a: number, b: number, c: number, d: number, e: number, f: number)

Path construction
-------------------------------------------

- op_m(x: number, y: number)
- op_l(x: number, y: number)
- op_c(x1: number, y1: number, x2: number, y2: number, x3: number, y3: number)
- op_v(x2: number, y2: number, x3: number, y3: number)
- op_y(x1: number, y1: number, x3: number, y3: number)
- op_h()
- op_re(x: number, y: number, w: number, h: number)

Path painting
-------------------------------------------

- op_S()
- op_s()
- op_F()
- op_f()
- op_fstar()
- op_B()
- op_Bstar()
- op_b()
- op_bstar()
- op_n()

Clipping paths
-------------------------------------------

- op_W()
- op_Wstar()

Text objects
-------------------------------------------

- op_BT()
- op_ET()

Text state
-------------------------------------------

- op_Tc(charSpace: number)
- op_Tw(wordSpace: number)
- op_Tz(scale: number)
- op_TL(leading: number)
- op_Tf(name: string, size: number)
- op_Tr(render: number)
- op_Ts(rise: number)

Text positioning
-------------------------------------------

- op_Td(tx: number, ty: number)
- op_TD(tx: number, ty: number)
- op_Tm(a: number, b: number, c: number, d: number, e: number, f: number)
- op_Tstar()

Text showing
-------------------------------------------

- op_TJ(textArray: Array of (string | number))
- op_Tj(stringOrByteArray: string | Array of number)
- op_squote(stringOrByteArray: string | Array of number)
- op_dquote(wordSpace: number, charSpace: number, stringOrByteArray: string | Array of number)

Type 3 fonts
-------------------------------------------

- op_d0(wx: number, wy: number)
- op_d1(wx: number, wy: number, llx: number, lly: number, urx: number, ury: number)

Color
-------------------------------------------

- op_CS(name: string, colorspace: `ColorSpace`)
- op_cs(name: string, colorspace: `ColorSpace`)
- op_SC_color(color: Array of number)
- op_sc_color(color: Array of number)

- op_SC_pattern(name: string, patternID: number, color: Array of number)
- op_sc_pattern(name: string, patternID: number, color: Array of number)
- op_SC_shade(name: string, shade: `Shade`)
- op_sc_shade(name: string, shade: `Shade`)

- op_G(gray: number)
- op_g(gray: number)
- op_RG(r: number, g: number, b: number)
- op_rg(r: number, g: number, b: number)
- op_K(c: number, m: number, y: number, k: number)
- op_k(c: number, m: number, y: number, k: number)

Shadings
-------------------------------------------

- op_sh(name: string, shade: Shade)

Inline images
-------------------------------------------

- op_BI(image: `Image`, colorspace: `ColorSpace`)

XObjects (Images and Forms)
-------------------------------------------

- op_Do_image(name: string, image: `Image`)
- op_Do_form(name: string, xobject: `PDFObject`, resources: `PDFObject`)

Marked content
-------------------------------------------

- op_MP(tag: string)
- op_DP(tag: string, raw: string)
- op_BMC(tag: string)
- op_BDC(tag: string, raw: string)
- op_EMC()

Compatibility
-------------------------------------------

- op_BX()
- op_EX()
