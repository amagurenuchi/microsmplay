#include "SongBrowser.h"

#include "MinaCalcAdapter.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {
bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool icontains(const std::string& str, const std::string& substr) {
    if (substr.empty()) return true;
    if (str.size() < substr.size()) return false;
    auto it = std::search(str.begin(), str.end(), substr.begin(), substr.end(),
        [](char ch1, char ch2) { return std::tolower(static_cast<unsigned char>(ch1)) == std::tolower(static_cast<unsigned char>(ch2)); });
    return it != str.end();
}
}

void SongBrowser::scanFolder(const std::filesystem::path& folder) {
    clear();
    scanFolderRecursive(folder);
    std::sort(songs_.begin(), songs_.end(), [](const SongEntry& a, const SongEntry& b) {
        return a.title < b.title;
    });
}

void SongBrowser::scanFolderRecursive(const std::filesystem::path& folder) {
    if (!std::filesystem::exists(folder) || !std::filesystem::is_directory(folder)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.is_directory()) {
            scanFolderRecursive(entry.path());
        } else if (entry.is_regular_file()) {
            const auto& path = entry.path();
            const std::string ext = path.extension().string();
            if (iequals(ext, ".sm") || iequals(ext, ".ssc")) {
                SongEntry song;
                song.folderPath = path.parent_path();
                song.chartPath = path;
                if (parseSongInfo(song)) {
                    songs_.push_back(std::move(song));
                }
            }
        }
    }
}

bool SongBrowser::parseSongInfo(SongEntry& entry) {
    ChartParser parser;
    std::string error;
    auto parsed = parser.parseFile(entry.chartPath, error);
    if (!parsed) {
        return false;
    }

    entry.title = parsed->title.empty() ? entry.chartPath.stem().string() : parsed->title;
    entry.artist = parsed->artist;

    for (size_t i = 0; i < parsed->charts.size(); ++i) {
        const auto& chart = parsed->charts[i];
        std::string desc = chart.difficulty;
        if (chart.meter > 0) {
            desc += " " + std::to_string(chart.meter);
        }

        if (const auto msd = calculateChartMsd(*parsed, chart, 1.0f)) {
            char buffer[32] = {};
            std::snprintf(buffer, sizeof(buffer), " [MSD %.2f]", *msd);
            desc += buffer;
        }

        entry.chartIndices.push_back(i);
        entry.chartLabels.push_back(std::move(desc));
    }

    return true;
}

void SongBrowser::clear() {
    songs_.clear();
    selectedIndex_ = -1;
    selectedChartIndex_ = 0;
}

std::vector<SongEntry> SongBrowser::filteredSongs() const {
    if (filterText_.empty()) {
        return songs_;
    }

    std::vector<SongEntry> filtered;
    for (const auto& song : songs_) {
        if (icontains(song.title, filterText_) || icontains(song.artist, filterText_)) {
            filtered.push_back(song);
        }
    }
    return filtered;
}
