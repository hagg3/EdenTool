#pragma once

#include "MCReverseTypes.h"
#include <set>
#include <string>

class MCToEdenMapper {
public:
    BlockClass classify(const std::string& blockName) const;
    EdenBlock mapToEden(BlockClass klass, const std::string& blockName);

private:
    std::set<std::string> unknownLogged;
};
