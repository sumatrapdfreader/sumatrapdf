# Overview
This version of Selawik is a test case and demonstration of OpenType 1.8 Font Variations technology and tables. It also includes some handy debugging characters.

This version of Selawik is intended for testing only, and is not recommended for shipping in applications, etc. For that, it is better to use the main branch of [Selawik](https://github.com/Microsoft/Selawik).

# Features

* Full TrueType hinting with VTT source tables included. See [Hinting](#hinting) for details.
* All tables required for OpenType 1.8 are present (see [Table Status](#table-status), below). This includes cvar (varied CVTs), GPOS/GDEF (kerning varies), and avar (coordinate space warping to match Segoe UI).

	Note: This version of Selawik does not include an MVAR because its vertical metrics do not change anywhere in the design space, thus there is no need for MVAR. A future release will contain an axis that varies vertical metrics as an excuse to have an MVAR.

* Numerous interesting debugging glyphs (requires liga to be enabled). For example, \axis1 will show the current normalized wght coordinate. See [Debugging Glyphs](#debugging-glyphs) for details.
* 1 axis: weight

## Table status
The following tables are currently supported:

- [x] fvar
- [x] gvar
- [x] cvar
- [x] avar (to match Segoe UI weights and metrics)
- [x] STAT
- [x] GPOS/GDEF - kerning
- [x] HVAR

Not yet complete: 

- [ ] GSUB/GDEF - to change dollar signs in the bold
- [ ] MVAR (future release)

## To do
* Add a second axis that varies vertical metrics so we need an MVAR table. This axis will not be one of the standard axes listed in the [OpenType 1.8 specification] (https://www.microsoft.com/typography/otspec/fvar.htm), so that it becomes an example of out to do a foundry-defined axis.
* Add Feature Variations (GPOS/GDEF) to switch dollar sign glyphs across weights.


# Debugging glyphs
Thanks to Greg Hitchcock's TrueType coding wizardry, this font includes many glyphs that are helpful for debugging implementations of variable fonts. It has a number of substitutions implemented as liga features:

Feature | Description
-------- | ----------
\axis1 | Shows the normalized coordinate on the wght axis for the currently selected instance (e.g. 1.0 for bold, -1.0 for light, or something in between).
\axis2 | Shows 0 because this font doesn't yet have a second axis.
\axis1hex | Same as \axis1 but in hex.
\axis2hex | Same as \axis2 but in hex.
\pointsize | Shows the point size passed to the TrueType rasterizer. Note that depending on how the application calls the rasterizer, this may not be what you expect - e.g. on Safari on MacOS, this is always 1024.
\ppem | Shows the pixels per em passed to the TrueType rastersize. Note that depending on how the application calls the rasterizer, this may not be what you expect - e.g. on Safari on MacOS, this is always 1024.
\ttversion | Shows the version of the TrueType rasterizer.
\ttmode | Shows the current TrueType rasterizer mode flags.
\boldtest | A glyph to help you detect artificial emboldening. The glyph has a vertical bar and a circle. The vertical bar's weight varies with the weight of the rest of the font: it gets bolder at bolder weights, lighter at lighter weights. The circle changes weight (and size) in opposition to the rest of the font: lighter at bold weights and vice versa. Thus, if you use this character and see both the circle and bar look bold, you're not looking at a true bold instance, but an algorithmically emboldened one.
\family | Shows the family name of the font.
\version | Shows the version of this font




# Hinting
This version of Selawik is primarily hinted with the light Latin hinting style Microsoft recommends for variable Latin fonts. The VTT Light Latin autohinter was used to create the first round of hints, which were then reviewed and touched up. 

This hinting style only uses CVTs for vertical metrics anchors (ascender, descender, cap height, x-height, and baseline). While this makes for an easy job hinting a Latin font, it makes for a poor test case because Selawik doesn't vary vertical metrics with weight, thus doesn't vary CVTs, thus doesn't need a cvar. So, to make it more interesting, we added CVT-based stem hints to the lowercase only. This provided the need to vary CVTs and thus require a cvar. Note that this was only done for testing purposes. For variable fonts, Microsoft recommends the light hinting style using the `ResYDist()` command instead of a CVT-based stem hint. 
