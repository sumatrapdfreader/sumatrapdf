#!/usr/bin/env bash

set -e
set -o pipefail

if [[ -z $GITHUB_TOKEN ]]; then
	echo "No GITHUB_TOKEN secret found, artifact publishing skipped"
	exit
fi

if ! hash ghr 2> /dev/null; then
	_GHR_VER=v0.14.0
	_GHR=ghr_${_GHR_VER}_linux_amd64
	mkdir -p $HOME/.local/bin
	curl -sfL https://github.com/tcnksm/ghr/releases/download/$_GHR_VER/$_GHR.tar.gz |
		tar xz -C $HOME/.local/bin --strip-components=1 $_GHR/ghr
fi

ghr -replace \
	-u $CIRCLE_PROJECT_USERNAME \
	-r $CIRCLE_PROJECT_REPONAME \
	$CIRCLE_TAG \
	$1
