#include "MC2EdenConverter.h"

#include "AnvilReader.h"
#include "Downsampler.h"
#include "EdenWriter.h"
#include "MCToEdenMapper.h"

#include <cstdio>
#include <vector>

bool MC2EdenConverter::convertRegionFolderToEden(const std::string& inputRegionFolder, const std::string& outputEdenPath) {
    AnvilReader reader;
    MCToEdenMapper mapper;
    Downsampler downsampler;
    EdenWriter writer;

    std::vector<ChunkColumn> chunks = reader.readRegionFolder(inputRegionFolder);
    if (chunks.empty()) {
        std::printf("No chunks were read from region folder: %s\n", inputRegionFolder.c_str());
        return false;
    }
    std::printf("Loaded %zu Minecraft chunks.\n", chunks.size());

    std::vector<EdenColumn> edenColumns;
    edenColumns.reserve(chunks.size());
    for (const ChunkColumn& cc : chunks) {
        edenColumns.push_back(downsampler.downsampleChunkToEden(cc, mapper));
    }
    std::printf("Downsampled to %zu Eden columns (height 64).\n", edenColumns.size());

    if (!writer.writeWorld(outputEdenPath, edenColumns)) {
        std::printf("Failed writing Eden output: %s\n", outputEdenPath.c_str());
        return false;
    }

    return true;
}
