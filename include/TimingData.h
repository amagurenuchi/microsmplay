#pragma once

#include "ChartTypes.h"

#include <string>
#include <vector>

class TimingData {
public:
    void setOffsetSeconds(double offsetSeconds);

    void setBpms(std::vector<BpmSegment> bpms);
    void setStops(std::vector<TimePauseSegment> stops);
    void setWarps(std::vector<WarpSegment> warps);
    void setDelays(std::vector<TimePauseSegment> delays);
    void setSpeeds(std::vector<SpeedSegment> speeds);
    void setScrolls(std::vector<ScrollSegment> scrolls);

    void buildLookup(double maxBeatHint);

    double beatToSeconds(double beat) const;
    double secondsToBeat(double seconds) const;
    double bpmAtBeat(double beat) const;
    double maxBpm() const;

private:
    struct Segment {
        double beatStart = 0.0;
        double beatEnd = 0.0;
        double timeStart = 0.0;
        double timeEnd = 0.0;
        double bpm = 120.0;
    };

    struct LookupPoint {
        double beat = 0.0;
        double seconds = 0.0;
    };

    double offsetSeconds_ = 0.0;

    std::vector<BpmSegment> bpms_;
    std::vector<TimePauseSegment> stops_;
    std::vector<WarpSegment> warps_;
    std::vector<TimePauseSegment> delays_;
    std::vector<SpeedSegment> speeds_;
    std::vector<ScrollSegment> scrolls_;

    std::vector<Segment> segments_;
    std::vector<LookupPoint> lookup_;
};
