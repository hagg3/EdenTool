#include "NBT.h"
#include <zlib.h>

namespace nbt {

static void writeBE32(std::vector<uint8_t>& out, uint32_t v) {
	out.push_back((v >> 24) & 0xFF);
	out.push_back((v >> 16) & 0xFF);
	out.push_back((v >> 8) & 0xFF);
	out.push_back(v & 0xFF);
}

static void writeBE16(std::vector<uint8_t>& out, uint16_t v) {
	out.push_back((v >> 8) & 0xFF);
	out.push_back(v & 0xFF);
}

static void writeBE64(std::vector<uint8_t>& out, uint64_t v) {
	for (int i = 7; i >= 0; --i) out.push_back((v >> (i*8)) & 0xFF);
}

void Buffer::writeU8(uint8_t v) { data.push_back(v); }
void Buffer::writeI16(int16_t v) { writeBE16(data, (uint16_t)v); }
void Buffer::writeI32(int32_t v) { writeBE32(data, (uint32_t)v); }
void Buffer::writeI64(int64_t v) { writeBE64(data, (uint64_t)v); }
void Buffer::writeBytes(const uint8_t* p, size_t n) { data.insert(data.end(), p, p+n); }

void writeTagHeader(Buffer& buf, TagType type, const std::string& name) {
	buf.writeU8((uint8_t)type);
	writeBE16(buf.data, (uint16_t)name.size());
	buf.writeBytes(reinterpret_cast<const uint8_t*>(name.data()), name.size());
}

void writeEnd(Buffer& buf) {
	buf.writeU8((uint8_t)TAG_End);
}

void writeByte(Buffer& buf, const std::string& name, int8_t value) {
	writeTagHeader(buf, TAG_Byte, name);
	buf.writeU8((uint8_t)value);
}

void writeShort(Buffer& buf, const std::string& name, int16_t value) {
	writeTagHeader(buf, TAG_Short, name);
	buf.writeI16(value);
}

void writeInt(Buffer& buf, const std::string& name, int32_t value) {
	writeTagHeader(buf, TAG_Int, name);
	buf.writeI32(value);
}

void writeLong(Buffer& buf, const std::string& name, int64_t value) {
	writeTagHeader(buf, TAG_Long, name);
	buf.writeI64(value);
}

void writeString(Buffer& buf, const std::string& name, const std::string& value) {
	writeTagHeader(buf, TAG_String, name);
	writeBE16(buf.data, (uint16_t)value.size());
	buf.writeBytes(reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

void writeByteArray(Buffer& buf, const std::string& name, const std::vector<uint8_t>& value) {
	writeTagHeader(buf, TAG_Byte_Array, name);
	buf.writeI32((int32_t)value.size());
	if (!value.empty()) buf.writeBytes(value.data(), value.size());
}

void writeIntArray(Buffer& buf, const std::string& name, const std::vector<int32_t>& value) {
	writeTagHeader(buf, TAG_Int_Array, name);
	buf.writeI32((int32_t)value.size());
	for (auto v : value) buf.writeI32(v);
}

void beginCompound(Buffer& buf, const std::string& name) {
	writeTagHeader(buf, TAG_Compound, name);
}

void endCompound(Buffer& buf) {
	writeEnd(buf);
}

void beginList(Buffer& buf, const std::string& name, TagType elemType, int32_t length) {
	writeTagHeader(buf, TAG_List, name);
	buf.writeU8((uint8_t)elemType);
	buf.writeI32(length);
}

void beginCompoundPayload(Buffer& buf) {
    // no header for list elements; element payload starts directly
}

void endCompoundPayload(Buffer& buf) {
    writeEnd(buf);
}

std::vector<uint8_t> compressZlib(const std::vector<uint8_t>& input) {
	// zlib compress (deflate)
	uLong srcLen = (uLong)input.size();
	uLong destLen = compressBound(srcLen);
	std::vector<uint8_t> out(destLen);
	int rv = compress2(out.data(), &destLen, input.data(), srcLen, Z_BEST_SPEED);
	if (rv != Z_OK) return {};
	out.resize(destLen);
	return out;
}

}


