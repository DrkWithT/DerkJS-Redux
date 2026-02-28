#!/bin/zsh

argc=$#

usage_exit() {
    printf "Usage: utility.sh [help | build | unittest | profile | sloc]\\n\\tutility.sh (re)build [debug | profile | release]\\n\\tutility.sh unittest\\n\\tutility.sh profile <JS file path>\\n";
    exit "$1";
}

if [[ $argc -lt 1 ]]; then
    usage_exit 1;
fi

action="$1"

if [[ $action = "help" ]]; then
    usage_exit 0;
elif [[ $action = "build" ]]; then
    rm -rf ./.cache;
    rm -f ./compile_commands.json;
    rm -f ./build/derkjs;
    cmake -S . -B build --preset "local-$2-build" && cmake --build build && mv ./build/compile_commands.json .;
elif [[ $action = "rebuild" ]]; then
    rm -rf ./.cache;
    rm -rf ./build/;
    cmake --fresh -S . -B build --preset "local-$2-build" && cmake --build build && mv ./build/compile_commands.json .;
elif [[ $action = "unittest" && $argc -eq 1 ]]; then
    # touch ./test_logs.txt;
    # ctest --test-dir build --timeout 2 -V 1> ./test_logs.txt;
    # usage_exit $? && echo "TESTS PASSED";
    usage_exit 1;
elif [[ $action = "profile" && $argc -eq 2 ]]; then
    samply record --save-only -o prof_tco.json -- ./build/derkjs_tco -r "$2";
elif [[ $action = "sloc" ]]; then
    wc -l ./src/derkjs_impl/**/*.ixx ./src/*.cpp;
else
    usage_exit 1;
fi
