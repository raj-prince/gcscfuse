# gcscfuse

C++ implementation of fuse offering for Google Cloud Storage.

## Projects

- [hello_fs](src/hello_fs/) - Minimal FUSE example with a single "Hello World" file
- [gcs_fs](src/gcs_fs/) - GCS FUSE implementation with read-only access to buckets

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

## Build gcs_fs

See [src/gcs_fs/README.md](src/gcs_fs/README.md) for prerequisites and setup.

```bash
cd src/gcs_fs/build
cmake ..
make
```

## Mount gcs_fs

```bash
./src/gcs_fs/build/gcs_fs <bucket-name> ~/gcs
```
