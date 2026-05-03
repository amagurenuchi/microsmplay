#pragma once

#include "miniaudio.h"

#include <atomic>
#include <filesystem>
#include <string>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool load(const std::filesystem::path& audioFile, std::string& error);
    bool start(std::string& error);
    bool resume(std::string& error);
    void stop();
    void pause();
    bool setPlaybackRate(float rate, std::string& error);

    double currentSeconds() const;
    bool isLoaded() const;
    bool isPlaying() const;
    float playbackRate() const;

private:
    static void dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);

    ma_decoder decoder_{};
    ma_device device_{};

    bool decoderInitialized_ = false;
    bool deviceInitialized_ = false;

    std::atomic<uint64_t> framesPlayed_{0};
    std::atomic<uint32_t> sampleRate_{48000};
    std::atomic<float> playbackRate_{1.0f};
    ma_uint32 sourceSampleRate_ = 0;
};
