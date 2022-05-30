/*
  Simple GO interface test program

  MIT License

  Copyright (c) 2018 struktur AG, Dirk Farin <farin@struktur.de>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

package main

import (
	"bytes"
	"fmt"
	"image"
	_ "image/jpeg"
	"image/png"
	"io/ioutil"
	"os"
	"path/filepath"

	"github.com/strukturag/libheif/go/heif"
)

// ==================================================
//                      TEST
// ==================================================

func savePNG(img image.Image, filename string) {
	var out bytes.Buffer
	if err := png.Encode(&out, img); err != nil {
		fmt.Printf("Could not encode image as PNG: %s\n", err)
	} else {
		if err := ioutil.WriteFile(filename, out.Bytes(), 0644); err != nil {
			fmt.Printf("Could not save PNG image as %s: %s\n", filename, err)
		} else {
			fmt.Printf("Written to %s\n", filename)
		}
	}
}

func testHeifHighlevel(filename string) {
	fmt.Printf("Performing highlevel conversion of %s\n", filename)
	file, err := os.Open(filename)
	if err != nil {
		fmt.Printf("Could not read file %s: %s\n", filename, err)
		return
	}
	defer file.Close()

	img, magic, err := image.Decode(file)
	if err != nil {
		fmt.Printf("Could not decode image: %s\n", err)
		return
	}

	fmt.Printf("Decoded image of type %s: %s\n", magic, img.Bounds())

	ext := filepath.Ext(filename)
	outFilename := filename[0:len(filename)-len(ext)] + "_highlevel.png"
	savePNG(img, outFilename)
}

func testHeifLowlevel(filename string) {
	fmt.Printf("Performing lowlevel conversion of %s\n", filename)
	c, err := heif.NewContext()
	if err != nil {
		fmt.Printf("Could not create context: %s\n", err)
		return
	}

	if err := c.ReadFromFile(filename); err != nil {
		fmt.Printf("Could not read file %s: %s\n", filename, err)
		return
	}

	nImages := c.GetNumberOfTopLevelImages()
	fmt.Printf("Number of top level images: %v\n", nImages)

	ids := c.GetListOfTopLevelImageIDs()
	fmt.Printf("List of top level image IDs: %#v\n", ids)

	if pID, err := c.GetPrimaryImageID(); err != nil {
		fmt.Printf("Could not get primary image id: %s\n", err)
	} else {
		fmt.Printf("Primary image: %v\n", pID)
	}

	handle, err := c.GetPrimaryImageHandle()
	if err != nil {
		fmt.Printf("Could not get primary image: %s\n", err)
		return
	}

	fmt.Printf("Image size: %v × %v\n", handle.GetWidth(), handle.GetHeight())

	img, err := handle.DecodeImage(heif.ColorspaceUndefined, heif.ChromaUndefined, nil)
	if err != nil {
		fmt.Printf("Could not decode image: %s\n", err)
	} else if i, err := img.GetImage(); err != nil {
		fmt.Printf("Could not get image: %s\n", err)
	} else {
		fmt.Printf("Rectangle: %v\n", i.Bounds())

		ext := filepath.Ext(filename)
		outFilename := filename[0:len(filename)-len(ext)] + "_lowlevel.png"
		savePNG(i, outFilename)
	}
}

func testHeifEncode(filename string) {
	file, err := os.Open(filename)
	if err != nil {
		fmt.Printf("failed to open file %v\n", file)
		return
	}
	defer file.Close()

	i, _, err := image.Decode(file)
	if err != nil {
		fmt.Printf("failed to decode image: %v\n", err)
		return
	}

	const quality = 100
	ctx, err := heif.EncodeFromImage(i, heif.CompressionHEVC, quality, heif.LosslessModeEnabled, heif.LoggingLevelFull)
	if err != nil {
		fmt.Printf("failed to heif encode image: %v\n", err)
		return
	}

	ext := filepath.Ext(filename)
	out := filename[0:len(filename)-len(ext)] + "_encoded.heif"
	if err := ctx.WriteToFile(out); err != nil {
		fmt.Printf("failed to write to file: %v\n", err)
		return
	}
	fmt.Printf("Written to %s\n", out)
}

func main() {
	fmt.Printf("libheif version: %v\n", heif.GetVersion())
	if len(os.Args) < 2 {
		fmt.Printf("USAGE: %s <filename>\n", os.Args[0])
		return
	}

	filename := os.Args[1]
	testHeifLowlevel(filename)
	fmt.Println()
	testHeifHighlevel(filename)
	fmt.Println()
	testHeifEncode(filename)
	fmt.Println("Done.")
}
