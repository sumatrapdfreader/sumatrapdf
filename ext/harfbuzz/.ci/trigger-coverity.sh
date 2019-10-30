#!/bin/bash

set -x
set -o errexit -o nounset

if test x"$TRAVIS_EVENT_TYPE" != x"cron"; then exit; fi
if test x"$TRAVIS_BRANCH" != x"master"; then exit; fi

git fetch --unshallow
git remote add upstream "https://$GH_TOKEN@github.com/harfbuzz/harfbuzz.git"
git push -q upstream master:coverity_scan
