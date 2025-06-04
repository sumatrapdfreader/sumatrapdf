function showString(str) {
	var out, c, i, n;
	n = str.length;
	if (Array.isArray(str)) {
		out = '<';
		for (i = 0; i < n; ++i) {
			c = str[i].toString(16);
			if (c.length == 1)
				out += '0';
			out += c;
		}
		out += '>';
	} else {
		out = '(';
		for (i = 0; i < n; ++i) {
			c = str[i];
			if (c == '(' || c == ')' || c == '\\')
				out += '\\';
			out += c;
		}
		out += ')';
	}
	return out;
}

function showArray(arr) {
	return arr.join(" ");
}

var traceProcessor = {
	op_w: function (a) { print(a, "w"); },
	op_j: function (a) { print(a, "j"); },
	op_J: function (a) { print(a, "J"); },
	op_M: function (a) { print(a, "M"); },
	op_ri: function (a) { print(a, "ri"); },
	op_i: function (a) { print(a, "i"); },
	op_d: function (array, phase) { print("[" + showArray(array) + "]", phase, "d"); },
	op_gs: function (name, dict) {
		print("/"+name, "gs");
		dict.forEach(function(v,k) {
			print("%", k, v);
		});
	},

	op_q: function () { print("q"); },
	op_Q: function () { print("Q"); },
	op_cm: function (a,b,c,d,e,f) { print(a,b,c,d,e,f, "cm"); },

	op_m: function (x,y) { print(x, y, "m"); },
	op_l: function (x,y) { print(x, y, "l"); },
	op_c: function (x1,y1,x2,y2,x3,y3) { print(x1,y1,x2,y2,x3,y3, "c"); },
	op_v: function (x2,y2,x3,y3) { print(x2,y2,x3,y3, "v"); },
	op_y: function (x1,y1,x3,y3) { print(x1,y1,x3,y3, "y"); },
	op_h: function () { print("h"); },
	op_re: function (x,y,w,h) { print(x,y,w,h, "re"); },

	op_S: function () { print("S"); },
	op_s: function () { print("s"); },
	op_F: function () { print("F"); },
	op_f: function () { print("f"); },
	op_fstar: function () { print("f*"); },
	op_B: function () { print("B"); },
	op_Bstar: function () { print("B*"); },
	op_b: function () { print("b"); },
	op_bstar: function () { print("b*"); },
	op_n: function () { print("n"); },
	op_W: function () { print("W"); },
	op_Wstar: function () { print("W*"); },

	op_BT: function () { print("BT"); },
	op_ET: function () { print("ET"); },

	op_Tc: function (charspace) { print(charspace, "Tc"); },
	op_Tw: function (wordspace) { print(wordspace, "Tw"); },
	op_Tz: function (scale) { print(scale, "Tz"); },
	op_TL: function (leading) { print(leading, "TL"); },
	op_Tr: function (render) { print(render, "Tr"); },
	op_Ts: function (rise) { print(rise, "Ts"); },

	op_Tf: function (name,size) { print("/"+name, size, "Tf"); },

	op_Td: function (x,y) { print(x,y, "Td"); },
	op_TD: function (x,y) { print(x,y, "TD"); },
	op_Tm: function (a,b,c,d,e,f) { print(a,b,c,d,e,f, "Tm"); },
	op_Tstar: function () { print("T*"); },

	op_TJ: function (text) {
		text = text.map(function (x) {
			if (typeof x != 'number')
				return showString(x);
			return x;
		});
		print("[" + showArray(text) + "] TJ")
	},
	op_Tj: function (text) { print(showString(text), "Tj"); },
	op_squote: function (text) { print(showString(text), "'"); },
	op_dquote: function (aw,ac,text) { print(aw, ac, showString(text), "\""); },

	op_d0: function (wx,wy) { print(wx,wy, "d0"); },
	op_d1: function (wx,wy,llx,lly,urx,ury) { print(wx,wy,llx,lly,urx,ury, "d1"); },

	op_CS: function (name,colorspace) { print("/"+name, colorspace, "CS"); },
	op_cs: function (name,colorspace) { print("/"+name, colorspace, "cs"); },
	op_SC_pattern: function (name,pattern,color) { print("/"+name, showArray(color), "SC%pattern"); },
	op_sc_pattern: function (name,pattern,color) { print("/"+name, showArray(color), "sc%pattern"); },
	op_SC_shade: function (name,shade) { print("/"+name, "SC%shade"); },
	op_sc_shade: function (name,shade) { print("/"+name, "sc%shade"); },
	op_SC_color: function (color) { print(showArray(color), "SC%color"); },
	op_sc_color: function (color) { print(showArray(color), "sc%color"); },
	op_G: function (g) { print(g, "G"); },
	op_g: function (g) { print(g, "g"); },
	op_RG: function (r,g,b) { print(r,g,b, "RG"); },
	op_rg: function (r,g,b) { print(r,g,b, "rg"); },
	op_K: function (c,m,y,k) { print(c,m,y,k, "K"); },
	op_k: function (c,m,y,k) { print(c,m,y,k, "k"); },

	op_BI: function (image) { print("% BI ... ID ... EI"); },
	op_sh: function (name,shade) { print("/"+name, "sh"); },
	op_Do_image: function (name,image) { print("/"+name, "Do%image"); },
	op_Do_form: function (name,form,page_res) { print("/"+name, "Do%form"); },

	op_MP: function (tag) { print("/"+tag, "MP"); },
	op_DP: function (tag,dict) { print("/"+tag, dict, "DP"); },
	op_BMC: function (tag) { print("/"+tag, "BMC"); },
	op_BDC: function (tag,dict) { print("/"+tag, dict, "BDC"); },
	op_EMC: function () { print("EMC"); },

	op_BX: function () { print("BX"); },
	op_EX: function () { print("EX"); },
};

var doc = Document.openDocument(scriptArgs[0]);
var page = doc.loadPage(scriptArgs[1]-1);
page.process(traceProcessor);
