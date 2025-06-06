#!/bin/sh

set -xe

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


config_gcc_linux() {
    cc='gcc'
    glfw_obj=rglfw.o
    glfw_obj=raymath.o
    bin='main.bin'
    pbd=''
    debug_flags="-g -ggdb"
}

config_mingw() {
    cc='/bin/x86_64-w64-mingw32-gcc'
    glfw_obj=rglfw.obj
    raymath_obj=raymath.obj
    bin='main.exe'
    pbd='main.pdb'
    debug_flags="-g --for-linker --pdb=\"$pbd\""
}

build() {
    # $cc -H rglfw.c -o $glfw_obj -c -lc -lm -g --for-linker --pdb="rglfw.pbd"
    flag_catch_bugs='-Wall -Wextra -Wpedantic -Werror -Wno-unused-function -Wno-error=pointer-sign -Wno-unused-parameter -Wno-unused-variable -Wno-strict-aliasing'

    pushd ./src/deps/glfw/
    [ -f "$glfw_obj" ] || $cc rglfw.c -o $glfw_obj -c -lc -lm -O3
    popd

    pushd ./src/
    [ -f "$raymath_obj" ] || $cc raymath.c -o $raymath_obj -I./deps/ -c -lc -lm -O3
    popd

    $cc -Isrc                                     \
        -std=c23                                  \
        -Wpedantic                                \
        src/main.c                                \
        src/deps/glfw/$glfw_obj                   \
        src/$raymath_obj                          \
        -o $bin                                   \
        -Isrc/deps/                               \
        -Isrc/deps/glfw/glfw/include/             \
        -lm -lgdi32 -luser32                      \
        $flag_catch_bugs                          \
        -O3

}

create_zip() {
    if [ -z "$1" ]; then
        echo "Usage: create_zip <output_zip_file_name>"
        return 1
    fi

    local zip_file="$1"
    zip -r "$zip_file" src/shaders/* main.exe

    if [ $? -eq 0 ]; then
        echo "Successfully created $zip_file"
    else
        echo "Failed to create zip file"
    fi
}


# flag_catch_bugs='-Wall -Wextra -Wpedantic -Werror -Wno-error=unused-function -Wno-error=pointer-sign -Wno-error=unused-parameter -Wno-error=unused-variable'

config_mingw
build
create_zip raymarch

