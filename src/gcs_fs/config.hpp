#pragma once

#include <string>
#include <vector>

/**
 * GCSFSConfig - Configuration options for GCSFS
 * 
 * Parses command-line flags and stores runtime configuration.
 */
struct GCSFSConfig {
    // Stat cache settings
    bool enable_stat_cache = true;
    
    // File content cache settings
    bool enable_file_content_cache = true;
    
    // Logging settings
    bool debug_mode = false;
    bool verbose_logging = false;
    
    // Bucket name (required)
    std::string bucket_name;
    
    // Mount point (required)
    std::string mount_point;
    
    // Remaining FUSE arguments
    std::vector<std::string> fuse_args;
    
    /**
     * Parse command-line arguments into config and FUSE args
     * 
     * @param argc Argument count
     * @param argv Argument values
     * @return Parsed configuration
     * @throws std::runtime_error if required arguments are missing or invalid
     */
    static GCSFSConfig parseFromArgs(int argc, char* argv[]);
    
    /**
     * Print usage information
     */
    static void printUsage(const char* program_name);
    
    /**
     * Convert config and fuse_args back to argc/argv format for FUSE
     * 
     * @param out_argc Output argument count
     * @param out_argv Output argument vector (caller must free)
     */
    void toFuseArgs(int& out_argc, char**& out_argv) const;
};
