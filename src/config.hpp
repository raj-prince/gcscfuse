#pragma once

#include <string>
#include <vector>
#include <optional>

/**
 * GCSFSConfig - Configuration options for GCSFS
 * 
 * Supports layered configuration from multiple sources:
 * 1. Defaults (lowest priority)
 * 2. YAML config file
 * 3. Environment variables
 * 4. Command-line arguments (highest priority)
 */
struct GCSFSConfig {
    // Stat cache settings
    bool enable_stat_cache = true;
    int stat_cache_timeout = 60;  // seconds, 0 = no timeout
    
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
     * Load configuration from all sources in priority order
     * 
     * @param argc Argument count
     * @param argv Argument values
     * @return Parsed configuration
     * @throws std::runtime_error if required arguments are missing or invalid
     */
    static GCSFSConfig load(int argc, char* argv[]);
    
    /**
     * Parse command-line arguments into config and FUSE args
     * This method applies CLI overrides to an existing config
     * 
     * @param argc Argument count
     * @param argv Argument values
     * @throws std::runtime_error if required arguments are missing or invalid
     */
    void parseFromArgs(int argc, char* argv[]);
    
    /**
     * Load configuration from YAML file
     * 
     * @param config_path Path to YAML config file
     * @return true if file was loaded successfully, false if file doesn't exist
     * @throws std::runtime_error if file exists but is invalid
     */
    bool loadFromYAML(const std::string& config_path);
    
    /**
     * Load configuration from environment variables
     * Recognizes: GCSFUSE_* variables
     */
    void loadFromEnv();
    
    /**
     * Set default values
     */
    void loadDefaults();
    
    /**
     * Validate configuration
     * @throws std::runtime_error if configuration is invalid
     */
    void validate() const;
    
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
    
private:
    /**
     * Extract --config flag from arguments before full parsing
     */
    static std::optional<std::string> extractConfigPath(int argc, char* argv[]);
};
