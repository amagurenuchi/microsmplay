#include "MinaCalcAdapter.h"

#include "MinaCalc/MinaCalc.h"
#include "TimingData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

namespace {
void setupTiming(TimingData& timing, const ParsedSong& song, const ParsedChart& chart) {
    timing.setOffsetSeconds(chart.hasSplitOffset ? chart.offsetSeconds : song.offsetSeconds);

    if (chart.hasSplitTiming) {
        timing.setBpms(chart.bpms.empty() ? song.bpms : chart.bpms);
        timing.setStops(chart.stops.empty() ? song.stops : chart.stops);
        timing.setWarps(chart.warps.empty() ? song.warps : chart.warps);
        timing.setDelays(chart.delays.empty() ? song.delays : chart.delays);
        timing.setSpeeds(chart.speeds.empty() ? song.speeds : chart.speeds);
        timing.setScrolls(chart.scrolls.empty() ? song.scrolls : chart.scrolls);
    } else {
        timing.setBpms(song.bpms);
        timing.setStops(song.stops);
        timing.setWarps(song.warps);
        timing.setDelays(song.delays);
        timing.setSpeeds(song.speeds);
        timing.setScrolls(song.scrolls);
    }

    double maxBeat = 0.0;
    for (const auto& note : chart.notes) {
        if (note.beat > maxBeat) {
            maxBeat = note.beat;
        }
    }
    timing.buildLookup(maxBeat);
}

bool contributesToMsd(NoteType type) {
    switch (type) {
    case NoteType::Tap:
    case NoteType::HoldHead:
    case NoteType::RollHead:
    case NoteType::Lift:
        return true;
    default:
        return false;
    }
}

std::vector<NoteInfo> buildNoteInfo(const ParsedSong& song, const ParsedChart& chart) {
    TimingData timing;
    setupTiming(timing, song, chart);

    std::map<double, unsigned> rows;
    for (const auto& note : chart.notes) {
        if (!contributesToMsd(note.type)) {
            continue;
        }
        if (note.column < 0 || note.column >= chart.columnCount || note.column >= 31) {
            continue;
        }

        const double rowTime = timing.beatToSeconds(note.beat);
        rows[rowTime] |= (1u << static_cast<unsigned>(note.column));
    }

    std::vector<NoteInfo> out;
    out.reserve(rows.size());
    for (const auto& [rowTime, notes] : rows) {
        out.push_back({static_cast<float>(rowTime), static_cast<int>(notes)});
    }
    return out;
}
}

std::optional<float> calculateChartMsd(const ParsedSong& song, const ParsedChart& chart, float musicRate) {
    const auto values = calculateChartSkillsets(song, chart, musicRate);
    if (!values) return std::nullopt;
    return (*values)[Skill_Overall];
}

std::optional<std::array<float, 8>> calculateChartSkillsets(const ParsedSong& song, const ParsedChart& chart, float musicRate) {
    if (chart.columnCount != 4) {
        return std::nullopt;
    }

    auto noteInfo = buildNoteInfo(song, chart);
    if (noteInfo.size() <= 1) {
        return std::nullopt;
    }

    static thread_local Calc calc;
    const auto skillsets = MinaSDCalc(noteInfo, musicRate, 0.93f, static_cast<unsigned>(chart.columnCount), &calc);
    if (skillsets.size() <= static_cast<size_t>(Skill_Overall)) {
        return std::nullopt;
    }

    std::array<float, 8> result{};
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = skillsets[i];
        if (!std::isfinite(result[i])) {
            return std::nullopt;
        }
    }
    return result;
}

std::optional<std::array<float, 8>> calculateIntervalSkillsets(const ParsedSong& song, const ParsedChart& chart, double startSeconds, double endSeconds, float musicRate) {
    if (chart.columnCount != 4) return std::nullopt;
    auto all = buildNoteInfo(song, chart);
    std::vector<NoteInfo> interval;
    for (const auto& row : all) if (row.rowTime >= startSeconds && row.rowTime < endSeconds) interval.push_back(row);
    if (interval.size() <= 1) return std::nullopt;
    static thread_local Calc calc;
    const auto values = MinaSDCalc(interval, musicRate, 0.93f, static_cast<unsigned>(chart.columnCount), &calc);
    if (values.size() <= static_cast<size_t>(Skill_Overall)) return std::nullopt;
    std::array<float, 8> result{};
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = values[i];
        if (!std::isfinite(result[i])) return std::nullopt;
    }
    return result;
}
