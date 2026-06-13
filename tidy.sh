#!/bin/bash
NLOHMANN=$(find ~/.conan2/p -name "json.hpp" 2>/dev/null | head -1 | xargs dirname | xargs dirname)

clang-tidy "$@" -- -std=c++20 -Iinclude -I$NLOHMANN -isystem $NLOHMANN
