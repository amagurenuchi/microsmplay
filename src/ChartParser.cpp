#include "ChartParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

double parseDoubleSafe(const std::string& value, double fallback = 0.0) {
    try {
        return std::stod(trim(value));
    } catch (...) {
        return fallback;
    }
}

int parseIntSafe(const std::string& value, int fallback = 0) {
    try {
        return std::stoi(trim(value));
    } catch (...) {
        return fallback;
    }
}

template <typename SegmentT>
std::vector<SegmentT> parseBeatValueList(const std::string& content) {
    std::vector<SegmentT> out;
    std::stringstream ss(content);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (token.empty()) {
            continue;
        }
        size_t eq = token.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string beatStr = token.substr(0, eq);
        std::string valueStr = token.substr(eq + 1);

        SegmentT seg;
        seg.beat = parseDoubleSafe(beatStr);

        if constexpr (std::is_same_v<SegmentT, BpmSegment>) {
            seg.bpm = parseDoubleSafe(valueStr, 120.0);
        } else if constexpr (std::is_same_v<SegmentT, TimePauseSegment>) {
            seg.seconds = parseDoubleSafe(valueStr, 0.0);
        } else if constexpr (std::is_same_v<SegmentT, WarpSegment>) {
            seg.beats = parseDoubleSafe(valueStr, 0.0);
        } else if constexpr (std::is_same_v<SegmentT, ScrollSegment>) {
            seg.factor = parseDoubleSafe(valueStr, 1.0);
        }

        out.push_back(seg);
    }
    return out;
}

std::vector<SpeedSegment> parseSpeeds(const std::string& content) {
    std::vector<SpeedSegment> out;
    std::stringstream ss(content);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = trim(token);
        if (token.empty()) {
            continue;
        }
        std::vector<std::string> parts;
        std::stringstream s2(token);
        std::string p;
        while (std::getline(s2, p, '=')) {
            parts.push_back(trim(p));
        }
        if (parts.size() < 2) {
            continue;
        }
        SpeedSegment seg;
        seg.beat = parseDoubleSafe(parts[0]);
        seg.ratio = parseDoubleSafe(parts[1], 1.0);
        if (parts.size() > 2) seg.delay = parseDoubleSafe(parts[2], 0.0);
        if (parts.size() > 3) seg.unit = parseIntSafe(parts[3], 0);
        out.push_back(seg);
    }
    return out;
}

void parseNotesBlock(const std::string& notesRaw, ParsedChart& chart) {
    std::vector<std::string> measures;
    std::string current;
    for (char ch : notesRaw) {
        if (ch == ',') {
            measures.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        measures.push_back(current);
    }

    int inferredColumns = chart.columnCount;
    for (const auto& m : measures) {
        std::stringstream ms(m);
        std::string row;
        while (std::getline(ms, row)) {
            row = trim(row);
            if (row.empty()) {
                continue;
            }
            if (row.rfind("//", 0) == 0) {
                continue;
            }
            inferredColumns = static_cast<int>(row.size());
            break;
        }
        if (inferredColumns > 0) {
            break;
        }
    }
    chart.columnCount = std::max(1, inferredColumns);

    for (size_t measureIdx = 0; measureIdx < measures.size(); ++measureIdx) {
        std::vector<std::string> rows;
        std::stringstream ms(measures[measureIdx]);
        std::string row;
        while (std::getline(ms, row)) {
            row = trim(row);
            if (row.empty()) {
                continue;
            }
            if (row.rfind("//", 0) == 0) {
                continue;
            }
            rows.push_back(row);
        }

        if (rows.empty()) {
            continue;
        }

        const double rowsCount = static_cast<double>(rows.size());
        for (size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx) {
            const std::string& line = rows[rowIdx];
            const double beat = static_cast<double>(measureIdx) * 4.0 +
                                (static_cast<double>(rowIdx) * 4.0 / rowsCount);

            for (int col = 0; col < chart.columnCount && col < static_cast<int>(line.size()); ++col) {
                char c = line[static_cast<size_t>(col)];
                NoteType type;
                bool create = true;

                switch (c) {
                case '1':
                    type = NoteType::Tap;
                    break;
                case '2':
                    type = NoteType::HoldHead;
                    break;
                case '4':
                    type = NoteType::RollHead;
                    break;
                case '3':
                    type = NoteType::HoldTail;
                    break;
                case 'A':
                case 'a':
                    type = NoteType::Attack;
                    break;
                case 'K':
                case 'k':
                    type = NoteType::AutoKeysound;
                    break;
                case 'I':
                case 'i':
                    type = NoteType::Invisible;
                    break;
                case 'M':
                case 'm':
                    type = NoteType::Mine;
                    break;
                case 'L':
                case 'l':
                    type = NoteType::Lift;
                    break;
                case 'F':
                case 'f':
                    type = NoteType::Fake;
                    break;
                default:
                    create = false;
                    type = NoteType::Tap;
                    break;
                }

                if (!create) {
                    continue;
                }

                chart.notes.push_back({beat, col, type, false, false});
            }
        }
    }
}

std::vector<std::string> extractTagBlocks(const std::string& text, const std::string& tagName) {
    std::vector<std::string> blocks;
    std::string needle = "#" + tagName + ":";
    std::string lowerText = toLower(text);
    std::string lowerNeedle = toLower(needle);

    size_t pos = 0;
    while (true) {
        size_t start = lowerText.find(lowerNeedle, pos);
        if (start == std::string::npos) {
            break;
        }
        size_t contentStart = start + needle.size();
        size_t end = text.find(';', contentStart);
        if (end == std::string::npos) {
            break;
        }
        blocks.push_back(text.substr(contentStart, end - contentStart));
        pos = end + 1;
    }

    return blocks;
}

std::string extractFirstTag(const std::string& text, const std::string& tagName) {
    auto blocks = extractTagBlocks(text, tagName);
    if (blocks.empty()) {
        return {};
    }
    return trim(blocks.front());
}

void parseGlobalTimingFromText(const std::string& text, ParsedSong& song) {
    song.bpms = parseBeatValueList<BpmSegment>(extractFirstTag(text, "BPMS"));
    song.stops = parseBeatValueList<TimePauseSegment>(extractFirstTag(text, "STOPS"));
    song.warps = parseBeatValueList<WarpSegment>(extractFirstTag(text, "WARPS"));
    song.delays = parseBeatValueList<TimePauseSegment>(extractFirstTag(text, "DELAYS"));
    song.speeds = parseSpeeds(extractFirstTag(text, "SPEEDS"));
    song.scrolls = parseBeatValueList<ScrollSegment>(extractFirstTag(text, "SCROLLS"));

    if (song.bpms.empty()) {
        song.bpms.push_back({0.0, 120.0});
    }
}

ParsedChart parseSmNotesBlock(const std::string& block) {
    ParsedChart chart;
    std::vector<std::string> fields;
    std::string cur;
    int colonsSeen = 0;

    for (char ch : block) {
        if (ch == ':' && colonsSeen < 5) {
            fields.push_back(cur);
            cur.clear();
            ++colonsSeen;
        } else {
            cur.push_back(ch);
        }
    }
    fields.push_back(cur);

    if (fields.size() >= 1) chart.stepType = trim(fields[0]);
    if (fields.size() >= 3) chart.difficulty = trim(fields[2]);
    if (fields.size() >= 4) chart.meter = parseIntSafe(fields[3], 0);
    if (fields.size() >= 6) parseNotesBlock(fields[5], chart);

    return chart;
}

std::vector<ParsedChart> parseSmCharts(const std::string& text) {
    std::vector<ParsedChart> charts;
    for (const auto& block : extractTagBlocks(text, "NOTES")) {
        ParsedChart chart = parseSmNotesBlock(block);
        if (!chart.stepType.empty() && toLower(chart.stepType).find("dance-single") != std::string::npos) {
            charts.push_back(std::move(chart));
        }
    }
    return charts;
}

std::vector<ParsedChart> parseSscCharts(const std::string& text) {
    std::vector<ParsedChart> charts;

    size_t pos = 0;
    const std::string marker = "#NOTEDATA:";
    while (true) {
        size_t start = toLower(text).find(toLower(marker), pos);
        if (start == std::string::npos) {
            break;
        }

        size_t next = toLower(text).find(toLower(marker), start + marker.size());
        std::string chunk = text.substr(start, next == std::string::npos ? std::string::npos : next - start);

        ParsedChart chart;
        chart.stepType = extractFirstTag(chunk, "STEPSTYPE");
        chart.difficulty = extractFirstTag(chunk, "DIFFICULTY");
        chart.meter = parseIntSafe(extractFirstTag(chunk, "METER"), 0);
        std::string splitOffset = extractFirstTag(chunk, "OFFSET");
        if (!splitOffset.empty()) {
            chart.hasSplitOffset = true;
            chart.offsetSeconds = parseDoubleSafe(splitOffset, 0.0);
        }

        std::string notes = extractFirstTag(chunk, "NOTES");
        parseNotesBlock(notes, chart);

        auto splitBpms = parseBeatValueList<BpmSegment>(extractFirstTag(chunk, "BPMS"));
        auto splitStops = parseBeatValueList<TimePauseSegment>(extractFirstTag(chunk, "STOPS"));
        auto splitWarps = parseBeatValueList<WarpSegment>(extractFirstTag(chunk, "WARPS"));
        auto splitDelays = parseBeatValueList<TimePauseSegment>(extractFirstTag(chunk, "DELAYS"));
        auto splitSpeeds = parseSpeeds(extractFirstTag(chunk, "SPEEDS"));
        auto splitScrolls = parseBeatValueList<ScrollSegment>(extractFirstTag(chunk, "SCROLLS"));

        chart.hasSplitTiming = !splitBpms.empty() || !splitStops.empty() || !splitWarps.empty() ||
                               !splitDelays.empty() || !splitSpeeds.empty() || !splitScrolls.empty();

        chart.bpms = std::move(splitBpms);
        chart.stops = std::move(splitStops);
        chart.warps = std::move(splitWarps);
        chart.delays = std::move(splitDelays);
        chart.speeds = std::move(splitSpeeds);
        chart.scrolls = std::move(splitScrolls);

        if (!chart.stepType.empty() && toLower(chart.stepType).find("dance-single") != std::string::npos) {
            charts.push_back(std::move(chart));
        }

        if (next == std::string::npos) {
            break;
        }
        pos = next;
    }

    return charts;
}

} // namespace

std::optional<ParsedSong> ChartParser::parseFile(const std::filesystem::path& filePath, std::string& error) const {
    error.clear();

    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        error = "Could not open chart file: " + filePath.string();
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();

    ParsedSong song;
    song.chartPath = filePath;

    std::string lowerExt = toLower(filePath.extension().string());
    parseGlobalTimingFromText(text, song);

    song.offsetSeconds = parseDoubleSafe(extractFirstTag(text, "OFFSET"), 0.0);
    song.title = extractFirstTag(text, "TITLE");
    song.artist = extractFirstTag(text, "ARTIST");

    std::string music = extractFirstTag(text, "MUSIC");
    if (!music.empty()) {
        song.musicPath = std::filesystem::path(trim(music));
    }

    if (lowerExt == ".ssc") {
        song.charts = parseSscCharts(text);
    } else {
        song.charts = parseSmCharts(text);
    }

    if (song.charts.empty()) {
        error = "No playable dance-single charts found.";
        return std::nullopt;
    }

    return song;
}
