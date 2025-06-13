package main

import "flag"

func main() {

	var (
		flgRunDev bool
	)
	{
		flag.BoolVar(&flgRunDev, "run-dev", false, "run in development mode")
		flag.Parse()
	}

	if flgRunDev {
		runServerDev()
		return
	}
	flag.Usage()
}
