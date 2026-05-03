#pragma once

#include "ChartTypes.h"

#include <optional>

class ChartParser {
public:
    std::optional<ParsedSong> parseFile(const std::filesystem::path& filePath, std::string& error) const;
};
