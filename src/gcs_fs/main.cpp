// GCS filesystem main entry point

#include "gcs_fs.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <bucket_name> <mount_point> [fuse_options...]\n";
        std::cerr << "Example: " << argv[0] << " my-bucket ~/gcs -f\n";
        return 1;
    }

    // First argument is the bucket name
    std::string bucket_name = argv[1];
    
    // Shift arguments for FUSE (skip bucket_name)
    char **fuse_argv = new char*[argc];
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i-1] = argv[i];
    }
    int fuse_argc = argc - 1;

    try {
        GCSFS fs(bucket_name);
        const auto status = fs.run(fuse_argc, fuse_argv);
        delete[] fuse_argv;
        return status;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        delete[] fuse_argv;
        return 1;
    }
}
