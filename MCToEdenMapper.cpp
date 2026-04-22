#include "MCToEdenMapper.h"
#include "Constants.h"

#include <cstdio>

BlockClass MCToEdenMapper::classify(const std::string& blockName) const {
    if (blockName == "minecraft:air" || blockName == "minecraft:cave_air" || blockName == "minecraft:void_air") {
        return AIR;
    }
    if (blockName == "minecraft:water" || blockName == "minecraft:flowing_water") {
        return LIQUID;
    }
    if (blockName == "minecraft:grass_block" || blockName == "minecraft:mycelium" || blockName == "minecraft:podzol") {
        return SURFACE;
    }
    if (blockName.find("leaves") != std::string::npos || blockName.find("sapling") != std::string::npos || blockName.find("grass") != std::string::npos) {
        return VEGETATION;
    }
    if (blockName == "minecraft:stone" ||
        blockName == "minecraft:dirt" ||
        blockName == "minecraft:sand" ||
        blockName == "minecraft:gravel" ||
        blockName == "minecraft:andesite" ||
        blockName == "minecraft:diorite" ||
        blockName == "minecraft:granite" ||
        blockName == "minecraft:bedrock") {
        return SOLID;
    }
    return IGNORE;
}

EdenBlock MCToEdenMapper::mapToEden(BlockClass klass, const std::string& blockName) {
    EdenBlock out;
    out.type = TYPE_NONE;
    out.color = 0;
    switch (klass) {
        case AIR:
            out.type = TYPE_NONE;
            break;
        case SOLID:
            out.type = TYPE_STONE;
            break;
        case SURFACE:
            out.type = TYPE_GRASS;
            break;
        case LIQUID:
            out.type = TYPE_WATER;
            break;
        case VEGETATION:
            out.type = TYPE_NONE; // TODO: optional conversion to a green Eden block.
            break;
        case IGNORE:
        default:
            out.type = TYPE_NONE;
            if (unknownLogged.insert(blockName).second) {
                std::printf("Unknown/ignored block type: %s\n", blockName.c_str());
            }
            break;
    }
    return out;
}
