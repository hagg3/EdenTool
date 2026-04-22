#include "AnvilWriter.h"
#include "NBT.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace nbt;

struct AnvilWriter::RegionFile {
	std::string path;
	FILE* fp;
	std::vector<uint8_t> header; // 8KB header in memory
	std::vector<uint8_t> sectors; // file content beyond header
	// track used sectors (0 reserved for header)
	std::vector<bool> used;
	RegionFile(): fp(nullptr) {}
};

static inline long long packKey(int rx, int rz) {
	return ((long long)rx << 32) ^ (long long)(rz & 0xffffffff);
}

static void ensureDir(const std::string& path) {
	mkdir(path.c_str(), 0755);
}

AnvilWriter::AnvilWriter(const std::string& worldDir): worldDir(worldDir) {
	ensureDir(worldDir);
	ensureDir(worldDir + "/region");
}

AnvilWriter::~AnvilWriter() {
	close();
}

AnvilWriter::RegionFile* AnvilWriter::getRegion(int regionX, int regionZ) {
	long long key = packKey(regionX, regionZ);
	auto it = regions.find(key);
	if (it != regions.end()) return it->second;
	RegionFile* rf = new RegionFile();
	rf->path = worldDir + "/region/r." + std::to_string(regionX) + "." + std::to_string(regionZ) + ".mca";
	rf->fp = fopen(rf->path.c_str(), "wb+");
	if (!rf->fp) return nullptr;
	// init header 8KB
	rf->header.assign(8192, 0);
	// write initial header to file
	fwrite(rf->header.data(), 1, rf->header.size(), rf->fp);
	fflush(rf->fp);
	// sector 0 and 1 reserved for header
	rf->used.assign(2, true);
	regions[key] = rf;
	return rf;
}

void AnvilWriter::writeChunk(int chunkX, int chunkZ,
	const std::vector<std::vector<uint8_t>>& sectionBlocks,
	const std::vector<std::vector<uint8_t>>& sectionData) {
    auto floorDiv32 = [](int v) -> int { return (v >= 0) ? (v / 32) : -((31 - v) / 32); };
    auto floorMod32 = [&](int v) -> int { int d = floorDiv32(v); return v - d * 32; };
    int regionX = floorDiv32(chunkX);
    int regionZ = floorDiv32(chunkZ);
    int localX = floorMod32(chunkX);
    int localZ = floorMod32(chunkZ);
    // Debug: report placement
    if (chunkX >= -2 && chunkX <= 2 && chunkZ >= -2 && chunkZ <= 2) {
        printf("Writing chunk (%d,%d) -> region r.%d.%d.mca local(%d,%d)\n", chunkX, chunkZ, regionX, regionZ, localX, localZ);
    }
	RegionFile* rf = getRegion(regionX, regionZ);
	if (!rf) return;

    // Build simple HeightMap (topmost non-air Y for each (x,z))
    std::vector<int32_t> heightMap(16*16, 0);
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            int h = 0;
            for (int s = (int)sectionBlocks.size()-1; s >= 0; --s) {
                if (sectionBlocks[s].empty()) continue;
                for (int y = 15; y >= 0; --y) {
                    int voxelIndex = (y * 16 + z) * 16 + x;
                    if (sectionBlocks[s][voxelIndex] != 0) { h = s*16 + y + 1; goto hm_done; }
                }
            }
            hm_done:
            heightMap[z*16 + x] = h;
        }
    }

    // Build NBT for chunk
	Buffer buf;
	beginCompound(buf, ""); // unnamed root compound (for chunk NBT it's usually named "")
	beginCompound(buf, "Level");
	writeInt(buf, "xPos", chunkX);
	writeInt(buf, "zPos", chunkZ);
	writeLong(buf, "LastUpdate", 0);
	writeLong(buf, "InhabitedTime", 0);
	writeByte(buf, "TerrainPopulated", 1);
	writeByte(buf, "LightPopulated", 1);
	writeByteArray(buf, "Biomes", std::vector<uint8_t>(256, 1)); // plains
    writeIntArray(buf, "HeightMap", heightMap);
    // Empty lists for entities and tile entities
    beginList(buf, "Entities", TAG_Compound, 0);
    beginList(buf, "TileEntities", TAG_Compound, 0);

    // Sections list (list of unnamed compounds)
    int sectionCount = 0;
    for (size_t i = 0; i < sectionBlocks.size(); ++i) if (!sectionBlocks[i].empty()) sectionCount++;
    beginList(buf, "Sections", TAG_Compound, sectionCount);
    for (size_t si = 0; si < sectionBlocks.size(); ++si) {
        if (sectionBlocks[si].empty()) continue;
        beginCompoundPayload(buf);
        writeByte(buf, "Y", (int8_t)si);
        // Blocks 4096 bytes
        writeByteArray(buf, "Blocks", sectionBlocks[si]);
        // Data 2048 nibbles (packed)
        writeByteArray(buf, "Data", sectionData[si]);
        // Light arrays
        writeByteArray(buf, "SkyLight", std::vector<uint8_t>(2048, 0xFF));
        writeByteArray(buf, "BlockLight", std::vector<uint8_t>(2048, 0x00));
        endCompoundPayload(buf);
    }

	endCompound(buf); // end Level
	endCompound(buf); // end root

	// Compress with zlib (type 2 in Anvil)
	std::vector<uint8_t> compressed = compressZlib(buf.data);
	if (compressed.empty()) return;

	// Prepare chunk payload: length (4), compression type (1), compressed data
	std::vector<uint8_t> payload;
	uint32_t length = (uint32_t)(1 + compressed.size());
	// big endian length
	payload.push_back((length >> 24) & 0xFF);
	payload.push_back((length >> 16) & 0xFF);
	payload.push_back((length >> 8) & 0xFF);
	payload.push_back(length & 0xFF);
	payload.push_back(2); // zlib
	payload.insert(payload.end(), compressed.begin(), compressed.end());

	// Determine number of 4096-byte sectors
	size_t total = payload.size();
	int sectorsNeeded = (int)((total + 4095) / 4096);
	// find first fit
	int offsetSector = -1;
	int run = 0;
	for (size_t i = 2; ; ++i) {
		if (i >= rf->used.size()) rf->used.push_back(false);
		if (!rf->used[i]) {
			run++;
			if (run == sectorsNeeded) {
				offsetSector = (int)(i - sectorsNeeded + 1);
				break;
			}
		} else {
			run = 0;
		}
	}
	for (int i = 0; i < sectorsNeeded; ++i) rf->used[offsetSector + i] = true;

	// Seek and write payload at 4KiB * offsetSector
	long long fileOffset = (long long)offsetSector * 4096LL;
	fseek(rf->fp, fileOffset, SEEK_SET);
	// pad payload to full sectors
	std::vector<uint8_t> padded = payload;
	padded.resize((size_t)sectorsNeeded * 4096, 0);
    size_t wrote = fwrite(padded.data(), 1, padded.size(), rf->fp);
	fflush(rf->fp);

	// Update header: location entry is 3 bytes offset, 1 byte sectors
	int locIndex = localX + localZ * 32;
	uint32_t loc = ((uint32_t)offsetSector << 8) | (uint32_t)sectorsNeeded;
	// write to header buffer
	rf->header[locIndex*4 + 0] = (loc >> 24) & 0xFF;
	rf->header[locIndex*4 + 1] = (loc >> 16) & 0xFF;
	rf->header[locIndex*4 + 2] = (loc >> 8) & 0xFF;
	rf->header[locIndex*4 + 3] = (loc) & 0xFF;
    // timestamps (set to non-zero current-ish time)
    uint32_t ts = (uint32_t)time(NULL);
    int tsIndex = 4096 + locIndex*4; // second 4KB table (timestamps)
    rf->header[tsIndex + 0] = (ts >> 24) & 0xFF;
    rf->header[tsIndex + 1] = (ts >> 16) & 0xFF;
    rf->header[tsIndex + 2] = (ts >> 8) & 0xFF;
    rf->header[tsIndex + 3] = (ts) & 0xFF;
	// write header back
    fseek(rf->fp, 0, SEEK_SET);
    fwrite(rf->header.data(), 1, rf->header.size(), rf->fp);
	fflush(rf->fp);
}

void AnvilWriter::close() {
	for (auto& kv : regions) {
		RegionFile* rf = kv.second;
		if (rf->fp) fclose(rf->fp);
		delete rf;
	}
	regions.clear();
}


