#!/bin/bash

set -x
set -o errexit -o nounset

DOCSDIR=build-docs
REVISION=$(git rev-parse --short HEAD)

rm -rf $DOCSDIR || exit
mkdir $DOCSDIR
cd $DOCSDIR

cp ../build/docs/html/* .
#cp ../build/docs/CNAME .

git init
git branch -m main
git config user.name "CI"
git config user.email "harfbuzz-admin@googlegroups.com"
set +x
echo "git remote add upstream \"https://\$GH_TOKEN@github.com/harfbuzz/harfbuzz.github.io.git\""
git remote add upstream "https://$GH_TOKEN@github.com/harfbuzz/harfbuzz.github.io.git"
set -x
git fetch upstream
git reset upstream/main

touch .
git add -A .

if [[ $(git status -s) ]]; then
  git commit -m "Rebuild docs for https://github.com/harfbuzz/harfbuzz/commit/$REVISION"
  git push -q upstream HEAD:main
fi
