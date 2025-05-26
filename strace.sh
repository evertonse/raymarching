#!/bin/sh

# Generate compile_commands.json for clangd using strace to "parse" the output of strace after runnig a command and putting it all in the json
# TODO: Make this simply automatic
# Usage: ./strace.sh build.sh

OUTPUT_FILE="compile_commands.json"
STRACE_LOG="/tmp/build_trace_$$"

# awk to extract compile commands from strace output
# Might ber possible with only shell or whatevs that might be more readable for everyone
extract_compile_commands() {
    awk '
    BEGIN {
        print "["
        first = 1
    }
    
    /execve.*gcc/ || /execve.*clang/ || /execve.*cc/ {
        # Extract the command line from execve
        if (match($0, /execve\("([^"]*)".*\[([^\]]*)\]/, arr)) {
            compiler = arr[1]
            args_str = arr[2]
            
            # Parse arguments
            gsub(/"/, "", args_str)
            gsub(/,/, "", args_str)
            
            # Check if this looks like a compile command (has .c file)
            if (match(args_str, /[^ ]*\.c/)) {
                if (!first) print ","
                first = 0
                
                printf "  {\n"
                printf "    \"directory\": \"%s\",\n", ENVIRON["PWD"]
                printf "    \"command\": \"%s %s\",\n", compiler, args_str
                
                # Extract source file
                if (match(args_str, /([^ ]*\.c)/, src)) {
                    printf "    \"file\": \"%s\"\n", src[1]
                } else {
                    printf "    \"file\": \"\"\n"
                }

                printf "  }"
            }
        }
    }
    
    END {
        print "\n]"
    }
    ' "$STRACE_LOG" > "$OUTPUT_FILE"
}

# Check if strace is available
if ! command -v strace >/dev/null 2>&1; then
    echo "Error: strace is required but not found" >&2
    exit 1
fi

if [ $# -eq 0 ]; then
    echo "Usage: $0 <function_name>" >&2
    exit 1
fi

# Initially it was supopose to just be a shell function, so the name stayed
FUNCTION_NAME="$1"

# Check if function exists
if ! command -v "$FUNCTION_NAME" >/dev/null 2>&1 && ! type "$FUNCTION_NAME" >/dev/null 2>&1; then
    echo "Error: Function '$FUNCTION_NAME' not found" >&2
    echo "Make sure to source this script or define the function first" >&2
    exit 1
fi

echo "Tracing build process..."

# Run the build command under strace
strace -f -e trace=execve -o "$STRACE_LOG" sh -c "$FUNCTION_NAME" 2>/dev/null

if [ $? -ne 0 ]; then
    echo "Build failed" >&2
    rm -f "$STRACE_LOG"
    exit 1
fi

echo "Extracting compile commands..."
extract_compile_commands

# Cleanup
rm -f "$STRACE_LOG"

echo "Generated $OUTPUT_FILE successfully!"
