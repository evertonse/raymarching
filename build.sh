#!/bin/bash

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
        echo "⏱️  Elapsed: ${elapsed}s"
    else
        local elapsed=$((end_s - __profile_start_s))
        echo "⏱️  Elapsed: $1 ${elapsed}s (low precision)"
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

    # Syntax ${variable<symbol>pattern}:
    #   <symbol> = # Removes the shortest match of pattern from the beginning of variable.
    #   <symbol> = ##            longest  match            from the beginning
    #   <symbol> = %             shortest match            from the end
    #   <symbol> = %%            longest  match            from the end

    # Extract the first word (top directory)
    top="${stack%% *}"
    #   Remove the first word from the stack
    stack="${stack#* }"

    cd "$top" || return 1
}
# --------------------------------------------------------------------------- #


WINDOWS_DESTINATION_DIR='C:\Dev\code\GPUCompute'
# DEBUGGER_DIRECTORY='C:\Dev\tools\raddbg\'
DEBUGGER_DIRECTORY='E:\Dev\programs'
DEBUGGER_EXECUTABLE_NAME='raddbg.exe'
DEBUGGER_FILEPATH="$DEBUGGER_DIRECTORY\\$DEBUGGER_EXECUTABLE_NAME"

config_gcc_linux() {
    cc='gcc'
    glfw_obj=rglfw.o
    bin='main.bin'
    pdb=''
    flags_debug="-g -ggdb"
}

config_mingw() {
    cxx='/bin/x86_64-w64-mingw32-g++'
    cc='/bin/x86_64-w64-mingw32-gcc' glfw_obj=rglfw.obj bin='main.exe'
    pdb='main.pdb'

    # TODO: Need to change debug compilation directory in mingw
    flags_debug="-g --for-linker --pdb=$pdb"
}

config_clang_from_linux_to_windows() {
    local target='x86_64-w64-windows-gnu' # Also valid: 'x86_64-w64-windows-gnu' 'x86_64-windows-gnu' but don't know the difference
    cc="clang --target=$target"
    cxx="clang++ --target=$target"
    glfw_obj=rglfw.obj
    bin='main.exe'
    pdb='main.pdb'

    #
    # Here we're trying to get clang to generated .pdb files for debugging
    # See: https://handmade.network/p/71/c-ode-clap/forums/t/3596-expected_date_for_windows_dwarf_support
    # First iteration was something like this: clang.exe -fuse-ld=lld.exe -g -gcodeview -Wl,/debug,/pdb:test.pdb
    #
    # NOTE: its important to set a 'debug-compile-dir' to the working directory you intend to launch the program.
    # This allows the debugger find your source code from the debug information otherwise it'll point to files where it was first built
    # which if they don't match the debbuger wont find it.
    #
    flags_debug_codeview_extra="-gcodeview-command-line -gcolumn-info"
    flags_debug_directory="-fdebug-prefix-map=$(pwd)=$WINDOWS_DESTINATION_DIR -fdebug-compilation-dir=$WINDOWS_DESTINATION_DIR"
    flags_debug_macro="-fdebug-macro"

    # Fixed debug bug with this resource: https://stackoverflow.com/questions/74416539/clang-14-does-not-generate-pdb-file
    flags_debug="-v -g -gcodeview -fuse-ld=lld -Wl,--pdb= $flags_debug_directory $flags_debug_codeview_extra"


    #
    # NOTE: WinDbg "works" with dwarf-5 embed-source. Flags would be:
    # flags_debug="-g -gdwarf-5 -gcolumn-info -gmodules -gembed-source $flags_debug_directory"
    #
    # But can try other versions just in case the Debugger only parses older formats
    # flags_debug="-g -gmodules -gdwarf-3 $flags_debug_directory"
    #
}

build() {
    #
    # See for optimization flags: https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
    #
    # From: https://carlpearson.net/post/20220301-gcc-flags/
    # And https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html
    #     -Wpedantic: Issue all the warnings demanded by strict ISO C++.
    #     -Wcast-align: warn whenever a pointer is cast such that the required alignment is increased (char* -> int*).
    #     -Wdisabled-optimization: warn if a requested optimization pass is disabled (e.g. code is too large, has some other feature that makes g++ give up).
    #     -Wcast-qual: warn when qualifier (const) is cast away, or introduces a qualifier in an unsafe way.
    #     -Wformat=2: same as -Wformat -Wformat-nonliteral -Wformat-security -Wformat-y2k. make sure printf-style function arguments match their format strings.
    #     -Wlogical-op: warn about suspicious use of logical operators, i.e. contexts where bitwise is more likely.
    #     -Wnull-dereference: warn if paths that dereference a null pointer are detected.
    #     -Wpointer-arith: warn about sizeof for function types or void.
    #     -Wshadow: warn about variable shadowing and global function shadowing.
    #     -Wswitch-enum: warn when a switch on an enum type is missing one of the enums.
    #     -Wswitch-default: warn whenever a switch statement does not have a default case*.
    #     -Wvla: warn about using variable-length arrays.
    #     -fno-rtti for cpp only, you know what it means
    #     -Wfloat-equal: useful because usually testing floating-point numbers for equality is bad.
    #     -Wundef: warn if an uninitialized identifier is evaluated in an #if directive.
    #     -Wstrict-prototypes: warn if a function is declared or defined without specifying the argument types.
    #     -Wstrict-overflow=5: warns about cases where the compiler optimizes based on the assumption that signed overflow does not occur (famous UB with big discussion when gcc implemented this). (The value 5 may be too strict, see the manual page.)
    #     -Wwrite-strings: give string constants the type const char[length] so that copying the address of one into a non-const char * pointer will get a warning.
    #     -Waggregate-return: warn if any functions that return structures or unions are defined or called.
    #

    # Annoying warnings removed
    flags_no_warn='-Wno-unused-command-line-argument -Wno-missing-braces -Wno-format-nonliteral -Wno-unused-function -Wno-error=pointer-sign -Wno-error=missing-braces -Wno-unused-parameter -Wno-unused-variable -Wno-strict-aliasing -Wno-unknown-warning-option -Wno-unused-variable -Wno-gnu-zero-variadic-macro-arguments -Wno-keyword-macro -Wno-unused-variable -Wno-self-assign -Wno-nan-infinity-disabled'


    # Collection of decently extra extra warnings
    flags_ub='-fwrapv -fno-strict-aliasing -ftrapv'
    flags_sanitize='-fsanitize=undefined'

    # Everybody does some implicit conversion on purpose of compares floats to zero, cant use this.
    flags_warn_conversions="-Wfloat-equal -Wconversion"

    # If we use -Wformat=2 then it wont allow runtime format strings which we use
    flags_warn1='-Wdisabled-optimization -Wduplicated-cond -Wformat=1 -Wvla -Wstrict-overflow=5'
    flags_warn2="-Wlogical-op -Wnull-dereference -Wpointer-arith -Woverloaded-virtual"

    # Shadowing is done a lot, cant use
    flags_warn_shadow="-Wshadow "

    # These are nice but can't use it because of vendors
    flags_warn_switch='-Wswitch-default -Wswitch-enum'
    # Also cant use because of vendors
    flags_warn_cast='-Wcast-qual -Wcast-align'

    flags_warn="-Wall -Wextra -Werror $flags_no_warn $flags_ub $flags_warn1 $flags_warn2"

    # Check only
    # flags_warn="$flags_warn -fsyntax-only"



    case "$1" in
        release)
            echo "Configuring for release"
            flags="-DRELEASE -static -pipe -static -O3 -ffast-math -fno-exceptions -finline-functions"
            ;;
        debug)
            echo "Configuring for debug"
            flags="-static -O0 $flags_debug $flags_warn"
            ;;
        *)
            echo "Configuring for dev speed"
            flags="-O0 -ferror-limit=8 $flags_warn"
            ;;
    esac

    pushd ./src/deps/glfw/
    [ -f "$glfw_obj" ] || $cc rglfw.c -o $glfw_obj -c -lc -lm -O3
    popd

    profile_start

    # Extensions from clang: https://clang.llvm.org/docs/LanguageExtensions.html#matrix-types
    # Extensions from gnu: https://gcc.gnu.org/onlinedocs/gcc/Syntax-Extensions.html
    # Language standard with GNU extensions
    # -std=c99
    # -std=c23
    std_flags="-std=gnu2x"  # GNU-extended C23 (equivalent to -std=gnu23)
    # std_flags="-std=nu2y"

    # Enable all C23 features and GNU extensions
    # extension_flags="-fenable-matrix -fms-extensions -fgnuc-version=13 -fgnu-keywords"
    extension_flags="-fenable-matrix"

    # Enable specific C23 features
    # c23_features="-fdeclspec -fblocks -fcoroutines-ts -fdouble-square-bracket-attributes"
    c23_features="-fdeclspec -fblocks -fcoroutines "
    c23_full="-fchar8_t -fexperimental-new-constant-interpreter"
    win_extras="-fms-compatibility -fdelayed-template-parsing -fms-extensions"

    # flags="$flags $c23_features $extension_flags $std_flags"
    # flags="$flags $std_flags $c23_features $c23_full"
    flags="$flags $std_flags $extension_flags"

    set -x
    $cc -Isrc  \
        $flags \
        src/main.c                         \
        src/deps/glfw/$glfw_obj            \
        -o $bin                            \
        -Isrc/deps/                        \
        -Isrc/deps/glfw/glfw/include/      \
        -lm -lgdi32 -luser32

    set +x
    # -lkernel32 -lwinmm

    profile_end

}

create_zip() {
    if [ -z "$1" ]; then
        echo "Usage: create_zip <output_zip_file_name>"
        return 1
    fi

    local zip_file="$1"
    zip -r "$zip_file" res/ src/renderer/shared/ src/shaders/* main.exe

    if [ $? -eq 0 ]; then
        echo "Successfully created $zip_file"
    else
        echo "Failed to create zip file"
    fi
}


# config_mingw
config_clang_from_linux_to_windows

# --------------------------
# Environment Detection
# --------------------------

sync_to_windows() {
    local pkill_cmd="$(wslpath 'C:\Windows\System32\taskkill.exe')"
    set -x
    $pkill_cmd /F /IM $DEBUGGER_EXECUTABLE_NAME || echo "$DEBUGGER_EXECUTABLE_NAME not open, which is fine."
    set +x

    # Old rsync -r --exclude='.git' --exclude='*.zip' --exclude='.cache' --exclude='*.obj' --size-only ./ "$(wslpath "$WINDOWS_DESTINATION_DIR")"
    local exclude_patterns=(
        --exclude='.git'
        --exclude='*.zip'
        --exclude='.cache'
        --exclude="$glfw_obj"
    )
    echo "Syncing files to Windows..."

    profile_start

    rync_flags='--size-only' # size_only might be wrong sometimes, albeit its fast
    # --times is important to let rsync skip some files next syncing point
    mkdir -p "$(wslpath "$WINDOWS_DESTINATION_DIR")" || true
    rsync -a --delete-delay --executability --times "${exclude_patterns[@]}" ./ "$(wslpath "$WINDOWS_DESTINATION_DIR")"

    profile_end "Syncing files into directory $WINDOWS_DESTINATION_DIR"
    # After syncing we don't want any '.pdb' files here
    rm -f *.pdb
}

on_wsl() {
  if grep -qEi '(microsoft|wsl)' /proc/version 2>/dev/null; then
    return 0
  else
    return 1
  fi
}

start_debugger() {
    local debugger="$(wslpath "$DEBUGGER_FILEPATH")"
    local target_dir="$(wslpath "$WINDOWS_DESTINATION_DIR")"
    echo "Starting debugger $debugger from $target_dir..."
    cd "$target_dir" && "$debugger" "$1" # Pass in the executable
}

main() {
    case "$1" in
        dev)
            build
            ./"$bin"
            ;;
        build)
            build "build"
            ;;
        release)
            build "release"
            create_zip release
            ./"$bin"
            ;;
        zip)
            build "release"
            create_zip raymarch
            ;;
        run)
            build "release"
            ./"$bin"
            ;;
        debug)
            if ! on_wsl; then
                echo "Debugging is only supported on WSL"
                exit 1
            fi

            build "debug"
            sync_to_windows
            start_debugger $bin
            ;;
        *)
            echo "Usage: $0 {build|run|debug}"
            exit 1
            ;;
    esac
}


main "$@"

