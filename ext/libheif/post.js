function StringToArrayBuffer(str) {
    var buf = new ArrayBuffer(str.length);
    var bufView = new Uint8Array(buf);
    for (var i=0, strLen=str.length; i<strLen; i++) {
        bufView[i] = str.charCodeAt(i);
    }
    return buf;
}

var HeifImage = function(handle) {
    this.handle = handle;
    this.img = null;
};

HeifImage.prototype.free = function() {
    if (this.handle) {
        libheif.heif_image_handle_release(this.handle);
        this.handle = null;
    }
};

HeifImage.prototype._ensureImage = function() {
    if (this.img) {
        return;
    }

    var img = libheif.heif_js_decode_image(this.handle,
        libheif.heif_colorspace_YCbCr, libheif.heif_chroma_420);
    if (!img || img.code) {
        console.log("Decoding image failed", this.handle, img);
        return;
    }

    this.data = new Uint8Array(StringToArrayBuffer(img.data));
    delete img.data;
    this.img = img;
};

HeifImage.prototype.get_width = function() {
    this._ensureImage();
    return this.img.width;
};

HeifImage.prototype.get_height = function() {
    this._ensureImage();
    return this.img.height;
};

HeifImage.prototype.is_primary = function() {
    this._ensureImage();
    return !!this.img.is_primary;
}

HeifImage.prototype.display = function(image_data, callback) {
    // Defer color conversion.
    var w = this.get_width();
    var h = this.get_height();

    setTimeout(function() {
        this._ensureImage();
        if (!this.img) {
            // Decoding failed.
            callback(null);
            return;
        }

        var yval;
        var uval;
        var vval;
        var xpos = 0;
        var ypos = 0;
        var yoffset = 0;
        var uoffset = 0;
        var voffset = 0;
        var x2;
        var i = 0;
        var maxi = w*h;
        var stridey = w;
        var strideu = Math.ceil(w / 2);
        var stridev = Math.ceil(w / 2);
        var h2 = Math.ceil(h / 2);
        var y = this.data;
        var u = this.data.subarray(stridey * h, stridey * h + (strideu * h2));
        var v = this.data.subarray(stridey * h + (strideu * h2), stridey * h + (strideu * h2) + (stridev * h2));
        var dest = image_data.data;
        while (i < maxi) {
            x2 = (xpos >> 1);
            yval = 1.164 * (y[yoffset + xpos] - 16);

            uval = u[uoffset + x2] - 128;
            vval = v[voffset + x2] - 128;
            dest[(i<<2)+0] = yval + 1.596 * vval;
            dest[(i<<2)+1] = yval - 0.813 * vval - 0.391 * uval;
            dest[(i<<2)+2] = yval + 2.018 * uval;
            dest[(i<<2)+3] = 0xff;
            i++;
            xpos++;

            if (xpos < w) {
                yval = 1.164 * (y[yoffset + xpos] - 16);
                dest[(i<<2)+0] = yval + 1.596 * vval;
                dest[(i<<2)+1] = yval - 0.813 * vval - 0.391 * uval;
                dest[(i<<2)+2] = yval + 2.018 * uval;
                dest[(i<<2)+3] = 0xff;
                i++;
                xpos++;
            }

            if (xpos === w) {
                xpos = 0;
                ypos++;
                yoffset += stridey;
                uoffset = ((ypos >> 1) * strideu);
                voffset = ((ypos >> 1) * stridev);
            }
        }
        callback(image_data);
    }.bind(this), 0);
};

var HeifDecoder = function() {
    this.decoder = null;
};

HeifDecoder.prototype.decode = function(buffer) {
    if (this.decoder) {
        libheif.heif_context_free(this.decoder);
    }
    this.decoder = libheif.heif_context_alloc();
    if (!this.decoder) {
        console.log("Could not create HEIF context");
        return [];
    }
    var error = libheif.heif_context_read_from_memory(this.decoder, buffer);
    if (error.code !== libheif.heif_error_Ok) {
        console.log("Could not parse HEIF file", error);
        return [];
    }

    var ids = libheif.heif_js_context_get_list_of_top_level_image_IDs(this.decoder);
    if (!ids || ids.code) {
        console.log("Error loading image ids", ids);
        return [];
    }
    else if (!ids.length) {
        console.log("No images found");
        return [];
    }

    var result = [];
    for (var i = 0; i < ids.length; i++) {
        var handle = libheif.heif_js_context_get_image_handle(this.decoder, ids[i]);
        if (!handle || handle.code) {
            console.log("Could not get image data for id", ids[i], handle);
            continue;
        }

        result.push(new HeifImage(handle));
    }
    return result;
};

var libheif = {
    // Expose high-level API.
    /** @expose */
    HeifDecoder: HeifDecoder,

    // Expose low-level API.
    /** @expose */
    fourcc: function(s) {
        return s.charCodeAt(0) << 24 |
            s.charCodeAt(1) << 16 |
            s.charCodeAt(2) << 8 |
            s.charCodeAt(3);
    }
};

var key;

// Expose enum values.
var enums = {
    "heif_error_code": true,
    "heif_suberror_code": true,
    "heif_compression_format": true,
    "heif_chroma": true,
    "heif_colorspace": true,
    "heif_channel": true
};
var e;
for (e in enums) {
    if (!enums.hasOwnProperty(e)) {
        continue;
    }
    for (key in Module[e]) {
        if (!Module[e].hasOwnProperty(key) ||
            key === "values") {
            continue;
        }

        libheif[key] = Module[e][key];
    }
}

// Expose internal C API.
for (key in Module) {
    if (enums.hasOwnProperty(key) || key.indexOf("heif_") !== 0) {
        continue;
    }
    libheif[key] = Module[key];
}

// don't pollute the global namespace
delete this['Module'];

// On IE this function is called with "undefined" as first parameter. Override
// with a version that supports this behaviour.
function createNamedFunction(name, body) {
    if (!name) {
      name = "function_" + (new Date());
    }
    name = makeLegalFunctionName(name);
    /*jshint evil:true*/
    return new Function(
        "body",
        "return function " + name + "() {\n" +
        "    \"use strict\";" +
        "    return body.apply(this, arguments);\n" +
        "};\n"
    )(body);
}

var root = this;

if (typeof exports !== 'undefined') {
    if (typeof module !== 'undefined' && module.exports) {
        /** @expose */
        exports = module.exports = libheif;
    }
    /** @expose */
    exports.libheif = libheif;
} else {
    /** @expose */
    root.libheif = libheif;
}

if (typeof define === "function" && define.amd) {
    /** @expose */
    define([], function() {
        return libheif;
    });
}

// NOTE: wrapped inside "(function() {" block from pre.js
}).call(this);
