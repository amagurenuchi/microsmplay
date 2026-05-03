#include "Renderer.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>

namespace {
using Glyph = std::array<const char*, 7>;

constexpr double kReferenceFieldHeight = 480.0;
constexpr double kPixelsPerBeatAtOneX = 64.0;

const std::unordered_map<char, Glyph> kFont = {
    {'0', {"01110", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'1', {"00100", "01100", "00100", "00100", "00100", "00100", "01110"}},
    {'2', {"01110", "10001", "00001", "00010", "00100", "01000", "11111"}},
    {'3', {"11110", "00001", "00001", "01110", "00001", "00001", "11110"}},
    {'4', {"00010", "00110", "01010", "10010", "11111", "00010", "00010"}},
    {'5', {"11111", "10000", "10000", "11110", "00001", "00001", "11110"}},
    {'6', {"01110", "10000", "10000", "11110", "10001", "10001", "01110"}},
    {'7', {"11111", "00001", "00010", "00100", "01000", "01000", "01000"}},
    {'8', {"01110", "10001", "10001", "01110", "10001", "10001", "01110"}},
    {'9', {"01110", "10001", "10001", "01111", "00001", "00001", "01110"}},
    {'A', {"01110", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'B', {"11110", "10001", "10001", "11110", "10001", "10001", "11110"}},
    {'C', {"01111", "10000", "10000", "10000", "10000", "10000", "01111"}},
    {'D', {"11110", "10001", "10001", "10001", "10001", "10001", "11110"}},
    {'E', {"11111", "10000", "10000", "11110", "10000", "10000", "11111"}},
    {'F', {"11111", "10000", "10000", "11110", "10000", "10000", "10000"}},
    {'G', {"01110", "10001", "10000", "10111", "10001", "10001", "01110"}},
    {'I', {"11111", "00100", "00100", "00100", "00100", "00100", "11111"}},
    {'L', {"10000", "10000", "10000", "10000", "10000", "10000", "11111"}},
    {'M', {"10001", "11011", "10101", "10101", "10001", "10001", "10001"}},
    {'O', {"01110", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'P', {"11110", "10001", "10001", "11110", "10000", "10000", "10000"}},
    {'R', {"11110", "10001", "10001", "11110", "10100", "10010", "10001"}},
    {'S', {"01111", "10000", "10000", "01110", "00001", "00001", "11110"}},
    {'T', {"11111", "00100", "00100", "00100", "00100", "00100", "00100"}},
    {'U', {"10001", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'V', {"10001", "10001", "10001", "10001", "10001", "01010", "00100"}},
    {'X', {"10001", "01010", "00100", "00100", "00100", "01010", "10001"}},
    {' ', {"00000", "00000", "00000", "00000", "00000", "00000", "00000"}},
    {':', {"00000", "00100", "00100", "00000", "00100", "00100", "00000"}},
};

SDL_Color judgmentColor(const std::string& j) {
    if (j == "MARVELOUS") return {140, 220, 255, 255};
    if (j == "PERFECT") return {255, 236, 140, 255};
    if (j == "GREAT") return {120, 255, 120, 255};
    if (j == "GOOD") return {255, 190, 120, 255};
    if (j == "BAD") return {255, 120, 120, 255};
    if (j == "MISS") return {255, 80, 80, 255};
    return {255, 255, 255, 255};
}

SDL_Color noteColor(NoteType type) {
    switch (type) {
    case NoteType::Tap: return {80, 180, 255, 255};
    case NoteType::HoldHead: return {120, 255, 160, 255};
    case NoteType::RollHead: return {180, 120, 255, 255};
    case NoteType::HoldTail: return {255, 255, 255, 255};
    case NoteType::Mine: return {255, 90, 90, 255};
    case NoteType::Lift: return {255, 220, 120, 255};
    case NoteType::Fake: return {150, 150, 150, 255};
    case NoteType::Attack: return {255, 120, 220, 255};
    case NoteType::AutoKeysound: return {120, 255, 255, 255};
    case NoteType::Invisible: return {90, 90, 110, 180};
    default: return {255, 255, 255, 255};
    }
}

double computeScrollPixelsPerSecond(const TimingData& timing, double beat, ScrollModMode mode, float value, double fieldHeight) {
    const double clampedValue = (std::max)(0.01f, value);
    const double bpm = timing.bpmAtBeat(beat);
    const double scale = fieldHeight / kReferenceFieldHeight;

    double effectiveBpm = bpm;
    switch (mode) {
    case ScrollModMode::XMod:
        effectiveBpm = bpm * clampedValue;
        break;
    case ScrollModMode::CMod:
        effectiveBpm = clampedValue;
        break;
    case ScrollModMode::MMod:
        effectiveBpm = timing.maxBpm() > 0.0 ? (bpm / timing.maxBpm()) * clampedValue : clampedValue;
        break;
    }

    return effectiveBpm * kPixelsPerBeatAtOneX * scale / 60.0;
}
}

bool Renderer::initialize(SDL_Renderer* renderer, std::string& error) {
    error.clear();
    if (!renderer) {
        error = "Null renderer passed to Renderer::initialize";
        return false;
    }
    renderer_ = renderer;
    ownsRenderer_ = false;
    return true;
}

void Renderer::shutdown() {
    renderer_ = nullptr;
}

void Renderer::draw(const GameState& game,
                    const TimingData& timing,
                    double currentAudioSeconds,
                    int columnCount,
                    int width,
                    int height,
                    double globalOffsetMs,
                    ScrollModMode scrollModMode,
                    float scrollSpeed,
                    bool downscroll) {
    if (!renderer_) {
        return;
    }

    SDL_SetRenderDrawColor(renderer_, 8, 10, 18, 255);
    SDL_RenderClear(renderer_);

    const int fieldWidth = std::min(width - 120, 480);
    const int laneGap = 4;
    const int laneWidth = (fieldWidth - laneGap * (columnCount - 1)) / std::max(1, columnCount);
    const int fieldX = (width - fieldWidth) / 2;
    const int receptorY = downscroll ? static_cast<int>(height * 0.25f) : static_cast<int>(height * 0.75f);
    const double fieldHeight = static_cast<double>(height);

    for (int c = 0; c < columnCount; ++c) {
        SDL_Rect laneRect{fieldX + c * (laneWidth + laneGap), 0, laneWidth, height};
        SDL_SetRenderDrawColor(renderer_, 20, 24, 34, 255);
        SDL_RenderFillRect(renderer_, &laneRect);

        SDL_SetRenderDrawColor(renderer_, 34, 40, 60, 255);
        SDL_RenderDrawRect(renderer_, &laneRect);

        SDL_Rect receptor{laneRect.x, receptorY - 8, laneWidth, 16};
        SDL_SetRenderDrawColor(renderer_, 235, 235, 235, 255);
        SDL_RenderFillRect(renderer_, &receptor);
    }

    for (const auto& note : game.notes()) {
        if (note.judged) {
            continue;
        }

        const double noteSeconds = timing.beatToSeconds(note.beat);
        const double deltaSeconds = noteSeconds - currentAudioSeconds;
        const double pixelsPerSecond = computeScrollPixelsPerSecond(timing, note.beat, scrollModMode, scrollSpeed, fieldHeight);
        const int y = receptorY + static_cast<int>((downscroll ? 1.0 : -1.0) * deltaSeconds * pixelsPerSecond);
        if (y < -20 || y > height + 20) {
            continue;
        }

        const int x = fieldX + note.column * (laneWidth + laneGap);
        SDL_Color color = noteColor(note.type);
        SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);

        switch (note.type) {
        case NoteType::Mine: {
            SDL_Rect h{x + laneWidth / 4, y - 3, laneWidth / 2, 6};
            SDL_Rect v{x + laneWidth / 2 - 3, y - 12, 6, 24};
            SDL_RenderFillRect(renderer_, &h);
            SDL_RenderFillRect(renderer_, &v);
            break;
        }
        case NoteType::Lift: {
            SDL_Rect body{x + 2, y - 8, laneWidth - 4, 16};
            SDL_RenderDrawRect(renderer_, &body);
            SDL_RenderDrawLine(renderer_, x + laneWidth / 2, y - 14, x + 4, y - 2);
            SDL_RenderDrawLine(renderer_, x + laneWidth / 2, y - 14, x + laneWidth - 4, y - 2);
            break;
        }
        case NoteType::Fake: {
            SDL_Rect body{x + 2, y - 8, laneWidth - 4, 16};
            SDL_RenderDrawRect(renderer_, &body);
            SDL_RenderDrawLine(renderer_, x + 2, y - 8, x + laneWidth - 2, y + 8);
            SDL_RenderDrawLine(renderer_, x + 2, y + 8, x + laneWidth - 2, y - 8);
            break;
        }
        case NoteType::HoldTail: {
            SDL_Rect body{x + laneWidth / 4, y - 6, laneWidth / 2, 12};
            SDL_RenderFillRect(renderer_, &body);
            break;
        }
        case NoteType::Invisible: {
            SDL_Rect body{x + 4, y - 6, laneWidth - 8, 12};
            SDL_RenderDrawRect(renderer_, &body);
            break;
        }
        case NoteType::Attack: {
            SDL_Rect body{x + 2, y - 10, laneWidth - 4, 20};
            SDL_RenderFillRect(renderer_, &body);
            SDL_SetRenderDrawColor(renderer_, 40, 10, 40, 255);
            SDL_RenderDrawLine(renderer_, x + laneWidth / 2, y - 8, x + laneWidth / 2, y + 8);
            SDL_RenderDrawLine(renderer_, x + laneWidth / 2, y - 8, x + laneWidth / 2 - 5, y - 2);
            SDL_RenderDrawLine(renderer_, x + laneWidth / 2, y - 8, x + laneWidth / 2 + 5, y - 2);
            break;
        }
        case NoteType::AutoKeysound: {
            SDL_Rect body{x + 2, y - 8, laneWidth - 4, 16};
            SDL_RenderFillRect(renderer_, &body);
            SDL_SetRenderDrawColor(renderer_, 20, 40, 40, 255);
            SDL_RenderDrawLine(renderer_, x + laneWidth / 2 - 6, y + 6, x + laneWidth / 2 - 6, y - 4);
            SDL_RenderDrawLine(renderer_, x + laneWidth / 2 - 6, y - 4, x + laneWidth / 2 + 1, y - 8);
            SDL_RenderDrawLine(renderer_, x + laneWidth / 2 + 1, y - 8, x + laneWidth / 2 + 1, y + 2);
            break;
        }
        case NoteType::RollHead: {
            SDL_Rect body{x + 2, y - 10, laneWidth - 4, 20};
            SDL_RenderFillRect(renderer_, &body);
            SDL_SetRenderDrawColor(renderer_, 45, 20, 70, 255);
            SDL_Rect inner{x + 7, y - 5, laneWidth - 14, 10};
            SDL_RenderDrawRect(renderer_, &inner);
            break;
        }
        case NoteType::HoldHead: {
            SDL_Rect body{x + 2, y - 10, laneWidth - 4, 20};
            SDL_RenderFillRect(renderer_, &body);
            SDL_SetRenderDrawColor(renderer_, 20, 60, 30, 255);
            SDL_Rect inner{x + 6, y - 4, laneWidth - 12, 8};
            SDL_RenderDrawRect(renderer_, &inner);
            break;
        }
        case NoteType::Tap:
        default: {
            SDL_Rect body{x + 2, y - 10, laneWidth - 4, 20};
            SDL_RenderFillRect(renderer_, &body);
            break;
        }
        }
    }

    drawText("COMBO:", 20, 20, 3, {255, 255, 255, 255});
    drawText(std::to_string(game.combo()), 20, 48, 4, {120, 255, 220, 255});

    const auto& tally = game.tally();
    drawText("M:" + std::to_string(tally.marvelous) + " P:" + std::to_string(tally.perfect), 20, height - 90, 2, {210, 210, 210, 255});
    drawText("G:" + std::to_string(tally.great) + " GD:" + std::to_string(tally.good), 20, height - 66, 2, {210, 210, 210, 255});
    drawText("B:" + std::to_string(tally.bad) + " MS:" + std::to_string(tally.miss), 20, height - 42, 2, {210, 210, 210, 255});

    std::string judgment = game.lastJudgmentText();
    if (!judgment.empty() && game.lastJudgmentAgeSeconds() < 0.6) {
        SDL_Color jc = judgmentColor(judgment);
        drawText(judgment, width / 2 - static_cast<int>(judgment.size()) * 9, height / 2, 3, jc);
    }

    if (game.isAutoplay()) {
        drawText("AUTOPLAY", width - 140, 20, 2, {255, 120, 120, 255});
        drawText("F8 TO DISABLE", width - 140, 38, 1, {180, 180, 180, 255});
    }
}

void Renderer::drawText(const std::string& text, int x, int y, int scale, SDL_Color color) {
    int penX = x;
    for (char c : text) {
        drawChar(static_cast<char>(std::toupper(static_cast<unsigned char>(c))), penX, y, scale, color);
        penX += 6 * scale;
    }
}

void Renderer::drawChar(char c, int x, int y, int scale, SDL_Color color) {
    auto it = kFont.find(c);
    if (it == kFont.end()) {
        it = kFont.find(' ');
    }

    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);

    const Glyph& g = it->second;
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (g[static_cast<size_t>(row)][static_cast<size_t>(col)] == '1') {
                SDL_Rect r{x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer_, &r);
            }
        }
    }
}

void Renderer::drawSegment(int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(renderer_, &r);
}
