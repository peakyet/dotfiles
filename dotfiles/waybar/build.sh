#!/bin/sh
set -eu
directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$directory/build"
flags=$(pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0 gio-unix-2.0 jsoncpp)
for module in grouped-taskbar workspace-pips; do
    temporary=$(mktemp "$directory/build/$module.so.XXXXXX")
    trap 'rm -f "$temporary"' EXIT
    c++ -std=c++17 -O2 -Wall -Wextra -shared -fPIC \
        "$directory/modules/$module.cpp" -o "$temporary" $flags
    mv "$temporary" "$directory/build/$module.so"
done
