#include "AudioEngine.h"
#include "ChartParser.h"
#include "GameState.h"
#include "MenuSystem.h"
#include "Renderer.h"
#include "TimingData.h"

#include <SDL.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {
int mapKeyToColumn(SDL_Keycode key) {
    switch (key) {
    case SDLK_d: return 0;
    case SDLK_f: return 1;
    case SDLK_j: return 2;
    case SDLK_k: return 3;
    default: return -1;
    }
}

std::filesystem::path resolveAudioPath(const ParsedSong& song) {
    if (song.musicPath.empty()) {
        return {};
    }
    std::filesystem::path audio = song.musicPath;
    if (audio.is_relative()) {
        audio = song.chartPath.parent_path() / audio;
    }
    return std::filesystem::weakly_canonical(audio);
}

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

struct GameSession {
    ParsedSong song;
    ParsedChart chart;
    TimingData timing;
    std::filesystem::path audioPath;
    size_t chartIndex = 0;
};

bool loadGameSession(const std::filesystem::path& chartPath, size_t chartIndex, GameSession& session, std::string& error) {
    ChartParser parser;
    auto parsed = parser.parseFile(chartPath, error);
    if (!parsed) {
        return false;
    }
    session.song = *parsed;

    if (chartIndex >= session.song.charts.size()) {
        chartIndex = 0;
    }
    session.chartIndex = chartIndex;
    session.chart = session.song.charts[chartIndex];

    setupTiming(session.timing, session.song, session.chart);

    session.audioPath = resolveAudioPath(session.song);
    if (session.audioPath.empty() || !std::filesystem::exists(session.audioPath)) {
        error = "Audio file not found: " + session.audioPath.string();
        return false;
    }

    return true;
}

bool runGameSession(SDL_Window* window, SDL_Renderer* sdlRenderer, Renderer& renderer,
                    const GameConfig& config, MenuSystem& menu) {
    GameSession session;
    std::string error;
    if (!loadGameSession(config.chartPath, config.chartIndex, session, error)) {
        menu.setLastError(error);
        return false;
    }

    GameConfig liveConfig = config;

    constexpr double kGlobalOffsetMs = 0.0;

    AudioEngine audio;
    std::string audioError;
    if (!audio.setPlaybackRate(liveConfig.rate, audioError)) {
        menu.setLastError(audioError);
        return false;
    }
    if (!audio.load(session.audioPath, audioError)) {
        menu.setLastError(audioError);
        return false;
    }
    if (!audio.setPlaybackRate(liveConfig.rate, audioError)) {
        menu.setLastError(audioError);
        return false;
    }
    if (!audio.start(audioError)) {
        menu.setLastError(audioError);
        return false;
    }

    GameState game;
    game.initialize(session.chart, session.timing, kGlobalOffsetMs);
    const double durationSeconds = audio.durationSeconds();

    bool running = true;
    float appliedRate = liveConfig.rate;
    double chartTime = 0.0;
    double previousAudioSeconds = 0.0;
    while (running) {
        const double audioSecondsAtFrameStart = audio.currentSeconds();
        const double chartTimeAtFrameStart = chartTime +
            (audioSecondsAtFrameStart - previousAudioSeconds) * liveConfig.rate;
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            menu.handleEvent(ev);

            if (ev.type == SDL_QUIT) {
                running = false;
                menu.setState(AppState::Quit);
            } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (ev.key.keysym.sym == SDLK_SPACE) {
                    if (audio.isPlaying()) {
                        audio.pause();
                    } else if (!audio.resume(audioError)) {
                        menu.setLastError(audioError);
                    }
                } else if (ev.key.keysym.sym == SDLK_F8) {
                    game.toggleAutoplay();
                }
                if (!game.isAutoplay() && audio.isPlaying()) {
                    int column = mapKeyToColumn(ev.key.keysym.sym);
                    if (column >= 0 && column < session.chart.columnCount) {
                        game.handleColumnPress(column, chartTimeAtFrameStart);
                    }
                }
            } else if (ev.type == SDL_KEYUP) {
                if (!game.isAutoplay() && audio.isPlaying()) {
                    int column = mapKeyToColumn(ev.key.keysym.sym);
                    if (column >= 0 && column < session.chart.columnCount) {
                        game.handleColumnRelease(column, chartTimeAtFrameStart);
                    }
                }
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h);
                if (ev.button.y >= h - 42) {
                    const double target = durationSeconds * std::clamp(static_cast<double>(ev.button.x) / std::max(1, w), 0.0, 1.0);
                    if (audio.seekSeconds(target)) {
                        const bool autoplay = game.isAutoplay();
                        game.initialize(session.chart, session.timing, kGlobalOffsetMs);
                        game.setAutoplay(autoplay);
                        chartTime = target; previousAudioSeconds = audio.currentSeconds();
                    }
                }
            }
        }

        if (liveConfig.rate != appliedRate) {
            if (!audio.setPlaybackRate(liveConfig.rate, audioError)) {
                menu.setLastError(audioError);
                liveConfig.rate = appliedRate;
            } else {
                appliedRate = liveConfig.rate;
            }
        }

        const double audioSeconds = audio.currentSeconds();
        chartTime += (audioSeconds - previousAudioSeconds) * liveConfig.rate;
        previousAudioSeconds = audioSeconds;
        const double nowSec = chartTime;
        game.update(nowSec);

        int width = 960;
        int height = 720;
        SDL_GetWindowSize(window, &width, &height);

        SDL_SetRenderDrawColor(sdlRenderer, 8, 10, 18, 255);
        SDL_RenderClear(sdlRenderer);

        renderer.draw(game, session.timing, chartTime, session.chart.columnCount, width, height,
                      kGlobalOffsetMs, liveConfig.scrollModMode, liveConfig.scrollSpeed,
                      liveConfig.rate, liveConfig.downscroll, durationSeconds);

        bool togglePlayback = false;
        bool requestExit = false;
        menu.beginFrame();
        menu.renderPlayOverlay(liveConfig, !audio.isPlaying(), togglePlayback, requestExit);
        menu.endFrame();
        SDL_RenderPresent(sdlRenderer);

        if (togglePlayback) {
            if (audio.isPlaying()) {
                audio.pause();
            } else if (!audio.resume(audioError)) {
                menu.setLastError(audioError);
            }
        }
        if (requestExit) {
            running = false;
        }

        SDL_Delay(1);
    }

    audio.stop();
    return true;
}
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "MicroSMPlay",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        800,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer) {
        std::cerr << "Failed to create SDL renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Renderer gameRenderer;
    std::string rendererError;
    if (!gameRenderer.initialize(sdlRenderer, rendererError)) {
        std::cerr << rendererError << "\n";
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    MenuSystem menu;
    if (!menu.initialize(window, sdlRenderer)) {
        std::cerr << "Failed to initialize menu system\n";
        gameRenderer.shutdown();
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            menu.handleEvent(ev);

            if (ev.type == SDL_QUIT) {
                running = false;
            }
        }

        if (menu.state() == AppState::Quit) {
            running = false;
            continue;
        }

        if (menu.state() == AppState::Playing && menu.shouldStartGame()) {
            GameConfig config = menu.gameConfig();
            menu.clearStartGameFlag();

            runGameSession(window, sdlRenderer, gameRenderer, config, menu);

            menu.setState(AppState::Menu);
        }

        SDL_SetRenderDrawColor(sdlRenderer, 8, 10, 18, 255);
        SDL_RenderClear(sdlRenderer);

        if (menu.state() == AppState::Menu) {
            menu.update(0.016f);
            menu.render();
        }

        SDL_RenderPresent(sdlRenderer);
        SDL_Delay(1);
    }

    menu.shutdown();
    gameRenderer.shutdown();
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
