#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <sys/stat.h>
#include <optional>

/**
 * StatCache - Trie-based cache for storing file/directory metadata
 * 
 * Efficiently stores stat information for paths in a trie structure.
 * Each node represents a path component (directory or file).
 */
class StatCache {
public:
    struct StatInfo {
        mode_t mode;           // File type and permissions
        off_t size;            // File size in bytes
        time_t mtime;          // Last modification time
        bool is_directory;     // True if this is a directory
        bool metadata_loaded;  // True if metadata has been fetched from GCS
        
        StatInfo() 
            : mode(0), size(0), mtime(0), is_directory(false), metadata_loaded(false) {}
    };

private:
    struct TrieNode {
        std::map<std::string, std::unique_ptr<TrieNode>> children;
        StatInfo stat_info;
        bool exists = false;  // True if this path exists (file or directory)
        
        TrieNode() = default;
    };

    std::unique_ptr<TrieNode> root_;

    // Helper to split path into components
    std::vector<std::string> splitPath(const std::string& path) const;
    
    // Helper to find or create a node for a path
    TrieNode* findNode(const std::string& path, bool create_if_missing = false);

public:
    StatCache();
    ~StatCache() = default;

    // Insert a file with its metadata
    void insertFile(const std::string& path, off_t size, time_t mtime);
    
    // Mark a path as a directory
    void insertDirectory(const std::string& path);
    
    // Get stat info for a path
    std::optional<StatInfo> getStat(const std::string& path) const;
    
    // Check if a path exists in the cache
    bool exists(const std::string& path) const;
    
    // Check if a path is a directory
    bool isDirectory(const std::string& path) const;
    
    // Remove a path from the cache
    void remove(const std::string& path);
    
    // Clear all cached data
    void clear();
    
    // Get all immediate children of a directory
    std::vector<std::string> listDirectory(const std::string& path) const;
};
