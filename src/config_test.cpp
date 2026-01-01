#include "config.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <unistd.h>

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Save original environment
        saveEnv("GCSFUSE_BUCKET");
        saveEnv("GCSFUSE_MOUNT_POINT");
        saveEnv("GCSFUSE_STAT_CACHE");
        saveEnv("GCSFUSE_FILE_CACHE");
        saveEnv("GCSFUSE_DEBUG");
        saveEnv("GCSFUSE_VERBOSE");
    }
    
    void TearDown() override {
        // Restore environment
        restoreEnv();
        
        // Clean up test files
        if (!test_yaml_file.empty()) {
            unlink(test_yaml_file.c_str());
        }
    }
    
    void saveEnv(const std::string& name) {
        const char* value = std::getenv(name.c_str());
        saved_env[name] = value ? std::string(value) : "";
    }
    
    void restoreEnv() {
        for (const auto& pair : saved_env) {
            if (pair.second.empty()) {
                unsetenv(pair.first.c_str());
            } else {
                setenv(pair.first.c_str(), pair.second.c_str(), 1);
            }
        }
    }
    
    void setEnv(const std::string& name, const std::string& value) {
        setenv(name.c_str(), value.c_str(), 1);
    }
    
    void unsetEnv(const std::string& name) {
        unsetenv(name.c_str());
    }
    
    std::string createTestYAML(const std::string& content) {
        test_yaml_file = "/tmp/test_config_" + std::to_string(getpid()) + ".yaml";
        std::ofstream file(test_yaml_file);
        file << content;
        file.close();
        return test_yaml_file;
    }
    
    std::map<std::string, std::string> saved_env;
    std::string test_yaml_file;
};

// Test default values
TEST_F(ConfigTest, LoadDefaults) {
    GCSFSConfig config;
    config.loadDefaults();
    
    EXPECT_TRUE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 60);
    EXPECT_TRUE(config.enable_file_content_cache);
    EXPECT_FALSE(config.debug_mode);
    EXPECT_FALSE(config.verbose_logging);
    EXPECT_TRUE(config.bucket_name.empty());
    EXPECT_TRUE(config.mount_point.empty());
}

// Test YAML loading - basic fields
TEST_F(ConfigTest, LoadFromYAML_BasicFields) {
    std::string yaml_content = R"(
bucket_name: test-bucket
mount_point: /mnt/test
enable_stat_cache: false
stat_cache_timeout: 30
enable_file_content_cache: false
debug: true
verbose: true
)";
    
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    EXPECT_EQ(config.bucket_name, "test-bucket");
    EXPECT_EQ(config.mount_point, "/mnt/test");
    EXPECT_FALSE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 30);
    EXPECT_FALSE(config.enable_file_content_cache);
    EXPECT_TRUE(config.debug_mode);
    EXPECT_TRUE(config.verbose_logging);
}

// Test YAML loading - quoted values
TEST_F(ConfigTest, LoadFromYAML_QuotedValues) {
    std::string yaml_content = R"(
bucket_name: "quoted-bucket"
mount_point: '/tmp/quoted'
)";
    
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    EXPECT_EQ(config.bucket_name, "quoted-bucket");
    EXPECT_EQ(config.mount_point, "/tmp/quoted");
}

// Test YAML loading - comments and empty lines
TEST_F(ConfigTest, LoadFromYAML_CommentsAndEmptyLines) {
    std::string yaml_content = R"(
# This is a comment
bucket_name: test-bucket

# Another comment
mount_point: /mnt/test
)";
    
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    EXPECT_EQ(config.bucket_name, "test-bucket");
    EXPECT_EQ(config.mount_point, "/mnt/test");
}

// Test YAML loading - file not found
TEST_F(ConfigTest, LoadFromYAML_FileNotFound) {
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_FALSE(config.loadFromYAML("/nonexistent/config.yaml"));
}

// Test YAML loading - invalid syntax
TEST_F(ConfigTest, LoadFromYAML_InvalidSyntax) {
    std::string yaml_content = "this is not valid yaml without colon";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    
    EXPECT_THROW(config.loadFromYAML(yaml_file), std::runtime_error);
}

// Test environment variable loading
TEST_F(ConfigTest, LoadFromEnv_AllVariables) {
    setEnv("GCSFUSE_BUCKET", "env-bucket");
    setEnv("GCSFUSE_MOUNT_POINT", "/env/mount");
    setEnv("GCSFUSE_STAT_CACHE", "false");
    setEnv("GCSFUSE_STAT_CACHE_TTL", "45");
    setEnv("GCSFUSE_FILE_CACHE", "false");
    setEnv("GCSFUSE_DEBUG", "true");
    setEnv("GCSFUSE_VERBOSE", "true");
    
    GCSFSConfig config;
    config.loadDefaults();
    config.loadFromEnv();
    
    EXPECT_EQ(config.bucket_name, "env-bucket");
    EXPECT_EQ(config.mount_point, "/env/mount");
    EXPECT_FALSE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 45);
    EXPECT_FALSE(config.enable_file_content_cache);
    EXPECT_TRUE(config.debug_mode);
    EXPECT_TRUE(config.verbose_logging);
}

// Test environment variable loading - boolean variations
TEST_F(ConfigTest, LoadFromEnv_BooleanVariations) {
    GCSFSConfig config;
    
    // Test "yes"
    config.loadDefaults();
    setEnv("GCSFUSE_STAT_CACHE", "yes");
    config.loadFromEnv();
    EXPECT_TRUE(config.enable_stat_cache);
    
    // Test "1"
    config.loadDefaults();
    setEnv("GCSFUSE_STAT_CACHE", "1");
    config.loadFromEnv();
    EXPECT_TRUE(config.enable_stat_cache);
    
    // Test "on"
    config.loadDefaults();
    setEnv("GCSFUSE_STAT_CACHE", "on");
    config.loadFromEnv();
    EXPECT_TRUE(config.enable_stat_cache);
    
    // Test "false"
    config.loadDefaults();
    setEnv("GCSFUSE_STAT_CACHE", "false");
    config.loadFromEnv();
    EXPECT_FALSE(config.enable_stat_cache);
    
    // Test "no"
    config.loadDefaults();
    setEnv("GCSFUSE_STAT_CACHE", "no");
    config.loadFromEnv();
    EXPECT_FALSE(config.enable_stat_cache);
}

// Test priority: CLI > Env > YAML > Defaults
TEST_F(ConfigTest, ConfigPriority_YAMLOverridesDefaults) {
    std::string yaml_content = R"(
bucket_name: yaml-bucket
enable_stat_cache: false
stat_cache_timeout: 30
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    config.loadFromYAML(yaml_file);
    
    EXPECT_EQ(config.bucket_name, "yaml-bucket");
    EXPECT_FALSE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 30);
}

// Test priority: Env overrides YAML
TEST_F(ConfigTest, ConfigPriority_EnvOverridesYAML) {
    std::string yaml_content = R"(
bucket_name: yaml-bucket
stat_cache_timeout: 30
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    setEnv("GCSFUSE_BUCKET", "env-bucket");
    setEnv("GCSFUSE_STAT_CACHE_TTL", "60");
    
    GCSFSConfig config;
    config.loadDefaults();
    config.loadFromYAML(yaml_file);
    config.loadFromEnv();
    
    EXPECT_EQ(config.bucket_name, "env-bucket");  // Env overrides YAML
    EXPECT_EQ(config.stat_cache_timeout, 60);     // Env overrides YAML
}

// Test priority: CLI overrides everything
TEST_F(ConfigTest, ConfigPriority_CLIOverridesAll) {
    std::string yaml_content = R"(
bucket_name: yaml-bucket
mount_point: /yaml/mount
enable_stat_cache: false
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    setEnv("GCSFUSE_BUCKET", "env-bucket");
    
    const char* argv[] = {
        "gcscfuse",
        "cli-bucket",
        "/cli/mount",
        "--disable-stat-cache",
        nullptr
    };
    
    GCSFSConfig config;
    config.loadDefaults();
    config.loadFromYAML(yaml_file);
    config.loadFromEnv();
    config.parseFromArgs(4, const_cast<char**>(argv));
    
    EXPECT_EQ(config.bucket_name, "cli-bucket");  // CLI overrides all
    EXPECT_EQ(config.mount_point, "/cli/mount");  // CLI overrides all
    EXPECT_FALSE(config.enable_stat_cache);       // CLI sets this
}

// Test validation
TEST_F(ConfigTest, Validate_MissingBucket) {
    GCSFSConfig config;
    config.loadDefaults();
    config.mount_point = "/mnt/test";
    
    EXPECT_THROW(config.validate(), std::runtime_error);
}

// Test validation
TEST_F(ConfigTest, Validate_MissingMountPoint) {
    GCSFSConfig config;
    config.loadDefaults();
    config.bucket_name = "test-bucket";
    
    EXPECT_THROW(config.validate(), std::runtime_error);
}

// Test validation
TEST_F(ConfigTest, Validate_NegativeTimeout) {
    GCSFSConfig config;
    config.loadDefaults();
    config.bucket_name = "test-bucket";
    config.mount_point = "/mnt/test";
    config.stat_cache_timeout = -1;
    
    EXPECT_THROW(config.validate(), std::runtime_error);
}

// Test validation - success
TEST_F(ConfigTest, Validate_Success) {
    GCSFSConfig config;
    config.loadDefaults();
    config.bucket_name = "test-bucket";
    config.mount_point = "/mnt/test";
    
    EXPECT_NO_THROW(config.validate());
}

// Test full load() method
TEST_F(ConfigTest, Load_FullIntegration) {
    std::string yaml_content = R"(
bucket_name: yaml-bucket
mount_point: /yaml/mount
stat_cache_timeout: 30
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    setEnv("GCSFUSE_STAT_CACHE", "false");
    setEnv("GCSFUSE_STAT_CACHE_TTL", "99");  // Will be overridden by YAML
    
    const char* argv[] = {
        "gcscfuse",
        "--config",
        yaml_file.c_str(),
        "--debug",
        nullptr
    };
    
    GCSFSConfig config = GCSFSConfig::load(4, const_cast<char**>(argv));
    
    EXPECT_EQ(config.bucket_name, "yaml-bucket");     // From YAML
    EXPECT_EQ(config.mount_point, "/yaml/mount");     // From YAML
    EXPECT_EQ(config.stat_cache_timeout, 99);         // From Env (YAML value gets overridden by env)
    EXPECT_FALSE(config.enable_stat_cache);           // From Env
    EXPECT_TRUE(config.debug_mode);                   // From CLI
}

// Test adding new config fields - unknown fields should be ignored gracefully
TEST_F(ConfigTest, LoadFromYAML_UnknownFieldsIgnored) {
    std::string yaml_content = R"(
bucket_name: test-bucket
mount_point: /mnt/test
unknown_field: some_value
future_feature: true
another_unknown: 123
enable_stat_cache: true
)";
    
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    
    // Should not throw and should load known fields correctly
    EXPECT_NO_THROW(config.loadFromYAML(yaml_file));
    EXPECT_EQ(config.bucket_name, "test-bucket");
    EXPECT_EQ(config.mount_point, "/mnt/test");
    EXPECT_TRUE(config.enable_stat_cache);
}

// Test adding new config fields - mixed with valid and invalid keys
TEST_F(ConfigTest, LoadFromYAML_MixedKnownUnknownFields) {
    std::string yaml_content = R"(
# Known fields
bucket_name: my-bucket
mount_point: /mnt/gcs
enable_stat_cache: false
stat_cache_timeout: 45

# Unknown/future fields that should be ignored
max_connections: 100
retry_policy: exponential
timeout_ms: 5000
custom_metadata:
  key1: value1
  key2: value2
)";
    
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    // Verify known fields are loaded correctly
    EXPECT_EQ(config.bucket_name, "my-bucket");
    EXPECT_EQ(config.mount_point, "/mnt/gcs");
    EXPECT_FALSE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 45);
    
    // Verify defaults remain for unset fields
    EXPECT_TRUE(config.enable_file_content_cache);  // Default value
}

// Test adding new config fields - empty YAML file
TEST_F(ConfigTest, LoadFromYAML_EmptyFile) {
    std::string yaml_content = "";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    
    // Empty YAML should load successfully and keep defaults
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    EXPECT_TRUE(config.bucket_name.empty());
    EXPECT_TRUE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 60);
}

// Test adding new config fields - YAML with only comments
TEST_F(ConfigTest, LoadFromYAML_OnlyComments) {
    std::string yaml_content = R"(
# This is a comment
# Another comment
# No actual configuration
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    EXPECT_TRUE(config.bucket_name.empty());
    EXPECT_TRUE(config.enable_stat_cache);
}

// Test adding new config fields - partial configuration
TEST_F(ConfigTest, LoadFromYAML_PartialConfig) {
    std::string yaml_content = R"(
bucket_name: partial-bucket
stat_cache_timeout: 90
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    // Specified fields should be set
    EXPECT_EQ(config.bucket_name, "partial-bucket");
    EXPECT_EQ(config.stat_cache_timeout, 90);
    
    // Unspecified fields should retain defaults
    EXPECT_TRUE(config.mount_point.empty());
    EXPECT_TRUE(config.enable_stat_cache);  // Default
    EXPECT_TRUE(config.enable_file_content_cache);  // Default
    EXPECT_FALSE(config.debug_mode);  // Default
}

// Test adding new config fields - all fields specified
TEST_F(ConfigTest, LoadFromYAML_AllFieldsSpecified) {
    std::string yaml_content = R"(
bucket_name: complete-bucket
mount_point: /mnt/complete
enable_stat_cache: false
stat_cache_timeout: 120
enable_file_content_cache: false
debug: true
verbose: true
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    // All fields should be set from YAML
    EXPECT_EQ(config.bucket_name, "complete-bucket");
    EXPECT_EQ(config.mount_point, "/mnt/complete");
    EXPECT_FALSE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 120);
    EXPECT_FALSE(config.enable_file_content_cache);
    EXPECT_TRUE(config.debug_mode);
    EXPECT_TRUE(config.verbose_logging);
}

// Test adding new config fields - boundary values
TEST_F(ConfigTest, LoadFromYAML_BoundaryValues) {
    std::string yaml_content = R"(
bucket_name: ""
mount_point: /
stat_cache_timeout: 0
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    EXPECT_TRUE(config.bucket_name.empty());
    EXPECT_EQ(config.mount_point, "/");
    EXPECT_EQ(config.stat_cache_timeout, 0);
}

// Test adding new config fields - special characters in values
TEST_F(ConfigTest, LoadFromYAML_SpecialCharacters) {
    std::string yaml_content = R"(
bucket_name: "bucket-with-dashes_and_underscores.dots"
mount_point: "/mnt/path/with spaces/and-special@chars"
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    EXPECT_EQ(config.bucket_name, "bucket-with-dashes_and_underscores.dots");
    EXPECT_EQ(config.mount_point, "/mnt/path/with spaces/and-special@chars");
}

// Test adding new config fields - integer edge cases
TEST_F(ConfigTest, LoadFromYAML_IntegerEdgeCases) {
    std::string yaml_content = R"(
bucket_name: test-bucket
mount_point: /mnt/test
stat_cache_timeout: 2147483647
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    EXPECT_EQ(config.stat_cache_timeout, 2147483647);
}

// Test adding new config fields - malformed boolean should throw
TEST_F(ConfigTest, LoadFromYAML_MalformedBoolean) {
    std::string yaml_content = R"(
bucket_name: test-bucket
enable_stat_cache: not_a_boolean
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    
    // Should throw because YAML-cpp can't parse "not_a_boolean" as bool
    EXPECT_THROW(config.loadFromYAML(yaml_file), std::runtime_error);
}

// Test adding new config fields - malformed integer should throw
TEST_F(ConfigTest, LoadFromYAML_MalformedInteger) {
    std::string yaml_content = R"(
bucket_name: test-bucket
stat_cache_timeout: not_a_number
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    
    // Should throw because YAML-cpp can't parse "not_a_number" as int
    EXPECT_THROW(config.loadFromYAML(yaml_file), std::runtime_error);
}

// Test new config fields override defaults correctly
TEST_F(ConfigTest, LoadFromYAML_NewFieldsOverrideDefaults) {
    std::string yaml_content = R"(
enable_stat_cache: false
stat_cache_timeout: 0
enable_file_content_cache: false
)";
    std::string yaml_file = createTestYAML(yaml_content);
    
    GCSFSConfig config;
    config.loadDefaults();
    
    // Verify defaults first
    EXPECT_TRUE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 60);
    EXPECT_TRUE(config.enable_file_content_cache);
    
    // Load YAML
    EXPECT_TRUE(config.loadFromYAML(yaml_file));
    
    // Verify YAML overrides defaults
    EXPECT_FALSE(config.enable_stat_cache);
    EXPECT_EQ(config.stat_cache_timeout, 0);
    EXPECT_FALSE(config.enable_file_content_cache);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
