#!/bin/bash
make -s -C platform/java
jshell -q --class-path build/java/debug -R-Djava.library.path="build/java/debug" platform/java/init.jshell "$@"
