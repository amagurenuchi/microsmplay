#pragma once

#include "GameState.h"
#include "MenuSystem.h"
#include "MinaCalcAdapter.h"
#include "TimingData.h"

#include <SDL.h>

#include <string>

class Renderer {
public:
    bool initialize(SDL_Renderer* renderer, std::string& error);
    void shutdown();

    void draw(const GameState& game,
              const TimingData& timing,
              double currentAudioSeconds,
              int columnCount,
              int width,
              int height,
              double globalOffsetMs,
              ScrollModMode scrollModMode,
              float scrollSpeed,
              float musicRate,
              bool downscroll,
              double durationSeconds,
              const ParsedSong& song,
              const ParsedChart& chart);

private:
    SDL_Renderer* renderer_ = nullptr;
    bool ownsRenderer_ = false;

    void drawText(const std::string& text, int x, int y, int scale, SDL_Color color);
    void drawChar(char c, int x, int y, int scale, SDL_Color color);
    void drawSegment(int x, int y, int w, int h, SDL_Color color);
};
