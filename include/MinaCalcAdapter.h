#pragma once

#include "ChartTypes.h"

#include <optional>
#include <array>

std::optional<float> calculateChartMsd(const ParsedSong& song, const ParsedChart& chart);
std::optional<std::array<float, 8>> calculateChartSkillsets(const ParsedSong& song, const ParsedChart& chart);
