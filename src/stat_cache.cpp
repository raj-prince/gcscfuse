#include "stat_cache.hpp"
#include <sstream>
#include <algorithm>

StatCache::StatCache() : root_(std::make_unique<TrieNode>()) {
    root_->exists = true;
    root_->stat_info.is_directory = true;
    root_->stat_info.mode = S_IFDIR | 0755;
    root_->stat_info.metadata_loaded = true;
}

std::vector<std::string> StatCache::splitPath(const std::string& path) const {
    std::vector<std::string> components;
    
    if (path.empty() || path == "/") {
        return components;
    }
    
    std::string cleaned = path;
    // Remove leading slash
    if (cleaned[0] == '/') {
        cleaned = cleaned.substr(1);
    }
    // Remove trailing slash
    if (!cleaned.empty() && cleaned.back() == '/') {
        cleaned = cleaned.substr(0, cleaned.length() - 1);
    }
    
    std::stringstream ss(cleaned);
    std::string component;
    while (std::getline(ss, component, '/')) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }
    
    return components;
}

StatCache::TrieNode* StatCache::findNode(const std::string& path, bool create_if_missing) {
    auto components = splitPath(path);
    
    TrieNode* current = root_.get();
    
    for (const auto& component : components) {
        auto it = current->children.find(component);
        
        if (it == current->children.end()) {
            if (!create_if_missing) {
                return nullptr;
            }
            current->children[component] = std::make_unique<TrieNode>();
        }
        
        current = current->children[component].get();
    }
    
    return current;
}

void StatCache::insertFile(const std::string& path, off_t size, time_t mtime) {
    auto components = splitPath(path);
    if (components.empty()) return;
    
    // Ensure all parent directories exist
    std::string parent_path;
    for (size_t i = 0; i < components.size() - 1; ++i) {
        parent_path += "/" + components[i];
        insertDirectory(parent_path);
    }
    
    // Insert the file
    TrieNode* node = findNode(path, true);
    if (node) {
        node->exists = true;
        node->stat_info.is_directory = false;
        node->stat_info.mode = S_IFREG | 0644;  // Read-write file
        node->stat_info.size = size;
        node->stat_info.mtime = mtime;
        node->stat_info.cache_time = time(nullptr);
        node->stat_info.metadata_loaded = true;
    }
}

void StatCache::insertDirectory(const std::string& path) {
    if (path.empty() || path == "/") return;
    
    TrieNode* node = findNode(path, true);
    if (node && !node->stat_info.metadata_loaded) {
        node->exists = true;
        node->stat_info.is_directory = true;
        node->stat_info.mode = S_IFDIR | 0755;
        node->stat_info.size = 0;
        node->stat_info.mtime = time(nullptr);
        node->stat_info.cache_time = time(nullptr);
        node->stat_info.metadata_loaded = true;
    }
}

std::optional<StatCache::StatInfo> StatCache::getStat(const std::string& path) const {
    if (path.empty() || path == "/") {
        return root_->stat_info;
    }
    
    TrieNode* node = const_cast<StatCache*>(this)->findNode(path, false);
    if (node && node->exists && node->stat_info.metadata_loaded) {
        // Check if expired (lazy eviction)
        if (isExpired(node->stat_info)) {
            return std::nullopt;
        }
        return node->stat_info;
    }
    
    return std::nullopt;
}

bool StatCache::exists(const std::string& path) const {
    if (path.empty() || path == "/") {
        return true;
    }
    
    TrieNode* node = const_cast<StatCache*>(this)->findNode(path, false);
    return node && node->exists;
}

bool StatCache::isDirectory(const std::string& path) const {
    if (path.empty() || path == "/") {
        return true;
    }
    
    TrieNode* node = const_cast<StatCache*>(this)->findNode(path, false);
    return node && node->exists && node->stat_info.is_directory;
}

void StatCache::clear() {
    root_ = std::make_unique<TrieNode>();
    root_->exists = true;
    root_->stat_info.is_directory = true;
    root_->stat_info.mode = S_IFDIR | 0755;
    root_->stat_info.metadata_loaded = true;
}

std::vector<std::string> StatCache::listDirectory(const std::string& path) const {
    std::vector<std::string> entries;
    
    TrieNode* node = const_cast<StatCache*>(this)->findNode(path, false);
    if (!node || !node->stat_info.is_directory) {
        return entries;
    }
    
    for (const auto& [name, child] : node->children) {
        if (child->exists) {
            entries.push_back(name);
        }
    }
    
    return entries;
}

void StatCache::remove(const std::string& path)
{
    std::vector<std::string> components = splitPath(path);
    if (components.empty()) {
        return;
    }
    
    TrieNode* current = root_.get();
    std::vector<TrieNode*> path_nodes;
    path_nodes.push_back(current);
    
    // Navigate to the target node
    for (const auto& component : components) {
        auto it = current->children.find(component);
        if (it == current->children.end()) {
            return; // Path doesn't exist, nothing to remove
        }
        current = it->second.get();
        path_nodes.push_back(current);
    }
    
    // Mark the node as non-existent
    current->exists = false;
    current->stat_info = StatInfo();
    
    // Optionally, clean up empty nodes (prune the trie)
    // Work backwards from the target to root
    for (int i = components.size() - 1; i >= 0; --i) {
        TrieNode* node = path_nodes[i + 1];
        
        // If node has no children and doesn't exist, remove it from parent
        if (node->children.empty() && !node->exists) {
            TrieNode* parent = path_nodes[i];
            parent->children.erase(components[i]);
        } else {
            // Stop pruning if node is still needed
            break;
        }
    }
}

