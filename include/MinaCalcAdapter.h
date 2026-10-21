#pragma once

#include "ChartTypes.h"

#include <optional>
#include <array>

std::optional<float> calculateChartMsd(const ParsedSong& song, const ParsedChart& chart, float musicRate = 1.0f);
std::optional<std::array<float, 8>> calculateChartSkillsets(const ParsedSong& song, const ParsedChart& chart, float musicRate = 1.0f);
std::optional<std::array<float, 8>> calculateIntervalSkillsets(const ParsedSong& song, const ParsedChart& chart, double startSeconds, double endSeconds, float musicRate = 1.0f);
