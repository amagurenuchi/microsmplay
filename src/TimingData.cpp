#include "TimingData.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
template <typename T>
void sortByBeat(std::vector<T>& entries) {
    std::sort(entries.begin(), entries.end(), [](const T& a, const T& b) {
        return a.beat < b.beat;
    });
}

constexpr double kEpsilon = 1e-8;
}

void TimingData::setOffsetSeconds(double offsetSeconds) {
    offsetSeconds_ = offsetSeconds;
}

void TimingData::setBpms(std::vector<BpmSegment> bpms) {
    bpms_ = std::move(bpms);
    if (bpms_.empty()) {
        bpms_.push_back({0.0, 120.0});
    }
    sortByBeat(bpms_);
    if (bpms_.front().beat > 0.0) {
        bpms_.insert(bpms_.begin(), {0.0, bpms_.front().bpm});
    }
}

void TimingData::setStops(std::vector<TimePauseSegment> stops) {
    stops_ = std::move(stops);
    sortByBeat(stops_);
}

void TimingData::setWarps(std::vector<WarpSegment> warps) {
    warps_ = std::move(warps);
    sortByBeat(warps_);
}

void TimingData::setDelays(std::vector<TimePauseSegment> delays) {
    delays_ = std::move(delays);
    sortByBeat(delays_);
}

void TimingData::setSpeeds(std::vector<SpeedSegment> speeds) {
    speeds_ = std::move(speeds);
    sortByBeat(speeds_);
}

void TimingData::setScrolls(std::vector<ScrollSegment> scrolls) {
    scrolls_ = std::move(scrolls);
    sortByBeat(scrolls_);
}

void TimingData::buildLookup(double maxBeatHint) {
    const double endBeat = std::max(16.0, maxBeatHint + 8.0);

    segments_.clear();
    lookup_.clear();

    std::vector<double> events;
    events.push_back(0.0);
    events.push_back(endBeat);

    for (const auto& b : bpms_) events.push_back(std::max(0.0, b.beat));
    for (const auto& s : stops_) events.push_back(std::max(0.0, s.beat));
    for (const auto& d : delays_) events.push_back(std::max(0.0, d.beat));
    for (const auto& w : warps_) {
        events.push_back(std::max(0.0, w.beat));
        events.push_back(std::max(0.0, w.beat + std::max(0.0, w.beats)));
    }

    std::sort(events.begin(), events.end());
    events.erase(std::unique(events.begin(), events.end(), [](double a, double b) {
        return std::abs(a - b) < kEpsilon;
    }), events.end());

    size_t bpmIndex = 0;
    double currentBpm = bpms_.front().bpm > 0.0 ? bpms_.front().bpm : 120.0;
    double timeCursor = 0.0;

    for (size_t i = 0; i + 1 < events.size(); ++i) {
        double beatStart = events[i];
        double beatEnd = events[i + 1];

        while (bpmIndex + 1 < bpms_.size() && bpms_[bpmIndex + 1].beat <= beatStart + kEpsilon) {
            ++bpmIndex;
            if (bpms_[bpmIndex].bpm > 0.0) {
                currentBpm = bpms_[bpmIndex].bpm;
            }
        }

        for (const auto& st : stops_) {
            if (std::abs(st.beat - beatStart) < kEpsilon && st.seconds > 0.0) {
                timeCursor += st.seconds;
            }
        }
        for (const auto& d : delays_) {
            if (std::abs(d.beat - beatStart) < kEpsilon && d.seconds > 0.0) {
                timeCursor += d.seconds;
            }
        }

        bool inWarp = false;
        for (const auto& w : warps_) {
            if (w.beats <= 0.0) {
                continue;
            }
            if (beatStart >= w.beat - kEpsilon && beatStart < w.beat + w.beats - kEpsilon) {
                inWarp = true;
                break;
            }
        }

        Segment seg;
        seg.beatStart = beatStart;
        seg.beatEnd = beatEnd;
        seg.timeStart = timeCursor;
        seg.bpm = currentBpm > 0.0 ? currentBpm : 120.0;

        if (inWarp) {
            seg.timeEnd = timeCursor;
        } else {
            seg.timeEnd = timeCursor + (beatEnd - beatStart) * 60.0 / seg.bpm;
            timeCursor = seg.timeEnd;
        }

        segments_.push_back(seg);
    }

    constexpr double lookupStep = 0.125;
    for (double beat = 0.0; beat <= endBeat + kEpsilon; beat += lookupStep) {
        lookup_.push_back({beat, beatToSeconds(beat)});
    }
}

double TimingData::beatToSeconds(double beat) const {
    if (segments_.empty()) {
        return beat * 0.5 - offsetSeconds_;
    }

    if (beat <= 0.0) {
        return -offsetSeconds_;
    }

    const Segment* last = &segments_.back();
    for (const auto& seg : segments_) {
        if (beat + kEpsilon < seg.beatEnd) {
            if (seg.timeEnd <= seg.timeStart + kEpsilon) {
                return seg.timeStart - offsetSeconds_;
            }
            const double localBeat = beat - seg.beatStart;
            return seg.timeStart + localBeat * 60.0 / seg.bpm - offsetSeconds_;
        }
        last = &seg;
    }

    if (last->timeEnd <= last->timeStart + kEpsilon) {
        return last->timeEnd - offsetSeconds_;
    }
    return last->timeEnd + (beat - last->beatEnd) * 60.0 / last->bpm - offsetSeconds_;
}

double TimingData::secondsToBeat(double seconds) const {
    if (segments_.empty()) {
        return std::max(0.0, (seconds + offsetSeconds_) * 2.0);
    }

    const double adjusted = seconds + offsetSeconds_;
    if (adjusted <= 0.0) {
        return 0.0;
    }

    for (const auto& seg : segments_) {
        if (adjusted <= seg.timeEnd + kEpsilon) {
            if (seg.timeEnd <= seg.timeStart + kEpsilon) {
                return seg.beatStart;
            }
            const double localSec = adjusted - seg.timeStart;
            return std::max(seg.beatStart, seg.beatStart + localSec * seg.bpm / 60.0);
        }
    }

    const auto& last = segments_.back();
    if (last.timeEnd <= last.timeStart + kEpsilon) {
        return last.beatEnd;
    }
    return last.beatEnd + (adjusted - last.timeEnd) * last.bpm / 60.0;
}

double TimingData::bpmAtBeat(double beat) const {
    if (segments_.empty()) {
        return 120.0;
    }

    if (beat <= 0.0) {
        return segments_.front().bpm;
    }

    for (const auto& seg : segments_) {
        if (beat < seg.beatEnd + kEpsilon) {
            return seg.bpm;
        }
    }

    return segments_.back().bpm;
}

double TimingData::maxBpm() const {
    double result = 120.0;
    for (const auto& seg : segments_) {
        result = (std::max)(result, seg.bpm);
    }
    return result;
}
