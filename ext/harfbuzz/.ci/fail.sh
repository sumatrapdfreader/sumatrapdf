#!/bin/bash

for f in $(find . -name '*.log' -not -name 'config.log'); do
    last=$(tail -1 $f)
    if [[ $last = FAIL* ]]; then
        echo '====' $f '===='
        cat $f
    elif [[ $last = PASS* ]]; then
        # Do nothing.
        true
    else
	# Travis Linux images has an old automake that does not match the
	# patterns above, so in case of doubt just print the file.
        cat $f
    fi
done

exit 1
