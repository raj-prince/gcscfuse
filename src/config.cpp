#include "config.hpp"
#include <yaml-cpp/yaml.h>
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <sys/stat.h>

namespace {
    bool parseBool(const std::string& value) {
        std::string v = value;
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        return v == "true" || v == "yes" || v == "1" || v == "on";
    }
}

void GCSFSConfig::loadDefaults() {
    enable_stat_cache = true;
    stat_cache_timeout = 60;
    enable_file_content_cache = true;
    debug_mode = false;
    verbose_logging = false;
    bucket_name = "";
    mount_point = "";
    fuse_args.clear();
}

bool GCSFSConfig::loadFromYAML(const std::string& config_path) {
    try {
        YAML::Node config = YAML::LoadFile(config_path);
        
        // Load YAML keys to config fields
        if (config["bucket_name"]) {
            bucket_name = config["bucket_name"].as<std::string>();
        }
        
        if (config["mount_point"]) {
            mount_point = config["mount_point"].as<std::string>();
        }
        
        if (config["enable_stat_cache"]) {
            enable_stat_cache = config["enable_stat_cache"].as<bool>();
        }
        
        if (config["stat_cache_timeout"]) {
            stat_cache_timeout = config["stat_cache_timeout"].as<int>();
        }
        
        if (config["enable_file_content_cache"]) {
            enable_file_content_cache = config["enable_file_content_cache"].as<bool>();
        }
        
        if (config["debug"]) {
            debug_mode = config["debug"].as<bool>();
        }
        
        if (config["verbose"]) {
            verbose_logging = config["verbose"].as<bool>();
        }
        
        return true;
    } catch (const YAML::BadFile&) {
        return false;  // File doesn't exist
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("YAML parsing error: " + std::string(e.what()));
    }
}

void GCSFSConfig::loadFromEnv() {
    // Check for GCSFUSE_* environment variables
    // Only override if bucket/mount not already set (allowing YAML to set them)
    if (const char* bucket = std::getenv("GCSFUSE_BUCKET")) {
        bucket_name = bucket;
    }
    if (const char* mount = std::getenv("GCSFUSE_MOUNT_POINT")) {
        mount_point = mount;
    }
    if (const char* cache = std::getenv("GCSFUSE_STAT_CACHE")) {
        enable_stat_cache = parseBool(cache);
    }
    if (const char* ttl = std::getenv("GCSFUSE_STAT_CACHE_TTL")) {
        stat_cache_timeout = std::atoi(ttl);
    }
    if (const char* file_cache = std::getenv("GCSFUSE_FILE_CACHE")) {
        enable_file_content_cache = parseBool(file_cache);
    }
    if (const char* debug = std::getenv("GCSFUSE_DEBUG")) {
        debug_mode = parseBool(debug);
    }
    if (const char* verbose = std::getenv("GCSFUSE_VERBOSE")) {
        verbose_logging = parseBool(verbose);
    }
}

std::optional<std::string> GCSFSConfig::extractConfigPath(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            return std::string(argv[i + 1]);
        }
        if (arg.rfind("--config=", 0) == 0) {
            return arg.substr(9);
        }
    }
    return std::nullopt;
}

GCSFSConfig GCSFSConfig::load(int argc, char* argv[]) {
    GCSFSConfig config;
    
    // Step 1: Load defaults
    config.loadDefaults();
    
    // Step 2: Check for --config flag and load YAML if present
    auto config_path = extractConfigPath(argc, argv);
    if (config_path) {
        if (!config.loadFromYAML(*config_path)) {
            throw std::runtime_error("Config file not found: " + *config_path);
        }
        if (config.debug_mode) {
            std::cout << "[DEBUG] Loaded config from: " << *config_path << std::endl;
        }
    }
    
    // Step 3: Load from environment variables
    config.loadFromEnv();
    
    // Step 4: Parse command-line arguments (highest priority)
    config.parseFromArgs(argc, argv);
    
    // Step 5: Validate final configuration
    config.validate();
    
    return config;
}

void GCSFSConfig::validate() const {
    if (bucket_name.empty()) {
        throw std::runtime_error("Bucket name is required (via config, env, or CLI)");
    }
    if (mount_point.empty()) {
        throw std::runtime_error("Mount point is required (via config, env, or CLI)");
    }
    if (stat_cache_timeout < 0) {
        throw std::runtime_error("stat_cache_timeout must be >= 0");
    }
}

void GCSFSConfig::parseFromArgs(int argc, char* argv[]) {
    
    // Define long options
    static struct option long_options[] = {
        {"config",                   required_argument, 0, 'c'},
        {"disable-stat-cache",        no_argument,       0, 's'},
        {"stat-cache-ttl",           required_argument, 0, 'T'},
        {"disable-file-cache",       no_argument,       0, 'f'},
        {"disable-file-content-cache",no_argument,       0, 'F'},
        {"enable-dummy-reader",      no_argument,       0, 'D'},
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
            case 'c':
                // Config file already processed in load(), skip here
                break;
            case 's':
                enable_stat_cache = false;
                break;
            case 'T':
                stat_cache_timeout = atoi(optarg);
                break;
            case 'f':
                // This could be -f for foreground (FUSE) or --disable-file-cache
                // Check if it's from long option
                if (option_index >= 0 && long_options[option_index].val == 'f') {
                    enable_file_content_cache = false;
                } else {
                    // It's FUSE -f flag, add to fuse_args
                    fuse_args.push_back("-f");
                }
                break;
            case 'F':
                // --disable-file-content-cache
                enable_file_content_cache = false;
                break;
            case 'D':
                // --enable-dummy-reader
                enable_dummy_reader = true;
                break;
            case 'd':
                // Could be --debug or FUSE -d
                if (option_index >= 0 && long_options[option_index].val == 'd') {
                    debug_mode = true;
                } else {
                    // FUSE -d flag
                    fuse_args.push_back("-d");
                }
                break;
            case 'v':
                verbose_logging = true;
                break;
            case 'o':
                // FUSE -o option
                fuse_args.push_back("-o");
                fuse_args.push_back(optarg);
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
    
    // Override with positional arguments if provided
    if (!positional_args.empty()) {
        bucket_name = positional_args[0];
    }
    if (positional_args.size() >= 2) {
        mount_point = positional_args[1];
    }
    
    // Any additional positional args go to FUSE
    for (size_t i = 2; i < positional_args.size(); i++) {
        fuse_args.push_back(positional_args[i]);
    }
}

void GCSFSConfig::printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <bucket_name> <mount_point> [options]\n";
    std::cout << "   or: " << program_name << " --config <config.yaml> [options]\n\n";
    std::cout << "Required arguments:\n";
    std::cout << "  bucket_name              GCS bucket name to mount\n";
    std::cout << "  mount_point              Directory to mount the filesystem\n\n";
    
    std::cout << "GCSFS options:\n";
    std::cout << "  --config=FILE            Load configuration from YAML file\n";
    std::cout << "  --disable-stat-cache     Disable stat metadata cache (enabled by default)\n";
    std::cout << "  --stat-cache-ttl=N       Stat cache timeout in seconds (default: 60, 0=no timeout)\n";
    std::cout << "  --disable-file-cache     Disable file content cache (enabled by default)\n";
    std::cout << "  --enable-dummy-reader    Use dummy reader for testing (returns zeros)\n";
    std::cout << "  --debug                  Enable debug logging\n";
    std::cout << "  --verbose                Enable verbose output\n";
    std::cout << "  --help                   Display this help message\n\n";
    
    std::cout << "FUSE options:\n";
    std::cout << "  -f                       Run in foreground\n";
    std::cout << "  -d                       Enable FUSE debug output\n";
    std::cout << "  -o option                Mount options (e.g., -o allow_other)\n\n";
    
    std::cout << "Environment variables:\n";
    std::cout << "  GCSFUSE_BUCKET           Bucket name (overridden by CLI/config)\n";
    std::cout << "  GCSFUSE_MOUNT_POINT      Mount point (overridden by CLI/config)\n";
    std::cout << "  GCSFUSE_STAT_CACHE       Enable stat cache (true/false)\n";
    std::cout << "  GCSFUSE_FILE_CACHE       Enable file cache (true/false)\n";
    std::cout << "  GCSFUSE_DEBUG            Enable debug mode (true/false)\n\n";
    
    std::cout << "Configuration priority (highest to lowest):\n";
    std::cout << "  1. Command-line arguments\n";
    std::cout << "  2. Environment variables\n";
    std::cout << "  3. YAML config file (--config)\n";
    std::cout << "  4. Default values\n\n";
    
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " my-bucket ~/mnt\n";
    std::cout << "  " << program_name << " --config config.yaml\n";
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
