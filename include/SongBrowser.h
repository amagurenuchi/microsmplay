#pragma once

#include "ChartParser.h"

#include <filesystem>
#include <string>
#include <vector>

struct SongEntry {
    std::filesystem::path folderPath;
    std::filesystem::path chartPath;
    std::string title;
    std::string artist;
    std::vector<size_t> chartIndices;
    std::vector<std::string> chartLabels;
};

class SongBrowser {
public:
    void scanFolder(const std::filesystem::path& folder);
    void clear();

    const std::vector<SongEntry>& songs() const { return songs_; }

    SongEntry* selectedSong() { return selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(songs_.size()) ? &songs_[selectedIndex_] : nullptr; }
    int selectedIndex() const { return selectedIndex_; }
    void setSelectedIndex(int index) { selectedIndex_ = index; }

    size_t selectedChartIndex() const { return selectedChartIndex_; }
    void setSelectedChartIndex(size_t index) { selectedChartIndex_ = index; }

    const std::string& filterText() const { return filterText_; }
    void setFilterText(const std::string& text) { filterText_ = text; }

    std::vector<SongEntry> filteredSongs() const;

private:
    std::vector<SongEntry> songs_;
    int selectedIndex_ = -1;
    size_t selectedChartIndex_ = 0;
    std::string filterText_;

    void scanFolderRecursive(const std::filesystem::path& folder);
    bool parseSongInfo(SongEntry& entry);
};
