#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ -z "${CXX_WIN:-}" ]] && [[ -x /workspace/tools/root/usr/bin/x86_64-w64-mingw32-g++-posix ]]; then
    CXX_WIN=/workspace/tools/root/usr/bin/x86_64-w64-mingw32-g++-posix
fi
CXX_WIN="${CXX_WIN:-x86_64-w64-mingw32-g++}"
mkdir -p build dist
"${CXX_HOST:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror tests/double_tap_test.cpp -o build/double_tap_test
./build/double_tap_test
"$CXX_WIN" -std=c++17 -O2 -Wall -Wextra -Werror -shared -static -static-libgcc -static-libstdc++ src/plugin.cpp -o dist/TwirlingGun.asi
