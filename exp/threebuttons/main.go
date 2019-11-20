// This package provides an example application built using the goey package
// that demonstrates three buttons with different behaviours.  The top button
// maintains a running count of how many times it has been clicked.  The middle
// button changes the vertical alignment of the buttons.  The bottom button
// changes the horizontal alignment.
//
// The different values for MainAxisAlignemnt and CrossAxisAlignment can be
// explored with this example.
//
// The management of scrollbars can be tested by using the environment variable
// GOEY_SCROLL.  Allowed values are 0 through 3, which enable no scrollbars,
// the vertical scrollbar, the horizontal scrollbar, or both scrollbars.
//
// Setting the environment variable GOEY_DIR=1 will change the layout of the
// buttons from vertical to horizontal.
package main

import (
	"fmt"
	"os"
	"strconv"

	"bitbucket.org/rj/goey"
	"bitbucket.org/rj/goey/base"
	"bitbucket.org/rj/goey/loop"
)

var (
	mainWindow *goey.Window
	clickCount int
	alignMain  = goey.SpaceAround
	alignCross = goey.Stretch
	direction  bool
)

func main() {
	if env := os.Getenv("GOEY_DIR"); env != "" {
		value, err := strconv.ParseUint(env, 10, 64)
		if err != nil {
			fmt.Println("Error: ", err)
		} else {
			direction = (value % 2) != 0
		}
	}

	err := loop.Run(createWindow)
	if err != nil {
		fmt.Println("Error: ", err)
	}
}

func createWindow() error {
	mw, err := goey.NewWindow("Three Buttons", render())
	if err != nil {
		return err
	}
	mainWindow = mw

	return nil
}

func updateWindow() {
	err := mainWindow.SetChild(render())
	if err != nil {
		fmt.Println("Error: ", err.Error())
	}
}

func cycleMainAxisAlign() {
	alignMain++
	if alignMain > goey.Homogeneous {
		alignMain = goey.MainStart
	}
	updateWindow()
}

func cycleCrossAxisAlign() {
	alignCross++
	if alignCross > goey.CrossEnd {
		alignCross = goey.Stretch
	}
	updateWindow()
}

func onfocus(ndx int) func() {
	return func() {
		fmt.Println("focus", ndx)
	}
}

func onblur(ndx int) func() {
	return func() {
		fmt.Println("blur", ndx)
	}
}

func render() base.Widget {
	text := "Click me!"
	if clickCount > 0 {
		text = text + "  (" + strconv.Itoa(clickCount) + ")"
	}

	children := []base.Widget{
		&goey.Button{Text: text,
			Default: true,
			OnClick: func() {
				clickCount++
				updateWindow()
			},
			OnFocus: onfocus(1),
			OnBlur:  onblur(1),
		},
		&goey.Button{Text: "Cycle main axis align",
			OnClick: cycleMainAxisAlign,
			OnFocus: onfocus(2),
			OnBlur:  onblur(2),
		},
		&goey.Button{Text: "Cycle cross axis align",
			OnClick: cycleCrossAxisAlign,
			OnFocus: onfocus(3),
			OnBlur:  onblur(3),
		},
	}

	if direction {
		return &goey.Padding{
			Insets: goey.DefaultInsets(),
			Child: &goey.HBox{
				AlignMain:  alignMain,
				AlignCross: alignCross,
				Children:   children,
			},
		}
	}

	return &goey.Padding{
		Insets: goey.DefaultInsets(),
		Child: &goey.VBox{
			AlignMain:  alignMain,
			AlignCross: alignCross,
			Children:   children,
		},
	}
}
