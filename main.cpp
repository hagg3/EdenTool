

#include "EdenFileLoader.h"
#include "MC2EdenConverter.h"
#include "TerrainGenerator.h"
#include "EdenWriter.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstdint>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string(argv[1]) == "mc2eden") {
        if (argc < 4) {
            printf("Usage: %s mc2eden <input_region_folder> <output_file.eden>\n", argv[0]);
            return 1;
        }
        MC2EdenConverter converter;
        bool ok = converter.convertRegionFolderToEden(argv[2], argv[3]);
        return ok ? 0 : 2;
    }
    if (argc >= 2 && std::string(argv[1]) == "generate") {
        if (argc < 6) {
            printf("Usage: %s generate <width> <depth> <seed> <output_file.eden> [baseHeight] [waterAmnt(1-5)]\n", argv[0]);
            return 1;
        }
        TerrainParams params;
        params.width = std::atoi(argv[2]);
        params.depth = std::atoi(argv[3]);
        params.seed = (uint32_t)std::strtoul(argv[4], nullptr, 10);
        params.baseHeight = (argc >= 7) ? std::atoi(argv[6]) : 30;
        params.waterAmnt = (argc >= 8) ? std::atoi(argv[7]) : 3;

        TerrainGenerator generator;
        std::vector<EdenColumn> columns;
        TerrainMetadata meta;
        if (!generator.generate(params, columns, &meta)) {
            return 2;
        }
        printf("Main columns: generated=%zu expected=%d\n", columns.size(), meta.expectedColumns);
        if ((int)columns.size() != meta.expectedColumns) {
            printf("Generator column mismatch; refusing to write partial world.\n");
            return 2;
        }
        EdenWriter writer;
        if (!writer.writeWorld(argv[5], columns, params.seed, "TerrainGen", meta.spawnX, meta.spawnY, meta.spawnZ, meta.expectedColumns)) {
            return 3;
        }
        printf("Generated Eden terrain world: %s\n", argv[5]);
        return 0;
    }

    EdenFileLoader* efl = new EdenFileLoader();

    // Remember to unzip the eden file before using this on a download from the shared world server.
    char worldFile[] = "FILE.eden";
    const char* outputWorld = "ConvertedWorld";

    printf("Hello world.\n");
    efl->convertToMinecraft(worldFile, outputWorld);
    printf("Minecraft world written to folder: %s\n", outputWorld);
    return 0;
}

