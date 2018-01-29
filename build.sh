#!/bin/bash

CC=clang++

echo "Compiling game"
$CC -std=c++11 -g src/main.cpp -o sokoban

