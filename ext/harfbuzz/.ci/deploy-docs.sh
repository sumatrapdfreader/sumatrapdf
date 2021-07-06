#!/bin/bash

set -x
set -o errexit -o nounset

TAG="$(git describe --exact-match --match "[0-9]*" HEAD 2>/dev/null || true)"

DOCSDIR=build-docs
REVISION=$(git rev-parse --short HEAD)

rm -rf $DOCSDIR || exit
mkdir $DOCSDIR
cd $DOCSDIR

cp ../build/docs/html/* .
#cp ../build/docs/CNAME .

git init
git config user.name "Travis CI"
git config user.email "travis@harfbuzz.org"
set +x
echo "git remote add upstream \"https://\$GH_TOKEN@github.com/harfbuzz/harfbuzz.github.io.git\""
git remote add upstream "https://$GH_TOKEN@github.com/harfbuzz/harfbuzz.github.io.git"
set -x
git fetch upstream
git reset upstream/master

touch .
git add -A .

if [[ $(git status -s) ]]; then
  git commit -m "Rebuild docs for https://github.com/harfbuzz/harfbuzz/commit/$REVISION"
  git push -q upstream HEAD:master
fi
