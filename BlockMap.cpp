#include "BlockMap.h"
#include "Constants.h"
#include <unordered_map>
#include <string>

static void pick(uint8_t id, uint8_t data, uint8_t& outId, uint8_t& outData) {
    outId = id; outData = data;
}

struct MCBlock { uint8_t id; uint8_t meta; };

static std::unordered_map<std::string, MCBlock> mcNameToBlock = {
    {"Stone Block", {1, 0}},
    {"Smooth Sandstone", {24, 2}},
    {"Glass", {20, 0}},
    {"Oak Leaves", {18, 0}},
    {"Oak Log", {17, 0}},
    {"Oak Planks", {5, 0}},
    {"Grass Block", {2, 0}},
    {"TNT", {46, 0}},
    {"Cobblestone", {4, 0}},
    {"Bricks", {45, 0}},
    {"Stone Bricks", {98, 0}},
    {"Andesite", {1, 5}},
    {"Chiseled Quartz Block", {155, 1}},
    {"Coal Block", {173, 0}},
    {"Jungle Planks", {5, 3}},
    {"White Wool", {35, 0}},
    {"Water", {9, 0}},
    {"Oak Fence", {85, 0}},
    {"Mossy Cobblestone", {48, 0}},
    {"Glowstone", {89, 0}},
    {"Cobblestone Stairs", {67, 0}},
    {"Oak Stairs", {53, 0}},
    {"Nether Brick Stairs", {114, 0}},
    {"Quartz Stairs", {156, 0}},
    {"Cobblestone Wall", {139, 0}},
    {"Nether Brick", {112, 0}},
    {"Block of Quartz", {155, 0}},
    {"Oak Door", {64, 0}},
    {"Sea Lantern", {169, 0}},
    {"Poppy", {38, 0}},
    {"Block of Iron", {42, 0}}
};

// Maps TYPE_* enums to block names in ConvList1.json
static std::unordered_map<int, std::string> blockEnumToName = {
    {TYPE_STONE, "Stone Block"},
    {TYPE_SAND, "Smooth Sandstone"},
    {TYPE_GLASS, "Glass"},
    {TYPE_LEAVES, "Oak Leaves"},
    {TYPE_TREE, "Oak Log"},
    {TYPE_WOOD, "Oak Planks"},
    {TYPE_GRASS, "Grass Block"},
    {TYPE_TNT, "TNT"},
    {TYPE_DARK_STONE, "Cobblestone"},
    {TYPE_GRASS2, "Grass Block"},
    {TYPE_GRASS3, "Grass Block"},
    {TYPE_BRICK, "Bricks"},
    {TYPE_COBBLESTONE, "Stone Bricks"},
    {TYPE_ICE, "Andesite"},
    {TYPE_CRYSTAL, "Chiseled Quartz Block"},
    {TYPE_TRAMPOLINE, "Coal Block"},
    {TYPE_LADDER, "Jungle Planks"},
    {TYPE_CLOUD, "White Wool"},
    {TYPE_WATER, "Water"},
    {TYPE_WEAVE, "Oak Fence"},
    {TYPE_VINE, "Mossy Cobblestone"},
    {TYPE_LAVA, "Glowstone"},
    {TYPE_STONE_RAMP1, "Cobblestone Stairs"},
    {TYPE_STONE_RAMP2, "Cobblestone Stairs"},
    {TYPE_STONE_RAMP3, "Cobblestone Stairs"},
    {TYPE_STONE_RAMP4, "Cobblestone Stairs"},
    {TYPE_WOOD_RAMP1, "Oak Stairs"},
    {TYPE_WOOD_RAMP2, "Oak Stairs"},
    {TYPE_WOOD_RAMP3, "Oak Stairs"},
    {TYPE_WOOD_RAMP4, "Oak Stairs"},
    {TYPE_SHINGLE_RAMP1, "Nether Brick Stairs"},
    {TYPE_SHINGLE_RAMP2, "Nether Brick Stairs"},
    {TYPE_SHINGLE_RAMP3, "Nether Brick Stairs"},
    {TYPE_SHINGLE_RAMP4, "Nether Brick Stairs"},
    {TYPE_ICE_RAMP1, "Quartz Stairs"},
    {TYPE_ICE_RAMP2, "Quartz Stairs"},
    {TYPE_ICE_RAMP3, "Quartz Stairs"},
    {TYPE_ICE_RAMP4, "Quartz Stairs"},
    {TYPE_STONE_SIDE1, "Cobblestone Wall"},
    {TYPE_STONE_SIDE2, "Cobblestone Wall"},
    {TYPE_STONE_SIDE3, "Cobblestone Wall"},
    {TYPE_STONE_SIDE4, "Cobblestone Wall"},
    {TYPE_WOOD_SIDE1, "Oak Fence"},
    {TYPE_WOOD_SIDE2, "Oak Fence"},
    {TYPE_WOOD_SIDE3, "Oak Fence"},
    {TYPE_WOOD_SIDE4, "Oak Fence"},
    {TYPE_SHINGLE_SIDE1, "Cobblestone Wall"},
    {TYPE_SHINGLE_SIDE2, "Cobblestone Wall"},
    {TYPE_SHINGLE_SIDE3, "Cobblestone Wall"},
    {TYPE_SHINGLE_SIDE4, "Cobblestone Wall"},
    {TYPE_ICE_SIDE1, "Cobblestone Wall"},
    {TYPE_ICE_SIDE2, "Cobblestone Wall"},
    {TYPE_ICE_SIDE3, "Cobblestone Wall"},
    {TYPE_ICE_SIDE4, "Cobblestone Wall"},
    {TYPE_SHINGLE, "Nether Brick"},
    {TYPE_GRADIENT, "Block of Quartz"},
    {TYPE_GLASS, "Glass"},
    {TYPE_WATER3, "Water"},
    {TYPE_WATER2, "Water"},
    {TYPE_WATER1, "Water"},
    {TYPE_LAVA3, "Glowstone"},
    {TYPE_LAVA2, "Glowstone"},
    {TYPE_LAVA1, "Glowstone"},
    {TYPE_DOOR1, "Oak Door"},
    {TYPE_DOOR2, "Oak Door"},
    {TYPE_DOOR3, "Oak Door"},
    {TYPE_DOOR4, "Oak Door"},
    {TYPE_LIGHTBOX, "Sea Lantern"},
    {TYPE_FLOWER, "Poppy"},
    {TYPE_STEEL, "Block of Iron"},
};

bool mapEdenToMinecraft(int8_t edenId, uint8_t /*edenColor*/, uint8_t& mcId, uint8_t& mcData) {
    if (edenId <= 0) return false; // air

    // JSON/mapping driven logic
    auto it = blockEnumToName.find(edenId);
    if (it != blockEnumToName.end()) {
        std::string bName = it->second;
        auto mbIt = mcNameToBlock.find(bName);
        if (mbIt != mcNameToBlock.end()) {
            MCBlock mb = mbIt->second;
            int meta = mb.meta;
            // Variant meta for ramps/doors/etc.
            if (bName.find("Stairs") != std::string::npos) {
				// Minecraft 1.12 stair meta (bottom): 0=E, 1=W, 2=S, 3=N
				// Eden ramp1..4 appear rotated in the original mapping; remap to match facing.
				static const uint8_t rampMeta[4] = {2, 1, 3, 0};
				if (edenId >= TYPE_WOOD_RAMP1 && edenId <= TYPE_WOOD_RAMP4) {
					meta = rampMeta[edenId - TYPE_WOOD_RAMP1];
				}
				else if (edenId >= TYPE_STONE_RAMP1 && edenId <= TYPE_STONE_RAMP4) {
					meta = rampMeta[edenId - TYPE_STONE_RAMP1];
				}
				else if (edenId >= TYPE_SHINGLE_RAMP1 && edenId <= TYPE_SHINGLE_RAMP4) {
					meta = rampMeta[edenId - TYPE_SHINGLE_RAMP1];
				}
				else if (edenId >= TYPE_ICE_RAMP1 && edenId <= TYPE_ICE_RAMP4) {
					meta = rampMeta[edenId - TYPE_ICE_RAMP1];
				}
			} // <-- This closes the 'if (bName.find("Stairs") != ...)' block
			pick(mb.id, meta, mcId, mcData);
			return true;
        }
    }

    // Fallback to old mappings for blocks not in JSON
    switch (edenId) {
        case TYPE_BEDROCK: pick(7, 0, mcId, mcData); return true;
        case TYPE_DIRT: pick(3, 0, mcId, mcData); return true;
        case TYPE_WATER:
        case TYPE_WATER1:
        case TYPE_WATER2:
        case TYPE_WATER3: pick(9, 0, mcId, mcData); return true; // water
        case TYPE_VINE: pick(106, 0, mcId, mcData); return true;
        case TYPE_LAVA:
        case TYPE_LAVA1:
        case TYPE_LAVA2:
        case TYPE_LAVA3: pick(11, 0, mcId, mcData); return true;
        case TYPE_GRADIENT: pick(159, 0, mcId, mcData); return true; // stained hardened clay
        case TYPE_FLOWER: pick(38, 0, mcId, mcData); return true;
        case TYPE_STEEL: pick(42, 0, mcId, mcData); return true; // iron block
        case TYPE_LIGHTBOX: pick(89, 0, mcId, mcData); return true; // glowstone
        default:
            // Unknown or unlisted type: use stone as generic fallback
            pick(1, 0, mcId, mcData);
            return true;
    }
}
