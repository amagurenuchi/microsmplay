#include "GameState.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr size_t kInvalidIndex = std::numeric_limits<size_t>::max();
}

void GameState::initialize(const ParsedChart& chart, const TimingData& timing, double globalOffsetMs) {
    timing_ = &timing;
    notes_ = chart.notes;
    std::sort(notes_.begin(), notes_.end(), [](const Note& a, const Note& b) {
        if (a.beat != b.beat) return a.beat < b.beat;
        return a.column < b.column;
    });

    noteTimes_.resize(notes_.size());
    noteEndTimes_.assign(notes_.size(), 0.0);
    holdTailIndex_.assign(notes_.size(), kInvalidIndex);
    holdActive_.assign(notes_.size(), false);
    holdDropped_.assign(notes_.size(), false);
    lastRollTriggerTime_.assign(notes_.size(), -1e9);
    for (size_t i = 0; i < notes_.size(); ++i) {
        noteTimes_[i] = timing_->beatToSeconds(notes_[i].beat);
    }

    std::vector<size_t> openHoldByColumn(static_cast<size_t>(std::max(1, chart.columnCount)), kInvalidIndex);
    for (size_t i = 0; i < notes_.size(); ++i) {
        const auto& note = notes_[i];
        if (note.column < 0 || note.column >= static_cast<int>(openHoldByColumn.size())) {
            continue;
        }
        if (isHoldHeadType(note.type)) {
            openHoldByColumn[static_cast<size_t>(note.column)] = i;
            continue;
        }
        if (note.type == NoteType::HoldTail) {
            size_t headIndex = openHoldByColumn[static_cast<size_t>(note.column)];
            if (headIndex != kInvalidIndex) {
                holdTailIndex_[headIndex] = i;
                noteEndTimes_[headIndex] = noteTimes_[i];
                openHoldByColumn[static_cast<size_t>(note.column)] = kInvalidIndex;
            }
        }
    }

    columnCursor_.assign(static_cast<size_t>(std::max(1, chart.columnCount)), 0);
    heldColumns_.assign(static_cast<size_t>(std::max(1, chart.columnCount)), false);

    combo_ = 0;
    tally_ = {};
    lastJudgment_ = Judgment::None;
    lastJudgmentTime_ = 0.0;
    currentTime_ = 0.0;
    globalOffsetSeconds_ = globalOffsetMs / 1000.0;
}

void GameState::update(double audioSeconds) {
    const double now = audioSeconds + globalOffsetSeconds_;
    currentTime_ = now;

    for (size_t i = 0; i < notes_.size(); ++i) {
        auto& note = notes_[i];
        if (holdActive_[i]) {
            const bool isHeld = note.column >= 0 && note.column < static_cast<int>(heldColumns_.size())
                ? heldColumns_[static_cast<size_t>(note.column)]
                : false;

            if (note.type == NoteType::RollHead && now - lastRollTriggerTime_[i] > windows_.roll) {
                dropHold(i, now);
                continue;
            }

            if (!isHeld) {
                dropHold(i, now);
                continue;
            }

            if (holdTailIndex_[i] != kInvalidIndex && now >= noteEndTimes_[i]) {
                completeHold(i, now);
            }
            continue;
        }

        if (note.judged) {
            continue;
        }

        if (note.type == NoteType::Fake || note.type == NoteType::Invisible || note.type == NoteType::AutoKeysound) {
            if (now >= noteTimes_[i]) {
                resolveNoteSilently(note, true);
            }
            continue;
        }

        if (note.type == NoteType::HoldTail) {
            if (now > noteTimes_[i] + windows_.bad) {
                resolveNoteSilently(note, true);
            }
            continue;
        }

        const double delta = now - noteTimes_[i];

        if (autoplay_) {
            if (isHoldHeadType(note.type)) {
                if (delta >= 0.0) {
                    activateHold(i, Judgment::Marvelous, now);
                }
            } else if (note.type == NoteType::Mine) {
                if (delta > windows_.mine) {
                    resolveNoteSilently(note, true);
                }
            } else if (note.type == NoteType::Lift) {
                if (delta >= 0.0) {
                    applyJudgment(note, Judgment::Marvelous, now);
                }
            } else if (isTapJudgedType(note.type)) {
                if (delta >= 0.0) {
                    applyJudgment(note, Judgment::Marvelous, now);
                }
            }
        } else {
            if (note.type == NoteType::Mine) {
                if (delta > windows_.mine) {
                    resolveNoteSilently(note, true);
                }
            } else if (delta > windows_.bad) {
                if (note.type == NoteType::Lift) {
                    applyJudgment(note, Judgment::Miss, now);
                } else if (isHoldHeadType(note.type)) {
                    applyJudgment(note, Judgment::Miss, now);
                    if (holdTailIndex_[i] != kInvalidIndex) {
                        resolveNoteSilently(notes_[holdTailIndex_[i]], false);
                    }
                } else if (isTapJudgedType(note.type)) {
                    applyJudgment(note, Judgment::Miss, now);
                }
            }
        }
    }
}

void GameState::handleColumnPress(int column, double audioSeconds) {
    if (column < 0) {
        return;
    }
    const double now = audioSeconds + globalOffsetSeconds_;
    currentTime_ = now;

    if (column < static_cast<int>(heldColumns_.size())) {
        heldColumns_[static_cast<size_t>(column)] = true;
    }

    for (size_t i = 0; i < notes_.size(); ++i) {
        if (!holdActive_[i] || notes_[i].column != column) {
            continue;
        }
        if (notes_[i].type == NoteType::RollHead) {
            lastRollTriggerTime_[i] = now;
        }
    }

    for (size_t i = 0; i < notes_.size(); ++i) {
        auto& note = notes_[i];
        if (note.judged || note.column != column || note.type != NoteType::Mine) {
            continue;
        }
        if (std::abs(noteTimes_[i] - now) <= windows_.mine) {
            applyJudgment(note, Judgment::HitMine, now);
            return;
        }
        if (noteTimes_[i] - now > windows_.mine) {
            break;
        }
    }

    size_t bestIndex = static_cast<size_t>(-1);
    double bestAbsDelta = 1e9;

    for (size_t i = 0; i < notes_.size(); ++i) {
        auto& note = notes_[i];
        if (note.judged || note.column != column) {
            continue;
        }

        if (note.type == NoteType::Fake || note.type == NoteType::Invisible || note.type == NoteType::AutoKeysound ||
            note.type == NoteType::HoldTail || note.type == NoteType::Mine || note.type == NoteType::Lift) {
            continue;
        }

        const double delta = noteTimes_[i] - now;
        const double absDelta = std::abs(delta);
        if (absDelta <= windows_.bad && absDelta < bestAbsDelta) {
            bestAbsDelta = absDelta;
            bestIndex = i;
        }

        if (delta > windows_.bad) {
            break;
        }
    }

    if (bestIndex == static_cast<size_t>(-1)) {
        return;
    }

    Judgment j = judgmentForDelta(bestAbsDelta);

    if (isHoldHeadType(notes_[bestIndex].type)) {
        activateHold(bestIndex, j, now);
        return;
    }

    applyJudgment(notes_[bestIndex], j, now);
}

void GameState::handleColumnRelease(int column, double audioSeconds) {
    if (column < 0) {
        return;
    }

    const double now = audioSeconds + globalOffsetSeconds_;
    currentTime_ = now;

    if (column < static_cast<int>(heldColumns_.size())) {
        heldColumns_[static_cast<size_t>(column)] = false;
    }

    size_t bestLiftIndex = kInvalidIndex;
    double bestAbsDelta = 1e9;

    for (size_t i = 0; i < notes_.size(); ++i) {
        auto& note = notes_[i];
        if (note.judged || note.column != column || note.type != NoteType::Lift) {
            continue;
        }

        const double delta = noteTimes_[i] - now;
        const double absDelta = std::abs(delta);
        if (absDelta <= windows_.bad && absDelta < bestAbsDelta) {
            bestAbsDelta = absDelta;
            bestLiftIndex = i;
        }

        if (delta > windows_.bad) {
            break;
        }
    }

    if (bestLiftIndex != kInvalidIndex) {
        applyJudgment(notes_[bestLiftIndex], judgmentForDelta(bestAbsDelta), now);
    }
}

const std::vector<Note>& GameState::notes() const {
    return notes_;
}

int GameState::combo() const {
    return combo_;
}

const JudgmentTally& GameState::tally() const {
    return tally_;
}

std::string GameState::lastJudgmentText() const {
    return judgmentToString(lastJudgment_);
}

double GameState::lastJudgmentAgeSeconds() const {
    if (lastJudgment_ == Judgment::None) {
        return 999.0;
    }
    return currentTime_ - lastJudgmentTime_;
}

std::string GameState::judgmentToString(Judgment j) {
    switch (j) {
    case Judgment::Marvelous: return "MARVELOUS";
    case Judgment::Perfect: return "PERFECT";
    case Judgment::Great: return "GREAT";
    case Judgment::Good: return "GOOD";
    case Judgment::Bad: return "BAD";
    case Judgment::Miss: return "MISS";
    case Judgment::Held: return "HELD";
    case Judgment::LetGo: return "LET GO";
    case Judgment::HitMine: return "MINE";
    default: return "";
    }
}

void GameState::applyJudgment(Note& note, Judgment j, double nowSeconds) {
    note.judged = true;
    note.hit = (j != Judgment::Miss && j != Judgment::LetGo && j != Judgment::HitMine);

    switch (j) {
    case Judgment::Marvelous: ++tally_.marvelous; ++combo_; break;
    case Judgment::Perfect: ++tally_.perfect; ++combo_; break;
    case Judgment::Great: ++tally_.great; ++combo_; break;
    case Judgment::Good: ++tally_.good; combo_ = 0; break;
    case Judgment::Bad: ++tally_.bad; combo_ = 0; break;
    case Judgment::Miss: ++tally_.miss; combo_ = 0; break;
    case Judgment::Held: ++tally_.held; ++combo_; break;
    case Judgment::LetGo: ++tally_.letGo; combo_ = 0; break;
    case Judgment::HitMine: ++tally_.hitMine; combo_ = 0; break;
    default: break;
    }

    lastJudgment_ = j;
    lastJudgmentTime_ = nowSeconds;
}

void GameState::resolveNoteSilently(Note& note, bool hit) {
    note.judged = true;
    note.hit = hit;
}

Judgment GameState::judgmentForDelta(double absDelta) const {
    if (absDelta <= windows_.marvelous) return Judgment::Marvelous;
    if (absDelta <= windows_.perfect) return Judgment::Perfect;
    if (absDelta <= windows_.great) return Judgment::Great;
    if (absDelta <= windows_.good) return Judgment::Good;
    return Judgment::Bad;
}

bool GameState::isTapJudgedType(NoteType type) const {
    return type == NoteType::Tap || type == NoteType::Attack;
}

bool GameState::isHoldHeadType(NoteType type) const {
    return type == NoteType::HoldHead || type == NoteType::RollHead;
}

void GameState::activateHold(size_t headIndex, Judgment j, double nowSeconds) {
    applyJudgment(notes_[headIndex], j, nowSeconds);
    if (holdTailIndex_[headIndex] != kInvalidIndex) {
        holdActive_[headIndex] = true;
        holdDropped_[headIndex] = false;
        lastRollTriggerTime_[headIndex] = nowSeconds;
    }
}

void GameState::completeHold(size_t headIndex, double nowSeconds) {
    holdActive_[headIndex] = false;
    if (holdTailIndex_[headIndex] != kInvalidIndex) {
        resolveNoteSilently(notes_[holdTailIndex_[headIndex]], true);
    }
    if (!holdDropped_[headIndex]) {
        Note holdResult;
        applyJudgment(holdResult, Judgment::Held, nowSeconds);
    }
}

void GameState::dropHold(size_t headIndex, double nowSeconds) {
    if (!holdActive_[headIndex]) {
        return;
    }
    holdActive_[headIndex] = false;
    holdDropped_[headIndex] = true;
    if (holdTailIndex_[headIndex] != kInvalidIndex) {
        resolveNoteSilently(notes_[holdTailIndex_[headIndex]], false);
    }
    Note holdResult;
    applyJudgment(holdResult, Judgment::LetGo, nowSeconds);
}
