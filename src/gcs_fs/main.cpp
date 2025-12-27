// GCS filesystem main entry point

#include "gcs_fs.hpp"
#include "config.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    try {
        // Parse configuration from command-line arguments
        GCSFSConfig config = GCSFSConfig::parseFromArgs(argc, argv);
        
        // Convert config back to FUSE arguments
        int fuse_argc;
        char** fuse_argv;
        config.toFuseArgs(fuse_argc, fuse_argv);
        
        // Create and run filesystem
        GCSFS fs(config.bucket_name, config);
        const auto status = fs.run(fuse_argc, fuse_argv);
        
        // Clean up allocated arguments
        for (int i = 0; i < fuse_argc; i++) {
            delete[] fuse_argv[i];
        }
        delete[] fuse_argv;
        
        return status;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "\nUse --help for usage information.\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }
}
