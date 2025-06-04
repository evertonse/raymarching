#!/bin/sh

stack=""
pushd() {
    if [ -z "$1" ]; then
        echo "Usage: pushd <directory>"
        return 1
    fi

    stack="$PWD $stack"
    echo "Entering directory $1"

    cd "$1" || return 1
}

popd() {
    if [ -z "$stack" ]; then
        echo "Directory stack is empty"
        return 1
    fi
    

    #
    # Syntax ${variable<symbol>pattern}:
    #   <symbol> = # Removes the shortest match of pattern from the beginning of variable.
    #   <symbol> = ##            longest  match            from the beginning
    #   <symbol> = %             shortest match            from the end
    #   <symbol> = %%            longest  match            from the end
    #

    # Extract the first word (top directory)
    top="${stack%% *}"

    #   Remove the first word from the stack
    stack="${stack#* }"

    cd "$top" || return 1
}

dirs() {
    echo "Current directory stack: $stack"
}

set -xe

build_with_gcc_linux() {
    cc=gcc
    pushd ./src/deps/glfw/
    [ -f "rglfw.o" ] || $cc rglfw.c -c -D_GLFW_X11 -lc -lm
    popd

    $cc -Isrc                                     \
        src/main.c                                \
        src/deps/glfw/rglfw.o                     \
        -Isrc/deps/                               \
        -Isrc/deps/glfw/glfw/include/             \
        -o main.bin                               \
        -g -ggdb                                  \
        -lm
}

# flag_catch_bugs='-Wall -Wextra -Wpedantic -Werror -Wno-error=unused-function -Wno-error=pointer-sign -Wno-error=unused-parameter -Wno-error=unused-variable'
flag_catch_bugs='-Wall -Wextra -Wpedantic -Werror -Wno-unused-function -Wno-error=pointer-sign -Wno-unused-parameter -Wno-unused-variable -Wno-strict-aliasing'

build_with_mingw() {
    extras_flags='-I'
    cc='/bin/x86_64-w64-mingw32-gcc'
    glfw_obj=rglfw.obj
    bin='main.exe'
    pbd='main.pdb'
    pushd ./src/deps/glfw/
    # [ -f "$glfw_obj" ] || $cc -H rglfw.c -o $glfw_obj -c -lc -lm -g --for-linker --pdb="rglfw.pbd"
    [ -f "$glfw_obj" ] || $cc rglfw.c -o $glfw_obj -c -lc -lm -O3
    popd

    debug_flags="-g --for-linker --pdb=\"$pbd\""
    $cc -Isrc                                     \
        -std=c23                                  \
        -Wpedantic                                \
        src/main.c                                \
        src/deps/glfw/$glfw_obj                   \
        -Isrc/deps/                               \
        -Isrc/deps/glfw/glfw/include/             \
        $flag_catch_bugs                          \
        -lm -lgdi32 -luser32                      \
        -O3                                       \
        -o $bin
        
}


build_with_mingw
# build_with_gcc_linux
# sh "$(pwd)/strace.sh" $0
