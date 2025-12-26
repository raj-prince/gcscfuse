# gcscfuse

C++ implementation of fuse offering for Google Cloud Storage.

## Build hello_fs

```bash
cd src/hello_fs/build
cmake ..
make
```

## Mount hello_fs

```bash
mkdir -p ~/gcs
./src/hello_fs/build/hello_fs ~/gcs
```

Unmount:
```bash
umount ~/gcs
```
