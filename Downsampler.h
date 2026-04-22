#pragma once

#include "MCReverseTypes.h"
#include "MCToEdenMapper.h"

class Downsampler {
public:
    EdenColumn downsampleChunkToEden(const ChunkColumn& in, MCToEdenMapper& mapper) const;
};
