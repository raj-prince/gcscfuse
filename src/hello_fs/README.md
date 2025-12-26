# HelloFS

A minimal FUSE filesystem example that demonstrates the C++ FUSE wrapper.

## What it does

Creates a read-only filesystem with:
- Root directory `/`
- Single file `/hello` containing "Hello World!\n"

## Build

```bash
cd build
cmake ..
make
```

## Run

```bash
mkdir -p ~/gcs
./build/hello_fs ~/gcs
```

Test:
```bash
ls ~/gcs
cat ~/gcs/hello
```

Unmount:
```bash
umount ~/gcs
```

## Implementation

- [hello.cpp](hello.cpp) - Main entry point
- [hello_fs.hpp](hello_fs.hpp) - HelloFS class definition
- [hello_fs.cpp](hello_fs.cpp) - FUSE operations implementation
