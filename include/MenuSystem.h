#pragma once

#include "SongBrowser.h"

#include <SDL.h>
#include <imgui.h>

#include <filesystem>
#include <functional>
#include <string>

enum class AppState {
    Menu,
    Playing,
    Quit
};

enum class ScrollModMode {
    XMod,
    CMod,
    MMod
};

struct GameConfig {
    std::filesystem::path chartPath;
    size_t chartIndex = 0;
    std::filesystem::path audioPath;
    ScrollModMode scrollModMode = ScrollModMode::XMod;
    float scrollSpeed = 1.0f;
    bool downscroll = false;
    float rate = 1.0f;
};

class MenuSystem {
public:
    bool initialize(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();

    void handleEvent(const SDL_Event& event);
    void update(float deltaTime);
    void render();
    void beginFrame();
    void endFrame();
    void renderPlayOverlay(GameConfig& config, bool isPaused, bool& togglePlayback, bool& requestExit);

    AppState state() const { return state_; }
    void setState(AppState state) { state_ = state; }

    bool shouldStartGame() const { return shouldStartGame_; }
    void clearStartGameFlag() { shouldStartGame_ = false; }

    GameConfig gameConfig() const { return gameConfig_; }

    void setLastError(const std::string& error) { lastError_ = error; }

private:
    AppState state_ = AppState::Menu;
    bool shouldStartGame_ = false;
    GameConfig gameConfig_;
    SDL_Renderer* renderer_ = nullptr;

    SongBrowser browser_;
    std::filesystem::path currentFolder_;
    char folderPathBuffer_[512] = {};
    char filterBuffer_[128] = {};
    std::string lastError_;
    bool showDemoWindow_ = false;

    void renderMenu();
    void renderSongList();
    void renderSongDetails();
    void renderFolderSelector();
    void renderPlayOptions(GameConfig& config, bool isPlaying);
    void startGame(const SongEntry& song, size_t chartIndex);

    std::filesystem::path getDefaultSongsFolder();
};
