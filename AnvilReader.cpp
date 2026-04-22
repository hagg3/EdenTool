#include "AnvilReader.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>

namespace {

struct ByteReader {
    const std::vector<uint8_t>& b;
    size_t p;

    explicit ByteReader(const std::vector<uint8_t>& bytes) : b(bytes), p(0) {}

    bool has(size_t n) const { return p + n <= b.size(); }
    uint8_t u8() { return has(1) ? b[p++] : 0; }
    uint16_t be16() { uint16_t v = (uint16_t)(u8() << 8); v |= u8(); return v; }
    uint32_t be32() { uint32_t v = (uint32_t)(u8() << 24); v |= (uint32_t)(u8() << 16); v |= (uint32_t)(u8() << 8); v |= u8(); return v; }
    uint64_t be64() {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | (uint64_t)u8();
        return v;
    }
    std::string str() {
        uint16_t n = be16();
        if (!has(n)) return "";
        std::string s(reinterpret_cast<const char*>(&b[p]), reinterpret_cast<const char*>(&b[p + n]));
        p += n;
        return s;
    }
    void skip(size_t n) { p = std::min(p + n, b.size()); }
};

enum NbtTag : uint8_t {
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

struct SectionData {
    int y = 0;
    std::vector<uint8_t> blocks12;
    std::vector<uint8_t> data12;
    std::vector<std::string> palette;
    std::vector<uint64_t> stateData;
};

struct ParsedChunk {
    int xPos = 0;
    int zPos = 0;
    std::vector<SectionData> sections;
};

static std::string blockNameFromLegacyId(uint8_t id) {
    switch (id) {
        case 0: return "minecraft:air";
        case 1: return "minecraft:stone";
        case 2: return "minecraft:grass_block";
        case 3: return "minecraft:dirt";
        case 7: return "minecraft:bedrock";
        case 8: return "minecraft:water";
        case 9: return "minecraft:water";
        case 12: return "minecraft:sand";
        case 13: return "minecraft:gravel";
        case 17: return "minecraft:oak_log";
        case 18: return "minecraft:oak_leaves";
        default: return "minecraft:unknown";
    }
}

static bool inflateAuto(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) {
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, 15 + 32) != Z_OK) return false;
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in.data()));
    zs.avail_in = (uInt)in.size();
    const size_t CHUNK = 1 << 15;
    int rc = Z_OK;
    do {
        size_t start = out.size();
        out.resize(start + CHUNK);
        zs.next_out = reinterpret_cast<Bytef*>(&out[start]);
        zs.avail_out = (uInt)CHUNK;
        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            inflateEnd(&zs);
            return false;
        }
        out.resize(start + (CHUNK - zs.avail_out));
    } while (rc != Z_STREAM_END);
    inflateEnd(&zs);
    return true;
}

static void skipNamedTagPayload(ByteReader& br, uint8_t tag);
static void parseSectionCompound(ByteReader& br, SectionData& section);
static void parseChunkCompound(ByteReader& br, ParsedChunk& outChunk);

static void skipListPayload(ByteReader& br) {
    uint8_t elemType = br.u8();
    uint32_t len = br.be32();
    for (uint32_t i = 0; i < len; ++i) {
        if (elemType == TAG_Compound) {
            while (br.has(1)) {
                uint8_t t = br.u8();
                if (t == TAG_End) break;
                (void)br.str();
                skipNamedTagPayload(br, t);
            }
        } else {
            skipNamedTagPayload(br, elemType);
        }
    }
}

static void skipNamedTagPayload(ByteReader& br, uint8_t tag) {
    switch (tag) {
        case TAG_Byte: br.skip(1); break;
        case TAG_Short: br.skip(2); break;
        case TAG_Int: br.skip(4); break;
        case TAG_Long: br.skip(8); break;
        case TAG_Float: br.skip(4); break;
        case TAG_Double: br.skip(8); break;
        case TAG_Byte_Array: { uint32_t n = br.be32(); br.skip(n); break; }
        case TAG_String: { (void)br.str(); break; }
        case TAG_List: skipListPayload(br); break;
        case TAG_Compound: {
            while (br.has(1)) {
                uint8_t t = br.u8();
                if (t == TAG_End) break;
                (void)br.str();
                skipNamedTagPayload(br, t);
            }
            break;
        }
        case TAG_Int_Array: { uint32_t n = br.be32(); br.skip((size_t)n * 4); break; }
        case TAG_Long_Array: { uint32_t n = br.be32(); br.skip((size_t)n * 8); break; }
        default: break;
    }
}

static std::vector<uint8_t> readByteArray(ByteReader& br) {
    uint32_t n = br.be32();
    std::vector<uint8_t> v;
    if (!br.has(n)) return v;
    v.insert(v.end(), br.b.begin() + (long)br.p, br.b.begin() + (long)(br.p + n));
    br.p += n;
    return v;
}

static std::vector<uint64_t> readLongArray(ByteReader& br) {
    uint32_t n = br.be32();
    std::vector<uint64_t> v;
    v.reserve(n);
    for (uint32_t i = 0; i < n; ++i) v.push_back(br.be64());
    return v;
}

static std::vector<std::string> readPaletteList(ByteReader& br) {
    std::vector<std::string> palette;
    uint8_t elemType = br.u8();
    uint32_t len = br.be32();
    if (elemType != TAG_Compound) {
        for (uint32_t i = 0; i < len; ++i) skipNamedTagPayload(br, elemType);
        return palette;
    }
    for (uint32_t i = 0; i < len; ++i) {
        std::string name = "minecraft:air";
        while (br.has(1)) {
            uint8_t t = br.u8();
            if (t == TAG_End) break;
            std::string tagName = br.str();
            if (t == TAG_String && tagName == "Name") {
                name = br.str();
            } else {
                skipNamedTagPayload(br, t);
            }
        }
        palette.push_back(name);
    }
    if (palette.empty()) palette = makeDefaultPalette();
    return palette;
}

static void parseBlockStatesCompound(ByteReader& br, SectionData& section) {
    while (br.has(1)) {
        uint8_t t = br.u8();
        if (t == TAG_End) break;
        std::string n = br.str();
        if (t == TAG_List && n == "palette") {
            section.palette = readPaletteList(br);
        } else if (t == TAG_Long_Array && n == "data") {
            section.stateData = readLongArray(br);
        } else {
            skipNamedTagPayload(br, t);
        }
    }
}

static void parseSectionCompound(ByteReader& br, SectionData& section) {
    while (br.has(1)) {
        uint8_t t = br.u8();
        if (t == TAG_End) break;
        std::string n = br.str();
        if (t == TAG_Byte && (n == "Y" || n == "y")) {
            section.y = (int8_t)br.u8();
        } else if (t == TAG_Byte_Array && n == "Blocks") {
            section.blocks12 = readByteArray(br);
        } else if (t == TAG_Byte_Array && n == "Data") {
            section.data12 = readByteArray(br);
        } else if (t == TAG_Compound && n == "block_states") {
            parseBlockStatesCompound(br, section);
        } else if (t == TAG_List && (n == "Palette" || n == "palette")) {
            section.palette = readPaletteList(br);
        } else if (t == TAG_Long_Array && (n == "BlockStates" || n == "block_states")) {
            section.stateData = readLongArray(br);
        } else {
            skipNamedTagPayload(br, t);
        }
    }
}

static void parseSectionsList(ByteReader& br, ParsedChunk& outChunk) {
    uint8_t elemType = br.u8();
    uint32_t len = br.be32();
    if (elemType != TAG_Compound) {
        for (uint32_t i = 0; i < len; ++i) skipNamedTagPayload(br, elemType);
        return;
    }
    for (uint32_t i = 0; i < len; ++i) {
        SectionData s;
        parseSectionCompound(br, s);
        outChunk.sections.push_back(s);
    }
}

static void parseChunkCompound(ByteReader& br, ParsedChunk& outChunk) {
    while (br.has(1)) {
        uint8_t t = br.u8();
        if (t == TAG_End) break;
        std::string n = br.str();
        if (t == TAG_Int && (n == "xPos" || n == "xpos")) {
            outChunk.xPos = (int32_t)br.be32();
        } else if (t == TAG_Int && (n == "zPos" || n == "zpos")) {
            outChunk.zPos = (int32_t)br.be32();
        } else if (t == TAG_List && (n == "Sections" || n == "sections")) {
            parseSectionsList(br, outChunk);
        } else if (t == TAG_Compound && n == "Level") {
            parseChunkCompound(br, outChunk);
        } else {
            skipNamedTagPayload(br, t);
        }
    }
}

static bool parseChunkNbt(const std::vector<uint8_t>& nbtBytes, ParsedChunk& outChunk) {
    ByteReader br(nbtBytes);
    if (!br.has(3)) return false;
    uint8_t rootType = br.u8();
    if (rootType != TAG_Compound) return false;
    (void)br.str();
    parseChunkCompound(br, outChunk);
    return true;
}

static uint32_t floorDiv32(int v) {
    return (uint32_t)((v >= 0) ? (v / 32) : -((31 - v) / 32));
}

static std::string unpackPaletteName(const SectionData& s, int blockIndex) {
    if (s.palette.empty()) return "minecraft:air";
    if (s.stateData.empty()) return s.palette[0];
    int bits = 4;
    while ((1U << bits) < s.palette.size()) bits++;
    uint64_t bitIndex = (uint64_t)blockIndex * (uint64_t)bits;
    uint64_t longIndex = bitIndex / 64;
    uint64_t startBit = bitIndex % 64;
    if (longIndex >= s.stateData.size()) return "minecraft:air";
    uint64_t v = s.stateData[(size_t)longIndex] >> startBit;
    if (startBit + (uint64_t)bits > 64 && longIndex + 1 < s.stateData.size()) {
        v |= s.stateData[(size_t)longIndex + 1] << (64 - startBit);
    }
    uint64_t mask = (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1ULL);
    size_t paletteIndex = (size_t)(v & mask);
    if (paletteIndex >= s.palette.size()) return "minecraft:air";
    return s.palette[paletteIndex];
}

static Chunk toChunk(const ParsedChunk& pc) {
    Chunk out;
    for (int x = 0; x < 16; ++x) for (int y = 0; y < 256; ++y) for (int z = 0; z < 16; ++z) out.blocks[x][y][z].blockName = "minecraft:air";

    for (const SectionData& s : pc.sections) {
        if (s.y < 0 || s.y >= 16) continue;
        int yBase = s.y * 16;
        if (!s.blocks12.empty()) {
            for (int yi = 0; yi < 16; ++yi) {
                for (int zi = 0; zi < 16; ++zi) {
                    for (int xi = 0; xi < 16; ++xi) {
                        int index = (yi * 16 + zi) * 16 + xi;
                        if (index >= (int)s.blocks12.size()) continue;
                        out.blocks[xi][yBase + yi][zi].blockName = blockNameFromLegacyId(s.blocks12[(size_t)index]);
                    }
                }
            }
        } else if (!s.palette.empty()) {
            for (int yi = 0; yi < 16; ++yi) {
                for (int zi = 0; zi < 16; ++zi) {
                    for (int xi = 0; xi < 16; ++xi) {
                        int index = (yi * 16 + zi) * 16 + xi;
                        out.blocks[xi][yBase + yi][zi].blockName = unpackPaletteName(s, index);
                    }
                }
            }
        }
    }
    return out;
}

static bool readAllBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    std::streamoff size = in.tellg();
    if (size <= 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize((size_t)size);
    in.read(reinterpret_cast<char*>(out.data()), size);
    return in.good();
}

static bool parseRegionPathCoords(const std::string& path, int& rx, int& rz) {
    std::filesystem::path p(path);
    std::string name = p.filename().string();
    if (name.size() < 8 || name.rfind("r.", 0) != 0 || name.find(".mca") == std::string::npos) return false;
    size_t p1 = name.find('.', 2);
    size_t p2 = name.find('.', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos) return false;
    rx = std::atoi(name.substr(2, p1 - 2).c_str());
    rz = std::atoi(name.substr(p1 + 1, p2 - p1 - 1).c_str());
    return true;
}

static std::vector<ChunkColumn> readSingleRegion(const std::string& mcaPath) {
    std::vector<ChunkColumn> result;
    std::vector<uint8_t> bytes;
    if (!readAllBytes(mcaPath, bytes) || bytes.size() < 8192) {
        std::printf("Failed reading region file: %s\n", mcaPath.c_str());
        return result;
    }

    int rx = 0;
    int rz = 0;
    (void)parseRegionPathCoords(mcaPath, rx, rz);

    for (int i = 0; i < 1024; ++i) {
        uint32_t loc = ((uint32_t)bytes[i * 4 + 0] << 24) | ((uint32_t)bytes[i * 4 + 1] << 16) | ((uint32_t)bytes[i * 4 + 2] << 8) | bytes[i * 4 + 3];
        uint32_t offSector = (loc >> 8) & 0xFFFFFF;
        uint32_t secCount = loc & 0xFF;
        if (offSector == 0 || secCount == 0) continue;
        size_t offset = (size_t)offSector * 4096;
        size_t span = (size_t)secCount * 4096;
        if (offset + span > bytes.size() || span < 5) {
            std::printf("Invalid chunk span in %s at index=%d\n", mcaPath.c_str(), i);
            continue;
        }
        uint32_t length = ((uint32_t)bytes[offset + 0] << 24) | ((uint32_t)bytes[offset + 1] << 16) | ((uint32_t)bytes[offset + 2] << 8) | bytes[offset + 3];
        uint8_t compression = bytes[offset + 4];
        if (length <= 1 || (size_t)length + 4 > span) continue;
        std::vector<uint8_t> compressed(bytes.begin() + (long)(offset + 5), bytes.begin() + (long)(offset + 4 + length));
        std::vector<uint8_t> nbt;
        if (compression == 2 || compression == 1) {
            if (!inflateAuto(compressed, nbt)) {
                std::printf("Decompression failed in %s at chunk index %d\n", mcaPath.c_str(), i);
                continue;
            }
        } else {
            std::printf("Unsupported compression type %u in %s\n", compression, mcaPath.c_str());
            continue;
        }

        ParsedChunk parsed;
        if (!parseChunkNbt(nbt, parsed)) {
            std::printf("Failed to parse NBT in %s at chunk index %d\n", mcaPath.c_str(), i);
            continue;
        }

        int localX = i % 32;
        int localZ = i / 32;
        int chunkX = parsed.xPos;
        int chunkZ = parsed.zPos;
        if (chunkX == 0 && chunkZ == 0) {
            // TODO: Infer better when xPos/zPos are absent; using region-local fallback.
            chunkX = rx * 32 + localX;
            chunkZ = rz * 32 + localZ;
        }

        ChunkColumn cc;
        cc.chunkX = chunkX;
        cc.chunkZ = chunkZ;
        cc.chunk = toChunk(parsed);
        result.push_back(cc);
    }
    return result;
}

} // namespace

std::vector<ChunkColumn> AnvilReader::readRegionFolder(const std::string& inputRegionFolder) {
    std::vector<ChunkColumn> all;
    namespace fs = std::filesystem;
    if (!fs::exists(inputRegionFolder) || !fs::is_directory(inputRegionFolder)) {
        std::printf("Input region folder does not exist: %s\n", inputRegionFolder.c_str());
        return all;
    }
    for (const auto& e : fs::directory_iterator(inputRegionFolder)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension().string() != ".mca") continue;
        std::vector<ChunkColumn> regionChunks = readSingleRegion(e.path().string());
        all.insert(all.end(), regionChunks.begin(), regionChunks.end());
        std::printf("Read %zu chunks from %s\n", regionChunks.size(), e.path().filename().string().c_str());
    }
    return all;
}

bool AnvilReader::debugPrintFirstChunk(const std::string& mcaPath) {
    std::vector<ChunkColumn> chunks = readSingleRegion(mcaPath);
    if (chunks.empty()) {
        std::printf("No readable chunks found in %s\n", mcaPath.c_str());
        return false;
    }
    const ChunkColumn& cc = chunks.front();
    std::printf("Debug first chunk (%d,%d)\n", cc.chunkX, cc.chunkZ);
    for (int y = 63; y >= 56; --y) {
        std::printf("y=%d ", y);
        for (int x = 0; x < 8; ++x) {
            std::string n = cc.chunk.blocks[x][y][0].blockName;
            if (n.rfind("minecraft:", 0) == 0) n = n.substr(10);
            std::printf("%s ", n.c_str());
        }
        std::printf("\n");
    }
    return true;
}
