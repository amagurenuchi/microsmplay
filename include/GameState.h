#pragma once

#include "ChartTypes.h"
#include "TimingData.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

enum class Judgment {
    Marvelous,
    Perfect,
    Great,
    Good,
    Bad,
    Miss,
    Held,
    LetGo,
    HitMine,
    None
};

struct JudgmentTally {
    int marvelous = 0;
    int perfect = 0;
    int great = 0;
    int good = 0;
    int bad = 0;
    int miss = 0;
    int held = 0;
    int letGo = 0;
    int hitMine = 0;
};

class GameState {
public:
    void initialize(const ParsedChart& chart, const TimingData& timing, double globalOffsetMs);

    void update(double audioSeconds);
    void handleColumnPress(int column, double audioSeconds);
    void handleColumnRelease(int column, double audioSeconds);

    const std::vector<Note>& notes() const;
    int combo() const;
    const JudgmentTally& tally() const;

    std::string lastJudgmentText() const;
    double lastJudgmentAgeSeconds() const;

    bool isAutoplay() const { return autoplay_; }
    void setAutoplay(bool enabled) { autoplay_ = enabled; }
    void toggleAutoplay() { autoplay_ = !autoplay_; }

private:
    struct Window {
        double marvelous = 0.0225;
        double perfect = 0.045;
        double great = 0.09;
        double good = 0.135;
        double bad = 0.18;
        double mine = 0.09;
        double roll = 0.30;
    };

    static std::string judgmentToString(Judgment j);
    void applyJudgment(Note& note, Judgment j, double nowSeconds);
    void resolveNoteSilently(Note& note, bool hit);
    Judgment judgmentForDelta(double absDelta) const;
    bool isTapJudgedType(NoteType type) const;
    bool isHoldHeadType(NoteType type) const;
    void activateHold(size_t headIndex, Judgment j, double nowSeconds);
    void completeHold(size_t headIndex, double nowSeconds);
    void dropHold(size_t headIndex, double nowSeconds);

    const TimingData* timing_ = nullptr;

    std::vector<Note> notes_;
    std::vector<double> noteTimes_;
    std::vector<double> noteEndTimes_;
    std::vector<size_t> holdTailIndex_;
    std::vector<bool> holdActive_;
    std::vector<bool> holdDropped_;
    std::vector<double> lastRollTriggerTime_;
    std::vector<bool> heldColumns_;
    std::vector<size_t> columnCursor_;

    int combo_ = 0;
    JudgmentTally tally_{};

    Judgment lastJudgment_ = Judgment::None;
    double lastJudgmentTime_ = 0.0;
    double currentTime_ = 0.0;

    double globalOffsetSeconds_ = 0.0;
    Window windows_{};
    bool autoplay_ = true;
};
