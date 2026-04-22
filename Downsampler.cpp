#include "Downsampler.h"

#include <map>
#include <string>

namespace {
static std::string modeBlockName4(const std::string& a, const std::string& b, const std::string& c, const std::string& d) {
    std::map<std::string, int> counts;
    counts[a]++;
    counts[b]++;
    counts[c]++;
    counts[d]++;

    int nonAirBest = 0;
    std::string nonAirName = "minecraft:air";
    int airCount = counts["minecraft:air"];
    for (const auto& kv : counts) {
        if (kv.first == "minecraft:air") continue;
        if (kv.second > nonAirBest) {
            nonAirBest = kv.second;
            nonAirName = kv.first;
        }
    }
    if (nonAirBest > 0) return nonAirName;
    if (airCount == 4) return "minecraft:air";
    return "minecraft:air";
}
}

EdenColumn Downsampler::downsampleChunkToEden(const ChunkColumn& in, MCToEdenMapper& mapper) const {
    EdenColumn out;
    out.x = in.chunkX;
    out.z = in.chunkZ;

    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            for (int ey = 0; ey < 64; ++ey) {
                int y0 = ey * 4;
                const std::string& b0 = in.chunk.blocks[x][y0 + 0][z].blockName;
                const std::string& b1 = in.chunk.blocks[x][y0 + 1][z].blockName;
                const std::string& b2 = in.chunk.blocks[x][y0 + 2][z].blockName;
                const std::string& b3 = in.chunk.blocks[x][y0 + 3][z].blockName;
                std::string mode = modeBlockName4(b0, b1, b2, b3);

                BlockClass klass = mapper.classify(mode);
                out.blocks[x][ey][z] = mapper.mapToEden(klass, mode);
            }
        }
    }
    return out;
}
