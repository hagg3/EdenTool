#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include <string>

// Minimal Anvil (.mca) region/chunk writer for Minecraft 1.12

class AnvilWriter {
public:
	explicit AnvilWriter(const std::string& worldDir);
	~AnvilWriter();

	// Write a single chunk at (chunkX, chunkZ) with provided 16x16x16 section blocks
	// sectionsBlocks: vector of 4 sections (Y=0..3), each 4096 block IDs and 4096 data nibbles
	// height up to 64 is supported; higher sections are omitted
	void writeChunk(int chunkX, int chunkZ,
		const std::vector<std::vector<uint8_t>>& sectionBlocks,
		const std::vector<std::vector<uint8_t>>& sectionData);

	// Flush and close all region files
	void close();

private:
	struct RegionFile;
	RegionFile* getRegion(int regionX, int regionZ);
	std::string worldDir;
	std::map<long long, RegionFile*> regions;
};


