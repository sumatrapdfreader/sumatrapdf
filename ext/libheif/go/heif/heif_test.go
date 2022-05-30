/*
 * GO interface to libheif
 * Copyright (c) 2018 struktur AG, Joachim Bauch <bauch@struktur.de>
 *
 * This file is part of heif, an example application using libheif.
 *
 * heif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * heif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with heif.  If not, see <http://www.gnu.org/licenses/>.
 */

package heif

import (
	"fmt"
	"image"
	"io/ioutil"
	"os"
	"path"
	"testing"
)

func TestGetVersion(t *testing.T) {
	version := GetVersion()
	if version == "" {
		t.Fatal("Version is missing")
	}
}

type decodeTest struct {
	colorspace Colorspace
	chroma     Chroma
}

func CheckHeifImage(t *testing.T, handle *ImageHandle, thumbnail bool) {
	handle.GetWidth()
	handle.GetHeight()
	handle.HasAlphaChannel()
	handle.HasDepthImage()
	count := handle.GetNumberOfDepthImages()
	if ids := handle.GetListOfDepthImageIDs(); len(ids) != count {
		t.Errorf("Expected %d depth image ids, got %d", count, len(ids))
	}
	if !thumbnail {
		count = handle.GetNumberOfThumbnails()
		ids := handle.GetListOfThumbnailIDs()
		if len(ids) != count {
			t.Errorf("Expected %d thumbnail image ids, got %d", count, len(ids))
		}
		for _, id := range ids {
			if thumb, err := handle.GetThumbnail(id); err != nil {
				t.Errorf("Could not get thumbnail %d: %s", id, err)
			} else {
				CheckHeifImage(t, thumb, true)
			}
		}
	}

	if img, err := handle.DecodeImage(ColorspaceUndefined, ChromaUndefined, nil); err != nil {
		t.Errorf("Could not decode image: %s", err)
	} else {
		img.GetColorspace()
		img.GetChromaFormat()
	}

	decodeTests := []decodeTest{
		decodeTest{ColorspaceYCbCr, Chroma420},
		decodeTest{ColorspaceYCbCr, Chroma422},
		decodeTest{ColorspaceYCbCr, Chroma444},
		decodeTest{ColorspaceRGB, Chroma444},
		decodeTest{ColorspaceRGB, ChromaInterleavedRGB},
		decodeTest{ColorspaceRGB, ChromaInterleavedRGBA},
		decodeTest{ColorspaceRGB, ChromaInterleavedRRGGBB_BE},
		decodeTest{ColorspaceRGB, ChromaInterleavedRRGGBBAA_BE},
	}
	for _, test := range decodeTests {
		if img, err := handle.DecodeImage(test.colorspace, test.chroma, nil); err != nil {
			t.Errorf("Could not decode image with %v / %v: %s", test.colorspace, test.chroma, err)
		} else {
			img.GetColorspace()
			img.GetChromaFormat()

			if _, err := img.GetImage(); err != nil {
				t.Errorf("Could not get image with %v /%v: %s", test.colorspace, test.chroma, err)
				continue
			}
		}
	}
}

func CheckHeifFile(t *testing.T, ctx *Context) {
	if count := ctx.GetNumberOfTopLevelImages(); count != 2 {
		t.Errorf("Expected %d top level images, got %d", 2, count)
	}
	if ids := ctx.GetListOfTopLevelImageIDs(); len(ids) != 2 {
		t.Errorf("Expected %d top level image ids, got %+v", 2, ids)
	}
	if _, err := ctx.GetPrimaryImageID(); err != nil {
		t.Errorf("Expected a primary image, got %s", err)
	}
	if handle, err := ctx.GetPrimaryImageHandle(); err != nil {
		t.Errorf("Could not get primary image handle: %s", err)
	} else {
		if !handle.IsPrimaryImage() {
			t.Error("Expected primary image")
		}
		CheckHeifImage(t, handle, false)
	}
}

func TestReadFromFile(t *testing.T) {
	ctx, err := NewContext()
	if err != nil {
		t.Fatalf("Can't create context: %s", err)
	}

	filename := path.Join("..", "..", "examples", "example.heic")
	if err := ctx.ReadFromFile(filename); err != nil {
		t.Fatalf("Can't read from %s: %s", filename, err)
	}

	CheckHeifFile(t, ctx)
}

func TestReadFromMemory(t *testing.T) {
	ctx, err := NewContext()
	if err != nil {
		t.Fatalf("Can't create context: %s", err)
	}

	filename := path.Join("..", "..", "examples", "example.heic")
	data, err := ioutil.ReadFile(filename)
	if err != nil {
		t.Fatalf("Can't read file %s: %s", filename, err)
	}
	if err := ctx.ReadFromMemory(data); err != nil {
		t.Fatalf("Can't read from memory: %s", err)
	}
	data = nil // Make sure future processing works if "data" is GC'd

	CheckHeifFile(t, ctx)
}

func TestReadImage(t *testing.T) {
	filename := path.Join("..", "..", "examples", "example.heic")
	fp, err := os.Open(filename)
	if err != nil {
		t.Fatalf("Could not open %s: %s", filename, err)
	}
	defer fp.Close()

	config, format1, err := image.DecodeConfig(fp)
	if err != nil {
		t.Fatalf("Could not load image config from %s: %s", filename, err)
	}
	if format1 != "heif" {
		t.Errorf("Expected format heif, got %s", format1)
	}
	if _, err := fp.Seek(0, 0); err != nil {
		t.Fatalf("Could not seek to start of %s: %s", filename, err)
	}

	img, format2, err := image.Decode(fp)
	if err != nil {
		t.Fatalf("Could not load image from %s: %s", filename, err)
	}
	if format2 != "heif" {
		t.Errorf("Expected format heif, got %s", format2)
	}

	r := img.Bounds()
	if config.Width != (r.Max.X-r.Min.X) || config.Height != (r.Max.Y-r.Min.Y) {
		fmt.Printf("Image size %+v does not match config %+v\n", r, config)
	}
}
