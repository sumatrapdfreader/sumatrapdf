#!/usr/bin/env bash

set -o nounset
set -o errexit
set -o pipefail

vagrant up

vagrant provision

vagrant halt
