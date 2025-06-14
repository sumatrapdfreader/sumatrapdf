package server

import (
	"fmt"

	"github.com/kjk/common/u"
)

func logf(s string, args ...interface{}) {
	if len(args) > 0 {
		s = fmt.Sprintf(s, args...)
	}
	fmt.Print(s)
	// logtastic.Log(s)
}

func logErrorf(format string, args ...interface{}) {
	s := format
	if len(args) > 0 {
		s = fmt.Sprintf(format, args...)
	}
	cs := u.GetCallstack(1)
	s = fmt.Sprintf("Error: %s\n%s\n", s, cs)
	fmt.Print(s)
	// logtastic.LogError(nil, s)
}
