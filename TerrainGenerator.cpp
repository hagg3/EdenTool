#include "TerrainGenerator.h"

#include "Constants.h"
#include "Noise.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {
static inline int clampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline size_t idx2D(int x, int z, int width) {
    return (size_t)z * (size_t)width + (size_t)x;
}

static inline uint32_t hash3(uint32_t seed, int x, int y, int z) {
    uint32_t h = seed ^ 0x9e3779b9u;
    h ^= (uint32_t)x * 374761393u;
    h ^= (uint32_t)y * 668265263u;
    h ^= (uint32_t)z * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= (h >> 16);
    return h;
}

static std::vector<std::string> loadColorNames() {
    std::ifstream in("color.txt");
    std::vector<std::string> out;
    if (!in) return out;
    std::string line;
    while (std::getline(in, line)) {
        std::string token;
        for (char c : line) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') token.push_back(c);
        }
        if (!token.empty()) out.push_back(token);
    }
    return out;
}

static std::vector<uint8_t> findColorsByKeywords(const std::vector<std::string>& names, const std::vector<std::string>& words) {
    std::vector<uint8_t> ids;
    for (size_t i = 0; i < names.size(); ++i) {
        for (const std::string& w : words) {
            if (names[i].find(w) != std::string::npos) {
                ids.push_back((uint8_t)i);
                break;
            }
        }
    }
    return ids;
}

static uint8_t pickColorFromPool(const std::vector<uint8_t>& pool, uint32_t seed, int x, int y, int z) {
    if (pool.empty()) return 0;
    uint32_t h = hash3(seed, x, y, z);
    return pool[h % (uint32_t)pool.size()];
}

static bool shouldPlaceLeafByFalloff(uint32_t seed, int x, int y, int z, double normalizedDist) {
    if (normalizedDist <= 0.0) return true;
    if (normalizedDist >= 1.0) return false;
    // Dense center, sparse edge, deterministic jitter.
    double baseKeep = 1.0 - normalizedDist * normalizedDist;
    uint32_t h = hash3(seed, x, y, z);
    double jitter = (double)(h & 1023u) / 1023.0;
    return jitter < baseKeep;
}

static int getTopSolidY(const std::vector<EdenColumn>& cols, int width, int depth, int wx, int wz) {
    if (wx < 0 || wz < 0 || wx >= width || wz >= depth) return 0;
    int colsX = width / 16;
    int cx = wx / 16, cz = wz / 16;
    int lx = wx % 16, lz = wz % 16;
    const EdenColumn& col = cols[(size_t)cz * (size_t)colsX + (size_t)cx];
    for (int y = 63; y >= 0; --y) {
        uint8_t t = col.blocks[lx][y][lz].type;
        if (t != TYPE_NONE && t != TYPE_WATER && t != TYPE_FLOWER && t != TYPE_LEAVES) return y;
    }
    return 0;
}
}

bool TerrainGenerator::generate(const TerrainParams& params, std::vector<EdenColumn>& outColumns, TerrainMetadata* outMeta) const {
    if (params.width <= 0 || params.depth <= 0 || (params.width % 16) != 0 || (params.depth % 16) != 0) {
        std::printf("Invalid terrain size. width/depth must be positive multiples of 16.\n");
        return false;
    }

    int waterAmnt = clampInt(params.waterAmnt, 1, 5);
    int waterLevel = 32;
    if (waterAmnt == 1) waterLevel = 40;
    else if (waterAmnt == 2) waterLevel = 35;
    else if (waterAmnt == 3) waterLevel = 32;
    else if (waterAmnt == 4) waterLevel = 27;
    else waterLevel = -1; // water disabled for fully-land mode
    const int snowHeight = 48;
    const int colsX = params.width / 16;
    const int colsZ = params.depth / 16;
    const int totalColumns = colsX * colsZ;
    Noise2D noise(params.seed);

    std::vector<std::string> colorNames = loadColorNames();
    std::vector<uint8_t> logColors = findColorsByKeywords(colorNames, {"Orange", "DarkOrange", "MediumDarkOrange", "VeryDarkOrange", "DarkYellow"});
    std::vector<uint8_t> leafColors = findColorsByKeywords(colorNames, {"Green", "Yellow", "Orange", "Red"});
    std::vector<uint8_t> flowerColors = findColorsByKeywords(colorNames, {"LightPink", "Pink", "Red", "Orange", "Yellow", "Purple"});
    std::vector<uint8_t> snowColors = findColorsByKeywords(colorNames, {"LightGray_White"});
    uint8_t snowColor = snowColors.empty() ? 0 : snowColors[0];

    outColumns.clear();
    outColumns.reserve((size_t)totalColumns);

    std::vector<int> heightmap((size_t)params.width * (size_t)params.depth, 0);
    std::vector<uint8_t> waterMask((size_t)params.width * (size_t)params.depth, 0);

    int minH = std::numeric_limits<int>::max();
    int maxH = std::numeric_limits<int>::min();
    int generated = 0;

    // Stage 1+2: heightmap generation and base terrain fill (preserve existing shape math).
    for (int cz = 0; cz < colsZ; ++cz) {
        for (int cx = 0; cx < colsX; ++cx) {
            EdenColumn col;
            col.x = cx;
            col.z = cz;

            for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) for (int y = 0; y < 64; ++y) {
                col.blocks[x][y][z].type = TYPE_NONE;
                col.blocks[x][y][z].color = 0;
            }

            for (int lx = 0; lx < 16; ++lx) {
                for (int lz = 0; lz < 16; ++lz) {
                    int wx = cx * 16 + lx;
                    int wz = cz * 16 + lz;

                    double n = noise.fractal((double)wx, (double)wz, 4, 0.02, 0.5);
                    int H = (int)((double)params.baseHeight + n * 18.0);
                    H = clampInt(H, 1, 63);
                    minH = std::min(minH, H);
                    maxH = std::max(maxH, H);
                    heightmap[idx2D(wx, wz, params.width)] = H;

                    int stoneTop = std::max(0, H - 3);
                    int dirtTop = std::max(0, H - 1);
                    for (int y = 0; y <= stoneTop && y < 64; ++y) col.blocks[lx][y][lz].type = TYPE_STONE;
                    for (int y = stoneTop + 1; y <= dirtTop && y < 64; ++y) col.blocks[lx][y][lz].type = TYPE_DIRT;
                    col.blocks[lx][H][lz].type = TYPE_GRASS;
                }
            }

            outColumns.push_back(col);
            generated++;
            if ((generated % 64) == 0 || generated == totalColumns) {
                std::printf("Generation progress: %d / %d columns\n", generated, totalColumns);
            }
        }
    }
    std::printf("Generator columns: generated=%d expected=%d\n", generated, totalColumns);

    // Stage 3: caves pass (underground only, protect surface shell).
    int cavesCarved = 0;
    for (int wz = 0; wz < params.depth; ++wz) {
        for (int wx = 0; wx < params.width; ++wx) {
            int H = heightmap[idx2D(wx, wz, params.width)];
            int cx = wx / 16, cz = wz / 16, lx = wx % 16, lz = wz % 16;
            EdenColumn& col = outColumns[(size_t)cz * (size_t)colsX + (size_t)cx];
            for (int y = 4; y <= H - 3; ++y) {
                double c = noise.fractal3D((double)wx, (double)y, (double)wz, 3, 0.08, 0.5);
                if (c > 0.52) {
                    col.blocks[lx][y][lz].type = TYPE_NONE;
                    col.blocks[lx][y][lz].color = 0;
                    cavesCarved++;
                }
            }
        }
    }

    // Stage 4: water pass (sea level + simple ponds/rivers).
    int waterBlocksPlaced = 0;
    for (int wz = 0; wz < params.depth; ++wz) {
        for (int wx = 0; wx < params.width; ++wx) {
            int H = heightmap[idx2D(wx, wz, params.width)];
            int cx = wx / 16, cz = wz / 16, lx = wx % 16, lz = wz % 16;
            EdenColumn& col = outColumns[(size_t)cz * (size_t)colsX + (size_t)cx];

            double pondN = noise.fractal((double)wx + 1000.0, (double)wz + 1000.0, 3, 0.03, 0.5);
            double riverN = noise.fractal((double)wx + 4000.0, (double)wz - 4000.0, 2, 0.01, 0.6);
            bool pond = (waterLevel >= 0) && (pondN > 0.62 && H < waterLevel + 2);
            bool river = (waterLevel >= 0) && (riverN > -0.03 && riverN < 0.03 && H < 45);
            int localWaterLevel = waterLevel + (pond ? 1 : 0);

            if (waterLevel >= 0 && (H < waterLevel || pond || river)) {
                col.blocks[lx][H][lz].type = TYPE_SAND;
                for (int y = H + 1; y <= localWaterLevel && y < 64; ++y) {
                    if (col.blocks[lx][y][lz].type != TYPE_WATER) {
                        col.blocks[lx][y][lz].type = TYPE_WATER;
                        waterBlocksPlaced++;
                    }
                    waterMask[idx2D(wx, wz, params.width)] = 1;
                }
            }
        }
    }

    // Stage 5: beach/surface adjustments near water.
    for (int wz = 1; wz < params.depth - 1; ++wz) {
        for (int wx = 1; wx < params.width - 1; ++wx) {
            int topY = getTopSolidY(outColumns, params.width, params.depth, wx, wz);
            int cx = wx / 16, cz = wz / 16, lx = wx % 16, lz = wz % 16;
            EdenColumn& col = outColumns[(size_t)cz * (size_t)colsX + (size_t)cx];
            if (col.blocks[lx][topY][lz].type != TYPE_GRASS) continue;

            int waterNeighbors = 0;
            for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dz == 0) continue;
                waterNeighbors += (int)waterMask[idx2D(wx + dx, wz + dz, params.width)];
            }
            if (waterNeighbors >= 3 && topY <= waterLevel + 2) col.blocks[lx][topY][lz].type = TYPE_SAND;
        }
    }

    // Stage 6: vegetation pass.
    int treesPlaced = 0;
    int flowersPlaced = 0;
    for (int wz = 2; wz < params.depth - 2; ++wz) {
        for (int wx = 2; wx < params.width - 2; ++wx) {
            int topY = getTopSolidY(outColumns, params.width, params.depth, wx, wz);
            int cx = wx / 16, cz = wz / 16, lx = wx % 16, lz = wz % 16;
            EdenColumn& col = outColumns[(size_t)cz * (size_t)colsX + (size_t)cx];
            if (topY <= waterLevel || topY + 1 >= 64) continue;
            if (col.blocks[lx][topY][lz].type != TYPE_GRASS || col.blocks[lx][topY + 1][lz].type != TYPE_NONE) continue;

            uint32_t h = hash3(params.seed, wx, topY, wz);
            if ((h % 1000u) < 6u) {
                int trunk = 3 + (int)(h % 5u); // 3..7
                uint8_t treeLogColor = pickColorFromPool(logColors, params.seed + 17u, wx, topY, wz);
                uint8_t treeLeafColor = pickColorFromPool(leafColors, params.seed + 31u, wx, topY, wz);
                for (int i = 1; i <= trunk && topY + i < 64; ++i) {
                    col.blocks[lx][topY + i][lz].type = TYPE_TREE;
                    col.blocks[lx][topY + i][lz].color = treeLogColor;
                }
                int leafTopY = std::min(63, topY + trunk);
                int leafPattern = (int)(h % 3u);
                // Leaves must be above trunk top: ay > leafTopY.
                for (int dz = -4; dz <= 4; ++dz) for (int dx = -4; dx <= 4; ++dx) for (int dy = 1; dy <= 5; ++dy) {
                    int ax = wx + dx, az = wz + dz, ay = leafTopY + dy;
                    if (ax < 0 || az < 0 || ax >= params.width || az >= params.depth || ay < 1 || ay >= 64) continue;

                    bool patternInside = false;
                    double norm = 1.0;
                    if (leafPattern == 0) {
                        // Round / ellipsoid canopy
                        double ex = (double)dx / 3.2;
                        double ey = (double)(dy - 2) / 2.2;
                        double ez = (double)dz / 3.2;
                        double d = ex * ex + ey * ey + ez * ez;
                        patternInside = (d <= 1.0);
                        norm = std::sqrt(std::min(1.0, d));
                    } else if (leafPattern == 1) {
                        // Wide / flat canopy with slight dome.
                        int radial = dx * dx + dz * dz;
                        patternInside = (radial <= 12 && dy >= 1 && dy <= 3);
                        double plane = std::sqrt((double)radial) / 3.6;
                        double vert = std::abs((double)(dy - 2)) / 1.8;
                        norm = std::min(1.0, (plane + vert) * 0.65);
                    } else {
                        // Tall / conifer-like layered cone.
                        int radial = dx * dx + dz * dz;
                        int maxR = 0;
                        if (dy >= 5) maxR = 0;
                        else if (dy == 4) maxR = 1;
                        else if (dy == 3) maxR = 2;
                        else if (dy == 2) maxR = 3;
                        else maxR = 4;
                        patternInside = (radial <= maxR * maxR);
                        norm = (maxR <= 0) ? 0.0 : std::min(1.0, std::sqrt((double)radial) / (double)maxR);
                    }
                    if (!patternInside) continue;
                    if (!shouldPlaceLeafByFalloff(params.seed + 97u, ax, ay, az, norm)) continue;

                    int acx = ax / 16, acz = az / 16, alx = ax % 16, alz = az % 16;
                    EdenColumn& acol = outColumns[(size_t)acz * (size_t)colsX + (size_t)acx];
                    if (acol.blocks[alx][ay][alz].type == TYPE_NONE) {
                        acol.blocks[alx][ay][alz].type = TYPE_LEAVES;
                        acol.blocks[alx][ay][alz].color = treeLeafColor;
                    }
                }
                treesPlaced++;
            } else if ((h % 1000u) < 12u) {
                col.blocks[lx][topY + 1][lz].type = TYPE_FLOWER;
                col.blocks[lx][topY + 1][lz].color = pickColorFromPool(flowerColors, params.seed + 47u, wx, topY + 1, wz);
                flowersPlaced++;
            }
        }
    }

    // Stage 7: snow pass for high altitudes.
    for (int wz = 0; wz < params.depth; ++wz) {
        for (int wx = 0; wx < params.width; ++wx) {
            int topY = getTopSolidY(outColumns, params.width, params.depth, wx, wz);
            if (topY < snowHeight) continue;
            int cx = wx / 16, cz = wz / 16, lx = wx % 16, lz = wz % 16;
            EdenColumn& col = outColumns[(size_t)cz * (size_t)colsX + (size_t)cx];
            uint8_t t = col.blocks[lx][topY][lz].type;
            if (t == TYPE_GRASS || t == TYPE_DIRT || t == TYPE_STONE || t == TYPE_SAND) {
                col.blocks[lx][topY][lz].type = TYPE_SAND;
                col.blocks[lx][topY][lz].color = snowColor;
            }
        }
    }

    int spawnX = params.width / 2;
    int spawnZ = params.depth / 2;
    int spawnY = clampInt(getTopSolidY(outColumns, params.width, params.depth, spawnX, spawnZ), 1, 63);

    if (outMeta) {
        outMeta->spawnX = spawnX;
        outMeta->spawnY = spawnY;
        outMeta->spawnZ = spawnZ;
        outMeta->treesPlaced = treesPlaced;
        outMeta->flowersPlaced = flowersPlaced;
        outMeta->caveBlocksCarved = cavesCarved;
        outMeta->minHeight = minH;
        outMeta->maxHeight = maxH;
        outMeta->expectedColumns = totalColumns;
        outMeta->generatedColumns = generated;
    }

    std::printf("Terrain generation complete. Height range: %d..%d\n", minH, maxH);
    std::printf("Caves carved blocks: %d, trees: %d, flowers: %d\n", cavesCarved, treesPlaced, flowersPlaced);
    std::printf("Water config: waterAmnt=%d seaLevel=%d waterBlocks=%d\n", waterAmnt, waterLevel, waterBlocksPlaced);
    std::printf("Spawn point: (%d, %d, %d)\n", spawnX, spawnY, spawnZ);
    return true;
}
