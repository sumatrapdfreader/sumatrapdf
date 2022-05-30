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
var Module = {
    print: function(text) {
        text = Array.prototype.slice.call(arguments).join(' ');
        console.log(text);
    },
    printErr: function(text) {
        text = Array.prototype.slice.call(arguments).join(' ');
        console.error(text);
    },
    canvas: {},
    noInitialRun: true
};
