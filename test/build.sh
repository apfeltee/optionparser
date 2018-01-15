#!/bin/bash

# so sue me for being lazy

for file in *.cpp; do
  clang++ -std=c++14 -I.. -g3 -ggdb "$file" -o "${file%.*}"
done
