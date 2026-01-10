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
