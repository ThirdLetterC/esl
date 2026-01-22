set shell := ["bash", "-cu"]

default: build

build:
    zig build

build-testclient:
    zig build testclient

run: build
    ./zig-out/bin/testclient

clean:
    rm -rf zig-out .zig-cache

format:
    clang-format-21 -i src/include/*.h src/*.c testclient.c

fmt: format
