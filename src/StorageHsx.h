#pragma once
#include "DataStoreHsx.h"
#include <filesystem>
namespace im {
class StorageHsx {
public:
    static void load(DataStoreHsx& data, const std::filesystem::path& path, bool backupOriginal = true);
    static void save(DataStoreHsx& data, const std::filesystem::path& path);
    static void validate(const DataStoreHsx& data);
private:
    static void readModern(DataStoreHsx& data, const std::filesystem::path& path, int version);
    static void readLegacy(DataStoreHsx& data, const std::filesystem::path& path);
};
}
