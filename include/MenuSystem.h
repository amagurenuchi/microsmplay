#pragma once

#include "SongBrowser.h"
#include "MinaCalcAdapter.h"

#include <SDL.h>
#include <imgui.h>

#include <filesystem>
#include <functional>
#include <string>
#include <optional>

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
    void updatePlayConfig(const GameConfig& config) { gameConfig_.scrollModMode = config.scrollModMode; gameConfig_.scrollSpeed = config.scrollSpeed; gameConfig_.downscroll = config.downscroll; gameConfig_.rate = config.rate; }

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
    std::filesystem::path skillCachePath_;
    size_t skillCacheChart_ = static_cast<size_t>(-1);
    float skillCacheRate_ = -1.0f;
    std::optional<std::array<float, 8>> skillCacheValues_;
    std::filesystem::path preferencesPath_;
    bool preferencesLoaded_ = false;

    void renderMenu();
    void renderSongList();
    void renderSongDetails();
    void renderFolderSelector();
    void renderPlayOptions(GameConfig& config, bool isPlaying);
    void startGame(const SongEntry& song, size_t chartIndex);

    std::filesystem::path getDefaultSongsFolder();
    std::filesystem::path getPreferencesPath() const;
    void loadPreferences();
    void savePreferences();
};
