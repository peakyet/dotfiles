#!/bin/sh
set -eu
directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
temporary=$(mktemp "$directory/workspace-pips.so.XXXXXX")
trap 'rm -f "$temporary"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -shared -fPIC \
    "$directory/workspace-pips.cpp" -o "$temporary" \
    $(pkg-config --cflags --libs gtk+-3.0 gio-unix-2.0 jsoncpp)
mv "$temporary" "$directory/workspace-pips.so"
