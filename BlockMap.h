#pragma once
#include <cstdint>

// Map Eden block ID (+optional color) to Minecraft 1.12 block ID and data (meta)
// Returns false for air/empty; true if a block should be placed
bool mapEdenToMinecraft(int8_t edenId, uint8_t edenColor, uint8_t& mcId, uint8_t& mcData);


