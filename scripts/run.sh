#!/bin/sh

./scripts/build.sh
./build/example
as -64 output.s -o output.o
ld output.o -o output
