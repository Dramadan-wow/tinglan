#pragma once
#include <filesystem>
namespace im {
class DataStoreHsx;
class DemoDataHsx {
public:
    static void create(DataStoreHsx& data);
    // Explicit fixture for isolated tests; never called by normal startup.
    static std::filesystem::path fixture(const std::filesystem::path& file);
};
}
