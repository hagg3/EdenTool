#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Voxel {
    std::string blockName;
};

struct Chunk {
    Voxel blocks[16][256][16];
};

struct ChunkColumn {
    int chunkX;
    int chunkZ;
    Chunk chunk;
};

struct EdenBlock {
    uint8_t type;
    uint8_t color;
};

struct EdenColumn {
    int x;
    int z;
    EdenBlock blocks[16][64][16];
};

typedef enum BlockClass {
    AIR,
    SOLID,
    SURFACE,
    LIQUID,
    VEGETATION,
    IGNORE
} BlockClass;

static inline std::vector<std::string> makeDefaultPalette() {
    return {"minecraft:air"};
}
