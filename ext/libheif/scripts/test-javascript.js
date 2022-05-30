/**
 * @preserve libheif.js HEIF decoder
 * (c)2017 struktur AG, http://www.struktur.de, opensource@struktur.de
 *
 * This file is part of libheif
 * https://github.com/strukturag/libheif
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */
(function() {

    console.log("Running libheif JavaScript tests ...");

    var libheif = require('../libheif.js');
    console.log("Loaded libheif.js", libheif.heif_get_version());

    // Ensure that no "undefined" properties are exported.
    var key;
    var missing = [];
    for (key in libheif) {
        if (!libheif.hasOwnProperty(key)) {
            continue;
        }
        if (typeof(libheif[key]) === "undefined") {
            missing.push(key);
        }
    }
    if (missing.length) {
        throw new Error("The following properties are not defined: " + missing);
    }

    // Decode the example file and make sure at least one image is returned.
    var fs = require('fs');
    fs.readFile('examples/example.heic', function(err, data) {
        if (err) {
            throw err; 
        }

        var decoder = new libheif.HeifDecoder();
        var image_data = decoder.decode(data);
        console.log("Loaded images:", image_data.length);
        if (!image_data.length) {
            throw new Error("Should have loaded images");
        }
    });

})();
