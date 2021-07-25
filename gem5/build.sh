#!/usr/bin/env sh

unbuffer scons ./build/X86/gem5.$1 -j100 2>&1 | sed  's/build\/X86\//src\//g'
