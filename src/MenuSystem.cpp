#include "MenuSystem.h"

#include "ChartParser.h"

#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <nfd.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace {
std::filesystem::path resolveAudioPath(const std::filesystem::path& chartPath, const std::filesystem::path& musicPath) {
    if (musicPath.empty()) {
        return {};
    }
    std::filesystem::path audio = musicPath;
    if (audio.is_relative()) {
        audio = chartPath.parent_path() / audio;
    }
    return std::filesystem::weakly_canonical(audio);
}

void openFolderPicker(std::filesystem::path& currentFolder, SongBrowser& browser, std::string& lastError, char* folderBuffer, size_t bufferSize) {
    nfdchar_t* outPath = nullptr;
    nfdresult_t result = NFD_PickFolder(&outPath, nullptr);
    if (result == NFD_OKAY && outPath) {
        std::filesystem::path newPath(outPath);
        if (std::filesystem::exists(newPath) && std::filesystem::is_directory(newPath)) {
            currentFolder = newPath;
            std::strncpy(folderBuffer, newPath.string().c_str(), bufferSize - 1);
            folderBuffer[bufferSize - 1] = '\0';
            browser.scanFolder(currentFolder);
            lastError.clear();
        } else {
            lastError = "Invalid folder selected";
        }
        NFD_FreePath(outPath);
    } else if (result == NFD_ERROR) {
        lastError = NFD_GetError();
    }
}
}

bool MenuSystem::initialize(SDL_Window* window, SDL_Renderer* renderer) {
    renderer_ = renderer;

    NFD_Init();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 4.0f;
    style.WindowRounding = 8.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.08f, 0.12f, 0.95f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.16f, 0.25f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.20f, 0.30f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.35f, 0.50f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.45f, 0.60f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.12f, 0.18f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.28f, 0.40f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.40f, 0.55f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.50f, 0.65f, 1.0f);

    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer)) {
        return false;
    }
    if (!ImGui_ImplSDLRenderer2_Init(renderer)) {
        return false;
    }

    currentFolder_ = getDefaultSongsFolder();
    std::strncpy(folderPathBuffer_, currentFolder_.string().c_str(), sizeof(folderPathBuffer_) - 1);

    return true;
}

void MenuSystem::shutdown() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    NFD_Quit();
}

void MenuSystem::handleEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
}

void MenuSystem::update(float deltaTime) {
    if (state_ == AppState::Menu) {
        if (browser_.songs().empty() && !currentFolder_.empty()) {
            browser_.scanFolder(currentFolder_);
        }
    }
}

void MenuSystem::beginFrame() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void MenuSystem::endFrame() {
    if (showDemoWindow_) {
        ImGui::ShowDemoWindow(&showDemoWindow_);
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
}

void MenuSystem::render() {
    beginFrame();

    if (state_ == AppState::Menu) {
        renderMenu();
    }

    endFrame();
}

void MenuSystem::renderMenu() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("MicroSMPlay", nullptr, windowFlags)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Folder...", "Ctrl+O")) {
                    openFolderPicker(currentFolder_, browser_, lastError_, folderPathBuffer_, sizeof(folderPathBuffer_));
                }
                if (ImGui::MenuItem("Rescan Folder", "F5")) {
                    browser_.scanFolder(currentFolder_);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit", "Esc")) {
                    state_ = AppState::Quit;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Demo Window", nullptr, &showDemoWindow_);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (!lastError_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text("Error: %s", lastError_.c_str());
            ImGui::PopStyleColor();
            if (ImGui::Button("Clear Error")) {
                lastError_.clear();
            }
            ImGui::Separator();
        }

        renderFolderSelector();
        renderSongList();
    }
    ImGui::End();
}

void MenuSystem::renderFolderSelector() {
    ImGui::Text("Songs Folder:");
    ImGui::SameLine();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::InputText("##folder", folderPathBuffer_, sizeof(folderPathBuffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::filesystem::path newPath(folderPathBuffer_);
        if (std::filesystem::exists(newPath) && std::filesystem::is_directory(newPath)) {
            currentFolder_ = newPath;
            browser_.scanFolder(currentFolder_);
            lastError_.clear();
        } else {
            lastError_ = "Invalid folder path";
        }
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Browse...", ImVec2(90, 0))) {
        openFolderPicker(currentFolder_, browser_, lastError_, folderPathBuffer_, sizeof(folderPathBuffer_));
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        browser_.scanFolder(currentFolder_);
    }

    ImGui::Separator();

    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::PushItemWidth(250);
    if (ImGui::InputText("##filter", filterBuffer_, sizeof(filterBuffer_))) {
        browser_.setFilterText(filterBuffer_);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("(%zu songs found)", browser_.filteredSongs().size());

    ImGui::Separator();
}

void MenuSystem::renderSongList() {
    auto filtered = browser_.filteredSongs();

    ImGuiWindowFlags listFlags = ImGuiWindowFlags_HorizontalScrollbar;
    ImVec2 listSize = ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetContentRegionAvail().y);

    if (ImGui::BeginChild("SongList", listSize, true, listFlags)) {
        for (size_t i = 0; i < filtered.size(); ++i) {
            const auto& song = filtered[i];

            bool isSelected = false;
            if (const auto* sel = browser_.selectedSong()) {
                isSelected = (sel->chartPath == song.chartPath);
            }

            ImGui::PushID(static_cast<int>(i));

            if (ImGui::Selectable(song.title.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                auto it = std::find_if(browser_.songs().begin(), browser_.songs().end(),
                    [&song](const SongEntry& e) { return e.chartPath == song.chartPath; });
                if (it != browser_.songs().end()) {
                    browser_.setSelectedIndex(static_cast<int>(std::distance(browser_.songs().begin(), it)));
                }

                if (ImGui::IsMouseDoubleClicked(0) && !song.chartIndices.empty()) {
                    startGame(song, browser_.selectedChartIndex());
                }
            }

            if (isSelected && ImGui::IsItemFocused()) {
                auto it = std::find_if(browser_.songs().begin(), browser_.songs().end(),
                    [&song](const SongEntry& e) { return e.chartPath == song.chartPath; });
                if (it != browser_.songs().end()) {
                    browser_.setSelectedIndex(static_cast<int>(std::distance(browser_.songs().begin(), it)));
                }
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    renderSongDetails();
}

void MenuSystem::renderSongDetails() {
    ImVec2 detailSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    if (ImGui::BeginChild("SongDetails", detailSize, true)) {
        const auto* song = browser_.selectedSong();

        if (!song) {
            ImGui::Text("Select a song to view details");
        } else {
            ImGui::Text("Title:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", song->title.c_str());

            ImGui::Text("Artist:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", song->artist.c_str());

            ImGui::Text("Folder:");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", song->folderPath.string().c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Select Difficulty:");

            if (song->chartIndices.empty()) {
                ImGui::Text("No charts available");
            } else {
                for (size_t i = 0; i < song->chartIndices.size(); ++i) {
                    bool isSelected = (i == browser_.selectedChartIndex());
                    const std::string& desc = song->chartLabels[i];

                    if (ImGui::RadioButton(desc.c_str(), isSelected)) {
                        browser_.setSelectedChartIndex(i);
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                renderPlayOptions(gameConfig_, false);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Play", ImVec2(120, 40))) {
                    startGame(*song, browser_.selectedChartIndex());
                }
                ImGui::SameLine();
                if (ImGui::Button("Play (Auto-select)", ImVec2(140, 40))) {
                    if (!song->chartIndices.empty()) {
                        startGame(*song, browser_.selectedChartIndex());
                    }
                }
            }
        }
    }
    ImGui::EndChild();
}

void MenuSystem::renderPlayOptions(GameConfig& config, bool isPlaying) {
    ImGui::Text("Play Options:");
    const float fullWidth = ImGui::GetContentRegionAvail().x;
    const float buttonWidth = 36.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float sliderWidth = fullWidth - (buttonWidth * 2.0f) - (spacing * 2.0f);

    ImGui::Text("Scroll Mod");
    const float modeWidth = (fullWidth - spacing * 2.0f) / 3.0f;
    if (config.scrollModMode == ScrollModMode::XMod) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.36f, 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.42f, 0.66f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.31f, 0.50f, 1.0f));
    }
    if (ImGui::Button("X", ImVec2(modeWidth, 0.0f))) {
        config.scrollModMode = ScrollModMode::XMod;
    }
    if (config.scrollModMode == ScrollModMode::XMod) {
        ImGui::PopStyleColor(3);
    }
    ImGui::SameLine();
    if (config.scrollModMode == ScrollModMode::CMod) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.36f, 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.42f, 0.66f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.31f, 0.50f, 1.0f));
    }
    if (ImGui::Button("C", ImVec2(modeWidth, 0.0f))) {
        config.scrollModMode = ScrollModMode::CMod;
    }
    if (config.scrollModMode == ScrollModMode::CMod) {
        ImGui::PopStyleColor(3);
    }
    ImGui::SameLine();
    if (config.scrollModMode == ScrollModMode::MMod) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.36f, 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.42f, 0.66f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.31f, 0.50f, 1.0f));
    }
    if (ImGui::Button("M", ImVec2(modeWidth, 0.0f))) {
        config.scrollModMode = ScrollModMode::MMod;
    }
    if (config.scrollModMode == ScrollModMode::MMod) {
        ImGui::PopStyleColor(3);
    }

    const char* speedLabel = config.scrollModMode == ScrollModMode::XMod ? "X-Mod Value" :
                             config.scrollModMode == ScrollModMode::CMod ? "C-Mod BPM" :
                                                                          "M-Mod BPM";
    const float speedMin = config.scrollModMode == ScrollModMode::XMod ? 0.25f : 50.0f;
    const float speedMax = config.scrollModMode == ScrollModMode::XMod ? 8.0f : 2000.0f;
    const float speedStep = config.scrollModMode == ScrollModMode::XMod ? 0.05f : 10.0f;
    const char* speedFormat = config.scrollModMode == ScrollModMode::XMod ? "%.2fx" : "%.0f";

    ImGui::Spacing();
    ImGui::Text("%s", speedLabel);
    if (ImGui::Button("-##speed", ImVec2(buttonWidth, 0.0f))) {
        config.scrollSpeed = (std::max)(speedMin, config.scrollSpeed - speedStep);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(sliderWidth);
    ImGui::SliderFloat("##speed", &config.scrollSpeed, speedMin, speedMax, speedFormat);
    ImGui::SameLine();
    if (ImGui::Button("+##speed", ImVec2(buttonWidth, 0.0f))) {
        config.scrollSpeed = (std::min)(speedMax, config.scrollSpeed + speedStep);
    }
    if (config.scrollModMode == ScrollModMode::XMod) {
        config.scrollSpeed = std::round(config.scrollSpeed * 100.0f) / 100.0f;
    } else {
        config.scrollSpeed = std::round(config.scrollSpeed / 10.0f) * 10.0f;
    }

    ImGui::Spacing();
    ImGui::Text("Scroll Direction");
    const float toggleWidth = (fullWidth - spacing) * 0.5f;
    if (!config.downscroll) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.36f, 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.42f, 0.66f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.31f, 0.50f, 1.0f));
    }
    if (ImGui::Button("Upscroll", ImVec2(toggleWidth, 0.0f))) {
        config.downscroll = false;
    }
    if (!config.downscroll) {
        ImGui::PopStyleColor(3);
    }
    ImGui::SameLine();
    if (config.downscroll) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.36f, 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.42f, 0.66f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.31f, 0.50f, 1.0f));
    }
    if (ImGui::Button("Downscroll", ImVec2(toggleWidth, 0.0f))) {
        config.downscroll = true;
    }
    if (config.downscroll) {
        ImGui::PopStyleColor(3);
    }

    ImGui::Spacing();
    ImGui::Text("Music Rate");
    if (ImGui::Button("-##rate", ImVec2(buttonWidth, 0.0f))) {
        config.rate = (std::max)(0.5f, config.rate - 0.05f);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(sliderWidth);
    ImGui::SliderFloat("##rate", &config.rate, 0.5f, 2.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("+##rate", ImVec2(buttonWidth, 0.0f))) {
        config.rate = (std::min)(2.0f, config.rate + 0.05f);
    }
    config.rate = std::round(config.rate * 100.0f) / 100.0f;

    if (isPlaying) {
        ImGui::TextDisabled("Changes apply live during play.");
    }
}

void MenuSystem::renderPlayOverlay(GameConfig& config, bool isPaused, bool& togglePlayback, bool& requestExit) {
    ImGuiIO& io = ImGui::GetIO();
    const float panelWidth = 280.0f;
    const ImVec2 panelSize(panelWidth, io.DisplaySize.y - 24.0f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth - 12.0f, 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Play Options", nullptr, flags)) {
        renderPlayOptions(config, true);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (isPaused) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Playback paused");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "Playback running");
        }

        if (ImGui::Button(isPaused ? "Play" : "Pause", ImVec2(-1, 36))) {
            togglePlayback = true;
        }
        if (ImGui::Button("Back to Menu", ImVec2(-1, 36))) {
            requestExit = true;
        }
    }
    ImGui::End();
}

void MenuSystem::startGame(const SongEntry& song, size_t chartIndex) {
    ChartParser parser;
    std::string error;
    auto parsed = parser.parseFile(song.chartPath, error);
    if (!parsed) {
        lastError_ = "Failed to parse chart: " + error;
        return;
    }

    if (chartIndex >= song.chartIndices.size()) {
        chartIndex = 0;
    }

    size_t parsedChartIndex = song.chartIndices[chartIndex];
    if (parsedChartIndex >= parsed->charts.size()) {
        parsedChartIndex = 0;
    }

    std::filesystem::path audioPath = resolveAudioPath(song.chartPath, parsed->musicPath);
    if (audioPath.empty() || !std::filesystem::exists(audioPath)) {
        lastError_ = "Audio file not found: " + audioPath.string();
        return;
    }

    gameConfig_.chartPath = song.chartPath;
    gameConfig_.chartIndex = parsedChartIndex;
    gameConfig_.audioPath = audioPath;

    shouldStartGame_ = true;
    state_ = AppState::Playing;
}

std::filesystem::path MenuSystem::getDefaultSongsFolder() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_MYMUSIC, nullptr, 0, path))) {
        std::filesystem::path musicPath(path);
        std::filesystem::path stepmaniaPath = musicPath / "StepMania" / "Songs";
        if (std::filesystem::exists(stepmaniaPath)) {
            return stepmaniaPath;
        }
    }
#endif
    return std::filesystem::current_path();
}
