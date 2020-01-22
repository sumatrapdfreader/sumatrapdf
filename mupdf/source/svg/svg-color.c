#include "mupdf/fitz.h"
#include "svg-imp.h"

#include <string.h>

/* Color keywords (white, blue, fuchsia)
 * System color keywords (ActiveBorder, ButtonFace -- need to find reasonable defaults)
 * #fb0 (expand to #ffbb00)
 * #ffbb00
 * rgb(255,255,255)
 * rgb(100%,100%,100%)
 *
 * "red icc-color(profileName,255,0,0)" (not going to support for now)
 */

struct
{
	const char *name;
	float red, green, blue;
}
svg_predefined_colors[] =
{
	{ "aliceblue", 240, 248, 255 },
	{ "antiquewhite", 250, 235, 215 },
	{ "aqua", 0, 255, 255 },
	{ "aquamarine", 127, 255, 212 },
	{ "azure", 240, 255, 255 },
	{ "beige", 245, 245, 220 },
	{ "bisque", 255, 228, 196 },
	{ "black", 0, 0, 0 },
	{ "blanchedalmond", 255, 235, 205 },
	{ "blue", 0, 0, 255 },
	{ "blueviolet", 138, 43, 226 },
	{ "brown", 165, 42, 42 },
	{ "burlywood", 222, 184, 135 },
	{ "cadetblue", 95, 158, 160 },
	{ "chartreuse", 127, 255, 0 },
	{ "chocolate", 210, 105, 30 },
	{ "coral", 255, 127, 80 },
	{ "cornflowerblue", 100, 149, 237 },
	{ "cornsilk", 255, 248, 220 },
	{ "crimson", 220, 20, 60 },
	{ "cyan", 0, 255, 255 },
	{ "darkblue", 0, 0, 139 },
	{ "darkcyan", 0, 139, 139 },
	{ "darkgoldenrod", 184, 134, 11 },
	{ "darkgray", 169, 169, 169 },
	{ "darkgreen", 0, 100, 0 },
	{ "darkgrey", 169, 169, 169 },
	{ "darkkhaki", 189, 183, 107 },
	{ "darkmagenta", 139, 0, 139 },
	{ "darkolivegreen", 85, 107, 47 },
	{ "darkorange", 255, 140, 0 },
	{ "darkorchid", 153, 50, 204 },
	{ "darkred", 139, 0, 0 },
	{ "darksalmon", 233, 150, 122 },
	{ "darkseagreen", 143, 188, 143 },
	{ "darkslateblue", 72, 61, 139 },
	{ "darkslategray", 47, 79, 79 },
	{ "darkslategrey", 47, 79, 79 },
	{ "darkturquoise", 0, 206, 209 },
	{ "darkviolet", 148, 0, 211 },
	{ "deeppink", 255, 20, 147 },
	{ "deepskyblue", 0, 191, 255 },
	{ "dimgray", 105, 105, 105 },
	{ "dimgrey", 105, 105, 105 },
	{ "dodgerblue", 30, 144, 255 },
	{ "firebrick", 178, 34, 34 },
	{ "floralwhite", 255, 250, 240 },
	{ "forestgreen", 34, 139, 34 },
	{ "fuchsia", 255, 0, 255 },
	{ "gainsboro", 220, 220, 220 },
	{ "ghostwhite", 248, 248, 255 },
	{ "gold", 255, 215, 0 },
	{ "goldenrod", 218, 165, 32 },
	{ "gray", 128, 128, 128 },
	{ "green", 0, 128, 0 },
	{ "greenyellow", 173, 255, 47 },
	{ "grey", 128, 128, 128 },
	{ "honeydew", 240, 255, 240 },
	{ "hotpink", 255, 105, 180 },
	{ "indianred", 205, 92, 92 },
	{ "indigo", 75, 0, 130 },
	{ "ivory", 255, 255, 240 },
	{ "khaki", 240, 230, 140 },
	{ "lavender", 230, 230, 250 },
	{ "lavenderblush", 255, 240, 245 },
	{ "lawngreen", 124, 252, 0 },
	{ "lemonchiffon", 255, 250, 205 },
	{ "lightblue", 173, 216, 230 },
	{ "lightcoral", 240, 128, 128 },
	{ "lightcyan", 224, 255, 255 },
	{ "lightgoldenrodyellow", 250, 250, 210 },
	{ "lightgray", 211, 211, 211 },
	{ "lightgreen", 144, 238, 144 },
	{ "lightgrey", 211, 211, 211 },
	{ "lightpink", 255, 182, 193 },
	{ "lightsalmon", 255, 160, 122 },
	{ "lightseagreen", 32, 178, 170 },
	{ "lightskyblue", 135, 206, 250 },
	{ "lightslategray", 119, 136, 153 },
	{ "lightslategrey", 119, 136, 153 },
	{ "lightsteelblue", 176, 196, 222 },
	{ "lightyellow", 255, 255, 224 },
	{ "lime", 0, 255, 0 },
	{ "limegreen", 50, 205, 50 },
	{ "linen", 250, 240, 230 },
	{ "magenta", 255, 0, 255 },
	{ "maroon", 128, 0, 0 },
	{ "mediumaquamarine", 102, 205, 170 },
	{ "mediumblue", 0, 0, 205 },
	{ "mediumorchid", 186, 85, 211 },
	{ "mediumpurple", 147, 112, 219 },
	{ "mediumseagreen", 60, 179, 113 },
	{ "mediumslateblue", 123, 104, 238 },
	{ "mediumspringgreen", 0, 250, 154 },
	{ "mediumturquoise", 72, 209, 204 },
	{ "mediumvioletred", 199, 21, 133 },
	{ "midnightblue", 25, 25, 112 },
	{ "mintcream", 245, 255, 250 },
	{ "mistyrose", 255, 228, 225 },
	{ "moccasin", 255, 228, 181 },
	{ "navajowhite", 255, 222, 173 },
	{ "navy", 0, 0, 128 },
	{ "oldlace", 253, 245, 230 },
	{ "olive", 128, 128, 0 },
	{ "olivedrab", 107, 142, 35 },
	{ "orange", 255, 165, 0 },
	{ "orangered", 255, 69, 0 },
	{ "orchid", 218, 112, 214 },
	{ "palegoldenrod", 238, 232, 170 },
	{ "palegreen", 152, 251, 152 },
	{ "paleturquoise", 175, 238, 238 },
	{ "palevioletred", 219, 112, 147 },
	{ "papayawhip", 255, 239, 213 },
	{ "peachpuff", 255, 218, 185 },
	{ "peru", 205, 133, 63 },
	{ "pink", 255, 192, 203 },
	{ "plum", 221, 160, 221 },
	{ "powderblue", 176, 224, 230 },
	{ "purple", 128, 0, 128 },
	{ "red", 255, 0, 0 },
	{ "rosybrown", 188, 143, 143 },
	{ "royalblue", 65, 105, 225 },
	{ "saddlebrown", 139, 69, 19 },
	{ "salmon", 250, 128, 114 },
	{ "sandybrown", 244, 164, 96 },
	{ "seagreen", 46, 139, 87 },
	{ "seashell", 255, 245, 238 },
	{ "sienna", 160, 82, 45 },
	{ "silver", 192, 192, 192 },
	{ "skyblue", 135, 206, 235 },
	{ "slateblue", 106, 90, 205 },
	{ "slategray", 112, 128, 144 },
	{ "slategrey", 112, 128, 144 },
	{ "snow", 255, 250, 250 },
	{ "springgreen", 0, 255, 127 },
	{ "steelblue", 70, 130, 180 },
	{ "tan", 210, 180, 140 },
	{ "teal", 0, 128, 128 },
	{ "thistle", 216, 191, 216 },
	{ "tomato", 255, 99, 71 },
	{ "turquoise", 64, 224, 208 },
	{ "violet", 238, 130, 238 },
	{ "wheat", 245, 222, 179 },
	{ "white", 255, 255, 255 },
	{ "whitesmoke", 245, 245, 245 },
	{ "yellow", 255, 255, 0 },
	{ "yellowgreen", 154, 205, 50 },
};

static int unhex(int chr)
{
	const char *hextable = "0123456789abcdef";
	return strchr(hextable, (chr|32)) - hextable;
}

static int ishex(int chr)
{
	if (chr >= '0' && chr <= '9') return 1;
	if (chr >= 'A' && chr <= 'F') return 1;
	if (chr >= 'a' && chr <= 'f') return 1;
	return 0;
}

void
svg_parse_color(fz_context *ctx, svg_document *doc, const char *str, float *rgb)
{
	int i, l, m, r, cmp;
	size_t n;

	rgb[0] = 0.0f;
	rgb[1] = 0.0f;
	rgb[2] = 0.0f;

	/* Crack hex-coded RGB */

	if (str[0] == '#')
	{
		str ++;

		n = strlen(str);
		if (n == 3 || (n > 3 && !ishex(str[3])))
		{
			rgb[0] = (unhex(str[0]) * 16 + unhex(str[0])) / 255.0f;
			rgb[1] = (unhex(str[1]) * 16 + unhex(str[1])) / 255.0f;
			rgb[2] = (unhex(str[2]) * 16 + unhex(str[2])) / 255.0f;
			return;
		}

		if (n >= 6)
		{
			rgb[0] = (unhex(str[0]) * 16 + unhex(str[1])) / 255.0f;
			rgb[1] = (unhex(str[2]) * 16 + unhex(str[3])) / 255.0f;
			rgb[2] = (unhex(str[4]) * 16 + unhex(str[5])) / 255.0f;
			return;
		}

		return;
	}

	/* rgb(X,Y,Z) -- whitespace allowed around numbers */

	else if (strstr(str, "rgb("))
	{
		int numberlen = 0;
		char numberbuf[50];

		str = str + 4;

		for (i = 0; i < 3; i++)
		{
			while (svg_is_whitespace_or_comma(*str))
				str ++;

			if (svg_is_digit(*str))
			{
				numberlen = 0;
				while (svg_is_digit(*str) && numberlen < (int)sizeof(numberbuf) - 1)
					numberbuf[numberlen++] = *str++;
				numberbuf[numberlen] = 0;

				if (*str == '%')
				{
					str ++;
					rgb[i] = fz_atof(numberbuf) / 100.0f;
				}
				else
				{
					rgb[i] = fz_atof(numberbuf) / 255.0f;
				}
			}
		}

		return;
	}

	/* TODO: parse icc-profile(X,Y,Z,W) syntax */

	/* Search for a pre-defined color */

	else
	{
		char keyword[50], *p;
		fz_strlcpy(keyword, str, sizeof keyword);
		p = keyword;
		while (*p && *p >= 'a' && *p <= 'z')
			++p;
		*p = 0;

		l = 0;
		r = sizeof(svg_predefined_colors) / sizeof(svg_predefined_colors[0]);

		while (l <= r)
		{
			m = (l + r) / 2;
			cmp = strcmp(svg_predefined_colors[m].name, keyword);
			if (cmp > 0)
				r = m - 1;
			else if (cmp < 0)
				l = m + 1;
			else
			{
				rgb[0] = svg_predefined_colors[m].red / 255.0f;
				rgb[1] = svg_predefined_colors[m].green / 255.0f;
				rgb[2] = svg_predefined_colors[m].blue / 255.0f;
				return;
			}
		}
	}
}

void
svg_parse_color_from_style(fz_context *ctx, svg_document *doc, const char *str,
	int *fill_is_set, float fill[3],
	int *stroke_is_set, float stroke[3])
{
	const char *p;

	p = strstr(str, "fill:");
	if (p)
	{
		p += 5;
		while (*p && svg_is_whitespace(*p))
			++p;
		if (strncmp(p, "none", 4) != 0)
		{
			svg_parse_color(ctx, doc, p, fill);
			*fill_is_set = 1;
		}
	}

	p = strstr(str, "stroke:");
	if (p)
	{
		p += 7;
		while (*p && svg_is_whitespace(*p))
			++p;
		if (strncmp(p, "none", 4) != 0)
		{
			svg_parse_color(ctx, doc, p, stroke);
			*stroke_is_set = 1;
		}
	}
}
