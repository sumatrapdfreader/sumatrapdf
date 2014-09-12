#!/usr/bin/env bash

set -o nounset
set -o errexit
set -o pipefail

if [ ! -e ./first_run_happened ]; then
  sudo apt-get update
  sudo apt-get --yes install build-essential git mercurial pkg-config
  sudo apt-get --yes install wget curl mingw-w64 mingw32

  echo "export GOPATH=\"`pwd`/GoPath\"" >> .bash_profile
  echo "export PATH=\"\$PATH:/usr/local/go/bin:\$GOPATH/bin\"" >> .bash_profile
  source .bash_profile

  echo -e "Host github.com\n  StrictHostKeyChecking no" > ~/.ssh/config

  touch ./first_run_happened
fi

get_go() {
  if [ ! -e /usr/local/go/ ]; then
    echo "installing go"
    curl -sS https://storage.googleapis.com/golang/go1.3.1.linux-amd64.tar.gz | sudo tar zx -C /usr/local/
  fi
}

get_go

cd /vagrant/sumatrapdf
go run tools/mingw/build.go
