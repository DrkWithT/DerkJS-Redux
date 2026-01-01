argc=$#

usage_exit() {
    echo "Usage: utility.sh [help | build | unittest | profile | sloc]\n\tutility.sh (re)build [debug | release]\n\tutility.sh unittest\n\tutility.sh profile <JS file path>\n";
    exit $1;
}

if [[ $argc -lt 1 ]]; then
    usage_exit 1;
fi

action="$1"
build_status=0

if [[ $action = "help" ]]; then
    usage_exit 0;
elif [[ $action = "build" && $argc -ge 2 ]]; then
    rm -rf ./.cache;
    rm -f ./build/compile_commands.json;
    rm -f ./build/derkjs;
    cmake -S . -B build --preset "local-$2-build" && cmake --build build;
elif [[ $action = "rebuild" && $argc -ge 2 ]]; then
    rm -rf ./.cache;
    rm -rf ./build/;
    cmake --fresh -S . -B build --preset "local-$2-build" && cmake --build build;
elif [[ $action = "unittest" && $argc -eq 1 ]]; then
    # touch ./test_logs.txt;
    # ctest --test-dir build --timeout 2 -V 1> ./test_logs.txt;
    # usage_exit $? && echo "TESTS PASSED";
    usage_exit 1;
elif [[ $action = "profile" && $argc -eq 2 ]]; then
    samply record --save-only -o prof.json -- ./build/derkjs $2
elif [[ $action = "sloc" ]]; then
    wc -l ./src/derkjs_impl/**/*.ixx ./src/main.cpp;
else
    usage_exit 1;
fi
