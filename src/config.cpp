#include "config.hpp"
#include <getopt.h>
#include <iostream>
#include <cstring>
#include <stdexcept>

GCSFSConfig GCSFSConfig::parseFromArgs(int argc, char* argv[]) {
    GCSFSConfig config;
    
    // Define long options
    static struct option long_options[] = {
        {"disable-stat-cache",        no_argument,       0, 's'},
        {"stat-cache-ttl",           required_argument, 0, 'T'},
        {"disable-file-cache",       no_argument,       0, 'f'},
        {"disable-file-content-cache",no_argument,       0, 'F'},
        {"debug",                    no_argument,       0, 'd'},
        {"verbose",                  no_argument,       0, 'v'},
        {"help",                     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Reset getopt state
    optind = 1;
    
    int opt;
    int option_index = 0;
    
    // First pass: extract bucket name and mount point (positional args)
    std::vector<std::string> positional_args;
    std::vector<std::string> all_args;
    
    for (int i = 1; i < argc; i++) {
        all_args.push_back(argv[i]);
    }
    
    // Parse options
    while ((opt = getopt_long(argc, argv, "o:dfvFh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                config.enable_stat_cache = false;
                break;
            case 'T':
                config.stat_cache_timeout = atoi(optarg);
                break;
            case 'f':
                // This could be -f for foreground (FUSE) or --disable-file-cache
                // Check if it's from long option
                if (option_index >= 0 && long_options[option_index].val == 'f') {
                    config.enable_file_content_cache = false;
                } else {
                    // It's FUSE -f flag, add to fuse_args
                    config.fuse_args.push_back("-f");
                }
                break;
            case 'F':
                // --disable-file-content-cache
                config.enable_file_content_cache = false;
                break;
            case 'd':
                // Could be --debug or FUSE -d
                if (option_index >= 0 && long_options[option_index].val == 'd') {
                    config.debug_mode = true;
                } else {
                    // FUSE -d flag
                    config.fuse_args.push_back("-d");
                }
                break;
            case 'v':
                config.verbose_logging = true;
                break;
            case 'o':
                // FUSE -o option
                config.fuse_args.push_back("-o");
                config.fuse_args.push_back(optarg);
                break;
            case 'h':
                printUsage(argv[0]);
                exit(0);
            case '?':
                // Unknown option, might be FUSE option
                break;
            default:
                break;
        }
    }
    
    // Collect positional arguments (bucket_name and mount_point)
    while (optind < argc) {
        positional_args.push_back(argv[optind++]);
    }
    
    // Validate positional arguments
    if (positional_args.empty()) {
        throw std::runtime_error("Missing required argument: bucket_name");
    }
    if (positional_args.size() < 2) {
        throw std::runtime_error("Missing required argument: mount_point");
    }
    
    config.bucket_name = positional_args[0];
    config.mount_point = positional_args[1];
    
    // Any additional positional args go to FUSE
    for (size_t i = 2; i < positional_args.size(); i++) {
        config.fuse_args.push_back(positional_args[i]);
    }
    
    return config;
}

void GCSFSConfig::printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <bucket_name> <mount_point> [options]\n\n";
    std::cout << "Required arguments:\n";
    std::cout << "  bucket_name              GCS bucket name to mount\n";
    std::cout << "  mount_point              Directory to mount the filesystem\n\n";
    
    std::cout << "GCSFS options:\n";
    std::cout << "  --disable-stat-cache     Disable stat metadata cache (enabled by default)\n";
    std::cout << "  --stat-cache-ttl=N       Stat cache timeout in seconds (default: 60, 0=no timeout)\n";
    std::cout << "  --disable-file-cache     Disable file content cache (enabled by default)\n";
    std::cout << "  --debug                  Enable debug logging\n";
    std::cout << "  --verbose                Enable verbose output\n";
    std::cout << "  --help                   Display this help message\n\n";
    
    std::cout << "FUSE options:\n";
    std::cout << "  -f                       Run in foreground\n";
    std::cout << "  -d                       Enable FUSE debug output\n";
    std::cout << "  -o option                Mount options (e.g., -o allow_other)\n\n";
    
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " my-bucket ~/mnt\n";
    std::cout << "  " << program_name << " my-bucket ~/mnt --disable-stat-cache -f\n";
    std::cout << "  " << program_name << " my-bucket ~/mnt --debug -o allow_other\n";
}

void GCSFSConfig::toFuseArgs(int& out_argc, char**& out_argv) const {
    // Build argument list: program_name, mount_point, [fuse_args]
    std::vector<std::string> args;
    args.push_back("gcs_fs");  // Program name
    args.push_back(mount_point);
    
    for (const auto& arg : fuse_args) {
        args.push_back(arg);
    }
    
    // Allocate argv array
    out_argc = static_cast<int>(args.size());
    out_argv = new char*[out_argc + 1];
    
    for (int i = 0; i < out_argc; i++) {
        out_argv[i] = new char[args[i].length() + 1];
        std::strcpy(out_argv[i], args[i].c_str());
    }
    out_argv[out_argc] = nullptr;
}
