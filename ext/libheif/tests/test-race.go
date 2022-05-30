/**
 * Go race detector
 *
 * MIT License
 *
 * Copyright (c) 2019 struktur AG, Joachim Bauch <bauch@struktur.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Based on an example by "Hans Duedal" (hans.duedal@gmail.com) to reproduce
 * https://github.com/strukturag/libheif/issues/147
 *
 */
package main

import (
	"fmt"
	"image"
	"io/ioutil"
	"os"
	"sync"

	"github.com/strukturag/libheif/go/heif"
)

func decodeImage(data []byte) (image.Image, int, int, error) {
	ctx, err := heif.NewContext()
	if err != nil {
		return nil, 0, 0, err
	}
	err = ctx.ReadFromMemory(data)
	if err != nil {
		return nil, 0, 0, err
	}
	imgh, err := ctx.GetPrimaryImageHandle()
	if err != nil {
		return nil, 0, 0, err
	}

	width := imgh.GetWidth()
	height := imgh.GetHeight()

	img, err := imgh.DecodeImage(heif.ColorspaceUndefined, heif.ChromaUndefined, nil)
	if err != nil {
		return nil, 0, 0, err
	}

	img, err = img.ScaleImage(width/2, height/2)
	if err != nil {
		return nil, 0, 0, err
	}

	goimg, err := img.GetImage()
	if err != nil {
		return nil, 0, 0, err
	}

	return goimg, width / 2, height / 2, nil
}

func main() {
	if len(os.Args) != 2 {
		fmt.Printf("USAGE: %s <filename>\n", os.Args[0])
		os.Exit(1)
	}

	filename := os.Args[1]
	imgbytes, err := ioutil.ReadFile(filename)
	if err != nil {
		panic(err)
	}

	count := 100

	var ready sync.WaitGroup
	var done sync.WaitGroup

	fmt.Printf("Decoding in %d goroutines ...", count)
	ready.Add(count)
	done.Add(count)
	for i := 0; i < count; i++ {
		go func() {
			defer done.Done()
			ready.Done()
			ready.Wait()
			_, _, _, err := decodeImage(imgbytes)
			if err != nil {
				panic(err)
			}
		}()
	}
	done.Wait()
	fmt.Println("ok")
}
