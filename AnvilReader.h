#pragma once

#include "MCReverseTypes.h"
#include <string>
#include <vector>

class AnvilReader {
public:
    // Reads all chunks from all region files under inputRegionFolder.
    // On parse errors, unsupported chunks are skipped.
    std::vector<ChunkColumn> readRegionFolder(const std::string& inputRegionFolder);

    // Stage 1 helper: read one .mca and print first chunk sample block names.
    bool debugPrintFirstChunk(const std::string& mcaPath);
};
