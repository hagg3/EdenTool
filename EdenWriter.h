#pragma once

#include "EdenFileLoader.h"
#include "MCReverseTypes.h"
#include <string>
#include <vector>

class EdenWriter {
public:
    bool writeWorld(
        const std::string& outPath,
        const std::vector<EdenColumn>& columns,
        uint32_t levelSeed = 0,
        const std::string& worldName = "MCImport",
        int spawnX = 0,
        int spawnY = 32,
        int spawnZ = 0,
        int expectedColumns = -1);
};
