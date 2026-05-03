#pragma once

#include "ChartTypes.h"

#include <optional>

std::optional<float> calculateChartMsd(const ParsedSong& song, const ParsedChart& chart);
