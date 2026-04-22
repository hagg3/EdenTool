#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>

// Minimal NBT writer for Minecraft Java 1.12 (big-endian)

namespace nbt {

enum TagType : uint8_t {
	TAG_End = 0,
	TAG_Byte = 1,
	TAG_Short = 2,
	TAG_Int = 3,
	TAG_Long = 4,
	TAG_Float = 5,
	TAG_Double = 6,
	TAG_Byte_Array = 7,
	TAG_String = 8,
	TAG_List = 9,
	TAG_Compound = 10,
	TAG_Int_Array = 11,
	TAG_Long_Array = 12
};

struct Buffer {
	std::vector<uint8_t> data;
	void writeU8(uint8_t v);
	void writeI16(int16_t v);
	void writeI32(int32_t v);
	void writeI64(int64_t v);
	void writeBytes(const uint8_t* p, size_t n);
};

// Building blocks for NBT
void writeTagHeader(Buffer& buf, TagType type, const std::string& name);
void writeEnd(Buffer& buf);
void writeByte(Buffer& buf, const std::string& name, int8_t value);
void writeShort(Buffer& buf, const std::string& name, int16_t value);
void writeInt(Buffer& buf, const std::string& name, int32_t value);
void writeLong(Buffer& buf, const std::string& name, int64_t value);
void writeString(Buffer& buf, const std::string& name, const std::string& value);
void writeByteArray(Buffer& buf, const std::string& name, const std::vector<uint8_t>& value);
void writeIntArray(Buffer& buf, const std::string& name, const std::vector<int32_t>& value);

// Start/finish a compound manually
void beginCompound(Buffer& buf, const std::string& name);
void endCompound(Buffer& buf);

// Start/finish a list (homogeneous type)
void beginList(Buffer& buf, const std::string& name, TagType elemType, int32_t length);

// Write an unnamed compound payload suitable as a List element (no tag header)
void beginCompoundPayload(Buffer& buf);
void endCompoundPayload(Buffer& buf);

// zlib (deflate) compression helper, returns compressed buffer
std::vector<uint8_t> compressZlib(const std::vector<uint8_t>& input);

}


