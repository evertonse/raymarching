#!/bin/sh

set -e
# Store timer state globally
__profile_start_s=""
__profile_start_ns=""

profile_start() {
    __profile_start_s=$(date +%s)
    __profile_start_ns=$(date +%N 2>/dev/null)
}

profile_end() {
    local end_s=$(date +%s)
    local end_ns=$(date +%N 2>/dev/null)

    if [ -n "$__profile_start_ns" ] && [ "$__profile_start_ns" != "$__profile_start_s" ]; then
        local elapsed=$(awk -v s1="$__profile_start_s" -v n1="$__profile_start_ns" -v s2="$end_s" -v n2="$end_ns" \
            'BEGIN { print (s2 - s1) + (n2 - n1)/1e9 }')
        echo "⏱️ Elapsed: ${elapsed}s"
    else
        local elapsed=$((end_s - __profile_start_s))
        echo "⏱️ Elapsed: ${elapsed}s (low precision)"
    fi
}


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
    bin='main.bin'
    pbd=''
    debug_flags="-g -ggdb"
}

config_mingw() {
    cxx='/bin/x86_64-w64-mingw32-g++'
    cc='/bin/x86_64-w64-mingw32-gcc'
    glfw_obj=rglfw.obj
    bin='main.exe'
    pbd='main.pdb'
    debug_flags="-g --for-linker --pdb=\"$pbd\""
}

build() {
    # From: https://carlpearson.net/post/20220301-gcc-flags/
    #     -Wall: turns on many warnings, but not "all."
    #     -Wextra: turns on even more warnings, but still not all.
    #     -Wpedantic: Issue all the warnings demanded by strict ISO C++.
    #     -Wcast-align: warn whenever a pointer is cast such that the required alignment is increased (char* -> int*).
    #     -Wcast-qual: warn when qualifier (const) is cast away, or introduces a qualifier in an unsafe way.
    #     -Wdisabled-optimization: warn if a requested optimization pass is disabled (e.g. code is too large, has some other feature that makes g++ give up).
    #     -Wduplicated-branches: warn if if-else branches have identical bodies.
    #     -Wduplicated-cond: warn about duplicated conditions in an if-else-if chain.
    #     -Wformat=2: same as -Wformat -Wformat-nonliteral -Wformat-security -Wformat-y2k. make sure printf-style function arguments match their format strings.
    #     -Wlogical-op: warn about suspicious use of logical operators, i.e. contexts where bitwise is more likely.
    #     -Wmissing-include-dirs: warn if a user-supplied include dir does not exist.
    #     -Wnull-dereference: warn if paths that dereference a null pointer are detected.
    #     -Woverloaded-virtual: warn when a function declaration hides virtual functions from a base class
    #     -Wpointer-arith: warn about sizeof for function types or void.
    #     -Wshadow: warn about variable shadowing and global function shadowing.
    #     -Wswitch-enum: warn when a switch on an enum type is missing one of the enums.
    #     -Wvla: warn about using variable-length arrays.
    #


    flag_nowarn='-Wno-format-nonliteral -Wno-unused-function -Wno-error=pointer-sign -Wno-error=missing-braces -Wno-unused-parameter -Wno-unused-variable -Wno-strict-aliasing -fwrapv -fno-strict-aliasing'
    flag_basic='-Wall -Wextra -Wpedantic -Werror'
    # flag_sanitize='-fsanitize=undefined'
    flag_catch_bugs="$flag_nowarn $flag_basic $flag_sanitize"
    flag_catch_bugs="$flag_catch_bugs -Wcast-align -Wdisabled-optimization -Wduplicated-cond -Wformat=2"
    # flag_catch_bugs="$flag_catch_bugs -Wcast-qual -Wduplicated-branches"
    # flag_catch_bugs="$flag_catch_bugs -Wlogical-op -Wmissing-include-dirs -Wnull-dereference -Woverloaded-virtual -Wpointer-arith -Wshadow -Wswitch-enum -Wvla"
    # Replace -O3 with -O1 or -Og for dev speed

    # -fno-rtti for cpp
    # For debugging
    # flags="$debug_flags"

    # `-march=native` this flag bugs out
    # `-pipe` to speed up intermediate file transfer between compiler stages
    flags="-pipe -static -O0 -ffast-math -fno-exceptions $flag_catch_bugs"

    flags="-O0 $flag_catch_bugs"


    pushd ./src/deps/glfw/
    [ -f "$glfw_obj" ] || $cc rglfw.c -o $glfw_obj -c -lc -lm -O3
    popd

    profile_start

    # perf stat
    # -std=c99                                  \
    # -std=c23                                  \
    set -x
    $cc -Isrc                    \
        src/main.c                         \
        src/deps/glfw/$glfw_obj            \
        -o $bin                            \
        -Isrc/deps/                        \
        -Isrc/deps/glfw/glfw/include/      \
        -lm -lgdi32 -luser32               \
        $flags
    set +x

    profile_end

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


config_mingw
build

if [ "$1" = "run" ]; then
    ./$bin
fi

create_zip raymarch

