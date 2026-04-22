#include "EdenWriter.h"

#include <cstdio>
#include <cstring>

bool EdenWriter::writeWorld(
    const std::string& outPath,
    const std::vector<EdenColumn>& columns,
    uint32_t levelSeed,
    const std::string& worldName,
    int spawnX,
    int spawnY,
    int spawnZ,
    int expectedColumns) {
    FILE* fp = std::fopen(outPath.c_str(), "wb");
    if (!fp) {
        std::printf("Failed to open output file: %s\n", outPath.c_str());
        return false;
    }

    WorldFileHeader header;
    std::memset(&header, 0, sizeof(header));
    header.level_seed = (int)levelSeed;
    header.pos.x = (float)spawnX;
    header.pos.y = (float)spawnY;
    header.pos.z = (float)spawnZ;
    header.home = header.pos;
    header.yaw = 0.0f;
    std::snprintf(header.name, sizeof(header.name), "%s", worldName.c_str());
    header.version = FILE_VERSION;

    if (std::fwrite(&header, sizeof(header), 1, fp) != 1) {
        std::fclose(fp);
        return false;
    }

    std::vector<ColumnIndex> indexes;
    indexes.reserve(columns.size());
    size_t writtenColumns = 0;

    for (const EdenColumn& col : columns) {
        ColumnIndex idx;
        idx.x = col.x;
        idx.z = col.z;
        idx.chunk_offset = (unsigned long long)std::ftell(fp);
        indexes.push_back(idx);

        // Eden column stores 4 vertical chunks of 16 blocks each.
        for (int cy = 0; cy < 4; ++cy) {
            block8 blockChunk[16 * 16 * 16];
            color8 colorChunk[16 * 16 * 16];
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    for (int y = 0; y < 16; ++y) {
                        int worldY = cy * 16 + y;
                        int i = x * 16 * 16 + z * 16 + y;
                        blockChunk[i] = (block8)col.blocks[x][worldY][z].type;
                        colorChunk[i] = (color8)col.blocks[x][worldY][z].color;
                    }
                }
            }
            if (std::fwrite(blockChunk, sizeof(blockChunk), 1, fp) != 1) {
                std::fclose(fp);
                return false;
            }
            if (std::fwrite(colorChunk, sizeof(colorChunk), 1, fp) != 1) {
                std::fclose(fp);
                return false;
            }
        }
        writtenColumns++;
    }

    header.directory_offset = (unsigned long long)std::ftell(fp);
    for (const ColumnIndex& idx : indexes) {
        if (std::fwrite(&idx, sizeof(ColumnIndex), 1, fp) != 1) {
            std::fclose(fp);
            return false;
        }
    }

    std::fseek(fp, 0, SEEK_SET);
    if (std::fwrite(&header, sizeof(header), 1, fp) != 1) {
        std::fclose(fp);
        return false;
    }

    std::fclose(fp);
    int expected = (expectedColumns >= 0) ? expectedColumns : (int)columns.size();
    std::printf("EdenWriter columns: written=%zu expected=%d\n", writtenColumns, expected);
    if ((int)writtenColumns != expected) {
        std::printf("Column count mismatch; aborting output as invalid.\n");
        return false;
    }
    std::printf("Wrote %zu columns to %s\n", columns.size(), outPath.c_str());
    return true;
}
