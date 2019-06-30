#!/bin/bash

function vexec
{
  echo "[$$] $@"
  "$@"
}

# so sue me for being lazy
CXX="clang++"
mkdir -p "./bin"
for infile in *.cpp; do
  base="$(basename "$infile")"
  nocpp="${base%.*}"
  exename="${nocpp}.exe"
  outfile="bin/$exename"
  vexec "$CXX" -std=c++17 -I../include -g3 -ggdb "$infile" -o "$outfile"
done
