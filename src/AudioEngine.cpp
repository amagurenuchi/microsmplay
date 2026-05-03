#include "AudioEngine.h"

#include <cstring>
#include <cmath>
#include <string>

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    stop();
    if (deviceInitialized_) {
        ma_device_uninit(&device_);
        deviceInitialized_ = false;
    }
    if (decoderInitialized_) {
        ma_decoder_uninit(&decoder_);
        decoderInitialized_ = false;
    }
}

bool AudioEngine::load(const std::filesystem::path& audioFile, std::string& error) {
    error.clear();

    stop();
    if (deviceInitialized_) {
        ma_device_uninit(&device_);
        deviceInitialized_ = false;
    }
    if (decoderInitialized_) {
        ma_decoder_uninit(&decoder_);
        decoderInitialized_ = false;
    }

    if (!std::filesystem::exists(audioFile)) {
        error = "Audio file not found: " + audioFile.string();
        return false;
    }

    ma_decoder decoderProbe{};
    ma_result res = ma_decoder_init_file(audioFile.string().c_str(), nullptr, &decoderProbe);
    if (res != MA_SUCCESS) {
        error = "Failed to decode audio file (error " + std::to_string(res) + "): " + audioFile.string();
        return false;
    }
    sourceSampleRate_ = decoderProbe.outputSampleRate;
    ma_decoder_uninit(&decoderProbe);

    const float requestedRate = playbackRate_.load();
    const ma_uint32 outputSampleRate = sourceSampleRate_ == 0
        ? 48000
        : static_cast<ma_uint32>(std::max(1.0, std::round(static_cast<double>(sourceSampleRate_) * requestedRate)));
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_unknown, 0, outputSampleRate);
    res = ma_decoder_init_file(audioFile.string().c_str(), &decoderConfig, &decoder_);
    if (res != MA_SUCCESS) {
        error = "Failed to decode audio file (error " + std::to_string(res) + "): " + audioFile.string();
        return false;
    }
    decoderInitialized_ = true;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = decoder_.outputFormat;
    config.playback.channels = decoder_.outputChannels;
    config.sampleRate = decoder_.outputSampleRate;
    config.dataCallback = &AudioEngine::dataCallback;
    config.pUserData = this;

    res = ma_device_init(nullptr, &config, &device_);
    if (res != MA_SUCCESS) {
        error = "Failed to initialize playback device.";
        ma_decoder_uninit(&decoder_);
        decoderInitialized_ = false;
        return false;
    }
    deviceInitialized_ = true;

    framesPlayed_.store(0);
    sampleRate_.store(config.sampleRate > 0 ? config.sampleRate : 48000);

    return true;
}

bool AudioEngine::start(std::string& error) {
    framesPlayed_.store(0);
    if (decoderInitialized_) {
        ma_decoder_seek_to_pcm_frame(&decoder_, 0);
    }
    return resume(error);
}

bool AudioEngine::resume(std::string& error) {
    error.clear();
    if (!deviceInitialized_) {
        error = "Audio device not initialized.";
        return false;
    }
    ma_result res = ma_device_start(&device_);
    if (res != MA_SUCCESS) {
        error = "Failed to start audio device.";
        return false;
    }
    return true;
}

void AudioEngine::pause() {
    if (deviceInitialized_ && ma_device_is_started(&device_)) {
        ma_device_stop(&device_);
    }
}

void AudioEngine::stop() {
    if (deviceInitialized_) {
        ma_device_stop(&device_);
    }
}

bool AudioEngine::setPlaybackRate(float rate, std::string& error) {
    error.clear();
    if (rate <= 0.0f) {
        error = "Playback rate must be greater than zero.";
        return false;
    }

    playbackRate_.store(rate);

    if (!decoderInitialized_) {
        return true;
    }

    const ma_uint32 baseRate = sourceSampleRate_ == 0 ? sampleRate_.load() : sourceSampleRate_;
    const ma_uint32 outputRate = static_cast<ma_uint32>(std::max(1.0, std::round(static_cast<double>(baseRate) * rate)));
    ma_result res = ma_data_converter_set_rate(&decoder_.converter, baseRate, outputRate);
    if (res != MA_SUCCESS) {
        error = "Failed to update playback rate.";
        return false;
    }

    decoder_.outputSampleRate = outputRate;
    sampleRate_.store(outputRate);
    return true;
}

double AudioEngine::currentSeconds() const {
    const uint64_t frames = framesPlayed_.load();
    const uint32_t sr = sampleRate_.load();
    if (sr == 0) {
        return 0.0;
    }
    return static_cast<double>(frames) / static_cast<double>(sr);
}

bool AudioEngine::isLoaded() const {
    return decoderInitialized_ && deviceInitialized_;
}

bool AudioEngine::isPlaying() const {
    return deviceInitialized_ && ma_device_is_started(&device_);
}

float AudioEngine::playbackRate() const {
    return playbackRate_.load();
}

void AudioEngine::dataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
    (void)input;
    auto* self = static_cast<AudioEngine*>(device->pUserData);
    if (!self || !self->decoderInitialized_) {
        std::memset(output, 0, frameCount * ma_get_bytes_per_frame(device->playback.format, device->playback.channels));
        return;
    }

    ma_uint64 framesRead = 0;
    ma_result res = ma_decoder_read_pcm_frames(&self->decoder_, output, frameCount, &framesRead);
    if (res != MA_SUCCESS) {
        framesRead = 0;
    }

    if (framesRead < frameCount) {
        unsigned char* outBytes = static_cast<unsigned char*>(output);
        const ma_uint32 bpf = ma_get_bytes_per_frame(device->playback.format, device->playback.channels);
        std::memset(outBytes + framesRead * bpf, 0, (frameCount - framesRead) * bpf);
    }

    self->framesPlayed_.fetch_add(static_cast<uint64_t>(framesRead));
}
