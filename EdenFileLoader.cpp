
#include "EdenFileLoader.h"
#include "AnvilWriter.h"
#include "BlockMap.h"
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <vector>


FILE* fp = NULL;
WorldFileHeader* sfh = NULL;

using namespace std;
int num_columns = 0;
vector<ColumnIndex*> colindexes;
void  EdenFileLoader::readDirectory() {

	num_columns = 0;
	int rn = fseek(fp, (long int)sfh->directory_offset, SEEK_SET);

	if (rn != 0)printf("seek to directory offset failed");

	while (true) {
		ColumnIndex* colIdx = (ColumnIndex*)malloc(sizeof(ColumnIndex));
		if (!colIdx)return;

    int nr = fread(colIdx, sizeof(ColumnIndex), 1, fp);
    if (nr != 1) {
			break;
		}
		colindexes.push_back(colIdx);
		num_columns++;
	}
	printf("read in column_directory_indexes, numcolumns: %d \n ", num_columns);

}
block8* blockarray = NULL;
color8* colorarray = NULL;
int g_offcx = 0;
int g_offcz = 0;
int chunkOffsetX;
int chunkOffsetZ;

block8 chunk_block_array[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
color8 chunk_color_array[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];

#define CHUNKS_PER_COLUMN_IN_FILE 4
bool EdenFileLoader::readColumn(int cx, int cz) {


	int idx = -1;
	for (int i = 0; i < num_columns; i++) {
		if (colindexes[i]->x == cx && colindexes[i]->z == cz) {
			idx = i;
			break;
		}
	}
	if (idx == -1)return false;

	int rn = fseek(fp, (long int)colindexes[idx]->chunk_offset, SEEK_SET);

	if (rn != 0) {
		printf("seek to directory offset failed\n");
		return false;
	}
	int adj_cx = cx - chunkOffsetX;
	int  adj_cz = cz - chunkOffsetZ;
	printf("loading column %d, %d \n", adj_cx, adj_cz);
	for (int cy = 0; cy < CHUNKS_PER_COLUMN_IN_FILE; cy++) {

    int nr = fread(chunk_block_array, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(block8), 1, fp);
    if (nr != 1) {
			printf("read blocks failed %d, %d\n", cx, cz);
			return false;
		}

    nr = fread(chunk_color_array, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(color8), 1, fp);
    if (nr != 1) {
			printf("read colors failed %d, %d\n", cx, cz);
			return false;
		}


		for (int x = 0; x < CHUNK_SIZE; x++) {
			for (int z = 0; z < CHUNK_SIZE; z++) {
				for (int y = 0; y < CHUNK_SIZE; y++) {

				//copy data to global block and color array's, becareful to avoid memory corruption

					GBLOCK(adj_cx * CHUNK_SIZE + x, adj_cz * CHUNK_SIZE + z, cy * CHUNK_SIZE + y) =
						chunk_block_array[x * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + y];


					
					GCOLOR(adj_cx * CHUNK_SIZE + x, adj_cz * CHUNK_SIZE + z, cy * CHUNK_SIZE + y) =
					chunk_color_array[x * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + y];
				}

			}
		}

	}


	return true;
}




void EdenFileLoader::loadWorld(char* name) {


	char buff[255];
	char cwd[FILENAME_MAX];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("Current working dir: %s\n", cwd);
	}
	else {
		perror("getcwd() error");
		return;
	}

	fp = fopen(name, "rb");
	if (fp == NULL) {
		printf("failed to open file: %s\n", name);
		return;
	}
	sfh = (WorldFileHeader*)malloc(sizeof(WorldFileHeader));
	if (!sfh)return;
	fread(sfh, sizeof(WorldFileHeader), 1, fp);
	printf(" loading file: %s\n file_format_version: %d\n", sfh->name, sfh->version);
	printf(" player_xyz_position: %.2f, %.2f, %.2f\n level_seed: %d\n home_xyz: %.2f, %.2f, %.2f\n", sfh->pos.x, sfh->pos.y, sfh->pos.z, sfh->level_seed,
		sfh->home.x, sfh->home.y, sfh->home.z);
	printf(" chunk_directory_offset: %ld  \n", (long)sfh->directory_offset);


	this->readDirectory();



	chunkOffsetX = sfh->pos.x / CHUNK_SIZE - T_READ_RADIUS;
	chunkOffsetZ = sfh->pos.z / CHUNK_SIZE - T_READ_RADIUS;
	int r = T_READ_RADIUS;
	//	int asdf=0;

	int ncol_loaded = 0;
	for (int x = chunkOffsetX; x < chunkOffsetX + 2 * r; x++) {
		for (int z = chunkOffsetZ; z < chunkOffsetZ + 2 * r; z++) {

			if (readColumn(x, z))
				ncol_loaded++;

		}
	}
	printf("loaded n_columns: %d  out of %d \n", ncol_loaded, r * 2 * r * 2);


	fclose(fp);


}


// Convert full world: iterate all ColumnIndex entries and export as Anvil chunks
void EdenFileLoader::convertToMinecraft(const char* edenPath, const char* outputWorldDir) {
	char cwd[FILENAME_MAX];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("Current working dir: %s\n", cwd);
	}

	fp = fopen(edenPath, "rb");
	if (fp == NULL) {
		printf("failed to open file: %s\n", edenPath);
		return;
	}
	sfh = (WorldFileHeader*)malloc(sizeof(WorldFileHeader));
	if (!sfh) { fclose(fp); return; }
	fread(sfh, sizeof(WorldFileHeader), 1, fp);
	printf("Converting file: %s (version %d)\n", sfh->name, sfh->version);
	printf("Chunk directory at: %ld\n", (long)sfh->directory_offset);

	colindexes.clear();
	this->readDirectory();

    AnvilWriter writer{std::string(outputWorldDir)};

    // Place the Eden player's column at Minecraft chunk (0,0)
    int playerChunkX = (int)(sfh->pos.x / CHUNK_SIZE);
    int playerChunkZ = (int)(sfh->pos.z / CHUNK_SIZE);
    printf("Recenter: subtracting player chunk (%d,%d) from all chunks.\n", playerChunkX, playerChunkZ);

    int exported = 0;
    int minCX =  1000000000, minCZ =  1000000000;
    int maxCX = -1000000000, maxCZ = -1000000000;
	for (int i = 0; i < num_columns; ++i) {
		int cx = colindexes[i]->x;
		int cz = colindexes[i]->z;
		int rn = fseek(fp, (long int)colindexes[i]->chunk_offset, SEEK_SET);
		if (rn != 0) {
			printf("seek to column failed for %d,%d\n", cx, cz);
			continue;
		}

		// Read 4 vertical chunks in this column and assemble 4 sections (Y=0..3)
		std::vector<std::vector<uint8_t>> sectionsBlocks(4);
		std::vector<std::vector<uint8_t>> sectionsData(4);
		for (int s = 0; s < 4; ++s) {
			sectionsBlocks[s].assign(CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE, 0);
			sectionsData[s].assign((CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE)/2, 0);
		}

		for (int cy = 0; cy < 4; cy++) {
            int nr = fread(chunk_block_array, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(block8), 1, fp);
            if (nr != 1) { printf("read blocks failed %d,%d\n", cx, cz); break; }
            nr = fread(chunk_color_array, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE * sizeof(color8), 1, fp);
            if (nr != 1) { printf("read colors failed %d,%d\n", cx, cz); break; }

			// Map each voxel to MC id+data and pack into section arrays
			for (int x = 0; x < CHUNK_SIZE; x++) {
				for (int z = 0; z < CHUNK_SIZE; z++) {
					for (int y = 0; y < CHUNK_SIZE; y++) {
						int idx = x * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + y;
						block8 bid = chunk_block_array[idx];
						color8 col = chunk_color_array[idx];
						uint8_t mcId=0, mcData=0;
						bool place = mapEdenToMinecraft(bid, col, mcId, mcData);
						int secIndex = cy;
						int voxelIndex = (y * CHUNK_SIZE + z) * CHUNK_SIZE + x; // Y,Z,X order expected by Anvil arrays
						if (place) {
							sectionsBlocks[secIndex][voxelIndex] = mcId;
						} else {
							sectionsBlocks[secIndex][voxelIndex] = 0; // air
						}
						// pack 4-bit data
						int nibbleIdx = voxelIndex >> 1;
						bool low = (voxelIndex & 1) == 0;
						uint8_t& nib = sectionsData[secIndex][nibbleIdx];
						if (low) nib = (nib & 0xF0) | (mcData & 0x0F); else nib = (nib & 0x0F) | ((mcData & 0x0F) << 4);
					}
				}
			}
		}

        // Write chunk recentered around origin
        int outCX = cx - playerChunkX;
        int outCZ = cz - playerChunkZ;
        writer.writeChunk(outCX, outCZ, sectionsBlocks, sectionsData);
        if (outCX < minCX) minCX = outCX; if (outCX > maxCX) maxCX = outCX;
        if (outCZ < minCZ) minCZ = outCZ; if (outCZ > maxCZ) maxCZ = outCZ;
		exported++;
		if (exported % 128 == 0) printf("Exported %d chunks...\n", exported);
	}

    writer.close();
    fclose(fp);
    printf("Done. Exported %d chunk columns. Chunk range X:[%d..%d] Z:[%d..%d].\n", exported, minCX, maxCX, minCZ, maxCZ);
    printf("Check for region file(s) in ConvertedWorld/region like r.%d.%d.mca covering origin r.0.0.mca.\n", 0, 0);
}
