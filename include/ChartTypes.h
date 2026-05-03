#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct BpmSegment {
    double beat = 0.0;
    double bpm = 120.0;
};

struct TimePauseSegment {
    double beat = 0.0;
    double seconds = 0.0;
};

struct WarpSegment {
    double beat = 0.0;
    double beats = 0.0;
};

struct SpeedSegment {
    double beat = 0.0;
    double ratio = 1.0;
    double delay = 0.0;
    int unit = 0;
};

struct ScrollSegment {
    double beat = 0.0;
    double factor = 1.0;
};

enum class NoteType {
    Tap,
    HoldHead,
    RollHead,
    HoldTail,
    Mine,
    Lift,
    Fake,
    Attack,
    AutoKeysound,
    Invisible
};

struct Note {
    double beat = 0.0;
    int column = 0;
    NoteType type = NoteType::Tap;
    bool judged = false;
    bool hit = false;
};

struct ParsedChart {
    std::string stepType;
    std::string difficulty;
    int meter = 0;
    std::vector<Note> notes;
    int columnCount = 4;

    bool hasSplitOffset = false;
    double offsetSeconds = 0.0;

    bool hasSplitTiming = false;
    std::vector<BpmSegment> bpms;
    std::vector<TimePauseSegment> stops;
    std::vector<WarpSegment> warps;
    std::vector<TimePauseSegment> delays;
    std::vector<SpeedSegment> speeds;
    std::vector<ScrollSegment> scrolls;
};

struct ParsedSong {
    std::filesystem::path chartPath;
    std::filesystem::path musicPath;
    double offsetSeconds = 0.0;
    std::string title;
    std::string artist;

    std::vector<BpmSegment> bpms;
    std::vector<TimePauseSegment> stops;
    std::vector<WarpSegment> warps;
    std::vector<TimePauseSegment> delays;
    std::vector<SpeedSegment> speeds;
    std::vector<ScrollSegment> scrolls;

    std::vector<ParsedChart> charts;
};
