#!/bin/sh

cmake -G Ninja -B build
cp build/compile_commands.json .
